/**
Copyright 2021,2022,2024,2025,2026 Carl van Mastrigt

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


#include <stdio.h>

#include "cvm_vk.h"
#include "vk/image_atlas.h"
#include "vk/image.h"
#include "data_structures/buddy_grid.h"


#define SOL_IA_HEADER_ENTRY_INDEX 0
#define SOL_IA_THRESHOLD_ENTRY_INDEX 1
#define SOL_IA_ALLOCATION_ENTRY_INDEX_START 2

struct sol_image_atlas_2_entry
{
	/** hash map key; used for hash map lookup upon defragmentation eviction top bit of this can be used to indicate the resource is transient*/
	uint64_t identifier;

	uint32_t grid_tile_index;

	uint32_t prev;
	uint32_t next;
};

/** may be possible to avoid an array in the regular fashion as grid tiles provide an "arrayable" index */
#define SOL_ARRAY_ENTRY_TYPE struct sol_image_atlas_2_entry
#define SOL_ARRAY_STRUCT_NAME sol_image_atlas_2_entry_array
#include "data_structures/array.h"


/** the map takes a unique identifier and gets the index in the array of entries
 * NOTE: in the map `key` is just the entries identifier */
static inline bool sol_image_atlas_2_identifier_entry_compare_equal(uint64_t key, uint32_t* entry_index, struct sol_image_atlas_2* atlas);
static inline uint64_t sol_image_atlas_2_entry_identifier_get(const uint32_t* entry_index, struct sol_image_atlas_2* atlas);

#define SOL_HASH_MAP_STRUCT_NAME sol_image_atlas_2_map
#define SOL_HASH_MAP_KEY_TYPE uint64_t
#define SOL_HASH_MAP_ENTRY_TYPE uint32_t
#define SOL_HASH_MAP_FUNCTION_KEYWORDS static
#define SOL_HASH_MAP_CONTEXT_TYPE struct sol_image_atlas_2*
#define SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(K, E, CTX) sol_image_atlas_2_identifier_entry_compare_equal(K, E, CTX)
#define SOL_HASH_MAP_KEY_HASH(K, CTX) K
#define SOL_HASH_MAP_KEY_FROM_ENTRY(E, CTX) sol_image_atlas_2_entry_identifier_get(E, CTX)
#include "data_structures/hash_map_implement.h"


struct sol_image_atlas_2
{
	/** provided description */
	struct sol_image_atlas_description description;

	struct sol_vk_supervised_image image;
	VkImageView image_view;

	struct sol_buddy_grid* grid;

	/** this is the moment that the most recent set of resources was used, its held onto by image atlas purely for convenience
	 * the single queue device access requirement of an image atlas means that generally external synchronization will be enough */
	struct sol_vk_timeline_semaphore_moment most_recent_usage_moment;


	/** all entry indices reference indices in this array, contains the actual information regarding vended tiles */
	struct sol_image_atlas_2_entry_array entry_array;


	/** map of identifiers to entries */
	struct sol_image_atlas_2_map itentifier_entry_map;

	/** effectively a rng seed used to increment through all available u64's in a random order vending unique values as it iterates
	 * by starting with zero (and returning the NEXT random number upon identifier request)
	 * it can be ensured that zero will not be hit for 2^64 requests, and thus treated as an invalid identifier */
	uint64_t current_identifier;

	bool accessor_active;

	bool most_recent_usage_moment_set;
};

static inline bool sol_image_atlas_2_identifier_entry_compare_equal(uint64_t key, uint32_t* entry_index, struct sol_image_atlas_2* atlas)
{
	const struct sol_image_atlas_2_entry* entry_data = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, *entry_index);
	return key == entry_data->identifier;
}

static inline uint64_t sol_image_atlas_2_entry_identifier_get(const uint32_t* entry_index, struct sol_image_atlas_2* atlas)
{
	const struct sol_image_atlas_2_entry* entry_data = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, *entry_index);
	return entry_data->identifier;
}






