/**
Copyright 2021,2022,2024,2025 Carl van Mastrigt

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
#include "vk/image_utils.h"
#include "vk/image.h"



#define SOL_IA_MIN_TILE_SIZE_EXPONENT 2u
#define SOL_IA_MIN_TILE_SIZE 4u
/** note: SOL_IMAGE_ATLAS_MIN_TILE_SIZE must be power of 2 */
#define SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT 13u
/** [4,16384] inclusive ^^ */

#define SOL_IA_IDENTIFIER_TRANSIENT_BIT ((uint64_t)(1llu << 63))

struct sol_image_atlas_entry
{
	/** hash map key; used for hash map lookup upon defragmentation eviction top bit of this can be used to indicate the resource is transient*/
	uint64_t identifier;

	/** the start of each accessor and the start/end of the queue itself use (unreferenced) atlas entries in the linked list to designate ranges
	 * the following are mutually exlusive; is_accessor indicating an accessor threshold entry and is_tile_entry indicating a pixel grid (real) entry
	 * if neither are set that is the root of the linked list held by the atlas itself and used for insertion */
	uint32_t is_tile_entry : 1;

	/** prev/next indices in linked list of entries my order of use, 16M is more than enough entries
	 * NOTE: 0 is reserved for the dummy start index
	 * SOL_IA_INVALID_IDX indicates an entry is not in the linked list, though this can also be inferred by other properties */
	uint64_t prev_entry_index : 21;
	uint64_t next_entry_index : 21;


	/** links within the 2D structure of an array layer of the atlas
	 * index zero is invalid, which should be the index of root node of the active entry linked list
	 * names indicate a cardinal direction from a corner;
	 * start corner is towards zero (top left of image)
	 * end corner is away from zero (bottom right of image) */
	uint64_t adj_start_left : 21;
	uint64_t adj_start_up   : 21;
	uint64_t adj_end_right  : 21;
	uint64_t adj_end_down   : 21;

	/** is this a free/available tile, if so identifier should be ignored */
	uint64_t is_available : 1;


	/** z-tile location is in terms of minimum entry pixel dimension (4)
	 * packed in such a way to order entries by layer first then top left -> bottom right
	 * i.e. packed like: | array layer (8) | z-tile location (24 == 12x + 12y) |
	 * this is used to sort/order entries (lower value means better placed) */
	uint32_t packed_location;

	/** index in availability heap of size class (x,y)
	 * NOTE: could possibly be unioned with prev/next as this is (presently) not used at the same time as being in a linked list */
	uint32_t heap_index : 21;



	/** size class is power of 2 times minimum entry pixel dimension (4)
	 * so: 4 * 2 ^ (0:15) is more than enough (4 << 15 = 128k, is larger than max texture size)
	 * maximum expected size class is 12 */
	uint32_t x_size_class : 4;
	uint32_t y_size_class : 4;

	/** 2 bits left over */
};

/** NOTE: 21 bits */
#define SOL_IA_INVALID_IDX      0x001FFFFFu
#define SOL_IA_MAX_ENTRIES      0x001FFFFFu

#define SOL_IA_P_LOC_X_MASK     0x00555555u
#define SOL_IA_P_LOC_Y_MASK     0x00AAAAAAu
#define SOL_IA_P_LOC_LAYER_MASK 0xFF000000u

/** note this gets location in pixels, REQUIRES that SOL_IA_MIN_TILE_SIZE_EXPONENT is 2*/
static inline uint32_t sol_ia_p_loc_get_x(uint32_t packed_location)
{
	/** 0x00555555 */
	packed_location = ((packed_location & 0x00444444u) << 1) | ((packed_location & 0x00111111u) << 2);
	/** 0x00CCCCCC */
	packed_location = ((packed_location & 0x00C0C0C0u) >> 2) | ((packed_location & 0x000C0C0Cu)     );
	/** 0x003C3C3C */
	return ((packed_location & 0x003C0000u) >> 8) | ((packed_location & 0x00003C00u) >> 4) | (packed_location & 0x0000003Cu);
	/** 0x00003FFC */
}

static inline uint32_t sol_ia_p_loc_get_y(uint32_t packed_location)
{
	/** 0x00AAAAAA */
	packed_location = ((packed_location & 0x00888888u)     ) | ((packed_location & 0x00222222u) << 1);
	/** 0x00CCCCCC */
	packed_location = ((packed_location & 0x00C0C0C0u) >> 2) | ((packed_location & 0x000C0C0Cu)     );
	/** 0x003C3C3C */
	return ((packed_location & 0x003C0000u) >> 8) | ((packed_location & 0x00003C00u) >> 4) | (packed_location & 0x0000003Cu);
	/** 0x00003FFC */
}

static inline uint32_t sol_ia_p_loc_get_layer(uint32_t packed_location)
{
	return packed_location >> 24;
}

#warning should have function for getting x and y simultaneously, should be possible to improve perf with u64 ops...

/** returns true if ( x >= adj_x  && x < adj_x + adj_size_x) */
static inline bool sol_ia_p_loc_start_in_range_x(uint32_t packed_location, uint32_t adjacent_packed_location, uint32_t adjacent_x_size_class)
{
	assert((packed_location & 0xFF000000u) == (adjacent_packed_location & 0xFF000000u));
	/** set all y bits to force carry when adding x value*/
	packed_location |= SOL_IA_P_LOC_Y_MASK;
	adjacent_packed_location |= SOL_IA_P_LOC_Y_MASK;
	const uint32_t adjacent_packed_location_end = (adjacent_packed_location + (1u << (adjacent_x_size_class * 2 + 0))) | SOL_IA_P_LOC_Y_MASK;

	return packed_location >= adjacent_packed_location && packed_location < adjacent_packed_location_end;
}

/** returns true if ( y >= adj_y  &&  y < adj_y + adj_size_y) */
static inline bool sol_ia_p_loc_start_in_range_y(uint32_t packed_location, uint32_t adjacent_packed_location, uint32_t adjacent_y_size_class)
{
	assert((packed_location & 0xFF000000u) == (adjacent_packed_location & 0xFF000000u));
	/** set all x bits to force carry when adding y value*/
	packed_location |= SOL_IA_P_LOC_X_MASK;
	adjacent_packed_location |= SOL_IA_P_LOC_X_MASK;
	const uint32_t adjacent_packed_location_end = (adjacent_packed_location + (1u << (adjacent_y_size_class * 2 + 1))) | SOL_IA_P_LOC_X_MASK;

	return packed_location >= adjacent_packed_location && packed_location < adjacent_packed_location_end;
}

/** returns true if ( x + size_x >= adj_x  &&  x + size_x <= adj_x + adj_size_x) */
static inline bool sol_ia_p_loc_end_in_range_x(uint32_t packed_location, uint32_t x_size_class, uint32_t adjacent_packed_location, uint32_t adjacent_x_size_class)
{
	assert((packed_location & 0xFF000000u) == (adjacent_packed_location & 0xFF000000u));
	/** set all y bits to force carry when adding x value*/
	packed_location |= SOL_IA_P_LOC_Y_MASK;
	adjacent_packed_location |= SOL_IA_P_LOC_Y_MASK;

	/** get end packed location */
	packed_location = (packed_location + (1u << (x_size_class * 2 + 0))) | SOL_IA_P_LOC_Y_MASK;
	const uint32_t adjacent_packed_location_end = (adjacent_packed_location + (1u << (adjacent_x_size_class * 2 + 0))) | SOL_IA_P_LOC_Y_MASK;

	return packed_location > adjacent_packed_location && packed_location <= adjacent_packed_location_end;
}

/** returns true if ( y + size_y >= adj_y  &&  y + size_y <= adj_y + adj_size_y) */
static inline bool sol_ia_p_loc_end_in_range_y(uint32_t packed_location, uint32_t y_size_class, uint32_t adjacent_packed_location, uint32_t adjacent_y_size_class)
{
	assert((packed_location & 0xFF000000u) == (adjacent_packed_location & 0xFF000000u));
	/** set all x bits to force carry when adding y value and avoid needing to mask them out later */
	packed_location |= SOL_IA_P_LOC_X_MASK;
	adjacent_packed_location |= SOL_IA_P_LOC_X_MASK;

	/** get end packed location */
	packed_location = (packed_location + (1u << (y_size_class * 2 + 1))) | SOL_IA_P_LOC_X_MASK;
	const uint32_t adjacent_packed_location_end = (adjacent_packed_location + (1u << (adjacent_y_size_class * 2 + 1))) | SOL_IA_P_LOC_X_MASK;

	return packed_location > adjacent_packed_location && packed_location <= adjacent_packed_location_end;
}



