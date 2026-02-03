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
#include <stdio.h>

#include "vk/buffer_atlas.h"

#include "sol_utils.h"
#include "vk/buffer.h"
#include "vk/timeline_semaphore.h"
#include "data_structures/indices_list.h"
#include "data_structures/indices_stack.h"
#include "data_structures/buddy_tree.h"


#define SOL_BUFFER_TABLE_RETAIN_BIT_COUNT 20
#define SOL_BUFFER_TABLE_MAX_RETAIN_COUNT (((uint32_t)1u << SOL_BUFFER_TABLE_RETAIN_BIT_COUNT) - 1u)

#define SOL_BUFFER_TABLE_INVALID_INDEX 0xFFFFFFFFu
#define SOL_BUFFER_TABLE_HEADER_INDEX 0


/** the map takes a unique identifier and gets the index in the array of entries
 * NOTE: in the map `key` is just the entries identifier */
static inline bool sol_vk_buffer_atlas_identifier_entry_compare_equal(uint64_t key, uint32_t* entry_index, struct sol_vk_buffer_atlas* table);
static inline uint64_t sol_vk_buffer_atlas_entry_identifier_get(const uint32_t* entry_index, struct sol_vk_buffer_atlas* table);

#define SOL_HASH_MAP_STRUCT_NAME sol_vk_buffer_atlas_region_map
#define SOL_HASH_MAP_KEY_TYPE uint64_t
#define SOL_HASH_MAP_ENTRY_TYPE uint32_t
#define SOL_HASH_MAP_FUNCTION_KEYWORDS static
#define SOL_HASH_MAP_CONTEXT_TYPE struct sol_vk_buffer_atlas*
#define SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(K, E, CTX) sol_vk_buffer_atlas_identifier_entry_compare_equal(K, E, CTX)
#define SOL_HASH_MAP_KEY_HASH(K, CTX) K
#define SOL_HASH_MAP_KEY_FROM_ENTRY(E, CTX) sol_vk_buffer_atlas_entry_identifier_get(E, CTX)
#include "data_structures/hash_map_implement.h"



struct sol_vk_buffer_atlas_region
{
	/** the hash in the map, necessary to store for removal purposes */
	uint64_t identifier;

	/** TODO: consider compressing this into 8 bytes */

	/** location in memory (in buddy allocator units: sol_vk_buffer_atlas.base_allocation_size) */
	uint32_t offset;

	/** indexes of the adjacent entries in the available region linked list 
	 * linked list is required for random removal upon re-activation of available regions */
	uint32_t prev;
	uint32_t next;


	/**	retain count of the region for the accessor that is permitted to write (primary) and all others (secondary) 
	 * primary is set/decided when written the first time */
	uint32_t retain_count : SOL_BUFFER_TABLE_RETAIN_BIT_COUNT;

	/** when the content was initialised; which accessor slot performed the initialisation
	 * subsequent accesses using the same slot will ALWAYS "see" the region as initialised rathern just after it was
	 * this is because an accessor acts like an ordered queue */
	uint32_t write_accessor_slot : 8;

	/** has the content been made available to secondary accessor slots 
	 * note secondary access precludes writing and writing precludes secondary access 
	 * as such mutable content should be completely inaccessible from secondary accessors as that could effectively make it immutable */
	uint32_t visible_from_read_accessors : 1;

	/** if an entry is transient it will be inacceible through the map, 
	 * only the initially returned offset can be used to access this entry 
	 * and it only lives as long as the access range where it was created */
	uint32_t is_transient : 1;
};


#define SOL_ARRAY_ENTRY_TYPE struct sol_vk_buffer_atlas_region
#define SOL_ARRAY_STRUCT_NAME sol_vk_buffer_atlas_region_array
#include "data_structures/array.h"


/** represents the range of accesses performed between an accessor acquire and release pair */
struct sol_vk_buffer_atlas_access_range
{
	struct sol_indices_stack retained_region_indices;
	/** this access range */
	struct sol_vk_timeline_semaphore_moment last_use_moment;
	/** TODO: make above a sequence?? */
	uint32_t accessor_slot;
};

