/**
Copyright 2024 Carl van Mastrigt

This file is part of solipsix.

solipsix is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

solipsix is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with solipsix.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "solipsix.h"


void sol_vk_command_pool_initialise(struct sol_vk_command_pool * pool, struct cvm_vk_device * device, uint32_t device_queue_family_index, uint32_t device_queue_index)
{
    VkResult result;
    VkCommandPoolCreateInfo command_pool_create_info=(VkCommandPoolCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .queueFamilyIndex=device_queue_family_index,
    };

    result = vkCreateCommandPool(device->device, &command_pool_create_info, device->host_allocator, &pool->pool);
    assert(result == VK_SUCCESS);

    pool->device_queue_family_index=device_queue_family_index;
    pool->device_queue_index=device_queue_index;
    pool->acquired_buffer_count=0;
    pool->submitted_buffer_count=0;

    sol_vk_command_buffer_stack_initialise(&pool->buffer_stack, 0);
}

void sol_vk_command_pool_terminate(struct sol_vk_command_pool* pool, struct cvm_vk_device* device)
{
    uint32_t i;
    assert(pool->acquired_buffer_count == pool->submitted_buffer_count);///not all acquired command buffers were submitted
    struct sol_vk_command_buffer* command_buffer;

    while(sol_vk_command_buffer_stack_remove_ptr(&pool->buffer_stack, &command_buffer))
    {
        vkFreeCommandBuffers(device->device, pool->pool, 1, &command_buffer->buffer);
        sol_vk_semaphore_submit_list_terminate(&command_buffer->signal_list);
        sol_vk_semaphore_submit_list_terminate(&command_buffer->wait_list);
    }

    sol_vk_command_buffer_stack_terminate(&pool->buffer_stack);

    vkDestroyCommandPool(device->device,pool->pool,NULL);
}

void sol_vk_command_pool_reset(struct sol_vk_command_pool * pool, struct cvm_vk_device * device)
{
    vkResetCommandPool(device->device, pool->pool, 0);
    assert(pool->acquired_buffer_count == pool->submitted_buffer_count);///not all acquired command buffers were submitted
    pool->acquired_buffer_count  = 0;
    pool->submitted_buffer_count = 0;
}



void sol_vk_command_pool_acquire_command_buffer(struct sol_vk_command_pool* pool, struct cvm_vk_device* device, struct sol_vk_command_buffer* command_buffer)
{
    const uint32_t command_buffer_allocation_count = 8;

    uint32_t i;
    VkCommandBuffer command_buffer_allocation_array[command_buffer_allocation_count];
    VkResult result;
    struct sol_vk_command_buffer* new_command_buffers;
    bool acquired;

    if( ! sol_vk_command_buffer_stack_remove(&pool->buffer_stack, command_buffer))
    {
        VkCommandBufferAllocateInfo allocate_info=
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = pool->pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = command_buffer_allocation_count,
        };

        result = vkAllocateCommandBuffers(device->device, &allocate_info, command_buffer_allocation_array);
        assert(result == VK_SUCCESS);

        new_command_buffers = sol_vk_command_buffer_stack_append_many_ptr(&pool->buffer_stack, command_buffer_allocation_count);

        for(i = 0; i < command_buffer_allocation_count; i++)
        {
            new_command_buffers[i].buffer = command_buffer_allocation_array[i];
            sol_vk_semaphore_submit_list_initialise(&new_command_buffers[i].signal_list, 8);
            sol_vk_semaphore_submit_list_initialise(&new_command_buffers[i].wait_list  , 8);
        }

        acquired = sol_vk_command_buffer_stack_remove(&pool->buffer_stack, command_buffer);
        assert(acquired);
    }

    /** make sure the semaphore lists are empty */
    assert(sol_vk_semaphore_submit_list_count(&command_buffer->signal_list) == 0);
    assert(sol_vk_semaphore_submit_list_count(&command_buffer->wait_list  ) == 0);

    pool->acquired_buffer_count++;

    VkCommandBufferBeginInfo command_buffer_begin_info = (VkCommandBufferBeginInfo)
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo=NULL
    };

    CVM_VK_CHECK(vkBeginCommandBuffer(command_buffer->buffer, &command_buffer_begin_info));
}

struct sol_vk_timeline_semaphore_moment sol_vk_command_pool_submit_command_buffer(struct sol_vk_command_pool* pool, struct cvm_vk_device* device, struct sol_vk_command_buffer* command_buffer, VkPipelineStageFlags2 completion_signal_stages)
{
    struct sol_vk_timeline_semaphore_moment completion_moment;
    const cvm_vk_device_queue_family * queue_family;
    cvm_vk_device_queue * queue;
    VkResult result;

    queue_family = device->queue_families + pool->device_queue_family_index;
    assert(pool->device_queue_index < queue_family->queue_count);
    queue = queue_family->queues + pool->device_queue_index;

    result = vkEndCommandBuffer(command_buffer->buffer);
    assert(result == VK_SUCCESS);

    completion_moment = sol_vk_timeline_semaphore_generate_new_moment(&queue->timeline);
    *sol_vk_semaphore_submit_list_append_ptr(&command_buffer->signal_list) = sol_vk_timeline_semaphore_moment_submit_info(&completion_moment, completion_signal_stages);

    VkSubmitInfo2 submit_info =
    {
        .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext=NULL,
        .flags=0,
        .waitSemaphoreInfoCount = sol_vk_semaphore_submit_list_count(&command_buffer->wait_list),
        .pWaitSemaphoreInfos    = sol_vk_semaphore_submit_list_data (&command_buffer->wait_list),
        .commandBufferInfoCount=1,
        .pCommandBufferInfos=(VkCommandBufferSubmitInfo[1])
        {
            {
                .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext=NULL,
                .commandBuffer=command_buffer->buffer,
                .deviceMask=0
            }
        },
        .signalSemaphoreInfoCount = sol_vk_semaphore_submit_list_count(&command_buffer->signal_list),
        .pSignalSemaphoreInfos    = sol_vk_semaphore_submit_list_data (&command_buffer->signal_list),
    };

    result = vkQueueSubmit2(queue->queue, 1, &submit_info, VK_NULL_HANDLE);
    assert(result == VK_SUCCESS);

    sol_vk_semaphore_submit_list_reset(&command_buffer->signal_list);
    sol_vk_semaphore_submit_list_reset(&command_buffer->wait_list);

    sol_vk_command_buffer_stack_append(&pool->buffer_stack, *command_buffer);

    pool->submitted_buffer_count++;

    return completion_moment;
}











