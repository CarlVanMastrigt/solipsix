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

#include "data_structures/indices_list.h"
#include "data_structures/buddy_tree.h"
#include "vk/timeline_semaphore.h"



struct sol_vk_buffer_table_region
{
	/** the hash in the map, necessary to store for removal purposes */
	uint64_t identifier;

	/** TODO: consider compressing this into 8 bytes */

	/** indexes of the adjacent entries in the available region linked list 
	 * linked list is required for random removal upon re-activation of available regions */
	uint32_t prev;
	uint32_t next;

	/** location in memory (in buddy allocator units) */
	uint32_t offset;

	/** essentially a reference count */
	uint32_t active_access_count : 23;

	/** has the content been initialised */
	uint32_t initialised : 1;

	/** when the content was initialised which accessor slot performed the initialisation
	 * subsequent accesses using the same slot will "see" the region as initialised 
	 * this is because an accessor acts like an ordered queue */
	uint32_t initialising_accessor_slot : 8;
};


#define SOL_ARRAY_ENTRY_TYPE struct sol_vk_buffer_table_region
#define SOL_ARRAY_STRUCT_NAME sol_vk_buffer_table_region_array
#include "data_structures/array.h"

/** represents the range of accesses performed between an accessor acquire and release pair */
struct sol_vk_buffer_table_access_range
{
	struct sol_indices_stack retained_region_indices;
	/** this access range */
	struct sol_indices_stack initialised_region_indices;
	struct sol_vk_timeline_semaphore_moment release_moment;
	/** TODO: make above a sequence?? */
};


struct sol_vk_buffer_table_accessor
{
	struct sol_vk_timeline_semaphore_moment most_recent_range_release_moment;
	/** TODO: make above a sequence?? */

	uint32_t active_range_index;

	bool active;
};



struct sol_vk_buffer_table
{
	struct sol_vk_buffer backing;

	struct sol_buddy_tree region_tree;



	/** note: accessor storage here is very similar to array, but its necessary to know which elements have been initialised */
	struct sol_vk_buffer_table_access_range* access_ranges;
	uint32_t access_range_space;
	uint32_t access_range_count;/** number that have been init */

	/** these have already been init but can be used */
	struct sol_indices_list available_access_range_indices;
	/** these are waiting on their release moment to pass */
	struct sol_indices_list retained_access_range_indices;




	struct sol_vk_buffer_table_accessor* accessors;
	uint32_t accessor_slot_count;



	struct sol_vk_buffer_table_region_array region_entries;

	/** this is the index of an entry/region that doesnt actually reference any location in the buffer
	 * this entry is the start-end of the linked list ring of available entries (this will always be zero) */
	// uint32_t header_region_entry_index;


	VkDeviceSize base_allocation_size;

	/** used to lock overarching structure (buddy tree and accessor use) - NYI 
	 * not sure its possible to justify this being a truly multithreaded data structure without a dedicated/monotonic upload thread that accessors are required to wait on 
	 * this is because one accessor may look it up first (and thus be responsible for uploading it) while another thinks its init but then actually reads it on GPU first (before it has been written by the other accessor) 
	 * 	^ even this requires waiting on all active accessors... 
	 * that being said... it is still possible as long as concurrent accessors can guarantee that they operate on non-overlapping sets of entries */
	// mtx_t mutex;


	#warning if accessors have "slots" that allow one to follow another AND regions/resources have state (created vs initialised vs dynamic) it should be possible to share resources between accesors (and accessor slots) in a multithreaded way - TODO
	/** ^ would be: a slot is chosed (by caller) to be responsible for initialising a region; others may query it and get "uninit" (they must use find) 
	 * could even encode the accessor slot that is permitted to create a resource in its identifier! */

	_Atomic uint64_t current_entry_identifier;
};