static inline void sol_image_atlas_2_entry_remove_from_queue(struct sol_image_atlas_2* atlas, struct sol_image_atlas_2_entry* entry)
{
	struct sol_image_atlas_2_entry* prev_entry;
	struct sol_image_atlas_2_entry* next_entry;

	assert(entry->next != SOL_U32_INVALID);
	assert(entry->prev != SOL_U32_INVALID);

	prev_entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, entry->prev);
	next_entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, entry->next);

	prev_entry->next = entry->next;
	next_entry->prev = entry->prev;

	entry->next = SOL_U32_INVALID;
	entry->prev = SOL_U32_INVALID;
}


static inline void sol_image_atlas_2_entry_add_to_queue_after(struct sol_image_atlas_2* atlas, uint32_t entry_index, uint32_t prev_entry_index)
{
	struct sol_image_atlas_2_entry* prev_entry;
	struct sol_image_atlas_2_entry* next_entry;
	struct sol_image_atlas_2_entry* entry;
	uint32_t next_entry_index;
	/** the root entry should not be removed in this way */

	entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, entry_index);

	assert(entry->next == SOL_U32_INVALID);
	assert(entry->prev == SOL_U32_INVALID);

	prev_entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, prev_entry_index);
	next_entry_index = prev_entry->next;
	next_entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, next_entry_index);

	prev_entry->next = entry_index;
	next_entry->prev = entry_index;

	entry->next = next_entry_index;
	entry->prev = prev_entry_index;
}

static inline void sol_image_atlas_2_entry_add_to_queue_before(struct sol_image_atlas_2* atlas, uint32_t entry_index, uint32_t next_entry_index)
{
	struct sol_image_atlas_2_entry* prev_entry;
	struct sol_image_atlas_2_entry* next_entry;
	struct sol_image_atlas_2_entry* entry;
	uint32_t prev_entry_index;
	/** the root entry should not be removed in this way */

	entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, entry_index);

	assert(entry->next == SOL_U32_INVALID);
	assert(entry->prev == SOL_U32_INVALID);

	next_entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, next_entry_index);
	prev_entry_index = next_entry->prev;
	prev_entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, prev_entry_index);

	prev_entry->next = entry_index;
	next_entry->prev = entry_index;

	entry->next = next_entry_index;
	entry->prev = prev_entry_index;
}

/** NOTE: entry referenced by this index may be completely destroyed, should ONLY call after all other uses of this entry have been completed */
static inline void sol_image_atlas_2_entry_evict(struct sol_image_atlas_2* atlas, uint32_t entry_index)
{
	struct sol_image_atlas_2_entry* entry;
	uint32_t map_entry_index;
	enum sol_map_operation_result map_remove_result;

	entry = sol_image_atlas_2_entry_array_withdraw_ptr(&atlas->entry_array, entry_index);

	assert(entry->identifier != 0);
	map_remove_result = sol_image_atlas_2_map_remove(&atlas->itentifier_entry_map, entry->identifier, &map_entry_index);
	assert(map_remove_result == SOL_MAP_SUCCESS_REMOVED);
	assert(map_entry_index == entry_index);
	entry->identifier = 0;

	sol_image_atlas_2_entry_remove_from_queue(atlas, entry);

	sol_buddy_grid_release(atlas->grid, entry->grid_tile_index);
}

static inline bool sol_image_atlas_2_evict_oldest_available_region(struct sol_image_atlas_2* atlas)
{
	const struct sol_image_atlas_2_entry* header_entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, SOL_IA_HEADER_ENTRY_INDEX);

	if(header_entry->next >= SOL_IA_ALLOCATION_ENTRY_INDEX_START)
	{
		sol_image_atlas_2_entry_evict(atlas, header_entry->next);
		return true;
	}

	return false;
}

struct sol_image_atlas_2* sol_image_atlas_2_create(const struct sol_image_atlas_description* description, struct cvm_vk_device* device)
{
	VkResult result;
	uint32_t entry_index;

	struct sol_image_atlas_2* atlas = malloc(sizeof(struct sol_image_atlas_2));

	assert(description->image_array_dimension > 0);
	assert(description->grid_tile_size.x > 0);
	assert(description->grid_tile_size.y > 0);

	atlas->description = *description;

	atlas->most_recent_usage_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;
	atlas->most_recent_usage_moment_set = false;