#define SOL_ARRAY_ENTRY_TYPE struct sol_image_atlas_entry
#define SOL_ARRAY_FUNCTION_PREFIX sol_image_atlas_entry_array
#define SOL_ARRAY_STRUCT_NAME sol_image_atlas_entry_array
#include "data_structures/array.h"


struct sol_image_atlas;

/** the map takes a unique identifier and gets the index in the array of entries
 * NOTE: in the map `key` is just the entries identifier */
static inline bool sol_image_atlas_identifier_entry_compare_equal(uint64_t key, uint32_t* entry_index, struct sol_image_atlas* atlas);
static inline uint64_t sol_image_atlas_entry_identifier_get(const uint32_t* entry_index, struct sol_image_atlas* atlas);

#define SOL_HASH_MAP_STRUCT_NAME sol_image_atlas_map
#define SOL_HASH_MAP_FUNCTION_PREFIX sol_image_atlas_map
#define SOL_HASH_MAP_KEY_TYPE uint64_t
#define SOL_HASH_MAP_ENTRY_TYPE uint32_t
#define SOL_HASH_MAP_FUNCTION_KEYWORDS static
#define SOL_HASH_MAP_CONTEXT_TYPE struct sol_image_atlas*
#define SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(K, E, CTX) sol_image_atlas_identifier_entry_compare_equal(K, E, CTX)
#define SOL_HASH_MAP_KEY_HASH(K, CTX) K
#define SOL_HASH_MAP_KEY_FROM_ENTRY(E, CTX) sol_image_atlas_entry_identifier_get(E, CTX)
#include "data_structures/hash_map_implement.h"


/** heaps store the z-tile ordered indices of available entries */
static inline bool sol_image_atlas_entry_packed_location_cmp_lt(const uint32_t* entry_a_index_ptr, const uint32_t* entry_b_index_ptr, struct sol_image_atlas* atlas);
static inline void sol_image_atlas_entry_set_heap_index(const uint32_t* entry_index_ptr, uint32_t new_index_in_heap, struct sol_image_atlas* atlas);

#define SOL_BINARY_HEAP_ENTRY_TYPE uint32_t
#define SOL_BINARY_HEAP_STRUCT_NAME sol_image_atlas_entry_availability_heap
#define SOL_BINARY_HEAP_FUNCTION_PREFIX sol_image_atlas_entry_availability_heap
#define SOL_BINARY_HEAP_CONTEXT_TYPE struct sol_image_atlas*
#define SOL_BINARY_HEAP_ENTRY_CMP_LT(A, B, CTX) sol_image_atlas_entry_packed_location_cmp_lt(A, B, CTX)
#define SOL_BINARY_HEAP_SET_ENTRY_INDEX(E, IDX, CTX) sol_image_atlas_entry_set_heap_index(E, IDX, CTX)
#include "data_structures/binary_heap.h"

/** TODO: could binary heap benefit from custom allocator (169 allocations here, that could all come from a single alloc)
 * TODO: binary heap could have custom size to use when 0 initial size is provided */


struct sol_image_atlas
{
	/** provided description */
	struct sol_image_atlas_description description;

	struct sol_vk_supervised_image image;
	VkImageView image_view;

	/** this is the management semaphore for accesses in the image atlas, it will signal availability and write completion
	 * users of the image atlas must signal moments vended to them to indicate reads and writes have completed
	 * users must also wait on the moment vended upon acquisition in order to ensure all prior modifications are visible */
	struct sol_vk_timeline_semaphore timeline_semaphore;
	struct sol_vk_timeline_semaphore_moment current_moment;


	/** all entry indices reference indices in this array, contains the actual information regarding vended tiles */
	struct sol_image_atlas_entry_array entry_array;


	/** map of identifiers to entries */
	struct sol_image_atlas_map itentifier_entry_map;

	/** x index then y index, ordered heaps of available indices  */
	struct sol_image_atlas_entry_availability_heap availablity_heaps[SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT][SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT];
	/** array index is x, bit index is y */
	uint16_t availability_masks[SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT];


	/** effectively a rng seed used to increment through all available u64's in a random order vending unique values as it iterates
	 * by starting with zero (and returning the NEXT random number upon identifier request)
	 * it can be ensured that zero will not be hit for 2^64 requests, and thus treated as an invalid identifier */
	uint64_t current_identifier;

	/** the (reserved/unused) entry delineating the start and end of the linked list of entries and the threshold of the current "access" */
	uint32_t header_entry_index;
	uint32_t threshold_entry_index;

	#warning using an accessor mask (instead of single accessor), while maintaining single threaded requirement, might be good for coordinating different sources of writes
	bool accessor_active;
};

static inline bool sol_image_atlas_identifier_entry_compare_equal(uint64_t key, uint32_t* entry_index, struct sol_image_atlas* atlas)
{
	const struct sol_image_atlas_entry* entry_data = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, *entry_index);
	return key == entry_data->identifier;
}

static inline uint64_t sol_image_atlas_entry_identifier_get(const uint32_t* entry_index, struct sol_image_atlas* atlas)
{
	const struct sol_image_atlas_entry* entry_data = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, *entry_index);
	return entry_data->identifier;
}

static inline bool sol_image_atlas_entry_packed_location_cmp_lt(const uint32_t* entry_a_index_ptr, const uint32_t* entry_b_index_ptr, struct sol_image_atlas* atlas)
{
	const struct sol_image_atlas_entry* entry_a = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, *entry_a_index_ptr);
	const struct sol_image_atlas_entry* entry_b = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, *entry_b_index_ptr);

	return entry_a->packed_location < entry_b->packed_location;
}

static inline void sol_image_atlas_entry_set_heap_index(const uint32_t* entry_index_ptr, uint32_t new_index_in_heap, struct sol_image_atlas* atlas)
{
	struct sol_image_atlas_entry* entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, *entry_index_ptr);
	entry->heap_index = new_index_in_heap;

	/** check heap index capacity hasn't been exceeded (index in heap can still be stored) */
	assert(entry->heap_index == new_index_in_heap);
}






static inline void sol_image_atlas_entry_remove_from_queue(struct sol_image_atlas* atlas, struct sol_image_atlas_entry* entry)
{
	struct sol_image_atlas_entry* prev_entry;
	struct sol_image_atlas_entry* next_entry;

	assert(entry->next_entry_index != SOL_IA_INVALID_IDX);
	assert(entry->prev_entry_index != SOL_IA_INVALID_IDX);

	prev_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry->prev_entry_index);
	next_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry->next_entry_index);

	prev_entry->next_entry_index = entry->next_entry_index;
	next_entry->prev_entry_index = entry->prev_entry_index;

	entry->next_entry_index = SOL_IA_INVALID_IDX;
	entry->prev_entry_index = SOL_IA_INVALID_IDX;
}


static inline void sol_image_atlas_entry_add_to_queue_after(struct sol_image_atlas* atlas, uint32_t entry_index, uint32_t prev_entry_index)
{
	struct sol_image_atlas_entry* prev_entry;
	struct sol_image_atlas_entry* next_entry;
	struct sol_image_atlas_entry* entry;
	uint32_t next_entry_index;
	/** the root entry should not be removed in this way */

	entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index);

	assert(entry->next_entry_index == SOL_IA_INVALID_IDX);
	assert(entry->prev_entry_index == SOL_IA_INVALID_IDX);

	prev_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, prev_entry_index);
	next_entry_index = prev_entry->next_entry_index;
	next_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, next_entry_index);

	prev_entry->next_entry_index = entry_index;
	next_entry->prev_entry_index = entry_index;

	entry->next_entry_index = next_entry_index;
	entry->prev_entry_index = prev_entry_index;
}