#define SOL_STACK_ENTRY_TYPE struct sol_vk_buffer_atlas_access_range
#define SOL_STACK_STRUCT_NAME sol_vk_buffer_atlas_access_range_stack
#include "data_structures/stack.h"


static inline void sol_vk_buffer_atlas_access_range_initialise(struct sol_vk_buffer_atlas_access_range* access_range)
{
	sol_indices_stack_initialise(&access_range->retained_region_indices, 64);
	access_range->last_use_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;
}

static inline void sol_vk_buffer_atlas_access_range_terminate(struct sol_vk_buffer_atlas_access_range* access_range)
{
	// assert(access_range->last_use_moment.semaphore == VK_NULL_HANDLE);
	sol_indices_stack_terminate(&access_range->retained_region_indices);
}


struct sol_vk_buffer_atlas_accessor
{
	struct sol_vk_timeline_semaphore_moment most_recent_usage_moment;
	/** TODO: maybe make above a sequence?? */

	/** NOTE: this is termporary holding of the struct, it is NOT a long lived initialised member */
	struct sol_vk_buffer_atlas_access_range access_range;

	bool active;
	bool most_recent_usage_moment_set;
};



struct sol_vk_buffer_atlas
{
	struct sol_vk_buffer backing;


	struct sol_buddy_tree region_tree;


	struct sol_vk_buffer_atlas_region_map* region_map;


	struct sol_vk_buffer_atlas_access_range_stack available_access_ranges;
	struct sol_vk_buffer_atlas_access_range_stack in_flight_access_ranges;
	uint32_t active_accessor_count;/* debug */


	uint32_t accessor_slot_count;
	struct sol_vk_buffer_atlas_accessor* accessors;


	/** an array of data associated with tracking each allocation/region (reference count, position in deallocation queue &c.)
	 * index zero is an entry/region that doesnt actually reference any location in the buffer
	 * this entry is the start & end of the linked list ring of available entries*/
	struct sol_vk_buffer_atlas_region_array region_array;


	VkDeviceSize base_allocation_size;

	/** atomic used to vend identifiers */
	_Atomic uint64_t current_entry_identifier;

	mtx_t mutex;
	bool multithreaded;
};



static inline bool sol_vk_buffer_atlas_identifier_entry_compare_equal(uint64_t key, uint32_t* entry_index, struct sol_vk_buffer_atlas* table)
{
	const struct sol_vk_buffer_atlas_region* region = sol_vk_buffer_atlas_region_array_access_entry(&table->region_array, *entry_index);
	return key == region->identifier;
}
static inline uint64_t sol_vk_buffer_atlas_entry_identifier_get(const uint32_t* entry_index, struct sol_vk_buffer_atlas* table)
{
	const struct sol_vk_buffer_atlas_region* region = sol_vk_buffer_atlas_region_array_access_entry(&table->region_array, *entry_index);
	return region->identifier;
}

/** returns false if there are no more available regions to evict */
static inline bool sol_vk_buffer_atlas_evict_oldest_available_allocation(struct sol_vk_buffer_atlas* table)
{
	struct sol_vk_buffer_atlas_region* header_region;
	struct sol_vk_buffer_atlas_region* evicted_region;
	uint32_t evicted_region_index;
	uint32_t next_index, prev_index;

	header_region = sol_vk_buffer_atlas_region_array_access_entry(&table->region_array, SOL_BUFFER_TABLE_HEADER_INDEX);

	if(header_region->prev == SOL_BUFFER_TABLE_HEADER_INDEX)
	{
		/** the available ring buffer/linked list is empty, there is nothing left to free */
		assert(header_region->next == SOL_BUFFER_TABLE_HEADER_INDEX);
		return false;
	}

	/** front of the queue/ oldest entry is one to evict */
	evicted_region_index = header_region->next;
	/** note: nothing else in this function can modify the arrays backing memory so this pointer will remain valid for as long as its used despite having been "removed" here */
	evicted_region = sol_vk_buffer_atlas_region_array_withdraw_ptr(&table->region_array, evicted_region_index);
	assert(evicted_region->prev == SOL_BUFFER_TABLE_HEADER_INDEX);
	assert(evicted_region->retain_count == 0);
	assert( ! evicted_region->is_transient);

	/** remove evicted from the linked list */
	sol_vk_buffer_atlas_region_array_access_entry(&table->region_array, evicted_region->next)->prev = SOL_BUFFER_TABLE_HEADER_INDEX;
	header_region->next = evicted_region->next;

	/** remove from the identifier hash map */
	sol_vk_buffer_atlas_region_map_remove(table->region_map, evicted_region->identifier, NULL);

	/** actually make the memory available in the buddy allocator */
	sol_buddy_tree_release(&table->region_tree, evicted_region->offset);

	return true;
}