	const VkImageCreateInfo image_create_info =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = description->format,
		.extent = (VkExtent3D)
		{
			.width  = description->grid_tile_size.x << description->image_x_dimension_exponent,
			.height = description->grid_tile_size.y << description->image_y_dimension_exponent,
			.depth  = 1u,
		},
		.mipLevels = 1,
		.arrayLayers = description->image_array_dimension,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = description->usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	result = sol_vk_supervised_image_initialise(&atlas->image, device, &image_create_info);
	assert(result == VK_SUCCESS);

	const VkImageViewCreateInfo view_create_info =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.image = atlas->image.image.image,/** i swear this encapsulation makes sense, despite it looking silly */
		.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
		.format = description->format,
		.components = (VkComponentMapping)
        {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = (VkImageSubresourceRange)
        {
        	/** do we want/need non colour atlases ??? */
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = description->image_array_dimension,
        }
	};

    result = vkCreateImageView(device->device, &view_create_info, device->host_allocator, &atlas->image_view);
    assert(result == VK_SUCCESS);


    struct sol_buddy_grid_description grid_description = {
    	.image_x_dimension_exponent = description->image_x_dimension_exponent,
    	.image_y_dimension_exponent = description->image_y_dimension_exponent,
    	.image_array_dimension = description->image_array_dimension,
    };
  	atlas->grid = sol_buddy_grid_create(grid_description);


	sol_image_atlas_2_entry_array_initialise(&atlas->entry_array, 16);

	/** NOTE: only up to 2M entries are actually supported by internal link indices, this upper bound on the map ENFRCES that this doesnt get exceeded */
	struct sol_hash_map_descriptor map_descriptor = 
	{/** move this to description? */
		.entry_space_exponent_initial = 12,// 4096
		.entry_space_exponent_limit = 20,
		.resize_fill_factor = 192,// out of 256
		.limit_fill_factor = 224,// out of 256
	};
	sol_image_atlas_2_map_initialise(&atlas->itentifier_entry_map, map_descriptor, atlas);

	/** NOTE: this will get updated/iterated before being vended, ergo 0 will not be vended for the first 2^64 identifiers, so may be treated as invalid */
	atlas->current_identifier = 0;
	atlas->accessor_active = false;

	/** indices zero and one are reserved, allocate them and make sure their indices are as expected */
	*sol_image_atlas_2_entry_array_append_ptr(&atlas->entry_array, &entry_index) = (struct sol_image_atlas_2_entry)
	{
		.identifier = 0,
		.grid_tile_index = SOL_U32_INVALID,
		.prev = SOL_IA_HEADER_ENTRY_INDEX,
		.next = SOL_IA_HEADER_ENTRY_INDEX,
	};
	assert(entry_index == SOL_IA_HEADER_ENTRY_INDEX);

	*sol_image_atlas_2_entry_array_append_ptr(&atlas->entry_array, &entry_index) = (struct sol_image_atlas_2_entry)
	{
		.identifier = 0,
		.grid_tile_index = SOL_U32_INVALID,
		.prev = SOL_U32_INVALID,
		.next = SOL_U32_INVALID,
	};
	assert(entry_index == SOL_IA_THRESHOLD_ENTRY_INDEX);

	return atlas;
}

void sol_image_atlas_2_destroy(struct sol_image_atlas_2* atlas, struct cvm_vk_device* device)
{
	struct sol_image_atlas_2_entry* header_entry;
	struct sol_image_atlas_2_entry* threshold_entry;

	/** must release current access before destroying */
	assert(!atlas->accessor_active);

	/** wait on and then release most recent access */
	if(atlas->most_recent_usage_moment_set)
	{
		sol_vk_timeline_semaphore_moment_wait(&atlas->most_recent_usage_moment, device);
	}

	threshold_entry = sol_image_atlas_2_entry_array_withdraw_ptr(&atlas->entry_array, SOL_IA_THRESHOLD_ENTRY_INDEX);
	/** the threshold entry must have been removed as part of the accessor being released */
	assert(threshold_entry->next == SOL_U32_INVALID);
	assert(threshold_entry->prev == SOL_U32_INVALID);


	/** free all entries in the expired linked list queue */
	while(sol_image_atlas_2_evict_oldest_available_region(atlas)) {/** intentional empty loop*/};


	header_entry = sol_image_atlas_2_entry_array_withdraw_ptr(&atlas->entry_array, SOL_IA_HEADER_ENTRY_INDEX);
	assert(header_entry->next == SOL_IA_HEADER_ENTRY_INDEX);
	assert(header_entry->prev == SOL_IA_HEADER_ENTRY_INDEX);


	assert(sol_image_atlas_2_entry_array_is_empty(&atlas->entry_array));


	sol_image_atlas_2_map_terminate(&atlas->itentifier_entry_map);
	sol_image_atlas_2_entry_array_terminate(&atlas->entry_array);

	sol_buddy_grid_destroy(atlas->grid);

	vkDestroyImageView(device->device, atlas->image_view, device->host_allocator);
	sol_vk_supervised_image_terminate(&atlas->image, device);

	free(atlas);
}