static inline void sol_image_atlas_entry_add_to_queue_before(struct sol_image_atlas* atlas, uint32_t entry_index, uint32_t next_entry_index)
{
	struct sol_image_atlas_entry* prev_entry;
	struct sol_image_atlas_entry* next_entry;
	struct sol_image_atlas_entry* entry;
	uint32_t prev_entry_index;
	/** the root entry should not be removed in this way */

	entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index);

	assert(entry->next_entry_index == SOL_IA_INVALID_IDX);
	assert(entry->prev_entry_index == SOL_IA_INVALID_IDX);

	next_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, next_entry_index);
	prev_entry_index = next_entry->prev_entry_index;
	prev_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, prev_entry_index);

	prev_entry->next_entry_index = entry_index;
	next_entry->prev_entry_index = entry_index;

	entry->next_entry_index = next_entry_index;
	entry->prev_entry_index = prev_entry_index;
}

static inline void sol_image_atlas_remove_available_entry(struct sol_image_atlas* atlas, struct sol_image_atlas_entry* entry)
{
	struct sol_image_atlas_entry_availability_heap* availability_heap;
	uint32_t validation_index;

	/** validate entry really does look available */
	assert(entry->is_available);
	assert(entry->is_tile_entry);
	assert(entry->identifier == 0);
	assert(entry->prev_entry_index == SOL_IA_INVALID_IDX);
	assert(entry->next_entry_index == SOL_IA_INVALID_IDX);

	availability_heap = &atlas->availablity_heaps[entry->x_size_class][entry->y_size_class];
	sol_image_atlas_entry_availability_heap_remove_index(availability_heap, entry->heap_index, &validation_index, atlas);

	/** make sure entry referenced the location in the heap that referenced the entry we were trying to remove */
	assert(sol_image_atlas_entry_array_access_entry(&atlas->entry_array, validation_index) == entry);

	/** if this was the last entry of the specified size; unset the bit in the availability masks to indicate as such  */
	if(sol_image_atlas_entry_availability_heap_count(availability_heap) == 0)
	{
		atlas->availability_masks[entry->x_size_class] &= ~(1 << entry->y_size_class);
	}
}


/** which entry takes precedence in coalescion may result in a change to the entry being removed */
static inline bool sol_image_atlas_entry_try_coalesce_horizontal(struct sol_image_atlas* atlas, uint32_t* entry_index_ptr)
{
	struct sol_image_atlas_entry* buddy_entry;
	struct sol_image_atlas_entry* coalesce_entry;
	struct sol_image_atlas_entry* adjacent_entry;
	uint32_t buddy_index, coalesce_index, adjacent_index;
	bool odd_offset;

	coalesce_index = *entry_index_ptr;
	coalesce_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, *entry_index_ptr);

	/** check for coalescable buddy */
	odd_offset = coalesce_entry->packed_location & (1u << (coalesce_entry->x_size_class * 2u + 0u));
	buddy_index = odd_offset ? coalesce_entry->adj_start_left : coalesce_entry->adj_end_right;
	if(buddy_index == 0)
	{
		/** root entry index (zero) indicated this entry has no valid neighbours in this direction
		 * must fill entire image to not have a valid buddy though */
		assert(coalesce_entry->x_size_class + SOL_IA_MIN_TILE_SIZE_EXPONENT == atlas->description.image_x_dimension_exponent);
		return false;
	}
	buddy_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, buddy_index);

	if( ! buddy_entry->is_available || buddy_entry->x_size_class != coalesce_entry->x_size_class || buddy_entry->y_size_class != coalesce_entry->y_size_class)
	{
		return false;
	}

	assert(buddy_entry->is_available);
	assert(buddy_entry->is_tile_entry);
	assert(buddy_entry->identifier == 0);

	/** remove buddy from availability heap */
	sol_image_atlas_remove_available_entry(atlas, buddy_entry);

	/** coalesce into the entry closer to zero, so swap entry and it's buddy if necessary to make this happen
	 * this makes the code that follows simpler/invariant and preserves the packed location value */
	if(odd_offset)
	{
		*entry_index_ptr = buddy_index;
		SOL_SWAP(coalesce_entry, buddy_entry);
		SOL_SWAP(coalesce_index, buddy_index);
	}

	/** make sure buddy is in the expected location (y same, x offset by size) */
	assert(sol_ia_p_loc_get_y(coalesce_entry->packed_location) == sol_ia_p_loc_get_y(buddy_entry->packed_location));
	assert(sol_ia_p_loc_get_x(coalesce_entry->packed_location) + (SOL_IA_MIN_TILE_SIZE << coalesce_entry->x_size_class) == sol_ia_p_loc_get_x(buddy_entry->packed_location));

	/** combine buddy with entry*/
	coalesce_entry->x_size_class++;

	coalesce_entry->adj_end_right = buddy_entry->adj_end_right;
	coalesce_entry->adj_end_down   = buddy_entry->adj_end_down;

	/** this combines the buddy (right tile of the 2) into the left tile
	 * so need to set any link that referenced the buddy to now reference the coalesced tile
	 * iterate all edges and replace references to buddy(index) with reference to entry(index) */

	/** traverse right side */
	adjacent_index = coalesce_entry->adj_end_right;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if(adjacent_entry->adj_start_left != buddy_index)
		{
			assert(sol_ia_p_loc_get_y(adjacent_entry->packed_location) < sol_ia_p_loc_get_y(buddy_entry->packed_location));
			break;
		}
		adjacent_entry->adj_start_left = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_up;
	}

	/** traverse bottom side */
	adjacent_index = coalesce_entry->adj_end_down;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if(adjacent_entry->adj_start_up != buddy_index)
		{
			assert(sol_ia_p_loc_get_x(adjacent_entry->packed_location) < sol_ia_p_loc_get_x(buddy_entry->packed_location));
			break;
		}
		adjacent_entry->adj_start_up = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_left;
	}

	/** traverse top side, first half may already reference coalesced index and so muct be skipped */
	adjacent_index = coalesce_entry->adj_start_up;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		/** when starting from top left may already be referencing correct entry (index), so should skip this if encountered */
		if(adjacent_entry->adj_end_down != coalesce_index)
		{
			if(adjacent_entry->adj_end_down != buddy_index)
			{
				/** adjacent entry that fails index check must meet the corner of the coalesced range or be larger than the coalesced allocation */
				assert(sol_ia_p_loc_get_x(adjacent_entry->packed_location) == sol_ia_p_loc_get_x(coalesce_entry->packed_location) + (SOL_IA_MIN_TILE_SIZE << coalesce_entry->x_size_class)
					|| adjacent_entry->x_size_class > coalesce_entry->x_size_class);
				break;
			}
			adjacent_entry->adj_end_down = coalesce_index;
		}

		adjacent_index = adjacent_entry->adj_end_right;
	}

	/** remove the buddy tile entirely, after clearing the entries data (which should not strictly be necessary) */
	*buddy_entry = (struct sol_image_atlas_entry){};
	sol_image_atlas_entry_array_remove(&atlas->entry_array, buddy_index);

	return true;
}