/** note: this is a slight hack, as it operates on the array directly and assumes the base region is also the header and the whole array is a single contiguous array of memory */
static inline void sol_vk_buffer_atlas_release_access_range(struct sol_vk_buffer_atlas* table, struct sol_vk_buffer_atlas_access_range access_range)
{
	struct sol_vk_buffer_atlas_region* region;
	struct sol_vk_buffer_atlas_region* header_region;
	struct sol_vk_buffer_atlas_region* newest_region;
	uint32_t region_index;

	/** note: this also empties the stack */
	while(sol_indices_stack_withdraw(&access_range.retained_region_indices, &region_index))
	{
		region = sol_vk_buffer_atlas_region_array_access_entry(&table->region_array, region_index);

		assert(region->next == SOL_BUFFER_TABLE_INVALID_INDEX && region->prev == SOL_BUFFER_TABLE_INVALID_INDEX);

		/** if it was not yet visible it is now */
		region->visible_from_read_accessors = true;

		assert(region->retain_count > 0);
		region->retain_count--;

		if(region->is_transient)
		{
			/** transient allocations should only have one reference */
			assert(region->retain_count == 0);

			/** transients can be made avaialable immediately */
			sol_buddy_tree_release(&table->region_tree, region->offset);
			sol_vk_buffer_atlas_region_array_withdraw(&table->region_array, region_index);
		}
		else if(region->retain_count == 0)
		{
			/** put this entry in the availability linked list, replacing the newest entry 
			 * note: this still works if the available region linked list is empty (header.prev == header && header.next == header) */

			header_region = sol_vk_buffer_atlas_region_array_access_entry(&table->region_array, SOL_BUFFER_TABLE_HEADER_INDEX);
			newest_region = sol_vk_buffer_atlas_region_array_access_entry(&table->region_array, header_region->prev);

			region->prev = header_region->prev;
			region->next = SOL_BUFFER_TABLE_HEADER_INDEX;
			
			newest_region->next = region_index;
			header_region->prev = region_index;
		}
	}

	access_range.last_use_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;

	sol_vk_buffer_atlas_access_range_stack_append(&table->available_access_ranges, access_range);
}


struct sol_vk_buffer_atlas* sol_vk_buffer_atlas_create(struct cvm_vk_device* device, const struct sol_vk_buffer_atlas_create_information* create_information)
{
	struct sol_vk_buffer_atlas_region* header_region_entry;
	uint32_t header_region_entry_index, accessor_slot_index;
	const uint32_t expected_range_count = sol_u32_exp_ge(create_information->accessor_slot_count * 2);

	struct sol_vk_buffer_atlas* table = malloc(sizeof(struct sol_vk_buffer_atlas));

	/** total buffer size should be a multiple of the base allocation size (i.e. the minumum allocation granularity) */
	assert((create_information->buffer_create_info.size % create_information->base_allocation_size) == 0);

	#warning make sure base allocation is a multiple of the required alignment for buffer usages


	sol_vk_buffer_initialise(&table->backing, device, &create_information->buffer_create_info, create_information->required_properties, create_information->desired_properties);

