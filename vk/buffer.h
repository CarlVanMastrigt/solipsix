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

#pragma once

#include <vulkan/vulkan.h>
#include <inttypes.h>

#include "data_structures/buffer.h"

struct cvm_vk_device;

struct sol_vk_buffer_copy_list;

struct sol_vk_buffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;/** may be VK_NULL_HANDLE if backed by the system */
    char* mapping;/** if non-null the buffers contents can be accessed directly from host/CPU */
    uint64_t unique_resource_identifier;

    VkMemoryPropertyFlags memory_properties;
    uint32_t memory_type_index;
};

VkResult sol_vk_buffer_initialise(struct sol_vk_buffer* buffer, struct cvm_vk_device* device, const VkBufferCreateInfo* buffer_create_info, VkMemoryPropertyFlags required_properties, VkMemoryPropertyFlags desired_properties);
void sol_vk_buffer_terminate(struct sol_vk_buffer* buffer, struct cvm_vk_device* device);

void sol_vk_buffer_flush_range(struct cvm_vk_device* device, const struct sol_vk_buffer* buffer, VkDeviceSize offset, VkDeviceSize size);

/** note: this may return a mapping of the underlying buffer if it supports host access 
 * if upload is required and upload buffer is insufficiently sized this may fail */
struct sol_buffer_segment sol_vk_buffer_prepare_copy(struct sol_vk_buffer* buffer, struct sol_vk_buffer_copy_list* copy_list, struct sol_buffer* upload_buffer, VkDeviceSize offset, VkDeviceSize size);



/** having a supervised buffer doesnt make sense as different parts of the buffer can be used in different states at different times 
 * this means barriering must be done manually */

/** does this buffer require flushes after modification from host/CPU (assumes is host visible) */
static inline bool sol_vk_buffer_requires_flush(const struct sol_vk_buffer* buffer)
{
    return !(buffer->memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}