/** which entry takes precedence in coalescion may result in a change to the entry being removed */
static inline bool sol_image_atlas_entry_try_coalesce_vertical(struct sol_image_atlas* atlas, uint32_t* entry_index_ptr)
{
	struct sol_image_atlas_entry* buddy_entry;
	struct sol_image_atlas_entry* coalesce_entry;
	struct sol_image_atlas_entry* adjacent_entry;
	uint32_t buddy_index, coalesce_index, adjacent_index;
	bool odd_offset;

	coalesce_index = *entry_index_ptr;
	coalesce_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, *entry_index_ptr);

	/** check for coalescable buddy */
	odd_offset = coalesce_entry->packed_location & (1u << (coalesce_entry->y_size_class * 2u + 1u));
	buddy_index = odd_offset ? coalesce_entry->adj_start_up : coalesce_entry->adj_end_down;
	if(buddy_index == 0)
	{
		/** root entry index (zero) indicated this entry has no valid neighbours in this direction
		 * must fill entire image to not have a valid buddy though */
		assert(coalesce_entry->y_size_class + SOL_IA_MIN_TILE_SIZE_EXPONENT == atlas->description.image_y_dimension_exponent);
		return false;
	}
	buddy_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, buddy_index);

	if( ! buddy_entry->is_available || buddy_entry->x_size_class != coalesce_entry->x_size_class || buddy_entry->y_size_class != coalesce_entry->y_size_class)
	{
		return false;
	}

	assert(buddy_entry->is_available);
	assert(buddy_entry->is_tile_entry);
	assert(buddy_entry->identifier == 0);

	/** remove buddy from availability heap */
	sol_image_atlas_remove_available_entry(atlas, buddy_entry);

	/** coalesce into the entry closer to zero, so swap entry and it's buddy if necessary to make this happen
	 * this makes the code that follows simpler/invariant and preserves the packed location value */
	if(odd_offset)
	{
		*entry_index_ptr = buddy_index;
		SOL_SWAP(coalesce_entry, buddy_entry);
		SOL_SWAP(coalesce_index, buddy_index);
	}

	/** make sure buddy is in the expected location (x same, y offset by size) */
	assert(sol_ia_p_loc_get_x(coalesce_entry->packed_location) == sol_ia_p_loc_get_x(buddy_entry->packed_location));
	assert(sol_ia_p_loc_get_y(coalesce_entry->packed_location) + (SOL_IA_MIN_TILE_SIZE << coalesce_entry->y_size_class) == sol_ia_p_loc_get_y(buddy_entry->packed_location));

	/** combine buddy with entry*/
	coalesce_entry->y_size_class++;

	coalesce_entry->adj_end_right = buddy_entry->adj_end_right;
	coalesce_entry->adj_end_down  = buddy_entry->adj_end_down;

	/** this combines the buddy (right tile of the 2) into the left tile
	 * so need to set any link that referenced the buddy to now reference the coalesced tile
	 * iterate all edges and replace references to buddy(index) with reference to entry(index) */

	/** traverse bottom side */
	adjacent_index = coalesce_entry->adj_end_down;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if(adjacent_entry->adj_start_up != buddy_index)
		{
			assert(sol_ia_p_loc_get_x(adjacent_entry->packed_location) < sol_ia_p_loc_get_x(buddy_entry->packed_location));
			break;
		}
		adjacent_entry->adj_start_up = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_left;
	}

	/** traverse right side */
	adjacent_index = coalesce_entry->adj_end_right;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if(adjacent_entry->adj_start_left != buddy_index)
		{
			assert(sol_ia_p_loc_get_y(adjacent_entry->packed_location) < sol_ia_p_loc_get_y(buddy_entry->packed_location));
			break;
		}
		adjacent_entry->adj_start_left = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_up;
	}

	/** traverse left side, first half may already reference coalesced index and so muct be skipped */
	adjacent_index = coalesce_entry->adj_start_left;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		/** when starting from top left may already be referencing correct entry (index), so should skip this if encountered */
		if(adjacent_entry->adj_end_right != coalesce_index)
		{
			if(adjacent_entry->adj_end_right != buddy_index)
			{
				/** adjacent entry that fails index check must meet the corner of the coalesced range or be larger than the coalesced allocation */
				assert(sol_ia_p_loc_get_y(adjacent_entry->packed_location) == sol_ia_p_loc_get_y(coalesce_entry->packed_location) + (SOL_IA_MIN_TILE_SIZE << coalesce_entry->y_size_class)
					|| adjacent_entry->y_size_class > coalesce_entry->y_size_class);
				break;
			}
			adjacent_entry->adj_end_right = coalesce_index;
		}

		adjacent_index = adjacent_entry->adj_end_down;
	}

	/** remove the buddy tile entirely, after clearing the entries data (which should not strictly be necessary) */
	*buddy_entry = (struct sol_image_atlas_entry){};
	sol_image_atlas_entry_array_remove(&atlas->entry_array, buddy_index);

	return true;
}

/** NOTE: entry referenced by this index may be completely destroyed, should ONLY call after all other uses of this entry have been completed */
static inline void sol_image_atlas_entry_make_available(struct sol_image_atlas* atlas, uint32_t entry_index)
{
	struct sol_image_atlas_entry_availability_heap* availability_heap;
	struct sol_image_atlas_entry* entry;
	bool coalesced;

	entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index);

	/** entry must have been removed from the map and queue before calling this function */
	assert(entry->prev_entry_index == SOL_IA_INVALID_IDX);
	assert(entry->next_entry_index == SOL_IA_INVALID_IDX);
	assert(entry->identifier == 0);
	assert(entry->is_tile_entry);

	entry->is_available = true;

	/** check to see if this entry can be coalesced with its neighbours */
	do
	{
		/** note: more agressively combine vertically, because also more agressively split vertically */
		if(entry->x_size_class < entry->y_size_class)
		{
			coalesced = sol_image_atlas_entry_try_coalesce_horizontal(atlas, &entry_index);
			if(!coalesced)
			{
				coalesced = sol_image_atlas_entry_try_coalesce_vertical(atlas, &entry_index);
			}
		}
		else
		{
			coalesced = sol_image_atlas_entry_try_coalesce_vertical(atlas, &entry_index);
			if(!coalesced)
			{
				coalesced = sol_image_atlas_entry_try_coalesce_horizontal(atlas, &entry_index);
			}
		}

		entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index);
	}
	while(coalesced);

	/** put the newly available entry in the correct avilibility heap */
	availability_heap = &atlas->availablity_heaps[entry->x_size_class][entry->y_size_class];
	sol_image_atlas_entry_availability_heap_append(availability_heap, entry_index, atlas);

	/** mark the bit in availability mask to indicate there is a tile/entry of the final coalesced size available */
	atlas->availability_masks[entry->x_size_class] |= (1 << entry->y_size_class);
}

/** NOTE: entry referenced by this index may be completely destroyed, should ONLY call after all other uses of this entry have been completed */
static inline void sol_image_atlas_entry_evict(struct sol_image_atlas* atlas, uint32_t entry_index)
{
	struct sol_image_atlas_entry* entry;
	uint32_t map_entry_index;
	enum sol_map_result map_remove_result;

	entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index);
	assert(entry->identifier != 0);
	assert(entry->is_tile_entry);

	map_remove_result = sol_image_atlas_map_remove(&atlas->itentifier_entry_map, entry->identifier, &map_entry_index);
	assert(map_remove_result == SOL_MAP_SUCCESS_REMOVED);
	assert(map_entry_index == entry_index);
	entry->identifier = 0;

	sol_image_atlas_entry_remove_from_queue(atlas, entry);

	sol_image_atlas_entry_make_available(atlas, entry_index);
}

/** NOTE: will always maintain the index of the split location, creating a new entry (and index with it) for the adjacent tile created by splitting
 * this should also only be called on available entries */