void sol_image_atlas_2_access_range_begin(struct sol_image_atlas_2* atlas)
{
	assert(!atlas->accessor_active);
	atlas->accessor_active = true;

	/** place the threshold to the front of the queue */
	sol_image_atlas_2_entry_add_to_queue_before(atlas, SOL_IA_THRESHOLD_ENTRY_INDEX, SOL_IA_HEADER_ENTRY_INDEX);
}

void sol_image_atlas_2_access_range_end(struct sol_image_atlas_2* atlas, const struct sol_vk_timeline_semaphore_moment* last_use_moment)
{
	struct sol_image_atlas_2_entry* threshold_entry;

	assert(atlas->accessor_active);
	atlas->accessor_active = false;

	#warning handle transients here

	/** NOTE: it is very important that nothing in this loop will alter the backing of the entry array
	 * doing so would invalidate the `threshold_entry` pointer 
	 * note that this works because transient entries are placed at the back of the queue of the current "access" (between the front of the queue and the threshold entry) */
	threshold_entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, SOL_IA_THRESHOLD_ENTRY_INDEX);

	/** remove the threshold from the queue */
	sol_image_atlas_2_entry_remove_from_queue(atlas, threshold_entry);

	assert(last_use_moment);

	atlas->most_recent_usage_moment = *last_use_moment;
	atlas->most_recent_usage_moment_set = true;
}

bool sol_image_atlas_2_get_wait_moment(const struct sol_image_atlas_2* atlas, struct sol_vk_timeline_semaphore_moment* wait_moment)
{
	*wait_moment = atlas->most_recent_usage_moment;
	return atlas->most_recent_usage_moment_set;
}

bool sol_image_atlas_2_access_range_is_active(struct sol_image_atlas_2* atlas)
{
	return atlas->accessor_active;
}

uint64_t sol_image_atlas_2_generate_entry_identifier(struct sol_image_atlas_2* atlas)
{
	/** 64 bit lcg copied from sol random */
	atlas->current_identifier = atlas->current_identifier * 0x5851F42D4C957F2Dlu + 0x7A4111AC0FFEE60Dlu;

	/** NOTE: top N bits may be cut off to reduce cycle length to 2^(64-n), every point an any 2^m cycle will be visited in the bottom m bits for a lcg */

	return atlas->current_identifier;
}

enum sol_image_atlas_result sol_image_atlas_2_find_identified_entry(struct sol_image_atlas_2* atlas, uint64_t entry_identifier, struct sol_image_atlas_location* entry_location)
{
	struct sol_image_atlas_2_entry* entry;
	enum sol_map_operation_result map_find_result;
	uint32_t* entry_index_ptr;
	uint32_t entry_index;
	struct sol_buddy_grid_location location;

	/** must have an accessor active to be able to use entries */
	assert(atlas->accessor_active);

	map_find_result = sol_image_atlas_2_map_find(&atlas->itentifier_entry_map, entry_identifier, &entry_index_ptr);

	if(map_find_result == SOL_MAP_SUCCESS_FOUND)
	{
		entry_index = *entry_index_ptr;
		entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, entry_index);

		assert(entry->identifier == entry_identifier);

		sol_image_atlas_2_entry_remove_from_queue(atlas, entry);
		sol_image_atlas_2_entry_add_to_queue_before(atlas, entry_index, SOL_IA_HEADER_ENTRY_INDEX);

