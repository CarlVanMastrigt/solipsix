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

#include "data_structures/buffer.h"

#include "vk/buffer.h"
#include "vk/buffer_utils.h"

#include "cvm_vk.h"


VkResult sol_vk_buffer_initialise(struct sol_vk_buffer* buffer, struct cvm_vk_device* device, const VkBufferCreateInfo* buffer_create_info, VkMemoryPropertyFlags required_properties, VkMemoryPropertyFlags desired_properties)
{
    VkResult result;
    VkResult map_result;
    uint32_t memory_type_index = SOL_U32_INVALID;
    void* mapping;

    *buffer = (struct sol_vk_buffer)
    {
        .buffer = VK_NULL_HANDLE,
        .memory = VK_NULL_HANDLE,
        .mapping = NULL,
    };

    VkMemoryDedicatedRequirements dedicated_requirements =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
        .pNext = NULL,
    };
    VkMemoryRequirements2 memory_requirements =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .pNext = &dedicated_requirements,
    };

    result = vkCreateBuffer(device->device, buffer_create_info, device->host_allocator, &buffer->buffer);

    if(result == VK_SUCCESS)
    {
        assert(buffer->buffer != VK_NULL_HANDLE);

        const VkBufferMemoryRequirementsInfo2 buffer_requirements_info =
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
            .pNext = NULL,
            .buffer = buffer->buffer,
        };

        vkGetBufferMemoryRequirements2(device->device, &buffer_requirements_info, &memory_requirements);

        if(!sol_vk_find_appropriate_memory_type(device, &memory_type_index, memory_requirements.memoryRequirements.memoryTypeBits, required_properties | desired_properties))
        {
            if(!sol_vk_find_appropriate_memory_type(device, &memory_type_index, memory_requirements.memoryRequirements.memoryTypeBits, required_properties))
            {
                result = VK_RESULT_MAX_ENUM;
            }
        }
    }

    if(result == VK_SUCCESS)
    {
        #warning unless dedicated allocation is required; can probably use an existing memory allocation with some offset (same w/ binding) -- possibly using a buddy allocator on device?
        const bool use_dedicated_allocation = dedicated_requirements.prefersDedicatedAllocation || dedicated_requirements.requiresDedicatedAllocation;

        const VkMemoryDedicatedAllocateInfo dedicated_allocate_info =
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            .pNext = NULL,
            .image = VK_NULL_HANDLE,
            .buffer  = (use_dedicated_allocation) ? buffer->buffer : VK_NULL_HANDLE,
        };

        const VkMemoryAllocateInfo memory_allocate_info =
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &dedicated_allocate_info,
            .allocationSize  = memory_requirements.memoryRequirements.size,
            .memoryTypeIndex = memory_type_index
        };

        buffer->memory_type_index = memory_type_index;
        buffer->memory_properties = device->memory_properties.memoryTypes[memory_type_index].propertyFlags;

        result = vkAllocateMemory(device->device, &memory_allocate_info, device->host_allocator, &buffer->memory);
    }

    if(result == VK_SUCCESS)
    {
        assert(buffer->memory != VK_NULL_HANDLE);

        result = vkBindBufferMemory(device->device, buffer->buffer, buffer->memory, 0);
    }

    if(result == VK_SUCCESS)
    {
        if(buffer->memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            /** buffer is host visible and was requested as such, so map memory */
            const VkMemoryMapInfo memory_map_info =
            {
                .sType = VK_STRUCTURE_TYPE_MEMORY_MAP_INFO,
                .pNext = NULL,
                .flags = 0,
                .memory = buffer->memory,
                .offset = 0,
                .size = VK_WHOLE_SIZE,
            };

            map_result = vkMapMemory2(device->device, &memory_map_info, &mapping);
            buffer->mapping = mapping;

            if(map_result != VK_SUCCESS)
            {
                buffer->mapping = NULL;
                if(required_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                {
                    /** if host visibility was required; failure to map should be considered a complete failure */
                    result = map_result;
                    fprintf(stderr, "unable to map host visible buffer");
                }
            }
        }
    }

    if(result == VK_SUCCESS)
    {
    	buffer->unique_resource_identifier = sol_vk_resource_unique_identifier_acquire(device);
    }
    else
    {
        sol_vk_buffer_terminate(buffer, device);
    }

    return result;
}

void sol_vk_buffer_terminate(struct sol_vk_buffer* buffer, struct cvm_vk_device* device)
{
    VkResult unmap_result;

    if(buffer->mapping != NULL)
    {
        const VkMemoryUnmapInfo memory_unmap_info =
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO,
            .pNext = NULL,
            .flags = 0,
            .memory = buffer->memory,
        };

        unmap_result = vkUnmapMemory2(device->device, &memory_unmap_info);
        assert(unmap_result == VK_SUCCESS);
        buffer->mapping = NULL;
    }
    if(buffer->buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device->device, buffer->buffer, device->host_allocator);
        buffer->buffer = VK_NULL_HANDLE;
    }
    if(buffer->memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->device, buffer->memory, device->host_allocator);
        buffer->memory = VK_NULL_HANDLE;
    }
}

void sol_vk_buffer_flush_range(struct cvm_vk_device* device, const struct sol_vk_buffer* buffer, VkDeviceSize offset, VkDeviceSize size)
{
    VkResult flush_result;

    assert(buffer->mapping);/** invalid to flush a range on an unmapped buffer */

    if(sol_vk_buffer_requires_flush(buffer))
    {
        const VkMappedMemoryRange flush_range = (VkMappedMemoryRange)
        {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .pNext = NULL,
            .memory = buffer->memory,
            .offset = offset,
            .size = size,
        };

        flush_result = vkFlushMappedMemoryRanges(device->device, 1, &flush_range);
        assert(flush_result == VK_SUCCESS);
    }
}

struct sol_buffer_segment sol_vk_buffer_prepare_copy(struct sol_vk_buffer* buffer, struct sol_vk_buffer_copy_list* copy_list, struct sol_buffer* upload_buffer, VkDeviceSize offset, VkDeviceSize size)
{
    if(buffer->mapping)
    {
        //
    }
    else
    {
        assert(upload_buffer);
        assert(copy_list);
    }
}
