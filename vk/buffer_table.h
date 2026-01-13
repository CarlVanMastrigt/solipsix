/**
Copyright 2026 Carl van Mastrigt

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

#include <inttypes.h>
#include <vulkan/vulkan.h>

struct cvm_vk_device;
struct sol_vk_buffer_table;
struct sol_vk_timeline_semaphore_moment;


enum sol_buffer_table_result
{
    SOL_BUFFER_TABLE_FAIL_FULL, /** there is no remaining space, need to wait for space to be made */
    SOL_BUFFER_TABLE_FAIL_MAP_FULL, /** hash map is full, need to wait for space to be made */
    SOL_BUFFER_TABLE_FAIL_ABSENT, /** if using find rather than obtain this will be returned if the entry is not already present */
    SOL_BUFFER_TABLE_FAIL_NOT_WRITABLE, /** an entry already retained is not writable, this eill be returned if that happens */
    SOL_BUFFER_TABLE_SUCCESS_FOUND, /** existing entry found; no need to initalise */
    SOL_BUFFER_TABLE_SUCCESS_INSERTED, /** existing entry not found; space was made but contents must be initialised */
};


// struct sol_buffer_table_allocation
// {
// 	VkDeviceSize offset;
// 	VkDeviceSize size;
// };

/** re-writable resources require only one user at a time, same as obtaining a non-extant entry */
#define SOL_BUFFER_TABLE_OBTAIN_FLAG_WRITE  0x00000001u

/** note: buffer_create_info->size MUST be a multiple of base_allocation_size */
struct sol_vk_buffer_table* sol_vk_buffer_table_create(struct cvm_vk_device* device, const VkBufferCreateInfo* buffer_create_info, VkMemoryPropertyFlags required_properties, VkMemoryPropertyFlags desired_properties, VkDeviceSize base_allocation_size);

void sol_vk_buffer_table_destroy(struct sol_vk_buffer_table* table, struct cvm_vk_device* device);


bool sol_vk_buffer_table_accessor_acquire(struct sol_vk_buffer_table* table, uint32_t* accessor_index_result);
bool sol_vk_buffer_table_accessor_release(struct sol_vk_buffer_table* table, uint32_t accessor_index, const struct sol_vk_timeline_semaphore_moment* release_moment);

/** acquire a unique identifier for accessing/indexing entries in the table
 * note: because a buffer can be used by multiple command buffers at once (even across queues) the concept of a transient allocation cannot be applicable the same way it is to the image atlas */
uint64_t sol_vk_buffer_table_acquire_entry_identifier(struct sol_vk_buffer_table* table);


/** note: due to being an automatically managed system: there is no guarantee that an entry that did exist previously will still exist; 
 * systems that use the buffer table must account for this */

/** if extant will ensure size matches */
enum sol_buffer_table_result sol_vk_buffer_table_entry_obtain(struct sol_vk_buffer_table* table, uint64_t entry_identifier, uint32_t accessor_index, VkDeviceSize size, uint32_t flags, VkDeviceSize* entry_offset);

/** size may be null, but onus is on caller to ensure size actually matches size that was provided on setup */
enum sol_buffer_table_result sol_vk_buffer_table_entry_find(struct sol_vk_buffer_table* table, uint64_t entry_identifier, uint32_t accessor_index, VkDeviceSize* entry_offset, VkDeviceSize* size);


/** if insertion happens but external systems fail to initialise an entry this should be called to release it
 * this must be called inside the provided accesssors acquire-release range */
bool sol_buffer_table_entry_release(struct sol_vk_buffer_table* table, uint32_t accessor_index, uint64_t entry_identifier);


#warning add utility/helper function to copy that acknowledges the fact that the buffer table may be visible to the host

struct sol_vk_buffer* sol_vk_buffer_table_access_buffer(struct sol_vk_buffer_table* table);
