/**
Copyright 2025 Carl van Mastrigt

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

#include <assert.h>

#include "vk/uniform_descriptor_allocator.h"
#include "vk/staging_buffer.h"



/** 
 * instead: upload small descriptors and cache on reasonable scale with segments stored ad returned in array? (avoids a lot of overhead)
 * this runs into issues with staging buffer not being multi-threaded (staging becomes an issue), as it would need to allocate new staging segments while writing data...
 * it WOULD be more compact though
 * if staging were single threaded and unshared the solution becomes much simpler...
 * either way the mehanics of smaller sets of writes do seem preferable...
 * fuck it, use the mutex locked staging backing (already is mutexed) with a stack of segments to release (segments could reference each other as a kind of "extend segment" and release internally)
 * 
 * could instead reserve the space from a dedicated staging buffer in an ad-hoc fashion per submodule and recycle/re-use that space in an ad-hoc fashion? 
 * ^ really not the best solution (especially for large uploads), could probably be better with task system integration???
 * 
 * some kind of compromise in the form of (semi?) dynamic(?) space allocated per (vulkan) QUEUE might be better, this matches freeing of stuff with the actual completion order of its use
 *  ^ this doesnt support multi-queue data use (which would be quite good...)
 * */

void sol_vk_uniform_descriptor_allocator_initialise(struct sol_vk_uniform_descriptor_allocator* uniform_allocator, struct cvm_vk_device* device, size_t max_size)
{
    #warning not 100% certain the below alignment is correct/enough in all cases
    VkDeviceSize alignment = sol_vk_buffer_alignment_requirements(device, device->properties.limits.minUniformBufferOffsetAlignment);//minUniformBufferOffsetAlignment

    uniform_allocator->staging_buffer = NULL;
    sol_vk_uniform_descriptor_entry_list_initialise(&uniform_allocator->descriptor_list, 16);
    sol_buffer_initialise(&uniform_allocator->host_buffer, max_size, alignment);
}

void sol_vk_uniform_descriptor_allocator_termiante(struct sol_vk_uniform_descriptor_allocator* uniform_allocator)
{
    assert(uniform_allocator->staging_buffer == NULL);
    /* should not yet have an allocation in the actual staging (API use issue) */

    sol_vk_uniform_descriptor_entry_list_terminate(&uniform_allocator->descriptor_list);
    sol_buffer_terminate(&uniform_allocator->host_buffer);
}

bool sol_vk_uniform_descriptor_allocator_append(struct sol_vk_uniform_descriptor_allocator* uniform_allocator, VkDescriptorSet set, uint32_t binding, uint32_t array_index, void* data, size_t size)
{
    struct sol_buffer_segment host_buffer_segment;

    assert(uniform_allocator->staging_buffer == NULL);
    /* should not yet have an allocation in the actual staging (API use issue) */

    host_buffer_segment = sol_buffer_fetch_aligned_segment(&uniform_allocator->host_buffer, size, 0);// use alignment of buffer

    if(host_buffer_segment.ptr)
    {
        memcpy(host_buffer_segment.ptr, data, size);

        *sol_vk_uniform_descriptor_entry_list_append_ptr(&uniform_allocator->descriptor_list) = (struct sol_vk_uniform_descriptor_entry)
        {
            .set = set,
            .binding = binding,
            .array_index = array_index,
            .size = size,
            .offset = host_buffer_segment.offset,
        };
        return true;
    }
    return false;
}

void sol_vk_uniform_descriptor_allocator_upload(struct sol_vk_uniform_descriptor_allocator* uniform_allocator, struct cvm_vk_device* device, struct sol_vk_staging_buffer* staging_buffer)
{
    const uint32_t write_batch_size = 8;
    VkDescriptorBufferInfo buffer_infos[write_batch_size];
    VkWriteDescriptorSet writes[write_batch_size];

    struct sol_vk_uniform_descriptor_entry entry;
    uint32_t i;
    bool removed_entry;
    VkDeviceSize absolute_offset;
    VkDeviceSize total_size;

    assert(uniform_allocator->staging_buffer == NULL);
    /* should not yet have an allocation in the actual staging (API use issue) */
    uniform_allocator->staging_buffer = staging_buffer;

    total_size = uniform_allocator->host_buffer.used_space;

    uniform_allocator->staging_allocation = sol_vk_staging_buffer_allocation_acquire(staging_buffer, device, total_size, 1);

    sol_buffer_copy(&uniform_allocator->host_buffer ,uniform_allocator->staging_allocation.mapping);
    sol_buffer_reset(&uniform_allocator->host_buffer);

    absolute_offset = uniform_allocator->staging_allocation.acquired_offset;

    sol_vk_staging_buffer_allocation_flush_range(staging_buffer, device, &uniform_allocator->staging_allocation, 0, total_size);

    for(i = 0; i < write_batch_size; i++)
    {
        buffer_infos[i] = (VkDescriptorBufferInfo)
        {
            .buffer = uniform_allocator->staging_allocation.acquired_buffer,
            // need to set rest later
        };

        writes[i] = (VkWriteDescriptorSet)
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            // .dstSet = set,
            // .dstBinding = binding,
            // .dstArrayElement = array_index,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = buffer_infos + i,
        };
    }

    i = 0;
    do
    {
        removed_entry = sol_vk_uniform_descriptor_entry_list_withdraw(&uniform_allocator->descriptor_list, &entry);
        if(removed_entry)
        {
            buffer_infos[i].offset = absolute_offset + entry.offset;
            buffer_infos[i].range = entry.size;

            writes[i].dstSet = entry.set;
            writes[i].dstBinding = entry.binding;
            writes[i].dstArrayElement = entry.array_index;

            i++;
        }

        if((!removed_entry && i) || i==write_batch_size)
        {
            vkUpdateDescriptorSets(device->device, i, writes, 0, NULL);
            i = 0;
        }
    }
    while(removed_entry);
}

void sol_vk_uniform_descriptor_allocator_finalise(struct sol_vk_uniform_descriptor_allocator* uniform_allocator, const struct sol_vk_timeline_semaphore_moment* release_moment)
{
    assert(uniform_allocator->staging_buffer);
    /* should not yet have an allocation in the actual staging (API use issue) */

    assert(sol_vk_uniform_descriptor_entry_list_count(&uniform_allocator->descriptor_list) == 0);
    /* should have removed all descriptors to write as part of upload */

    sol_vk_staging_buffer_allocation_release(uniform_allocator->staging_buffer, &uniform_allocator->staging_allocation, release_moment);
    uniform_allocator->staging_buffer = NULL;
}
