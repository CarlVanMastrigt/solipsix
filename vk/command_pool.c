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


void cvm_vk_command_pool_initialise(cvm_vk_command_pool * pool, const cvm_vk_device * device, uint32_t device_queue_family_index, uint32_t device_queue_index)
{
    uint32_t i;

    VkCommandPoolCreateInfo command_pool_create_info=(VkCommandPoolCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .queueFamilyIndex=device_queue_family_index,
    };

    CVM_VK_CHECK(vkCreateCommandPool(device->device,&command_pool_create_info,NULL,&pool->pool));

    pool->device_queue_family_index=device_queue_family_index;
    pool->device_queue_index=device_queue_index;
    pool->acquired_buffer_count=0;
    pool->submitted_buffer_count=0;
    pool->total_buffer_count=4;
    pool->buffers=malloc(sizeof(VkCommandBuffer)*4);

    VkCommandBufferAllocateInfo allocate_info=
    {
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext=NULL,
        .commandPool=pool->pool,
        .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount=4,
    };

    CVM_VK_CHECK(vkAllocateCommandBuffers(device->device,&allocate_info,pool->buffers));
}

void cvm_vk_command_pool_terminate(cvm_vk_command_pool * pool, const cvm_vk_device * device)
{
    vkFreeCommandBuffers(device->device,pool->pool,pool->total_buffer_count,pool->buffers);
    free(pool->buffers);

    vkDestroyCommandPool(device->device,pool->pool,NULL);
}

void cvm_vk_command_pool_reset(cvm_vk_command_pool * pool, const cvm_vk_device * device)
{
    vkResetCommandPool(device->device, pool->pool, 0);
    assert(pool->acquired_buffer_count == pool->submitted_buffer_count);///not all acquired command buffers were submitted
    pool->acquired_buffer_count  = 0;
    pool->submitted_buffer_count = 0;
}






void cvm_vk_command_pool_acquire_command_buffer(cvm_vk_command_pool * pool, const cvm_vk_device * device, cvm_vk_command_buffer * command_buffer)
{
    if(pool->acquired_buffer_count==pool->total_buffer_count)
    {
        pool->buffers=realloc(pool->buffers,sizeof(VkCommandBuffer)*(pool->total_buffer_count+4));

        VkCommandBufferAllocateInfo allocate_info=
        {
            .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext=NULL,
            .commandPool=pool->pool,
            .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount=4,
        };

        CVM_VK_CHECK(vkAllocateCommandBuffers(device->device,&allocate_info,pool->buffers+pool->total_buffer_count));
        pool->total_buffer_count+=4;
    }

    command_buffer->parent_pool=pool;
    command_buffer->buffer=pool->buffers[pool->acquired_buffer_count++];
    command_buffer->signal_count=0;
    command_buffer->wait_count=0;

    VkCommandBufferBeginInfo command_buffer_begin_info=(VkCommandBufferBeginInfo)
    {
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext=NULL,
        .flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo=NULL
    };

    CVM_VK_CHECK(vkBeginCommandBuffer(command_buffer->buffer, &command_buffer_begin_info));
}

struct sol_vk_timeline_semaphore_moment cvm_vk_command_pool_submit_command_buffer(cvm_vk_command_pool* pool, const struct cvm_vk_device* device, cvm_vk_command_buffer * command_buffer, VkPipelineStageFlags2 completion_signal_stages)
{
    struct sol_vk_timeline_semaphore_moment completion_moment;
    const cvm_vk_device_queue_family * queue_family;
    cvm_vk_device_queue * queue;

    queue_family = device->queue_families + pool->device_queue_family_index;
    assert(pool->device_queue_index < queue_family->queue_count);
    queue = queue_family->queues + pool->device_queue_index;

    CVM_VK_CHECK(vkEndCommandBuffer(command_buffer->buffer));

    assert(command_buffer->signal_count<4);
    completion_moment = sol_vk_timeline_semaphore_generate_new_moment(&queue->timeline);
    command_buffer->signal_info[command_buffer->signal_count++] = sol_vk_timeline_semaphore_moment_submit_info(&completion_moment, completion_signal_stages);

    VkSubmitInfo2 submit_info=
    {
        .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext=NULL,
        .flags=0,
        .waitSemaphoreInfoCount=command_buffer->wait_count,
        .pWaitSemaphoreInfos=command_buffer->wait_info,
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
        .signalSemaphoreInfoCount=command_buffer->signal_count,
        .pSignalSemaphoreInfos=command_buffer->signal_info,
    };

    CVM_VK_CHECK(vkQueueSubmit2(queue->queue, 1, &submit_info, VK_NULL_HANDLE));

    pool->submitted_buffer_count++;

    return completion_moment;
}


void cvm_vk_command_buffer_wait_on_timeline_moment(cvm_vk_command_buffer * command_buffer, const struct sol_vk_timeline_semaphore_moment * moment, VkPipelineStageFlags2 wait_stages)
{
    assert(command_buffer->wait_count<11);

    command_buffer->wait_info[command_buffer->wait_count++]=sol_vk_timeline_semaphore_moment_submit_info(moment,wait_stages);
}

void cvm_vk_command_buffer_add_wait_info(cvm_vk_command_buffer* command_buffer, const VkSemaphoreSubmitInfo* info, uint32_t count)
{
    assert(command_buffer->wait_count + count <= 11);
    memcpy(command_buffer->wait_info + command_buffer->wait_count, info, sizeof(VkSemaphoreSubmitInfo) * count);
    command_buffer->wait_count += count;
}
void cvm_vk_command_buffer_add_signal_info(cvm_vk_command_buffer* command_buffer, const VkSemaphoreSubmitInfo* info, uint32_t count)
{
    assert(command_buffer->signal_count + count <= 4);
    memcpy(command_buffer->signal_info + command_buffer->signal_count, info, sizeof(VkSemaphoreSubmitInfo) * count);
    command_buffer->signal_count += count;
}











