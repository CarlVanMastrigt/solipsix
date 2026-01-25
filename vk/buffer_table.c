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
#include "data_structures/indices_stack.h"
#include "data_structures/buddy_tree.h"
#include "vk/timeline_semaphore.h"





/** the map takes a unique identifier and gets the index in the array of entries
 * NOTE: in the map `key` is just the entries identifier */
static inline bool sol_vk_buffer_table_identifier_entry_compare_equal(uint64_t key, uint32_t* entry_index, struct sol_vk_buffer_table* table);
static inline uint64_t sol_vk_buffer_table_entry_identifier_get(const uint32_t* entry_index, struct sol_vk_buffer_table* table);

#define SOL_HASH_MAP_STRUCT_NAME sol_vk_buffer_table_map
#define SOL_HASH_MAP_KEY_TYPE uint64_t
#define SOL_HASH_MAP_ENTRY_TYPE uint32_t
#define SOL_HASH_MAP_FUNCTION_KEYWORDS static
#define SOL_HASH_MAP_CONTEXT_TYPE struct sol_vk_buffer_table*
#define SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(K, E, CTX) sol_vk_buffer_table_identifier_entry_compare_equal(K, E, CTX)
#define SOL_HASH_MAP_KEY_HASH(K, CTX) K
#define SOL_HASH_MAP_KEY_FROM_ENTRY(E, CTX) sol_vk_buffer_table_entry_identifier_get(E, CTX)
#include "data_structures/hash_map_implement.h"


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
	uint32_t accessor_slot;
};


struct sol_vk_buffer_table_accessor
{
	struct sol_vk_timeline_semaphore_moment most_recent_range_release_moment;
	/** TODO: make above a sequence?? */

	uint32_t active_range_index;

	bool active;
	bool moment_acquired;
	bool moment_set;
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



	struct sol_vk_buffer_table_region_array region_array;

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

	mtx_t mutex;
	bool multithreaded;
};



static inline bool sol_vk_buffer_table_identifier_entry_compare_equal(uint64_t key, uint32_t* entry_index, struct sol_vk_buffer_table* table)
{
	const struct sol_vk_buffer_table_region* region = sol_vk_buffer_table_region_array_access_entry(&table->region_array, *entry_index);
	return key == region->identifier;
}
static inline uint64_t sol_vk_buffer_table_entry_identifier_get(const uint32_t* entry_index, struct sol_vk_buffer_table* table)
{
	const struct sol_vk_buffer_table_region* region = sol_vk_buffer_table_region_array_access_entry(&table->region_array, *entry_index);
	return region->identifier;
}






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
			.moment_set = false,
			.moment_acquired = false,
		};
	}


	/** make this initial count larger? */
	sol_vk_buffer_table_region_array_initialise(&table->region_array, 256);

	header_region_entry = sol_vk_buffer_table_region_array_append_ptr(&table->region_array, &header_region_entry_index);
	/** is assumed the header entry gets index 0 */
	assert(header_region_entry_index == 0);
	*header_region_entry = (struct sol_vk_buffer_table_region)
	{
		.next = header_region_entry_index,
		.prev = header_region_entry_index,
	};


	table->base_allocation_size = create_information->base_allocation_size;
	atomic_init(&table->current_entry_identifier, 0);

	table->multithreaded = create_information->multithreaded;
	if(table->multithreaded)
	{
		mtx_init(&table->mutex, mtx_plain);
	}
}