static inline void sol_image_atlas_entry_split_horizontally(struct sol_image_atlas* atlas, uint32_t split_index)
{
	struct sol_image_atlas_entry_availability_heap* availability_heap;
	struct sol_image_atlas_entry* split_entry;
	struct sol_image_atlas_entry* buddy_entry;
	struct sol_image_atlas_entry* adjacent_entry;
	uint32_t buddy_index, adjacent_index;

	/** split entry must be accessed after buddy as append alters the array (may relocate in memory) */
	buddy_entry = sol_image_atlas_entry_array_append_ptr(&atlas->entry_array, &buddy_index);
	split_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, split_index);

	/** validate entry to split really does look available */
	assert(split_entry == sol_image_atlas_entry_array_access_entry(&atlas->entry_array, split_index));
	assert(split_entry->is_available);
	assert(split_entry->is_tile_entry);
	assert(split_entry->identifier == 0);
	assert(split_entry->prev_entry_index == SOL_IA_INVALID_IDX);
	assert(split_entry->next_entry_index == SOL_IA_INVALID_IDX);

	/** in order to split entry must be larger than the min size */
	assert(split_entry->x_size_class > 0);
	split_entry->x_size_class--;

	/** check the split entries offset is valid for it's size class */
	assert((split_entry->packed_location & (1u << (split_entry->x_size_class * 2u + 0u))) == 0u);

	*buddy_entry = (struct sol_image_atlas_entry)
	{
		.identifier = 0,
		.next_entry_index = SOL_IA_INVALID_IDX,
		.prev_entry_index = SOL_IA_INVALID_IDX,
		.adj_start_left = split_index,
		.adj_start_up = 0,/** must be set */
		.adj_end_right = split_entry->adj_end_right,
		.adj_end_down  = split_entry->adj_end_down,
		.packed_location = split_entry->packed_location | (1u << (split_entry->x_size_class * 2u + 0u)),
		.x_size_class = split_entry->x_size_class,
		.y_size_class = split_entry->y_size_class,
		// .heap_index
		.is_tile_entry = true,
		.is_available = true,
	};

	split_entry->adj_end_right = buddy_index;
	split_entry->adj_end_down = 0;/** must be set */

	/** traverse right side */
	adjacent_index = buddy_entry->adj_end_right;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);

		if(adjacent_entry->adj_start_left != split_index)
		{
			assert(sol_ia_p_loc_get_y(adjacent_entry->packed_location) < sol_ia_p_loc_get_y(buddy_entry->packed_location));
			break;
		}
		adjacent_entry->adj_start_left = buddy_index;

		adjacent_index = adjacent_entry->adj_start_up;
	}

	/** traverse bottom side */
	adjacent_index = buddy_entry->adj_end_down;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);

		/** equivalent to `if(sol_ia_p_loc_get_x(adjacent_entry->packed_location) < sol_ia_p_loc_get_x(buddy_entry->packed_location))` with fewer instructions */
		if((adjacent_entry->packed_location & SOL_IA_P_LOC_X_MASK) < (buddy_entry->packed_location & SOL_IA_P_LOC_X_MASK))
		{
			assert(split_entry->adj_end_down == 0);
			split_entry->adj_end_down = adjacent_index;
			break;
		}
		adjacent_entry->adj_start_up = buddy_index;

		adjacent_index = adjacent_entry->adj_start_left;
	}

	/** traverse top side */
	adjacent_index = split_entry->adj_start_up;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);

		if(sol_ia_p_loc_start_in_range_x(buddy_entry->packed_location, adjacent_entry->packed_location, adjacent_entry->x_size_class))
		{
			assert(buddy_entry->adj_start_up == 0);
			buddy_entry->adj_start_up = adjacent_index;
		}

		if(adjacent_entry->adj_end_down != split_index)
		{
			/** adjacent entry that fails index check must not have ended in the x range that would have referenced the entry before it was split */
			assert(!sol_ia_p_loc_end_in_range_x(adjacent_entry->packed_location, adjacent_entry->x_size_class, split_entry->packed_location, split_entry->x_size_class + 1));
			break;
		}

		/** change the end down of an adjacent in the range that overlaps with the newly produced buddy */
		if(sol_ia_p_loc_end_in_range_x(adjacent_entry->packed_location, adjacent_entry->x_size_class, buddy_entry->packed_location, buddy_entry->x_size_class))
		{
			adjacent_entry->adj_end_down = buddy_index;
		}

		adjacent_index = adjacent_entry->adj_end_right;
	}

	/** make sure that unknown/unset adjacents got set if they need to be */
	assert(split_entry->adj_end_down != 0 || buddy_entry->adj_end_down == 0);
	assert(buddy_entry->adj_start_up != 0 || split_entry->adj_start_up == 0);

	/** put buddy allocation on heap */
	availability_heap = &atlas->availablity_heaps[buddy_entry->x_size_class][buddy_entry->y_size_class];
	sol_image_atlas_entry_availability_heap_append(availability_heap, buddy_index, atlas);

	/** mark the bit in availability mask to indicate there is a tile/entry of the split off buddies size available */
	assert((atlas->availability_masks[buddy_entry->x_size_class] & (1u << buddy_entry->y_size_class)) == 0);
	atlas->availability_masks[buddy_entry->x_size_class] |= (1u << buddy_entry->y_size_class);
}

/** NOTE: will always maintain the index of the split location, creating a new entry (and index with it) for the adjacent tile created by splitting
 * this should also only be called on available entries */
static inline void sol_image_atlas_entry_split_vertically(struct sol_image_atlas* atlas, uint32_t split_index)
{
	struct sol_image_atlas_entry_availability_heap* availability_heap;
	struct sol_image_atlas_entry* split_entry;
	struct sol_image_atlas_entry* buddy_entry;
	struct sol_image_atlas_entry* adjacent_entry;
	uint32_t buddy_index, adjacent_index;

	/** split entry must be accessed after buddy as append alters the array (may relocate in memory) */
	buddy_entry = sol_image_atlas_entry_array_append_ptr(&atlas->entry_array, &buddy_index);
	split_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, split_index);

	/** validate entry to split really does look available */
	assert(split_entry->is_available);
	assert(split_entry->is_tile_entry);
	assert(split_entry->identifier == 0);
	assert(split_entry->prev_entry_index == SOL_IA_INVALID_IDX);
	assert(split_entry->next_entry_index == SOL_IA_INVALID_IDX);

	/** in order to split entry must be larger than the min size */
	assert(split_entry->y_size_class > 0);
	split_entry->y_size_class--;

	/** check the split entries offset is valid for it's size class */
	assert((split_entry->packed_location & (1u << (split_entry->y_size_class * 2u + 1u))) == 0u);

	*buddy_entry = (struct sol_image_atlas_entry)
	{
		.identifier = 0,
		.next_entry_index = SOL_IA_INVALID_IDX,
		.prev_entry_index = SOL_IA_INVALID_IDX,
		.adj_start_left = 0,/** must be set */
		.adj_start_up = split_index,
		.adj_end_right = split_entry->adj_end_right,
		.adj_end_down  = split_entry->adj_end_down,
		.packed_location = split_entry->packed_location | (1u << (split_entry->y_size_class * 2u + 1u)),
		.x_size_class = split_entry->x_size_class,
		.y_size_class = split_entry->y_size_class,
		// .heap_index
		.is_tile_entry = true,
		.is_available = true,
	};

	split_entry->adj_end_right = 0;/** must be set */
	split_entry->adj_end_down = buddy_index;

	/** traverse bottom side */
	adjacent_index = buddy_entry->adj_end_down;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);

		if(adjacent_entry->adj_start_up != split_index)
		{
			assert(sol_ia_p_loc_get_x(adjacent_entry->packed_location) < sol_ia_p_loc_get_x(buddy_entry->packed_location));
			break;
		}
		adjacent_entry->adj_start_up = buddy_index;

		adjacent_index = adjacent_entry->adj_start_left;
	}

	/** traverse right side */
	adjacent_index = buddy_entry->adj_end_right;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		/** equivalent to `if(sol_ia_p_loc_get_y(adjacent_entry->packed_location) < sol_ia_p_loc_get_y(buddy_entry->packed_location))` with fewer instructions */
		if((adjacent_entry->packed_location & SOL_IA_P_LOC_Y_MASK) < (buddy_entry->packed_location & SOL_IA_P_LOC_Y_MASK))
		{
			assert(split_entry->adj_end_right == 0);
			split_entry->adj_end_right = adjacent_index;
			break;
		}
		adjacent_entry->adj_start_left = buddy_index;

		adjacent_index = adjacent_entry->adj_start_up;
	}

	/** traverse left side */
	adjacent_index = split_entry->adj_start_left;
	while(adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);

		if(sol_ia_p_loc_start_in_range_y(buddy_entry->packed_location, adjacent_entry->packed_location, adjacent_entry->y_size_class))
		{
			assert(buddy_entry->adj_start_left == 0);
			buddy_entry->adj_start_left = adjacent_index;
		}

		if(adjacent_entry->adj_end_right != split_index)
		{
			/** adjacent entry that fails index check must not have ended in the y range that would have referenced the entry before it was split */
			assert(!sol_ia_p_loc_end_in_range_y(adjacent_entry->packed_location, adjacent_entry->y_size_class, split_entry->packed_location, split_entry->y_size_class + 1));
			break;
		}

		/** NOTE: if adjacent would exceed the y range where it should set end_right to the buddy index, it would have hit break above */
		/** if(sol_ia_p_loc_get_y(adjacent_entry->packed_location) >= sol_ia_p_loc_get_y(buddy_entry->packed_location)) */
		// if((adjacent_entry->packed_location & SOL_IA_P_LOC_Y_MASK) >= (buddy_entry->packed_location & SOL_IA_P_LOC_Y_MASK))
		if(sol_ia_p_loc_end_in_range_y(adjacent_entry->packed_location, adjacent_entry->y_size_class, buddy_entry->packed_location, buddy_entry->y_size_class))
		{
			adjacent_entry->adj_end_right = buddy_index;
		}

		adjacent_index = adjacent_entry->adj_end_down;
	}

	/** make sure that unknown/unset adjacents got set if they need to be */
	assert(split_entry->adj_end_right != 0  || buddy_entry->adj_end_right == 0);
	assert(buddy_entry->adj_start_left != 0 || split_entry->adj_start_left == 0);

	/** put buddy allocation on heap */
	availability_heap = &atlas->availablity_heaps[buddy_entry->x_size_class][buddy_entry->y_size_class];
	sol_image_atlas_entry_availability_heap_append(availability_heap, buddy_index, atlas);

	/** mark the bit in availability mask to indicate there is a tile/entry of the split off buddies size available */
	assert((atlas->availability_masks[buddy_entry->x_size_class] & (1u << buddy_entry->y_size_class)) == 0);
	atlas->availability_masks[buddy_entry->x_size_class] |= (1u << buddy_entry->y_size_class);
}