		location = sol_buddy_grid_get_location(atlas->grid, entry->grid_tile_index);

		entry_location->array_layer = location.array_layer;
		entry_location->offset = u16_vec2_mul(location.xy_offset, atlas->description.grid_tile_size);

		return SOL_IMAGE_ATLAS_SUCCESS_FOUND;
	}
	else
	{
		assert(map_find_result == SOL_MAP_FAIL_ABSENT);
		return SOL_IMAGE_ATLAS_FAIL_ABSENT;
	}
}

enum sol_image_atlas_result sol_image_atlas_2_obtain_identified_entry(struct sol_image_atlas_2* atlas, uint64_t entry_identifier, u16_vec2 size, struct sol_image_atlas_location* entry_location)
{
	/** write access will never return found, only absent or inserted */
	struct sol_image_atlas_2_entry_availability_heap* availability_heap;
	struct sol_image_atlas_2_entry* entry;
	enum sol_map_operation_result map_find_result, map_obtain_result;
	uint32_t* entry_index_in_map;
	uint32_t entry_index, x_size_class, y_size_class, grid_tile_index;
	struct sol_buddy_grid_location location;
	u16_vec2 grid_size;

	/** is invalid to request an entry with no pixels */
	assert(size.x > 0 && size.y > 0);

	/** must have an accessor active to be able to use entries */
	assert(atlas->accessor_active);
	/** zero identifier is reserved */
	assert(entry_identifier != 0);

	

	map_find_result = sol_image_atlas_2_map_find(&atlas->itentifier_entry_map, entry_identifier, &entry_index_in_map);

	if(map_find_result == SOL_MAP_SUCCESS_FOUND)
	{
		entry_index = *entry_index_in_map;
		entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, entry_index);

		assert(entry->identifier == entry_identifier);

		sol_image_atlas_2_entry_remove_from_queue(atlas, entry);

		/** put in entry queue sector of accessor,
		 * found and NOT replaced (hot path) */
		sol_image_atlas_2_entry_add_to_queue_before(atlas, entry_index, SOL_IA_HEADER_ENTRY_INDEX);

		location = sol_buddy_grid_get_location(atlas->grid, entry->grid_tile_index);
		
		entry_location->array_layer = location.array_layer;
		entry_location->offset = u16_vec2_mul(location.xy_offset, atlas->description.grid_tile_size);

		return SOL_IMAGE_ATLAS_SUCCESS_FOUND;
	}

	assert(map_find_result == SOL_MAP_FAIL_ABSENT);
	/** else: entry was not found */

	grid_size.x = (size.x + atlas->description.grid_tile_size.x - 1) / atlas->description.grid_tile_size.x;
	grid_size.y = (size.y + atlas->description.grid_tile_size.y - 1) / atlas->description.grid_tile_size.y;
	// grid_size.x = (size.x + 3) >> 2;
	// grid_size.y = (size.y + 3) >> 2;


	while( ! sol_buddy_grid_acquire(atlas->grid, grid_size, &grid_tile_index))
	{
		/** no entry of requested size available, need to make space by freeing unused entries
		 * NOTE: this invalidates the map result */
		if( ! sol_image_atlas_2_evict_oldest_available_region(atlas))
		{
			/** no more space can be made in order to accommodate the requested entry */
			return SOL_IMAGE_ATLAS_FAIL_IMAGE_FULL;
		}
	}

	while((map_obtain_result = sol_image_atlas_2_map_obtain(&atlas->itentifier_entry_map, entry_identifier, &entry_index_in_map)) == SOL_MAP_FAIL_FULL)
	{
		fprintf(stderr, "image atlas identifier map is full or has hashed a number of entries very poorly, this should not be possible in reasonable scenarios");
		/** try removing entries to make space in the hash map that addresses entries */  
		if( ! sol_image_atlas_2_evict_oldest_available_region(atlas))
		{
			/** no more space can be made in order to accommodate the requested entry
			 * must return the acquired entry to the available state before returning the correct error code
			 * NOTE: this is sufficiently rare that its not worth special pre-checking of availability */
			sol_buddy_grid_release(atlas->grid, grid_tile_index);
			return SOL_IMAGE_ATLAS_FAIL_MAP_FULL;
		}
	}
	assert(map_obtain_result == SOL_MAP_SUCCESS_INSERTED);

	entry = sol_image_atlas_2_entry_array_append_ptr(&atlas->entry_array, &entry_index);

	*entry_index_in_map = entry_index;

	entry->identifier = entry_identifier;
	entry->grid_tile_index = grid_tile_index;
	entry->prev = SOL_U32_INVALID;
	entry->next = SOL_U32_INVALID;

	sol_image_atlas_2_entry_add_to_queue_before(atlas, entry_index, SOL_IA_HEADER_ENTRY_INDEX);

	location = sol_buddy_grid_get_location(atlas->grid, entry->grid_tile_index);
		
	entry_location->array_layer = location.array_layer;
	entry_location->offset = u16_vec2_mul(location.xy_offset, atlas->description.grid_tile_size);

	return SOL_IMAGE_ATLAS_SUCCESS_INSERTED;
}