	sol_buddy_tree_initialise(&table->region_tree, create_information->buffer_create_info.size / create_information->base_allocation_size);

	const struct sol_hash_map_descriptor map_descriptor = 
	{
		.entry_space_exponent_initial = 12,//4096
		.entry_space_exponent_limit = 24,//16M
		.resize_fill_factor = 160,// out of 256
		.limit_fill_factor = 192,// out of 256
	};
	table->region_map = sol_vk_buffer_atlas_region_map_create(map_descriptor, table);

	sol_vk_buffer_atlas_access_range_stack_initialise(&table->available_access_ranges, 16);
	sol_vk_buffer_atlas_access_range_stack_initialise(&table->in_flight_access_ranges, 16);
	table->active_accessor_count = 0;


	table->accessors = malloc(sizeof(struct sol_vk_buffer_atlas_accessor) * create_information->accessor_slot_count);
	table->accessor_slot_count = create_information->accessor_slot_count;

	for(accessor_slot_index = 0; accessor_slot_index < create_information->accessor_slot_count; accessor_slot_index++)
	{
		table->accessors[accessor_slot_index] = (struct sol_vk_buffer_atlas_accessor)
		{
			#warning instead of null make this a device created dummy moment using a dummy semaphore
			.most_recent_usage_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL,
			.active = false,
			.most_recent_usage_moment_set = false,
		};
	}


	sol_vk_buffer_atlas_region_array_initialise(&table->region_array, 256);

	header_region_entry = sol_vk_buffer_atlas_region_array_append_ptr(&table->region_array, &header_region_entry_index);
	/** is assumed the header entry gets index 0 (SOL_BUFFER_TABLE_HEADER_INDEX) */
	assert(header_region_entry_index == SOL_BUFFER_TABLE_HEADER_INDEX);
	*header_region_entry = (struct sol_vk_buffer_atlas_region)
	{
		.identifier = 0,
		.next = SOL_BUFFER_TABLE_HEADER_INDEX,
		.prev = SOL_BUFFER_TABLE_HEADER_INDEX,
	};


	table->base_allocation_size = create_information->base_allocation_size;
	atomic_init(&table->current_entry_identifier, 0);

	table->multithreaded = create_information->multithreaded;
	if(table->multithreaded)
	{
		mtx_init(&table->mutex, mtx_plain);
	}


	return table;
}

void sol_vk_buffer_atlas_destroy(struct sol_vk_buffer_atlas* table, struct cvm_vk_device* device)
{
	struct sol_vk_buffer_atlas_region* header_region_entry;
	struct sol_vk_buffer_atlas_access_range access_range;

	/** wait on then release all in flight access ranges */
	while(sol_vk_buffer_atlas_access_range_stack_withdraw(&table->in_flight_access_ranges, &access_range))
	{
		sol_vk_timeline_semaphore_moment_wait(&access_range.last_use_moment, device);
		sol_vk_buffer_atlas_release_access_range(table, access_range);
	}

	/** while there are available allocations; evict them */
	while(sol_vk_buffer_atlas_evict_oldest_available_allocation(table));

	/** is assumed the header entry gets index 0 */
	header_region_entry = sol_vk_buffer_atlas_region_array_withdraw_ptr(&table->region_array, SOL_BUFFER_TABLE_HEADER_INDEX);
	/** the available ring linked list (header_region_entry) should be empty */
	assert(header_region_entry->identifier == 0 && header_region_entry->prev == SOL_BUFFER_TABLE_HEADER_INDEX && header_region_entry->next == SOL_BUFFER_TABLE_HEADER_INDEX);


	/** all accessors must have been released before this point of termination **/
	assert(table->active_accessor_count == 0);
	assert(sol_vk_buffer_atlas_access_range_stack_is_empty(&table->in_flight_access_ranges));
	assert(sol_vk_buffer_atlas_region_array_is_empty(&table->region_array));

	sol_vk_buffer_atlas_region_array_terminate(&table->region_array);

	while(sol_vk_buffer_atlas_access_range_stack_withdraw(&table->available_access_ranges, &access_range))
	{
		/** clean up access ranges */
		sol_vk_buffer_atlas_access_range_terminate(&access_range);
	}


	/** free access ranges */
	sol_vk_buffer_atlas_access_range_stack_terminate(&table->available_access_ranges);
	sol_vk_buffer_atlas_access_range_stack_terminate(&table->in_flight_access_ranges);

	/** free accessor slot backing (i.e. the accessors) */
	free(table->accessors);

	sol_vk_buffer_atlas_region_map_destroy(table->region_map);
	sol_buddy_tree_terminate(&table->region_tree);
	sol_vk_buffer_terminate(&table->backing, device);

	if(table->multithreaded)
	{
		mtx_destroy(&table->mutex);
	}

	free(table);
}

