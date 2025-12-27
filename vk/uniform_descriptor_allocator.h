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
#include "vk/staging_buffer.h"

struct cvm_vk_device;
struct sol_vk_staging_buffer;

struct sol_vk_uniform_descriptor_entry
{
    VkDescriptorSet set;
    uint32_t binding;
    uint32_t array_index;
    uint32_t size;
    uint32_t offset;// don't support more than 4GB staging!
};

#define SOL_STACK_ENTRY_TYPE struct sol_vk_uniform_descriptor_entry
#define SOL_STACK_STRUCT_NAME sol_vk_uniform_descriptor_entry_list
#include "data_structures/stack.h"

// reinventing the shunt buffer...
struct sol_vk_uniform_descriptor_allocator
{
    struct sol_vk_staging_buffer* staging_buffer;// presence indicates this is awaiting finalisation
    struct sol_buffer host_buffer;// require a host side buffer/allocation to move to staging (this will change)
    struct sol_vk_uniform_descriptor_entry_list descriptor_list;

    struct sol_vk_staging_buffer_allocation staging_allocation;
};

// present implementation is very inefficient
// struct sol_vk_uniform_descriptor_allocator
// {
//     size_t alignment;
//     struct sol_vk_write_descriptor_set_list write_list;
//     struct sol_vk_descriptor_buffer_info_list info_list;
//     struct sol_buffer host_buffer;// require a host side buffer/allocation to move to staging
// };

void sol_vk_uniform_descriptor_allocator_initialise(struct sol_vk_uniform_descriptor_allocator* uniform_allocator, struct cvm_vk_device* device, size_t max_size);
void sol_vk_uniform_descriptor_allocator_termiante(struct sol_vk_uniform_descriptor_allocator* uniform_allocator);


bool sol_vk_uniform_descriptor_allocator_append(struct sol_vk_uniform_descriptor_allocator* uniform_allocator, VkDescriptorSet set, uint32_t binding, uint32_t array_index, void* data, size_t size);

void sol_vk_uniform_descriptor_allocator_upload(struct sol_vk_uniform_descriptor_allocator* uniform_allocator, struct cvm_vk_device* device, struct sol_vk_staging_buffer* staging_buffer);

/** also resets the buffer */
void sol_vk_uniform_descriptor_allocator_finalise(struct sol_vk_uniform_descriptor_allocator* uniform_allocator, const struct sol_vk_timeline_semaphore_moment* release_moment);