static inline void sol_vk_buffer_table_initialise(struct sol_vk_buffer_table* table, struct cvm_vk_device* device, const struct sol_vk_buffer_table_create_information* create_information)
{
	struct sol_vk_buffer_table_region* header_region_entry;
	const uint32_t expected_range_count = sol_u32_exp_ge(create_information->accessor_slot_count * 2);
	uint32_t header_region_entry_index, accessor_slot_index;

	assert((create_information->buffer_create_info.size % create_information->base_allocation_size) == 0);
	sol_vk_buffer_initialise(&table->backing, device, &create_information->buffer_create_info, create_information->required_properties, create_information->desired_properties);

	sol_buddy_tree_initialise(&table->region_tree, create_information->buffer_create_info.size / create_information->base_allocation_size);

	table->access_range_count = 0;
	/** want a po2 number of them, but gte than the count */
	table->access_range_space = expected_range_count;
	table->access_ranges = malloc(sizeof(struct sol_vk_buffer_table_access_range) * table->access_range_space);

	sol_indices_list_initialise(&table->available_access_range_indices, expected_range_count);
	sol_indices_list_initialise(&table->retained_access_range_indices, expected_range_count);


	table->accessors = malloc(sizeof(struct sol_vk_buffer_table_accessor) * create_information->accessor_slot_count);
	table->accessor_slot_count = create_information->accessor_slot_count;
	for(accessor_slot_index = 0; accessor_slot_index < create_information->accessor_slot_count; accessor_slot_index++)
	{
		table->accessors[accessor_slot_index] = (struct sol_vk_buffer_table_accessor)
		{
			#warning instead of null make this a device created dummy moment using a dummy semaphore
			.most_recent_range_release_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL,
			.active = false,
		};
	}


	/** make this count larger */
	sol_vk_buffer_table_region_array_initialise(&table->region_entries, 256);

	header_region_entry = sol_vk_buffer_table_region_array_append_ptr(&table->region_entries, &header_region_entry_index);
	/** is assumed the header entry gets index 0 */
	assert(header_region_entry_index == 0);
	*header_region_entry = (struct sol_vk_buffer_table_region)
	{
		.next = header_region_entry_index,
		.prev = header_region_entry_index,
	};





	table->base_allocation_size = create_information->base_allocation_size;
}

static inline void sol_vk_buffer_table_terminate(struct sol_vk_buffer_table* table, struct cvm_vk_device* device)
{
	struct sol_vk_buffer_table_region* header_region_entry;
	#warning wait on accessors

	#warning clean up regions including header

	/** is assumed the header entry gets index 0 */
	header_region_entry = sol_vk_buffer_table_region_array_remove_ptr(&table->region_entries, 0);
	/** the available ring linked list (header_region_entry) should be empty */
	assert(header_region_entry->identifier == 0 && header_region_entry->prev == 0 && header_region_entry->next == 0);


	/** all accessors must have been released before returning **/
	assert(table->access_range_count == sol_indices_list_count(&table->available_access_range_indices));
	assert(sol_vk_buffer_table_region_array_is_empty(&table->region_entries));

	sol_vk_buffer_table_region_array_terminate(&table->region_entries);


	/** free access ranges */
	sol_indices_list_terminate(&table->available_access_range_indices);
	sol_indices_list_terminate(&table->retained_access_range_indices);
	free(table->access_ranges);

	/** free accessor slot backing (i.e. the accessors) */
	free(table->accessors);


	sol_buddy_tree_terminate(&table->region_tree);
	sol_vk_buffer_terminate(&table->backing, device);
}

struct sol_vk_buffer_table* sol_vk_buffer_table_create(struct cvm_vk_device* device, const struct sol_vk_buffer_table_create_information* create_information)
{
	struct sol_vk_buffer_table* table = malloc(sizeof(struct sol_vk_buffer_table));
	sol_vk_buffer_table_initialise(table, device, create_information);
	return table;
}

void sol_vk_buffer_table_destroy(struct sol_vk_buffer_table* table, struct cvm_vk_device* device)
{
	sol_vk_buffer_table_terminate(table, device);
	free(table);
}