uint64_t sol_vk_buffer_atlas_generate_entry_identifier(struct sol_vk_buffer_atlas* table, bool mutable)
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



#warning TODO: to prevent regions being constantly moved in and out of the available linked list; accessors should have SOME mechanism to delay their release a little longer if they are frequently used (must be balanced against wanting to make entries available ASAP)
#warning perhaps when under memory stress (below 25% available?) clear all, otherwise only clear until one access range is left in the clear queue per accessor AND attempt to clear queue when out of memory during a run 
#warning potentially clear old range after a range gets completed?
static inline void sol_vk_buffer_atlas_release_completed_access_ranges(struct sol_vk_buffer_atlas* table, struct cvm_vk_device *device)
{
	uint32_t in_flight_range_index;
	struct sol_vk_buffer_atlas_access_range access_range;

	in_flight_range_index = sol_vk_buffer_atlas_access_range_stack_count(&table->in_flight_access_ranges);

	while(in_flight_range_index--)
	{
		access_range = sol_vk_buffer_atlas_access_range_stack_get_entry(&table->in_flight_access_ranges, in_flight_range_index);

		if(sol_vk_timeline_semaphore_moment_query(&access_range.last_use_moment, device))
		{
			sol_vk_buffer_atlas_access_range_stack_evict_index(&table->in_flight_access_ranges, in_flight_range_index);

			/** accessors work has completed, i.e. it has been released */
			sol_vk_buffer_atlas_release_access_range(table, access_range);
		}
	}
}



void sol_vk_buffer_atlas_access_range_begin(struct sol_vk_buffer_atlas* table, uint32_t accessor_slot, struct cvm_vk_device *device)
{
	struct sol_vk_buffer_atlas_accessor* accessor;

	if(table->multithreaded)
	{
		mtx_lock(&table->mutex);
	}

	assert(table->active_accessor_count < table->accessor_slot_count);
	table->active_accessor_count++;

	sol_vk_buffer_atlas_release_completed_access_ranges(table, device);

	assert(accessor_slot < table->accessor_slot_count);
	accessor = table->accessors + accessor_slot;
	assert( ! accessor->active);
	accessor->active = true;

	if( ! sol_vk_buffer_atlas_access_range_stack_withdraw(&table->available_access_ranges, &accessor->access_range))
	{
		/** if there isn't an available access range: initialise a new one */
		sol_vk_buffer_atlas_access_range_initialise(&accessor->access_range);
	}

	assert(sol_indices_stack_is_empty(&accessor->access_range.retained_region_indices));

	/** associate this range with the intended slot */
	accessor->access_range.accessor_slot = accessor_slot;

	if(table->multithreaded)
	{
		mtx_unlock(&table->mutex);
	}
}