static inline bool sol_image_atlas_acquire_available_entry_of_size(struct sol_image_atlas* atlas, uint32_t required_x_size_class, uint32_t required_y_size_class, uint32_t* entry_index_ptr)
{
	struct sol_image_atlas_entry_availability_heap* availability_heap;
	uint32_t entry_index, x_min, y_min, min_split_count, x, y, split_count, y_mask;
	uint16_t availiability_mask;
	bool acquired_available;

	min_split_count = UINT32_MAX;
	y_mask = -(1u << required_y_size_class);

	for(x = required_x_size_class; x < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; x++)
	{
		availiability_mask = atlas->availability_masks[x] & y_mask;
		if(availiability_mask)
		{
			y = sol_u32_ctz(availiability_mask);
			split_count = y + x;
			if(split_count <= min_split_count)
			{
				min_split_count = split_count;
				x_min = x;
				y_min = y;
			}
		}
	}

	if(min_split_count == UINT32_MAX)
	{
		return false;
	}

	/** must have index space to allocate newly split tiles */
	split_count = min_split_count - required_x_size_class - required_y_size_class;
	if( sol_image_atlas_entry_array_active_count(&atlas->entry_array) + split_count >= SOL_IA_MAX_ENTRIES)
	{
		return false;
	}


	availability_heap = &atlas->availablity_heaps[x_min][y_min];
	acquired_available = sol_image_atlas_entry_availability_heap_remove(availability_heap, entry_index_ptr, atlas);
	entry_index = *entry_index_ptr;
	/** mask should properly track availability */
	assert(acquired_available);

	/** if this was the last entry of the specified size; unset the bit in the availability masks to indicate as such  */
	if(sol_image_atlas_entry_availability_heap_count(availability_heap) == 0)
	{
		atlas->availability_masks[x_min] &= ~(1 << y_min);
	}


	/** validate entry really does look available */
	assert(sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index)->is_available);
	assert(sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index)->is_tile_entry);
	assert(sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index)->identifier == 0);
	assert(sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index)->prev_entry_index == SOL_IA_INVALID_IDX);
	assert(sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index)->next_entry_index == SOL_IA_INVALID_IDX);

	while(x_min != required_x_size_class || y_min != required_y_size_class)
	{
		/** check entry is currently of expected size */
		assert(sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index)->x_size_class == x_min);
		assert(sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index)->y_size_class == y_min);

		/** preferentially split vertically if possible */
		if(x_min == required_x_size_class || (y_min >= x_min && y_min != required_y_size_class))
		{
			assert(sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index)->y_size_class > required_y_size_class);
			sol_image_atlas_entry_split_vertically(atlas, entry_index);
			y_min--;
		}
		else
		{
			assert(sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index)->x_size_class > required_x_size_class);
			sol_image_atlas_entry_split_horizontally(atlas, entry_index);
			x_min--;
		}
	}

	return true;
}





struct sol_image_atlas* sol_image_atlas_create(const struct sol_image_atlas_description* description, struct cvm_vk_device* device)
{
	uint32_t x_size_class, y_size_class, array_layer, entry_index;
	struct sol_image_atlas* atlas = malloc(sizeof(struct sol_image_atlas));
	VkResult result;

	assert(description->image_x_dimension_exponent >= 8 && description->image_x_dimension_exponent <= 16);
	assert(description->image_y_dimension_exponent >= 8 && description->image_y_dimension_exponent <= 16);
	assert(description->image_array_dimension > 0);

	atlas->description = *description;

	sol_vk_timeline_semaphore_initialise(&atlas->timeline_semaphore, device);
	atlas->current_moment = sol_vk_timeline_semaphore_get_current_moment(&atlas->timeline_semaphore);

	const VkImageCreateInfo image_create_info =
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = description->format,
		.extent = (VkExtent3D)
		{
			.width  = 1u << description->image_x_dimension_exponent,
			.height = 1u << description->image_y_dimension_exponent,
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

	sol_image_atlas_entry_array_initialise(&atlas->entry_array, 1024);

	/** NOTE: only up to 2M entries are actually supported */
	struct sol_hash_map_descriptor map_descriptor = {
		.entry_space_exponent_initial = 12,// 2^12
		.entry_space_exponent_limit = 21,// 2^21
		.resize_fill_factor = 160,// out of 256
		.limit_fill_factor = 192,// out of 256
	};
	sol_image_atlas_map_initialise(&atlas->itentifier_entry_map, map_descriptor, atlas);

	/** NOTE: this will get updated/iterated before being vended, ergo 0 will not be vended for the first 2^64 identifiers, so may be treated as invalid */
	atlas->current_identifier = 0;
	atlas->accessor_active = false;

	*sol_image_atlas_entry_array_append_ptr(&atlas->entry_array, &atlas->header_entry_index) = (struct sol_image_atlas_entry)
	{
		.identifier = 0,
		.is_tile_entry = false,
		.prev_entry_index = atlas->header_entry_index,
		.next_entry_index = atlas->header_entry_index,
	};

	*sol_image_atlas_entry_array_append_ptr(&atlas->entry_array, &atlas->threshold_entry_index) = (struct sol_image_atlas_entry)
	{
		.identifier = 0,
		.is_tile_entry = false,
		.prev_entry_index = SOL_IA_INVALID_IDX,
		.next_entry_index = SOL_IA_INVALID_IDX,
	};

	/** for tile entries 2d structure (the buddy portion), index zero is reserved, make sure the unchanging root occupies this index */
	assert(atlas->header_entry_index == 0);


	for(x_size_class = 0; x_size_class < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; x_size_class++)
	{
		atlas->availability_masks[x_size_class] = 0;
		for(y_size_class = 0; y_size_class < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; y_size_class++)
		{
			sol_image_atlas_entry_availability_heap_initialise(&atlas->availablity_heaps[x_size_class][y_size_class], 0);
		}
	}

	/** populate availability for atlas image sized entries */
	x_size_class = atlas->description.image_x_dimension_exponent - SOL_IA_MIN_TILE_SIZE_EXPONENT;
	y_size_class = atlas->description.image_y_dimension_exponent - SOL_IA_MIN_TILE_SIZE_EXPONENT;

	printf("image atlas populated with %u : 4*2^%u x 4*2^%u layers\n",atlas->description.image_array_dimension, x_size_class, y_size_class);

	for(array_layer = 0; array_layer < atlas->description.image_array_dimension; array_layer++)
	{
		/** for every array layer make an entry filling the slive available */
		*sol_image_atlas_entry_array_append_ptr(&atlas->entry_array, &entry_index) = (struct sol_image_atlas_entry)
		{
			.identifier = 0, /** "invalid" identifier */
			.next_entry_index = SOL_IA_INVALID_IDX,
			.prev_entry_index = SOL_IA_INVALID_IDX,
			.adj_start_left     = 0,
			.adj_start_up       = 0,
			.adj_end_right = 0,
			.adj_end_down   = 0,
			.packed_location = array_layer << 24,
			.x_size_class = x_size_class,
			.y_size_class = y_size_class,
			// .heap_index
			.is_tile_entry = true,
			.is_available = true,
		};
		/** put the entry in the heap and set availability mask */
		sol_image_atlas_entry_availability_heap_append(&atlas->availablity_heaps[x_size_class][y_size_class], entry_index, atlas);
		atlas->availability_masks[x_size_class] |= 1 << y_size_class;
	}

	return atlas;
}

void sol_image_atlas_destroy(struct sol_image_atlas* atlas, struct cvm_vk_device* device)
{
	uint32_t x_size_class, y_size_class;
	struct sol_image_atlas_entry* header_entry;
	uint32_t entry_index;
	struct sol_image_atlas_entry* entry;
	struct sol_image_atlas_entry* threshold_entry;

	/** must release current access before destroying */
	assert(!atlas->accessor_active);

	/** wait on and then release most recent access */
	if(atlas->current_moment.semaphore != VK_NULL_HANDLE)
	{
		sol_vk_timeline_semaphore_moment_wait(&atlas->current_moment, device);
	}

	threshold_entry = sol_image_atlas_entry_array_remove_ptr(&atlas->entry_array, atlas->threshold_entry_index);
	/** the threshold entry must have been removed as part of the accessor being released */
	assert(threshold_entry->next_entry_index == SOL_IA_INVALID_IDX);
	assert(threshold_entry->prev_entry_index == SOL_IA_INVALID_IDX);


	/** free all entries in the expired linked list queue
	 * NOTE: it is very important that nothing in this loop will alter the backing of the entry array
	 * doing so would invalidate the header entry pointer */
	header_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, atlas->header_entry_index);
	while(header_entry->next_entry_index != atlas->header_entry_index)
	{
		/** check we arent encountering transient resources here */
		assert((sol_image_atlas_entry_array_access_entry(&atlas->entry_array, header_entry->next_entry_index)->identifier & SOL_IA_IDENTIFIER_TRANSIENT_BIT) == 0);

		sol_image_atlas_entry_evict(atlas, header_entry->next_entry_index);
	}
	assert(header_entry->next_entry_index == atlas->header_entry_index);
	assert(header_entry->prev_entry_index == atlas->header_entry_index);
	sol_image_atlas_entry_array_remove(&atlas->entry_array, atlas->header_entry_index);