uint64_t sol_vk_buffer_table_acquire_entry_identifier(struct sol_vk_buffer_table* table)
{
	uint64_t old_entry_identifier, new_entry_identifier;

	old_entry_identifier = atomic_load_explicit(&table->current_entry_identifier, memory_order_relaxed);

	do
	{
		/** this is using the same LCG as image atlas, perhaps one should be changed */
		new_entry_identifier = old_entry_identifier * 0x5851F42D4C957F2Dlu + 0x7A4111AC0FFEE60Dlu;
	}
	while(atomic_compare_exchange_weak_explicit(&table->current_entry_identifier, &old_entry_identifier, new_entry_identifier, memory_order_relaxed, memory_order_relaxed));

	return new_entry_identifier;
}




static inline void sol_vk_buffer_table_access_range_initialise(struct sol_vk_buffer_table_access_range* access_range)
{
	sol_indices_stack_initialise(&access_range->retained_region_indices, 64);
	access_range->release_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;
}

static inline void sol_vk_buffer_table_access_range_terminate(struct sol_vk_buffer_table_access_range* access_range)
{
	assert(access_range->release_moment.semaphore == VK_NULL_HANDLE);
	sol_indices_stack_terminate(&access_range->retained_region_indices);
}


static inline void sol_vk_buffer_table_try_releasing_retained_access_ranges(struct sol_vk_buffer_table* table, struct cvm_vk_device *device)
{
	uint32_t retained_range_list_index;
	uint32_t retained_range_index;
	struct sol_vk_buffer_table_access_range* retained_range;

	retained_range_list_index = 0;

	while(retained_range_list_index < sol_indices_list_count(&table->retained_access_range_indices))
	{
		retained_range_index = sol_indices_list_get_entry(&table->retained_access_range_indices, retained_range_list_index);
		retained_range = table->access_ranges + retained_range_index;

		if(sol_vk_timeline_semaphore_moment_query(&retained_range->release_moment, device))
		{
			/** accessors work has completed, i.e. it has been released */


			/** move this range from the retained list to the available list */
			sol_indices_list_remove_entry(&table->retained_access_range_indices, retained_range_list_index);
			sol_indices_list_append(&table->available_access_range_indices, retained_range_index);
		}
		else
		{
			retained_range_list_index++;
		}
	}
}

struct sol_vk_timeline_semaphore_moment sol_vk_buffer_table_accessor_acquire(struct sol_vk_buffer_table* table, uint32_t accessor_slot, struct cvm_vk_device *device)
{
	struct sol_vk_buffer_table_accessor* accessor;
	uint32_t access_range_index;

	sol_vk_buffer_table_try_releasing_retained_access_ranges(table, device);

	assert(accessor_slot < table->accessor_slot_count);
	accessor = table->accessors + accessor_slot;
	assert( ! accessor->active);
	

	if(!sol_indices_list_remove(&table->available_access_range_indices, &access_range_index))
	{
		/** get a new accessor, currently always possible: unbounded accessor count */
		if(table->access_range_count == table->access_range_space)
		{
			table->access_range_space *= 2;
			table->access_ranges = realloc(table->access_ranges, sizeof(struct sol_vk_buffer_table_access_range) * table->access_range_space);
		}

		access_range_index = table->access_range_count++;
		sol_vk_buffer_table_access_range_initialise(table->access_ranges + access_range_index);
	}

	accessor->active_range_index = access_range_index;
	accessor->active = true;

	return accessor->most_recent_range_release_moment;
}


void sol_vk_buffer_table_accessor_release(struct sol_vk_buffer_table* table, uint32_t accessor_slot, const struct sol_vk_timeline_semaphore_moment* release_moment)
{
	struct sol_vk_buffer_table_accessor* accessor;
	uint32_t access_range_index;

	assert(accessor_slot < table->accessor_slot_count);
	accessor = table->accessors + accessor_slot;
	assert(accessor->active);

	sol_indices_list_append(&table->retained_access_range_indices, accessor->active_range_index);

	accessor->most_recent_range_release_moment = *release_moment;
}