void sol_vk_buffer_atlas_access_range_end(struct sol_vk_buffer_atlas* table, uint32_t accessor_slot, const struct sol_vk_timeline_semaphore_moment* last_use_moment)
{
	struct sol_vk_buffer_atlas_accessor* accessor;


	assert(accessor_slot < table->accessor_slot_count);
	accessor = table->accessors + accessor_slot;
	assert(accessor->active);
	assert(last_use_moment);


	accessor->most_recent_usage_moment = *last_use_moment;
	accessor->most_recent_usage_moment_set = true;
	accessor->active = false;

	if(table->multithreaded)
	{
		mtx_lock(&table->mutex);
	}

	assert(table->active_accessor_count > 0);
	table->active_accessor_count--;

	accessor->access_range.last_use_moment = *last_use_moment;

	/** move ownership of this access range to the "in flight" list */
	sol_vk_buffer_atlas_access_range_stack_append(&table->in_flight_access_ranges, accessor->access_range);
	accessor->access_range = (struct sol_vk_buffer_atlas_access_range){};

	if(table->multithreaded)
	{
		mtx_unlock(&table->mutex);
	}
}


/** this shouldn't need the mutex lock as accessor management/setup is required to be single threaded */
bool sol_vk_buffer_atlas_access_range_wait_moment(struct sol_vk_buffer_atlas* table, uint32_t accessor_slot, struct sol_vk_timeline_semaphore_moment* wait_moment)
{
	struct sol_vk_buffer_atlas_accessor* accessor;

	assert(accessor_slot < table->accessor_slot_count);
	accessor = table->accessors + accessor_slot;

	/** must only call this function between begin and end range */
	assert(accessor->active);

	*wait_moment = accessor->most_recent_usage_moment;

	return accessor->most_recent_usage_moment_set;
}

/** must be called inside the mutex lock if multithreaded
 * should be called on resources that are already extant and just need to be retained by this access range
 * may fail to retain if not found but not yet initialised by its (different) initialising accessor slot */
static inline enum sol_buffer_atlas_result sol_vk_buffer_atlas_region_retain(struct sol_vk_buffer_atlas* table, uint32_t accessor_slot, uint32_t region_index, VkDeviceSize* entry_offset, VkDeviceSize* size)
{
	struct sol_vk_buffer_atlas_region* region;
	uint32_t next_index, prev_index;

	region = sol_vk_buffer_atlas_region_array_access_entry(&table->region_array, region_index);

	if(region->write_accessor_slot == accessor_slot || region->visible_from_read_accessors)
	{
		/** retain count of zero is indicator that this entry is available, move it out of the available list when encountered */
		if(region->retain_count == 0)
		{
			next_index = region->next;
			prev_index = region->prev;
			assert(next_index != SOL_BUFFER_TABLE_INVALID_INDEX && prev_index != SOL_BUFFER_TABLE_INVALID_INDEX);

			/** pull the region out of the available linked list */
			sol_vk_buffer_atlas_region_array_access_entry(&table->region_array, prev_index)->next = next_index;
			sol_vk_buffer_atlas_region_array_access_entry(&table->region_array, next_index)->prev = prev_index;
			region->next = SOL_BUFFER_TABLE_INVALID_INDEX;
			region->prev = SOL_BUFFER_TABLE_INVALID_INDEX;
		}

		/** region not in the available linked list should not have valid next/prev links */
		assert(region->next == SOL_BUFFER_TABLE_INVALID_INDEX && region->prev == SOL_BUFFER_TABLE_INVALID_INDEX);

		/** add (the index of) the region to the retained list for the accessors active access range */
		sol_indices_stack_append(&table->accessors[accessor_slot].access_range.retained_region_indices, region_index);

		/** make sure not to exceed retain count type */
		assert(region->retain_count < SOL_BUFFER_TABLE_MAX_RETAIN_COUNT);

		region->retain_count++;
		*entry_offset = table->base_allocation_size * region->offset;

		if(size)
		{
			*size = table->base_allocation_size << sol_buddy_tree_query_allocation_size_exponent(&table->region_tree, region->offset);
		}

		return SOL_BUFFER_TABLE_SUCCESS_FOUND;
	}
	else
	{
		return SOL_BUFFER_TABLE_FAIL_NOT_INITIALISED;
	}
}