	x_size_class = atlas->description.image_x_dimension_exponent - SOL_IA_MIN_TILE_SIZE_EXPONENT;
	y_size_class = atlas->description.image_y_dimension_exponent - SOL_IA_MIN_TILE_SIZE_EXPONENT;
	assert(sol_image_atlas_entry_availability_heap_count(&atlas->availablity_heaps[x_size_class][y_size_class]) == atlas->description.image_array_dimension);

	while(sol_image_atlas_entry_availability_heap_remove(&atlas->availablity_heaps[x_size_class][y_size_class], &entry_index, atlas))
	{
		entry = sol_image_atlas_entry_array_remove_ptr(&atlas->entry_array, entry_index);
		/** assert contents are as expected */
		assert(entry->x_size_class == x_size_class);
		assert(entry->y_size_class == y_size_class);
		assert((entry->packed_location & 0x00FFFFFFu) == 0u);
		assert(entry->next_entry_index == SOL_IA_INVALID_IDX);
		assert(entry->prev_entry_index == SOL_IA_INVALID_IDX);
		assert(entry->adj_start_left == 0);
		assert(entry->adj_start_up == 0);
		assert(entry->adj_end_right == 0);
		assert(entry->adj_end_down == 0);
		assert(entry->identifier == 0);
		assert(entry->is_tile_entry);
		assert(entry->is_available);
	}

	assert(sol_image_atlas_entry_array_is_empty(&atlas->entry_array));


	for(x_size_class = 0; x_size_class < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; x_size_class++)
	{
		for(y_size_class = 0; y_size_class < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; y_size_class++)
		{
			assert(sol_image_atlas_entry_availability_heap_count(&atlas->availablity_heaps[x_size_class][y_size_class]) == 0);
			sol_image_atlas_entry_availability_heap_terminate(&atlas->availablity_heaps[x_size_class][y_size_class]);
		}
	}

	sol_image_atlas_map_terminate(&atlas->itentifier_entry_map);
	sol_image_atlas_entry_array_terminate(&atlas->entry_array);

	sol_vk_timeline_semaphore_terminate(&atlas->timeline_semaphore, device);

	vkDestroyImageView(device->device, atlas->image_view, device->host_allocator);
	sol_vk_supervised_image_terminate(&atlas->image, device);

	free(atlas);
}

struct sol_vk_timeline_semaphore_moment sol_image_atlas_access_scope_setup_begin(struct sol_image_atlas* atlas)
{
	assert(!atlas->accessor_active);
	atlas->accessor_active = true;

	/** place the threshold to the front of the queue */
	sol_image_atlas_entry_add_to_queue_before(atlas, atlas->threshold_entry_index, atlas->header_entry_index);

	return atlas->current_moment;
}

struct sol_vk_timeline_semaphore_moment sol_image_atlas_access_scope_setup_end(struct sol_image_atlas* atlas)
{
	struct sol_image_atlas_entry* threshold_entry;

	assert(atlas->accessor_active);
	atlas->accessor_active = false;

	/** NOTE: it is very important that nothing in this loop will alter the backing of the entry array
	 * doing so would invalidate the threshold entry pointer */
	threshold_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, atlas->threshold_entry_index);
	while(sol_image_atlas_entry_array_access_entry(&atlas->entry_array, threshold_entry->next_entry_index)->identifier & SOL_IA_IDENTIFIER_TRANSIENT_BIT)
	{
		sol_image_atlas_entry_evict(atlas, threshold_entry->next_entry_index);
	}

	/** remove the threshold from the queue */
	sol_image_atlas_entry_remove_from_queue(atlas, threshold_entry);

	atlas->current_moment = sol_vk_timeline_semaphore_generate_new_moment(&atlas->timeline_semaphore);

	return atlas->current_moment;
}

bool sol_image_atlas_access_scope_is_active(struct sol_image_atlas* atlas)
{
	return atlas->accessor_active;
}

uint64_t sol_image_atlas_acquire_entry_identifier(struct sol_image_atlas* atlas, bool transient)
{
	/** 64 bit lcg copied from sol random */
	atlas->current_identifier = atlas->current_identifier * 0x5851F42D4C957F2Dlu + 0x7A4111AC0FFEE60Dlu;

	/** NOTE: top N bits may be cut off to reduce cycle length to 2^(64-n), every point an any 2^m cycle will be visited in the bottom m bits for a lcg */

	if(transient)
	{
		return atlas->current_identifier | SOL_IA_IDENTIFIER_TRANSIENT_BIT;
	}
	else
	{
		return atlas->current_identifier & (~SOL_IA_IDENTIFIER_TRANSIENT_BIT);
	}
}

enum sol_image_atlas_result sol_image_atlas_entry_find(struct sol_image_atlas* atlas, uint64_t entry_identifier, struct sol_image_atlas_location* entry_location)
{
	struct sol_image_atlas_entry* entry;
	enum sol_map_result map_find_result;
	uint32_t* entry_index_ptr;
	uint32_t entry_index;

	/** must have an accessor active to be able to use entries */
	assert(atlas->accessor_active);

	map_find_result = sol_image_atlas_map_find(&atlas->itentifier_entry_map, entry_identifier, &entry_index_ptr);

	if(map_find_result == SOL_MAP_SUCCESS_FOUND)
	{
		#warning handle write vs read case (does it make sense to be able to write after find, rather than obtain? )

		entry_index = *entry_index_ptr;
		entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index);

		assert(entry->identifier == entry_identifier);

		sol_image_atlas_entry_remove_from_queue(atlas, entry);
		if(entry_identifier & SOL_IA_IDENTIFIER_TRANSIENT_BIT)
		{
			sol_image_atlas_entry_add_to_queue_after(atlas, entry_index, atlas->threshold_entry_index);
		}
		else
		{
			sol_image_atlas_entry_add_to_queue_before(atlas, entry_index, atlas->header_entry_index);
		}

		entry_location->array_layer = sol_ia_p_loc_get_layer(entry->packed_location);
		entry_location->offset.x = sol_ia_p_loc_get_x(entry->packed_location);
		entry_location->offset.y = sol_ia_p_loc_get_y(entry->packed_location);

		return SOL_IMAGE_ATLAS_SUCCESS_FOUND;
	}
	else
	{
		assert(map_find_result == SOL_MAP_FAIL_ABSENT);
		return SOL_IMAGE_ATLAS_FAIL_ABSENT;
	}
}

enum sol_image_atlas_result sol_image_atlas_entry_obtain(struct sol_image_atlas* atlas, uint64_t entry_identifier, u16_vec2 size, uint32_t flags, struct sol_image_atlas_location* entry_location)
{
	/** NOTE acquiring space (when required) will change the hash map
	 * as such the pointer returned for obtain will no longer be valid and is not worth keeping around
	 * so its better to attempt find and then obtain instead once space has been made */

	/** write access will never return found, only absent or inserted */
	struct sol_image_atlas_entry_availability_heap* availability_heap;
	struct sol_image_atlas_entry* entry;
	const struct sol_image_atlas_entry* top_available_entry;
	const struct sol_image_atlas_entry* evictable_entry;
	const struct sol_image_atlas_entry* header_entry;
	enum sol_map_result map_find_result, map_remove_result;
	uint32_t* entry_index_in_map;
	uint32_t entry_index, x_size_class, y_size_class, top_available_entry_index;
	bool size_mismatch, better_location;

