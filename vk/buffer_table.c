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

#include <assert.h>
#include <threads.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "vk/buffer_table.h"

#include "vk/buffer.h"

#include "data_structures/indices_stack.h"
#include "data_structures/indices_queue.h"
#include "data_structures/buddy_tree.h"
#include "vk/timeline_semaphore.h"



struct sol_vk_buffer_table_region
{
	/** the hash in the map, used  */
	uint64_t identifier;

	#warning crap, can probably compress what follows this into 8 bytes...

	/** location in memory (in buddy allocator units) */
	uint32_t offset;

	/** indexes of the adjacent entries in the available region linked list */
	uint32_t prev;
	uint32_t next;

	/** essentially a reference count */
	uint32_t active_access_count;
};
/// 16 bytes with packing, 24 without

#define SOL_ARRAY_ENTRY_TYPE struct sol_vk_buffer_table_region
#define SOL_ARRAY_STRUCT_NAME sol_vk_buffer_table_region_array
#include "data_structures/array.h"

struct sol_vk_buffer_table_accessor
{
	struct sol_indices_stack retained_region_indices;
	struct sol_vk_timeline_semaphore_moment release_moment;
};

struct sol_vk_buffer_table
{
	struct sol_vk_buffer backing;

	struct sol_buddy_tree region_tree;

	/** note: accessor storage here is very similar to array, but its necessary to know which elements have been initialised */
	struct sol_vk_buffer_table_accessor* accessors;
	uint32_t accessor_space;
	uint32_t accessor_count;/** number that have been init */

	/** these have already been init */
	struct sol_indices_stack available_accessor_indices;


	/** indices of entries that are not active in any accessor but are still being retained in an attempt to reduce overhead */
	#warning NO NO NO, queue doesnt support random removal (in an efficient manner) so using a queue will not work, must use a linked list, same as image atlas
	// struct sol_indices_queue inactive_entry_indices;


	VkDeviceSize base_allocation_size;

	/** used to lock overarching structure (buddy tree and accessor use) - NYI 
	 * not sure its possible to justify this being a truly multithreaded data structure without a dedicated/monotonic upload thread that accessors are required to wait on 
	 * this is because one accessor may look it up first (and thus be responsible for uploading it) while another thinks its init but then actually reads it on GPU first (before it has been written by the other accessor) 
	 * 	^ even this requires waiting on all active accessors... 
	 * that being said... it is still possible as long as concurrent accessors can guarantee that they operate on non-overlapping sets of entries */
	// mtx_t mutex;

	_Atomic uint64_t entry_identifier_monotonic;

	struct sol_vk_buffer_table_region_array entries;

	/** this is the index of an entry/region that doesnt actually reference any location in the buffer
	 * this entry is the start-end of the linked list ring of available entries */
	uint32_t header_entry_index;
};


static inline void sol_vk_buffer_table_initialise(struct sol_vk_buffer_table* table, struct cvm_vk_device* device, const VkBufferCreateInfo* buffer_create_info, VkMemoryPropertyFlags required_properties, VkMemoryPropertyFlags desired_properties, VkDeviceSize base_allocation_size)
{
	assert((buffer_create_info->size % base_allocation_size) == 0);
	sol_vk_buffer_initialise(&table->backing, device, buffer_create_info, required_properties, desired_properties);

	sol_buddy_tree_initialise(&table->region_tree, buffer_create_info->size / base_allocation_size);

	table->accessor_count = 0;
	table->accessor_space = 64;
	table->accessors = malloc(sizeof(struct sol_vk_buffer_table_accessor) * table->accessor_space);

	sol_indices_stack_initialise(&table->available_accessor_indices, 64);

	table->base_allocation_size = base_allocation_size;
}

static inline void sol_vk_buffer_table_terminate(struct sol_vk_buffer_table* table, struct cvm_vk_device* device)
{
	#warning wait on accessors

	/** all accessors must have been released before returning **/
	assert(table->accessor_count == sol_indices_stack_count(&table->available_accessor_indices));

	sol_indices_stack_terminate(&table->available_accessor_indices);
	free(table->accessors);
	sol_buddy_tree_terminate(&table->region_tree);
	sol_vk_buffer_terminate(&table->backing, device);
}

struct sol_vk_buffer_table* sol_vk_buffer_table_create(struct cvm_vk_device* device, const VkBufferCreateInfo* buffer_create_info, VkMemoryPropertyFlags required_properties, VkMemoryPropertyFlags desired_properties, VkDeviceSize base_allocation_size)
{
	struct sol_vk_buffer_table* table = malloc(sizeof(struct sol_vk_buffer_table));
	sol_vk_buffer_table_initialise(table, device, buffer_create_info, required_properties, desired_properties, base_allocation_size);
	return table;
}

void sol_vk_buffer_table_destroy(struct sol_vk_buffer_table* table, struct cvm_vk_device* device)
{
	sol_vk_buffer_table_terminate(table, device);
	free(table);
}

static inline void sol_vk_buffer_table_accessor_initialise(struct sol_vk_buffer_table_accessor* accessor)
{
	sol_indices_stack_initialise(&accessor->retained_region_indices, 64);
	accessor->release_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;
}

static inline void sol_vk_buffer_table_accessor_terminate(struct sol_vk_buffer_table_accessor* accessor)
{
	assert(accessor->release_moment.semaphore == VK_NULL_HANDLE);
	sol_indices_stack_terminate(&accessor->retained_region_indices);
}