enum sol_buffer_atlas_result sol_vk_buffer_atlas_find_identified_region(struct sol_vk_buffer_atlas* table, uint64_t region_identifier, uint32_t accessor_slot, VkDeviceSize* entry_offset, VkDeviceSize* size)
{
	enum sol_buffer_atlas_result result;
	enum sol_map_operation_result map_find_result;
	uint32_t* region_index_ptr;

	assert(entry_offset != NULL);

	if(table->multithreaded)
	{
		mtx_lock(&table->mutex);
	}

	map_find_result = sol_vk_buffer_atlas_region_map_find(table->region_map, region_identifier, &region_index_ptr);
	if(map_find_result == SOL_MAP_SUCCESS_FOUND)
	{
		result = sol_vk_buffer_atlas_region_retain(table, accessor_slot, *region_index_ptr, entry_offset, size);
	}
	else
	{
		/** if not found the only valid alternative should be absent */
		assert(map_find_result == SOL_MAP_FAIL_ABSENT);
		result = SOL_BUFFER_TABLE_FAIL_ABSENT;
	}

	if(table->multithreaded)
	{
		mtx_unlock(&table->mutex);
	}

	return result;
}

static inline enum sol_buffer_atlas_result sol_vk_buffer_atlas_allocate_identified_region(struct sol_vk_buffer_atlas* table, uint64_t region_identifier, uint32_t accessor_slot, uint32_t size_exponent, VkDeviceSize* entry_offset)
{
	uint32_t* region_index_ptr;
	uint32_t buffer_offset;
	bool allocated_buddy_entry;

	while( ! sol_buddy_tree_has_space(&table->region_tree, size_exponent))
	{
		if( ! sol_vk_buffer_atlas_evict_oldest_available_allocation(table))
		{
			/** no more entries to free, have run out of space */
			return SOL_BUFFER_TABLE_FAIL_FULL;
		}
	}

	while(true)
	{
		switch(sol_vk_buffer_atlas_region_map_obtain(table->region_map, region_identifier, &region_index_ptr)) 
		{
		case SOL_MAP_FAIL_FULL:
			fprintf(stderr, "buffer table identifier map is full or has hashed a number of entries very poorly, this should not be possible in reasonable scenarios");
			if( ! sol_vk_buffer_atlas_evict_oldest_available_allocation(table))
			{
				/** no more entries to free, map has somehow run out of space */
				return SOL_BUFFER_TABLE_FAIL_MAP_FULL;
			}
			break;
		case SOL_MAP_SUCCESS_INSERTED:
			allocated_buddy_entry = sol_buddy_tree_acquire(&table->region_tree, size_exponent, &buffer_offset);
			/** tree was checked for space prior to above command (at start of the function) */
			assert(allocated_buddy_entry);

			/** create and initialise the region metadata */
			*sol_vk_buffer_atlas_region_array_append_ptr(&table->region_array, region_index_ptr) = (struct sol_vk_buffer_atlas_region)
			{
				.identifier = region_identifier,
				/** not in available list so links are invalid */
				.prev = SOL_BUFFER_TABLE_INVALID_INDEX,
				.next = SOL_BUFFER_TABLE_INVALID_INDEX,
				.offset = buffer_offset,
				.retain_count = 1,/** this, the write access retains the allocation */
				.write_accessor_slot = accessor_slot,
				.visible_from_read_accessors = false,
				.is_transient = false,
			};

			*entry_offset = table->base_allocation_size * buffer_offset;

			/** add (the index of) the region to the retained list for the accessors active access range */
			sol_indices_stack_append(&table->accessors[accessor_slot].access_range.retained_region_indices, *region_index_ptr);

			return SOL_BUFFER_TABLE_SUCCESS_INSERTED;
		default:
			/** in this state no other return from the hash map makes sense, 
			 * it should not have already been present, otherwise allocation wouldn't be necessary */
			assert(false);
		}
	}
}

enum sol_buffer_atlas_result sol_vk_buffer_atlas_obtain_identified_region(struct sol_vk_buffer_atlas* table, uint64_t region_identifier, uint32_t accessor_slot, VkDeviceSize size, VkDeviceSize* entry_offset)
{
	enum sol_buffer_atlas_result result;
	enum sol_map_operation_result map_find_result;
	uint32_t* region_index_ptr;
	VkDeviceSize located_size;