	/** is invalid to request an entry with no pixels */
	assert(size.x > 0 && size.y > 0);

	/** must have an accessor active to be able to use entries */
	assert(atlas->accessor_active);
	/** zero identifier is reserved */
	assert(entry_identifier != 0);

	x_size_class = SOL_MAX(sol_u32_exp_ge(size.x), SOL_IA_MIN_TILE_SIZE_EXPONENT) - SOL_IA_MIN_TILE_SIZE_EXPONENT;
	y_size_class = SOL_MAX(sol_u32_exp_ge(size.y), SOL_IA_MIN_TILE_SIZE_EXPONENT) - SOL_IA_MIN_TILE_SIZE_EXPONENT;

	map_find_result = sol_image_atlas_map_find(&atlas->itentifier_entry_map, entry_identifier, &entry_index_in_map);
	size_mismatch = false;

	entry = NULL;// not needed
	if(map_find_result == SOL_MAP_SUCCESS_FOUND)
	{
		entry_index = *entry_index_in_map;
		entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index);

		assert(entry->identifier == entry_identifier);

		/** its supported to request a different sized entry if also accessing with write_access
		 * so need to handle reallocation/repositioning of entries in this case */
		size_mismatch = x_size_class != entry->x_size_class || y_size_class != entry->y_size_class;
		/** if there is a size mismatch must have requested write access */
		assert(!size_mismatch || (flags & SOL_IMAGE_ATLAS_OBTAIN_FLAG_WRITE));

		sol_image_atlas_entry_remove_from_queue(atlas, entry);

		if(flags & SOL_IMAGE_ATLAS_OBTAIN_FLAG_WRITE)
		{
			#warning move this to a function...?
			/** use this opportunity to move to a "better" location if one exists
			 * a better analysis might check if splitting new location would result in a better location
			 * balanced against current locations potential to coalesce,
			 * (maybe the general search region for new location is determined or weighted by present locations coalescion potential)
			 * couls also spurriously fail (requiring re-creation of resource, though this is unnecessarily expensive) */
			if(atlas->availability_masks[x_size_class] & (1u << y_size_class))
			{
				availability_heap = &atlas->availablity_heaps[x_size_class][y_size_class];
				top_available_entry_index = *sol_image_atlas_entry_availability_heap_access_top(availability_heap);
				top_available_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, top_available_entry_index);

				better_location = top_available_entry->packed_location < entry->packed_location;
			}
		}

		if(size_mismatch || better_location)
		{
			entry->identifier = 0;
			/** make entry available and replace it, mark map as errant in this location until then (not strictly needed) */
			sol_image_atlas_entry_make_available(atlas, entry_index);

			/** NOTE: this invalidates the entry so set it as such and poison the map for this identifier
			 * neither of these are actually necessary */
			*entry_index_in_map = 0;
			entry = NULL;
		}
		else
		{
			/** put in entry queue sector of accessor,
			 * found and NOT replaced (hot path) */
			if(entry_identifier & SOL_IA_IDENTIFIER_TRANSIENT_BIT)
			{
				sol_image_atlas_entry_add_to_queue_after(atlas, entry_index, atlas->threshold_entry_index);
			}
			else
			{
				sol_image_atlas_entry_add_to_queue_before(atlas, entry_index, atlas->header_entry_index);
			}
			*entry_location = (struct sol_image_atlas_location)
			{
				.array_layer = sol_ia_p_loc_get_layer(entry->packed_location),
				.offset.x = sol_ia_p_loc_get_x(entry->packed_location),
				.offset.y = sol_ia_p_loc_get_y(entry->packed_location),
			};
			return SOL_IMAGE_ATLAS_SUCCESS_FOUND;
		}
	}

	/** no entry, either not found or needs to be replaced */
	if( ! sol_image_atlas_acquire_available_entry_of_size(atlas, x_size_class, y_size_class, &entry_index))
	{
		/** no entry of requested size available, need to make space by freeing unused entries
		 * NOTE: this invalidates the map result */
		entry_index_in_map = NULL;
		/** NOTE: it is very important that nothing in this loop will alter the backing of the entry array
		 * doing so would invalidate the header entry pointer */
		header_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, atlas->header_entry_index);
		do
		{
			if(header_entry->next_entry_index != atlas->threshold_entry_index)
			{
				/** check we arent encountering transient resources here */
				assert((sol_image_atlas_entry_array_access_entry(&atlas->entry_array, header_entry->next_entry_index)->identifier & SOL_IA_IDENTIFIER_TRANSIENT_BIT) == 0);

				sol_image_atlas_entry_evict(atlas, header_entry->next_entry_index);
			}
			else
			{
				/** failed to allocate an entry of this size because the map is full, ergo fail */
				if(map_find_result == SOL_MAP_SUCCESS_FOUND)
				{
					/** should only fail to reallocate an entry if there was a size mismatch */
					assert(size_mismatch);
					/** if the entry did exist in the map it must be removed as we cannot actually back it, and an unbacked entry is illegal */
					map_remove_result = sol_image_atlas_map_remove(&atlas->itentifier_entry_map, entry_identifier, NULL);
					assert(map_remove_result == SOL_MAP_SUCCESS_REMOVED);
				}
				return SOL_IMAGE_ATLAS_FAIL_IMAGE_FULL;
			}
		}
		while( ! sol_image_atlas_acquire_available_entry_of_size(atlas, x_size_class, y_size_class, &entry_index));
	}

	entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index);

	assert(entry->is_available);
	assert(entry->identifier == 0);

	/** if the map has been altered then the pointer to the old entry will have become invalid (indicated by being set to null)
	 * as such the entrty in the map must be obtained again */
	if(entry_index_in_map == NULL)
	{
		switch(sol_image_atlas_map_obtain(&atlas->itentifier_entry_map, entry_identifier, &entry_index_in_map))
		{
		case SOL_MAP_FAIL_FULL:
			/** should not fail to find this entry again if it did exist previously
			 * as it will not have been removed, but may have been relocated */
			assert(map_find_result != SOL_MAP_SUCCESS_FOUND);
			fprintf(stderr, "image atlas identifier map is full or has suffered a hash colission, this should not be possible in reasonable scenarios");
			/** could not create space in the map so put the entry back on the available heap and return failure
			 * this should be very unlikely*/
			sol_image_atlas_entry_make_available(atlas, entry_index);
			return SOL_IMAGE_ATLAS_FAIL_MAP_FULL;
		default:
			assert(entry_index_in_map != NULL);
		}
	}


	*entry_index_in_map = entry_index;
	// assert some things about entry here?
	entry->identifier = entry_identifier;
	entry->is_available = false;
	if(entry_identifier & SOL_IA_IDENTIFIER_TRANSIENT_BIT)
	{
		sol_image_atlas_entry_add_to_queue_after(atlas, entry_index, atlas->threshold_entry_index);
	}
	else
	{
		sol_image_atlas_entry_add_to_queue_before(atlas, entry_index, atlas->header_entry_index);
	}

	*entry_location = (struct sol_image_atlas_location)
	{
		.array_layer = sol_ia_p_loc_get_layer(entry->packed_location),
		.offset.x = sol_ia_p_loc_get_x(entry->packed_location),
		.offset.y = sol_ia_p_loc_get_y(entry->packed_location),
	};

	return SOL_IMAGE_ATLAS_SUCCESS_INSERTED;
}

bool sol_image_atlas_entry_release(struct sol_image_atlas* atlas, uint64_t entry_identifier)
{
	assert(false);// NYI
	enum sol_map_result map_find_result;
	uint32_t* entry_index_in_map;

	map_find_result = sol_image_atlas_map_find(&atlas->itentifier_entry_map, entry_identifier, &entry_index_in_map);

	if(map_find_result == SOL_MAP_SUCCESS_FOUND)
	{
		sol_image_atlas_entry_make_available(atlas, *entry_index_in_map);

		return true;
	}

	return false;
}

struct sol_vk_supervised_image* sol_image_atlas_access_supervised_image(struct sol_image_atlas* atlas)
{
	return &atlas->image;
}

VkImageView sol_image_atlas_access_image_view(struct sol_image_atlas* atlas)
{
	return atlas->image_view;
}