// enum sol_image_atlas_2_result sol_image_atlas_2_obtain_transient_entry(struct sol_image_atlas_2* atlas, u16_vec2 size, struct sol_image_atlas_2_location* entry_location)
// {
// 	struct sol_image_atlas_2_entry* entry;
// 	uint32_t entry_index, x_size_class, y_size_class;

// 	/** is invalid to request an entry with no pixels */
// 	assert(size.x > 0 && size.y > 0);

// 	/** must have an accessor active to be able to use entries */
// 	assert(atlas->accessor_active);

// 	x_size_class = SOL_MAX(sol_u32_exp_ge(size.x), SOL_IA_MIN_TILE_SIZE_EXPONENT) - SOL_IA_MIN_TILE_SIZE_EXPONENT;
// 	y_size_class = SOL_MAX(sol_u32_exp_ge(size.y), SOL_IA_MIN_TILE_SIZE_EXPONENT) - SOL_IA_MIN_TILE_SIZE_EXPONENT;

// 	while( ! sol_image_atlas_2_acquire_available_entry_of_size(atlas, x_size_class, y_size_class, &entry_index))
// 	{
// 		/** no entry of requested size available, need to make space by freeing unused entries
// 		 * NOTE: this invalidates the map result */
// 		if( ! sol_image_atlas_2_evict_oldest_available_region(atlas))
// 		{
// 			/** no more space can be made in order to accommodate the requested entry */
// 			return SOL_IMAGE_ATLAS_FAIL_IMAGE_FULL;
// 		}
// 	}

// 	entry = sol_image_atlas_2_entry_array_access_entry(&atlas->entry_array, entry_index);

// 	assert(entry->is_available);
// 	assert(entry->identifier == 0);

// 	entry->is_available = false;
// 	entry->is_transient = true;
// 	sol_image_atlas_2_entry_add_to_queue_before(atlas, entry_index, SOL_IA_HEADER_ENTRY_INDEX);

// 	*entry_location = (struct sol_image_atlas_2_location)
// 	{
// 		.array_layer = sol_ia_p_loc_get_layer(entry->packed_location),
// 		.offset.x = sol_ia_p_loc_get_x(entry->packed_location),
// 		.offset.y = sol_ia_p_loc_get_y(entry->packed_location),
// 	};

// 	return SOL_IMAGE_ATLAS_SUCCESS_INSERTED;
// }

// bool sol_image_atlas_2_entry_release(struct sol_image_atlas_2* atlas, uint64_t entry_identifier)
// {
// 	assert(false);// NYI
// 	enum sol_map_operation_result map_find_result;
// 	uint32_t* entry_index_in_map;

// 	map_find_result = sol_image_atlas_2_map_find(&atlas->itentifier_entry_map, entry_identifier, &entry_index_in_map);

// 	if(map_find_result == SOL_MAP_SUCCESS_FOUND)
// 	{
// 		sol_image_atlas_2_entry_make_available(atlas, *entry_index_in_map);

// 		return true;
// 	}

// 	return false;
// }

struct sol_vk_supervised_image* sol_image_atlas_2_access_supervised_image(struct sol_image_atlas_2* atlas)
{
	return &atlas->image;
}

VkImageView sol_image_atlas_2_access_image_view(struct sol_image_atlas_2* atlas)
{
	return atlas->image_view;
}