	/** when calculating size exponent need to round up base allocation sizes */
	const uint32_t size_exponent = sol_u64_exp_ge((size + table->base_allocation_size - 1) / table->base_allocation_size);

	assert(size > 0);
	assert(entry_offset);

	if(table->multithreaded)
	{
		mtx_lock(&table->mutex);
	}

	map_find_result = sol_vk_buffer_atlas_region_map_find(table->region_map, region_identifier, &region_index_ptr);
	if(map_find_result == SOL_MAP_SUCCESS_FOUND)
	{
		result = sol_vk_buffer_atlas_region_retain(table, accessor_slot, *region_index_ptr, entry_offset, &located_size);
		/** if already extant on disk, should have the appropraie size class for the requested size */
		assert(located_size == table->base_allocation_size << size_exponent);
	}
	else
	{
		/** if not found the only valid alternative should be absent */
		assert(map_find_result == SOL_MAP_FAIL_ABSENT);
		result = sol_vk_buffer_atlas_allocate_identified_region(table, region_identifier, accessor_slot, size_exponent, entry_offset);
	}

	if(table->multithreaded)
	{
		mtx_unlock(&table->mutex);
	}

	return result;
}

static inline enum sol_buffer_atlas_result sol_vk_buffer_atlas_allocate_transient_region(struct sol_vk_buffer_atlas* table, uint32_t accessor_slot, uint32_t size_exponent, VkDeviceSize* entry_offset)
{
	uint32_t* region_index_ptr;
	uint32_t buffer_offset;

	while( ! sol_buddy_tree_acquire(&table->region_tree, size_exponent, &buffer_offset))
	{
		if( ! sol_vk_buffer_atlas_evict_oldest_available_allocation(table))
		{
			/** no more entries to free, have run out of space */
			return SOL_BUFFER_TABLE_FAIL_FULL;
		}
	}

	/** create and initialise the region metadata */
	*sol_vk_buffer_atlas_region_array_append_ptr(&table->region_array, region_index_ptr) = (struct sol_vk_buffer_atlas_region)
	{
		.identifier = 0,
		/** not in available list so links are invalid */
		.prev = SOL_BUFFER_TABLE_INVALID_INDEX,
		.next = SOL_BUFFER_TABLE_INVALID_INDEX,
		.offset = buffer_offset,
		.retain_count = 1,/** this, the write access retains the allocation */
		.write_accessor_slot = accessor_slot,
		.visible_from_read_accessors = false,
		.is_transient = true,
	};

	*entry_offset = table->base_allocation_size * buffer_offset;

	/** add the newly created region (its index) to the retained list for this access range */
	sol_indices_stack_append(&table->accessors[accessor_slot].access_range.retained_region_indices, *region_index_ptr);

	return SOL_BUFFER_TABLE_SUCCESS_INSERTED;
}

enum sol_buffer_atlas_result sol_vk_buffer_atlas_obtain_transient_region(struct sol_vk_buffer_atlas* table, uint32_t accessor_slot, VkDeviceSize size, VkDeviceSize* entry_offset)
{
	enum sol_buffer_atlas_result result;

	/** when calculating size exponent need to round up base allocation sizes */
	const uint32_t size_exponent = sol_u64_exp_ge((size + table->base_allocation_size - 1) / table->base_allocation_size);

	assert(size > 0);
	assert(entry_offset);

	if(table->multithreaded)
	{
		mtx_lock(&table->mutex);
	}

	result = sol_vk_buffer_atlas_allocate_transient_region(table, accessor_slot, size_exponent, entry_offset);

	if(table->multithreaded)
	{
		mtx_unlock(&table->mutex);
	}

	return result;
}


struct sol_vk_buffer* sol_vk_buffer_atlas_access_buffer(struct sol_vk_buffer_atlas* table)
{
	return &table->backing;
}