static inline void sol_vk_buffer_table_terminate(struct sol_vk_buffer_table* table, struct cvm_vk_device* device)
{
	struct sol_vk_buffer_table_region* header_region_entry;
	#warning wait on accessors

	#warning clean up regions including header

	/** is assumed the header entry gets index 0 */
	header_region_entry = sol_vk_buffer_table_region_array_remove_ptr(&table->region_array, 0);
	/** the available ring linked list (header_region_entry) should be empty */
	assert(header_region_entry->identifier == 0 && header_region_entry->prev == 0 && header_region_entry->next == 0);


	/** all accessors must have been released before returning **/
	assert(table->access_range_count == sol_indices_list_count(&table->available_access_range_indices));
	assert(sol_vk_buffer_table_region_array_is_empty(&table->region_array));

	sol_vk_buffer_table_region_array_terminate(&table->region_array);


	/** free access ranges */
	sol_indices_list_terminate(&table->available_access_range_indices);
	sol_indices_list_terminate(&table->retained_access_range_indices);
	free(table->access_ranges);

	/** free accessor slot backing (i.e. the accessors) */
	free(table->accessors);


	sol_buddy_tree_terminate(&table->region_tree);
	sol_vk_buffer_terminate(&table->backing, device);

	if(table->multithreaded)
	{
		mtx_destroy(&table->mutex);
	}
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

/** note: this is a slight hack, as it operates on the array directly and assumes the base region is also the header and the whole array is a single contiguous array of memory */
static inline void sol_vk_buffer_table_release_region_reference(struct sol_vk_buffer_table_region* base_region, uint32_t region_index)
{
	struct sol_vk_buffer_table_region* region = base_region + region_index;
	struct sol_vk_buffer_table_region* header_region = base_region;
	struct sol_vk_buffer_table_region* newest_region = base_region + header_region->prev;

	assert(region->next == SOL_U32_INVALID && region->prev == SOL_U32_INVALID);
	assert(region->active_access_count > 0);

	region->active_access_count--;

	if(region->active_access_count == 0)
	{
		/** put this entry in the availability linked list, replacing the newest entry 
		 * note: this still works if the available region linked list is empty (header.prev == header && header.next == header) */

		region->prev = header_region->prev;
		region->next = 0;/* header */
		
		newest_region->next = region_index;
		header_region->prev = region_index;
	}
}

static inline void sol_vk_buffer_table_try_releasing_retained_access_ranges(struct sol_vk_buffer_table* table, struct cvm_vk_device *device)
{
	uint32_t range_list_index, range_index, region_list_index, region_index;
	struct sol_vk_buffer_table_access_range* access_range;

	struct sol_vk_buffer_table_region* region;
	struct sol_vk_buffer_table_region* base_region;

	/** because this array will not be modified by this function it is safe to hold onto these pointers */
	base_region = sol_vk_buffer_table_region_array_base_pointer(&table->region_array);

	range_list_index = 0;

	while(range_list_index < sol_indices_list_count(&table->retained_access_range_indices))
	{
		range_index = sol_indices_list_get_entry(&table->retained_access_range_indices, range_list_index);
		access_range = table->access_ranges + range_index;

		if(sol_vk_timeline_semaphore_moment_query(&access_range->release_moment, device))
		{
			/** accessors work has completed, i.e. it has been released */
			while(sol_indices_stack_remove(&access_range->initialised_region_indices, &region_index))
			{
				region = base_region + region_index;

				assert(region->initialising_accessor_slot == access_range->accessor_slot);
				assert( ! region->initialised);
				region->initialised = true;

				sol_vk_buffer_table_release_region_reference(base_region, region_index);
			}

			while(sol_indices_stack_remove(&access_range->retained_region_indices, &region_index))
			{
				sol_vk_buffer_table_release_region_reference(base_region, region_index);
			}

			/** range should effectively be emptied/reset before being put on the available list */
			assert(sol_indices_stack_is_empty(&access_range->initialised_region_indices));
			assert(sol_indices_stack_is_empty(&access_range->retained_region_indices));

			/** move this range from the retained list to the available list */
			sol_indices_list_remove_entry(&table->retained_access_range_indices, range_list_index);
			sol_indices_list_append(&table->available_access_range_indices, range_index);
			/** by removing this range from the list we have replaced the entry at this index in the list and reduced the lists count
			 * as a result the `range_list_index` should not be incremented and instead this list index should be checked again with its replacement */
		}
		else
		{
			range_list_index++;
		}
	}
}

void sol_vk_buffer_table_accessor_acquire(struct sol_vk_buffer_table* table, uint32_t accessor_slot, struct cvm_vk_device *device)
{
	struct sol_vk_buffer_table_accessor* accessor;
	struct sol_vk_buffer_table_access_range* access_range;
	uint32_t access_range_index;

	if(table->multithreaded)
	{
		mtx_lock(&table->mutex);
	}

	sol_vk_buffer_table_try_releasing_retained_access_ranges(table, device);

	assert(accessor_slot < table->accessor_slot_count);
	accessor = table->accessors + accessor_slot;
	assert( ! accessor->active);

	if(!sol_indices_list_remove(&table->available_access_range_indices, &access_range_index))
	{
		if(table->access_range_count == table->access_range_space)
		{
			table->access_range_space *= 2;
			table->access_ranges = realloc(table->access_ranges, sizeof(struct sol_vk_buffer_table_access_range) * table->access_range_space);
		}

		access_range_index = table->access_range_count++;
		sol_vk_buffer_table_access_range_initialise(table->access_ranges + access_range_index);
	}

	access_range = table->access_ranges + access_range_index;
	assert(sol_indices_stack_is_empty(&access_range->initialised_region_indices));
	assert(sol_indices_stack_is_empty(&access_range->retained_region_indices));
	access_range->accessor_slot = accessor_slot;

	if(table->multithreaded)
	{
		mtx_unlock(&table->mutex);
	}

	accessor->active_range_index = access_range_index;
	accessor->active = true;
	accessor->moment_acquired = false;
}

void sol_vk_buffer_table_accessor_release(struct sol_vk_buffer_table* table, uint32_t accessor_slot, const struct sol_vk_timeline_semaphore_moment* release_moment)
{
	struct sol_vk_buffer_table_accessor* accessor;
	struct sol_vk_buffer_table_access_range* access_range;

	assert(accessor_slot < table->accessor_slot_count);
	accessor = table->accessors + accessor_slot;
	assert(accessor->active);
	assert(accessor->moment_acquired);/** its necessary to (try to) acquire the wait moment for this access range before releasing the range */

	accessor->most_recent_range_release_moment = *release_moment;
	accessor->moment_set = true;
	accessor->active = false;

	if(table->multithreaded)
	{
		mtx_lock(&table->mutex);
	}

	access_range = table->access_ranges + accessor->active_range_index;
	access_range->release_moment = *release_moment;

	sol_indices_list_append(&table->retained_access_range_indices, accessor->active_range_index);

	if(table->multithreaded)
	{
		mtx_unlock(&table->mutex);
	}
}


/** this shouldn't need the mutex lock as accessor management/setup is required to be single threaded */
bool sol_vk_buffer_table_accessor_get_wait_moment(struct sol_vk_buffer_table* table, uint32_t accessor_slot, struct sol_vk_timeline_semaphore_moment* wait_moment)
{
	struct sol_vk_buffer_table_accessor* accessor;

	assert(accessor_slot < table->accessor_slot_count);
	accessor = table->accessors + accessor_slot;
	assert(accessor->active);

	*wait_moment = accessor->most_recent_range_release_moment;

	accessor->moment_acquired = true;

	return accessor->moment_set;
}



enum sol_buffer_table_result sol_vk_buffer_table_entry_obtain(struct sol_vk_buffer_table* table, uint64_t entry_identifier, uint32_t accessor_slot, VkDeviceSize size, uint32_t flags, VkDeviceSize* entry_offset)
{
	//
}

enum sol_buffer_table_result sol_vk_buffer_table_entry_find(struct sol_vk_buffer_table* table, uint64_t entry_identifier, uint32_t accessor_slot, VkDeviceSize* entry_offset, VkDeviceSize* size)
{
	enum sol_buffer_table_result result;

	if(table->multithreaded)
	{
		mtx_lock(&table->mutex);
	}

	//

	if(table->multithreaded)
	{
		mtx_unlock(&table->mutex);
	}

	return result;
}



#warning I'm not sure the following is actually possible, (though it or something like it seems necessary)
/** if a resource is obtained on one thread, then another, the second thread sees the resource as being available
 * but if the first thread then cannot initialise the backing (and so needs to release it) then the second thread has effectively gained use of uninitialised data
 * each thread COULD use a different accessor, and obtain could be modified to permit attempted access by different accessors, but this removes the ability to "always" get/produce data the frame it was created!
 * 
 * the only way to alleviate is to make sure that the ability to initialise the resource is known up front by the user, in which case a single accessor is desired, but both approaches should remain open
 * this really puts an undesirable onus on caller to understand how the buffer table works and reason about the implications of its use
 * (maybe this is okay because its only applicable in the multithreaded case?)
 * 
 * COULD alter obtain such that it requires another call (to finalise or release the resource/entry) if the resource/entry was not already present
 * 
 * this all might be irrelevant anyway - because this backs a buffer, which may be CPU accessible, the interface could/should just return an initialisation pointer when obtain is called
 * this requires that staging is provided (at the callsite?) as well - but resources might NOT want to use upload to initialise a resorce (procedural gen contents via compute shader) so don't always need staging
 * 
 * ergo - onus on caller to ensure space is available beforehand */
#warning  ^ in which case - probably also worth making this change to image atlas!

void sol_buffer_table_entry_release(struct sol_vk_buffer_table* table, uint32_t accessor_index, uint64_t entry_identifier);