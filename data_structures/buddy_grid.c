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

/** note derived/extracted from image grid algorithm */

#include <assert.h>
#include <stdio.h>

#include "data_structures/buddy_grid.h"


/** maximum xy dimension classes; the maximum number of sizes a tile can be (0-12 inclusive) */
#define SOL_BUDDY_GRID_SIZE_CLASS_COUNT 13

struct sol_buddy_grid_entry
{

	/** links within the 2D structure of an array layer of the grid
	 * index zero is invalid, which should be the index of root node of the active entry linked list
	 * names indicate a cardinal direction from a corner;
	 * start corner is towards zero (top left of region)
	 * end corner is away from zero (bottom right of region) */
	uint32_t adj_start_left;
	uint32_t adj_start_up;
	uint32_t adj_end_right;
	uint32_t adj_end_down;

	/** index in availability heap of size class (x,y)
	 * NOTE: could possibly be unioned with prev/next as this is (presently) not used at the same time as being in a linked list 
	 * NOTE: could also represent position in "in use" heap to inform defragmentation */
	uint32_t heap_index;

	/** z-tile location is in terms of minimum entry pixel dimension (4)
	 * packed in such a way to order entries by layer first then top left -> bottom right
	 * i.e. packed like: | array layer (8) | z-tile location (24 == 12x + 12y) |
	 * this is used to sort/order entries (lower value means better placed) */
	uint32_t packed_location;

	/** unpacked location of region/entry */ 
	u16_vec2 xy_offset;
	uint8_t array_layer;


	/** size class is power of 2 times minimum entry pixel dimension (4)
	 * so: 4 * 2 ^ (0:15) is more than enough (4 << 15 = 128k, is larger than max texture size)
	 * maximum expected size class is 12 */
	uint8_t x_size_class : 4;
	uint8_t y_size_class : 4;


	uint8_t is_available : 1;
};

#define SOL_ARRAY_ENTRY_TYPE struct sol_buddy_grid_entry
#define SOL_ARRAY_STRUCT_NAME sol_buddy_grid_entry_array
#include "data_structures/array.h"

/** heaps store the z-tile ordered indices of available entries */
static inline bool sol_buddy_grid_entry_packed_location_cmp_lt(const uint32_t* entry_a_index_ptr, const uint32_t* entry_b_index_ptr, struct sol_buddy_grid* grid);
static inline void sol_buddy_grid_entry_set_heap_index(const uint32_t* entry_index_ptr, uint32_t new_index_in_heap, struct sol_buddy_grid* grid);

#define SOL_BINARY_HEAP_ENTRY_TYPE uint32_t
#define SOL_BINARY_HEAP_STRUCT_NAME sol_buddy_grid_entry_availability_heap
#define SOL_BINARY_HEAP_CONTEXT_TYPE struct sol_buddy_grid*
#define SOL_BINARY_HEAP_ENTRY_CMP_LT(A, B, CTX) sol_buddy_grid_entry_packed_location_cmp_lt(A, B, CTX)
#define SOL_BINARY_HEAP_SET_ENTRY_INDEX(E, IDX, CTX) sol_buddy_grid_entry_set_heap_index(E, IDX, CTX)
#include "data_structures/binary_heap.h"

struct sol_buddy_grid
{
	/** provided description */
	struct sol_buddy_grid_description description;


	/** all entry indices reference indices in this array, contains the actual information regarding vended tiles */
	struct sol_buddy_grid_entry_array entry_array;


	/** x index then y index, ordered heaps of available indices  */
	struct sol_buddy_grid_entry_availability_heap availablity_heaps[SOL_BUDDY_GRID_SIZE_CLASS_COUNT][SOL_BUDDY_GRID_SIZE_CLASS_COUNT];

	/** array index is x, bit index is y */
	uint16_t availability_masks[SOL_BUDDY_GRID_SIZE_CLASS_COUNT];
};


static inline bool sol_buddy_grid_entry_packed_location_cmp_lt(const uint32_t* entry_a_index_ptr, const uint32_t* entry_b_index_ptr, struct sol_buddy_grid* grid)
{
	const struct sol_buddy_grid_entry* entry_a = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, *entry_a_index_ptr);
	const struct sol_buddy_grid_entry* entry_b = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, *entry_b_index_ptr);

	return entry_a->packed_location < entry_b->packed_location;
}
static inline void sol_buddy_grid_entry_set_heap_index(const uint32_t* entry_index_ptr, uint32_t new_index_in_heap, struct sol_buddy_grid* grid)
{
	struct sol_buddy_grid_entry* entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, *entry_index_ptr);
	entry->heap_index = new_index_in_heap;
}



#define SOL_BUDDY_GRID_PACKED_X_MASK     0x00555555u
#define SOL_BUDDY_GRID_PACKED_Y_MASK     0x00AAAAAAu
#define SOL_BUDDY_GRID_PACKED_LAYER_MASK 0xFF000000u

/** x on even bits, y on odd bits */
#define SOL_BUDDY_GRID_PACKED_X_BASE     0x00000001u
#define SOL_BUDDY_GRID_PACKED_Y_BASE     0x00000002u

/** note this gets location in pixels, REQUIRES that SOL_IA_MIN_TILE_SIZE_EXPONENT is 2*/
static inline uint32_t sol_buddy_grid_packed_loc_get_x(uint32_t packed_location)
{
	/** 0x00555555 */
	packed_location = ((packed_location & 0x00444444u) >> 1) | ((packed_location & 0x00111111u)     );
	/** 0x00333333 */
	packed_location = ((packed_location & 0x00303030u) >> 2) | ((packed_location & 0x00030303u)     );
	/** 0x000F0F0F */
	return ((packed_location & 0x000F0000u) >> 8) | ((packed_location & 0x00000F00u) >> 4) | (packed_location & 0x0000000Fu);
	/** 0x00000FFF */
}

static inline uint32_t sol_buddy_grid_packed_loc_get_y(uint32_t packed_location)
{
	/** 0x00AAAAAA */
	packed_location = ((packed_location & 0x00888888u) >> 2) | ((packed_location & 0x00222222u) >> 1);
	/** 0x00333333 */
	packed_location = ((packed_location & 0x00303030u) >> 2) | ((packed_location & 0x00030303u)     );
	/** 0x000F0F0F */
	return ((packed_location & 0x000F0000u) >> 8) | ((packed_location & 0x00000F00u) >> 4) | (packed_location & 0x0000000Fu);
	/** 0x00000FFF */
}

static inline uint32_t sol_buddy_grid_packed_loc_get_layer(uint32_t packed_location)
{
	return packed_location >> 24;
}




static inline void sol_buddy_grid_remove_available_entry(struct sol_buddy_grid* grid, struct sol_buddy_grid_entry* entry)
{
	struct sol_buddy_grid_entry_availability_heap* availability_heap;
	uint32_t validation_index;

	/** validate entry really does look available */
	assert(entry->is_available);

	availability_heap = &grid->availablity_heaps[entry->x_size_class][entry->y_size_class];
	sol_buddy_grid_entry_availability_heap_withdraw_index(availability_heap, entry->heap_index, &validation_index, grid);

	/** make sure entry referenced the location in the heap that referenced the entry we were trying to remove */
	assert(sol_buddy_grid_entry_array_access_entry(&grid->entry_array, validation_index) == entry);

	/** if this was the last entry of the specified size; unset the bit in the availability masks to indicate as such  */
	if(sol_buddy_grid_entry_availability_heap_count(availability_heap) == 0)
	{
		grid->availability_masks[entry->x_size_class] &= ~(1 << entry->y_size_class);
	}
}


/** which entry takes precedence in coalescion may result in a change to the entry being removed */
static inline bool sol_buddy_grid_entry_try_coalesce_horizontal(struct sol_buddy_grid* grid, uint32_t* entry_index_ptr)
{
	struct sol_buddy_grid_entry* buddy_entry;
	struct sol_buddy_grid_entry* coalesce_entry;
	struct sol_buddy_grid_entry* adjacent_entry;
	uint32_t buddy_index, coalesce_index, adjacent_index;
	bool odd_offset;

	coalesce_index = *entry_index_ptr;
	coalesce_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, *entry_index_ptr);

	/** check for coalescable buddy */
	odd_offset = coalesce_entry->xy_offset.x & (1u << coalesce_entry->x_size_class);
	buddy_index = odd_offset ? coalesce_entry->adj_start_left : coalesce_entry->adj_end_right;
	if(buddy_index == 0)
	{
		/** root entry index (zero) indicated this entry has no valid neighbours in this direction
		 * must fill entire image to not have a valid buddy though */
		assert(coalesce_entry->x_size_class == grid->description.image_x_dimension_exponent);
		assert(coalesce_entry->xy_offset.x == 0);
		return false;
	}
	buddy_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, buddy_index);

	if( ! buddy_entry->is_available || buddy_entry->x_size_class != coalesce_entry->x_size_class || buddy_entry->y_size_class != coalesce_entry->y_size_class)
	{
		return false;
	}

	assert(buddy_entry->is_available);

	/** remove buddy from availability heap */
	sol_buddy_grid_remove_available_entry(grid, buddy_entry);

	/** coalesce into the entry closer to zero, so swap entry and it's buddy if necessary to make this happen
	 * this makes the code that follows simpler/invariant and preserves the packed location value */
	if(odd_offset)
	{
		*entry_index_ptr = buddy_index;
		SOL_SWAP(coalesce_entry, buddy_entry);
		SOL_SWAP(coalesce_index, buddy_index);
	}

	/** make sure buddy is in the expected location (y same, x offset by size) */
	assert(coalesce_entry->xy_offset.y == buddy_entry->xy_offset.y);
	assert(coalesce_entry->xy_offset.x + (1u << coalesce_entry->x_size_class) == buddy_entry->xy_offset.x);

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
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);
		if(adjacent_entry->adj_start_left != buddy_index)
		{
			assert(adjacent_entry->xy_offset.y < buddy_entry->xy_offset.y);
			break;
		}
		adjacent_entry->adj_start_left = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_up;
	}

	/** traverse bottom side */
	adjacent_index = coalesce_entry->adj_end_down;
	while(adjacent_index)
	{
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);
		if(adjacent_entry->adj_start_up != buddy_index)
		{
			assert(adjacent_entry->xy_offset.x < buddy_entry->xy_offset.x);
			break;
		}
		adjacent_entry->adj_start_up = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_left;
	}

	/** traverse top side, first half may already reference coalesced index and so muct be skipped */
	adjacent_index = coalesce_entry->adj_start_up;
	while(adjacent_index)
	{
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);
		/** when starting from top left may already be referencing correct entry (index), so should skip this if encountered */
		if(adjacent_entry->adj_end_down != coalesce_index)
		{
			if(adjacent_entry->adj_end_down != buddy_index)
			{
				/** adjacent entry that fails index check must meet the corner of the coalesced range or be larger than the coalesced allocation */
				assert(adjacent_entry->xy_offset.x == coalesce_entry->xy_offset.x + (1u << coalesce_entry->x_size_class)
					|| adjacent_entry->x_size_class > coalesce_entry->x_size_class);
				break;
			}
			adjacent_entry->adj_end_down = coalesce_index;
		}

		adjacent_index = adjacent_entry->adj_end_right;
	}

	/** remove the buddy tile entirely, after clearing the entries data (which should not strictly be necessary) */
	*buddy_entry = (struct sol_buddy_grid_entry){};
	sol_buddy_grid_entry_array_withdraw(&grid->entry_array, buddy_index);

	return true;
}

/** which entry takes precedence in coalescion may result in a change to the entry being removed */
static inline bool sol_buddy_grid_entry_try_coalesce_vertical(struct sol_buddy_grid* grid, uint32_t* entry_index_ptr)
{
	struct sol_buddy_grid_entry* buddy_entry;
	struct sol_buddy_grid_entry* coalesce_entry;
	struct sol_buddy_grid_entry* adjacent_entry;
	uint32_t buddy_index, coalesce_index, adjacent_index;
	bool odd_offset;

	coalesce_index = *entry_index_ptr;
	coalesce_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, *entry_index_ptr);

	/** check for coalescable buddy */
	odd_offset = coalesce_entry->xy_offset.y & (1u << coalesce_entry->y_size_class);
	buddy_index = odd_offset ? coalesce_entry->adj_start_up : coalesce_entry->adj_end_down;
	if(buddy_index == 0)
	{
		/** root entry index (zero) indicated this entry has no valid neighbours in this direction
		 * must fill entire image to not have a valid buddy though */
		assert(coalesce_entry->y_size_class == grid->description.image_y_dimension_exponent);
		return false;
	}
	buddy_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, buddy_index);

	if( ! buddy_entry->is_available || buddy_entry->x_size_class != coalesce_entry->x_size_class || buddy_entry->y_size_class != coalesce_entry->y_size_class)
	{
		return false;
	}

	assert(buddy_entry->is_available);

	/** remove buddy from availability heap */
	sol_buddy_grid_remove_available_entry(grid, buddy_entry);

	/** coalesce into the entry closer to zero, so swap entry and it's buddy if necessary to make this happen
	 * this makes the code that follows simpler/invariant and preserves the packed location value */
	if(odd_offset)
	{
		*entry_index_ptr = buddy_index;
		SOL_SWAP(coalesce_entry, buddy_entry);
		SOL_SWAP(coalesce_index, buddy_index);
	}

	/** make sure buddy is in the expected location (x same, y offset by size) */
	assert(coalesce_entry->xy_offset.x == buddy_entry->xy_offset.x);
	assert(coalesce_entry->xy_offset.y + (1u << coalesce_entry->y_size_class) == buddy_entry->xy_offset.y);

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
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);
		if(adjacent_entry->adj_start_up != buddy_index)
		{
			assert(adjacent_entry->xy_offset.x < buddy_entry->xy_offset.x);
			break;
		}
		adjacent_entry->adj_start_up = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_left;
	}

	/** traverse right side */
	adjacent_index = coalesce_entry->adj_end_right;
	while(adjacent_index)
	{
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);
		if(adjacent_entry->adj_start_left != buddy_index)
		{
			assert(adjacent_entry->xy_offset.y < buddy_entry->xy_offset.y);
			break;
		}
		adjacent_entry->adj_start_left = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_up;
	}

	/** traverse left side, first half may already reference coalesced index and so muct be skipped */
	adjacent_index = coalesce_entry->adj_start_left;
	while(adjacent_index)
	{
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);
		/** when starting from top left may already be referencing correct entry (index), so should skip this if encountered */
		if(adjacent_entry->adj_end_right != coalesce_index)
		{
			if(adjacent_entry->adj_end_right != buddy_index)
			{
				/** adjacent entry that fails index check must meet the corner of the coalesced range or be larger than the coalesced allocation */
				assert(adjacent_entry->xy_offset.y == coalesce_entry->xy_offset.y + (1u << coalesce_entry->y_size_class)
					|| adjacent_entry->y_size_class > coalesce_entry->y_size_class);
				break;
			}
			adjacent_entry->adj_end_right = coalesce_index;
		}

		adjacent_index = adjacent_entry->adj_end_down;
	}

	/** remove the buddy tile entirely, after clearing the entries data (which should not strictly be necessary) */
	*buddy_entry = (struct sol_buddy_grid_entry){};
	sol_buddy_grid_entry_array_withdraw(&grid->entry_array, buddy_index);

	return true;
}


/** NOTE: entry referenced by this index may be completely destroyed, should ONLY call after all other uses of this entry have been completed */
static inline void sol_buddy_grid_entry_make_available(struct sol_buddy_grid* grid, uint32_t entry_index)
{
	struct sol_buddy_grid_entry_availability_heap* availability_heap;
	struct sol_buddy_grid_entry* entry;
	bool coalesced;

	entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index);
	assert( ! entry->is_available);

	entry->is_available = true;

	/** check to see if this entry can be coalesced with its neighbours */
	do
	{
		/** note: more agressively combine vertically, because also more agressively split vertically */
		if(entry->x_size_class < entry->y_size_class)
		{
			coalesced = sol_buddy_grid_entry_try_coalesce_horizontal(grid, &entry_index);
			if(!coalesced)
			{
				coalesced = sol_buddy_grid_entry_try_coalesce_vertical(grid, &entry_index);
			}
		}
		else
		{
			coalesced = sol_buddy_grid_entry_try_coalesce_vertical(grid, &entry_index);
			if(!coalesced)
			{
				coalesced = sol_buddy_grid_entry_try_coalesce_horizontal(grid, &entry_index);
			}
		}

		entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index);
	}
	while(coalesced);

	/** put the newly available entry in the correct avilibility heap */
	availability_heap = &grid->availablity_heaps[entry->x_size_class][entry->y_size_class];
	sol_buddy_grid_entry_availability_heap_append(availability_heap, entry_index, grid);

	/** mark the bit in availability mask to indicate there is a tile/entry of the final coalesced size available */
	grid->availability_masks[entry->x_size_class] |= (1 << entry->y_size_class);
}

/** NOTE: will always maintain the index of the split location, creating a new entry (and index with it) for the adjacent tile created by splitting
 * this should also only be called on available entries */
static inline void sol_buddy_grid_entry_split_horizontally(struct sol_buddy_grid* grid, uint32_t split_index)
{
	struct sol_buddy_grid_entry_availability_heap* availability_heap;
	struct sol_buddy_grid_entry* split_entry;
	struct sol_buddy_grid_entry* buddy_entry;
	struct sol_buddy_grid_entry* adjacent_entry;
	uint32_t buddy_index, adjacent_index;
	uint16_t adjacent_end_x, buddy_end_x;

	/** split entry must be accessed after buddy as append alters the array (may relocate in memory) */
	buddy_entry = sol_buddy_grid_entry_array_append_ptr(&grid->entry_array, &buddy_index);
	split_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, split_index);

	buddy_end_x = split_entry->xy_offset.x + (1u << split_entry->x_size_class);

	/** validate entry to split really does look available */
	assert(split_entry == sol_buddy_grid_entry_array_access_entry(&grid->entry_array, split_index));
	assert(split_entry->is_available);

	/** in order to split entry must be larger than the min size */
	assert(split_entry->x_size_class > 0);
	split_entry->x_size_class--;

	/** check the split entries offset is valid for it's size class */
	assert((split_entry->xy_offset.x & (1u << split_entry->x_size_class)) == 0u);

	*buddy_entry = (struct sol_buddy_grid_entry)
	{
		.adj_start_left = split_index,
		.adj_start_up = 0,/** must be set */
		.adj_end_right = split_entry->adj_end_right,
		.adj_end_down  = split_entry->adj_end_down,
		.heap_index = 0xFFFFFFFFu,//??
		.packed_location = split_entry->packed_location | (SOL_BUDDY_GRID_PACKED_X_BASE << (split_entry->x_size_class * 2u)),
		.xy_offset.x = split_entry->xy_offset.x + (1u << split_entry->x_size_class),
		.xy_offset.y = split_entry->xy_offset.y,
		.array_layer = split_entry->array_layer,
		.x_size_class = split_entry->x_size_class,
		.y_size_class = split_entry->y_size_class,
		.is_available = true,
	};

	assert(sol_buddy_grid_packed_loc_get_x(buddy_entry->packed_location) == buddy_entry->xy_offset.x);
	assert(sol_buddy_grid_packed_loc_get_y(buddy_entry->packed_location) == buddy_entry->xy_offset.y);
	assert(sol_buddy_grid_packed_loc_get_layer(buddy_entry->packed_location) == buddy_entry->array_layer);
	assert(buddy_end_x == buddy_entry->xy_offset.x + (1u << buddy_entry->x_size_class));

	split_entry->adj_end_right = buddy_index;
	split_entry->adj_end_down = 0;/** must be set */

	/** traverse right side */
	adjacent_index = buddy_entry->adj_end_right;
	while(adjacent_index)
	{
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);

		if(adjacent_entry->adj_start_left != split_index)
		{
			assert(adjacent_entry->xy_offset.y < buddy_entry->xy_offset.y);
			break;
		}
		adjacent_entry->adj_start_left = buddy_index;

		adjacent_index = adjacent_entry->adj_start_up;
	}

	/** traverse bottom side */
	adjacent_index = buddy_entry->adj_end_down;
	while(adjacent_index)
	{
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);

		if(adjacent_entry->xy_offset.x < buddy_entry->xy_offset.x)
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
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);

		/** is this adjacent entry directly above the top left of the buddy allocation */
		if((buddy_entry->xy_offset.x >= adjacent_entry->xy_offset.x) && (buddy_entry->xy_offset.x < adjacent_entry->xy_offset.x + (1u << adjacent_entry->x_size_class)))
		{
			assert(buddy_entry->adj_start_up == 0);
			buddy_entry->adj_start_up = adjacent_index;
		}

		if(adjacent_entry->adj_end_down != split_index)
		{
			/** adjacent entry that fails index check must not have ended in the x range that would have referenced the entry before it was split */
			assert(adjacent_entry->xy_offset.x + (1u << adjacent_entry->x_size_class) > buddy_end_x);
			break;
		}

		/** does the adjacents end fall before or at the end of the buddy but after its start, if so the buddy should replace the adjacent "down of end" link */
		/** NOTE: if adjacent would exceed the x range where it should set end_right to the buddy index, it would have hit break above */
		adjacent_end_x = adjacent_entry->xy_offset.x + (1u << adjacent_entry->x_size_class);
		//assert(adjacent_end_x <= buddy_end_x);// i think this is always true...
		if(adjacent_end_x > buddy_entry->xy_offset.x && adjacent_end_x <= buddy_end_x)
		{
			adjacent_entry->adj_end_down = buddy_index;
		}

		adjacent_index = adjacent_entry->adj_end_right;
	}

	/** make sure that unknown/unset adjacents got set if they need to be */
	assert(split_entry->adj_end_down != 0 || buddy_entry->adj_end_down == 0);
	assert(buddy_entry->adj_start_up != 0 || split_entry->adj_start_up == 0);

	/** put buddy allocation on heap */
	availability_heap = &grid->availablity_heaps[buddy_entry->x_size_class][buddy_entry->y_size_class];
	sol_buddy_grid_entry_availability_heap_append(availability_heap, buddy_index, grid);

	/** mark the bit in availability mask to indicate there is a tile/entry of the split off buddies size available */
	assert((grid->availability_masks[buddy_entry->x_size_class] & (1u << buddy_entry->y_size_class)) == 0);
	grid->availability_masks[buddy_entry->x_size_class] |= (1u << buddy_entry->y_size_class);
}

/** NOTE: will always maintain the index of the split location, creating a new entry (and index with it) for the adjacent tile created by splitting
 * this should also only be called on available entries */
static inline void sol_buddy_grid_entry_split_vertically(struct sol_buddy_grid* grid, uint32_t split_index)
{
	struct sol_buddy_grid_entry_availability_heap* availability_heap;
	struct sol_buddy_grid_entry* split_entry;
	struct sol_buddy_grid_entry* buddy_entry;
	struct sol_buddy_grid_entry* adjacent_entry;
	uint32_t buddy_index, adjacent_index;
	uint16_t adjacent_end_y, buddy_end_y;

	/** split entry must be accessed after buddy as append alters the array (may relocate in memory) */
	buddy_entry = sol_buddy_grid_entry_array_append_ptr(&grid->entry_array, &buddy_index);
	split_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, split_index);

	buddy_end_y = split_entry->xy_offset.y + (1u << split_entry->y_size_class);

	/** validate entry to split really does look available */
	assert(split_entry->is_available);

	/** in order to split entry must be larger than the min size */
	assert(split_entry->y_size_class > 0);
	split_entry->y_size_class--;

	/** check the split entries offset is valid for it's size class */
	assert((split_entry->xy_offset.y & (1u << split_entry->y_size_class)) == 0u);

	*buddy_entry = (struct sol_buddy_grid_entry)
	{
		.adj_start_left = 0,/** must be set */
		.adj_start_up = split_index,
		.adj_end_right = split_entry->adj_end_right,
		.adj_end_down  = split_entry->adj_end_down,
		.heap_index = 0xFFFFFFFFu,//??
		.packed_location = split_entry->packed_location | (SOL_BUDDY_GRID_PACKED_Y_BASE << (split_entry->y_size_class * 2u)),
		.xy_offset.x = split_entry->xy_offset.x,
		.xy_offset.y = split_entry->xy_offset.y + (1u << split_entry->y_size_class),
		.array_layer = split_entry->array_layer,
		.x_size_class = split_entry->x_size_class,
		.y_size_class = split_entry->y_size_class,
		.is_available = true,
	};

	assert(sol_buddy_grid_packed_loc_get_x(buddy_entry->packed_location) == buddy_entry->xy_offset.x);
	assert(sol_buddy_grid_packed_loc_get_y(buddy_entry->packed_location) == buddy_entry->xy_offset.y);
	assert(sol_buddy_grid_packed_loc_get_layer(buddy_entry->packed_location) == buddy_entry->array_layer);
	assert(buddy_end_y == buddy_entry->xy_offset.y + (1u << buddy_entry->y_size_class));

	split_entry->adj_end_right = 0;/** must be set */
	split_entry->adj_end_down = buddy_index;

	/** traverse bottom side */
	adjacent_index = buddy_entry->adj_end_down;
	while(adjacent_index)
	{
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);

		if(adjacent_entry->adj_start_up != split_index)
		{
			assert(adjacent_entry->xy_offset.x < buddy_entry->xy_offset.x);
			break;
		}
		adjacent_entry->adj_start_up = buddy_index;

		adjacent_index = adjacent_entry->adj_start_left;
	}

	/** traverse right side */
	adjacent_index = buddy_entry->adj_end_right;
	while(adjacent_index)
	{
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);
		if(adjacent_entry->xy_offset.y < buddy_entry->xy_offset.y)
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
		adjacent_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, adjacent_index);

		/** is this adjacent entry directly left of the top left of the buddy allocation */
		if((buddy_entry->xy_offset.y >= adjacent_entry->xy_offset.y) && (buddy_entry->xy_offset.y < adjacent_entry->xy_offset.y + (1u << adjacent_entry->y_size_class)))
		{
			assert(buddy_entry->adj_start_left == 0);
			buddy_entry->adj_start_left = adjacent_index;
		}

		if(adjacent_entry->adj_end_right != split_index)
		{
			/** adjacent entry that fails index check must not have ended in the y range that would have referenced the entry before it was split */
			assert(adjacent_entry->xy_offset.y + (1u << adjacent_entry->y_size_class) >= buddy_end_y);
			break;
		}

		/** does the adjacents end fall before or at the end of the buddy but after its start, if so the buddy should replace the adjacent "right of end" link */
		/** NOTE: if adjacent would exceed the y range where it should set end_right to the buddy index, it would have hit break above */
		adjacent_end_y = adjacent_entry->xy_offset.y + (1u << adjacent_entry->y_size_class);
		//assert(adjacent_end_y <= buddy_end_y);// i think this is always true...
		if(adjacent_end_y > buddy_entry->xy_offset.y && adjacent_end_y <= buddy_end_y)
		{
			adjacent_entry->adj_end_right = buddy_index;
		}

		adjacent_index = adjacent_entry->adj_end_down;
	}

	/** make sure that unknown/unset adjacents got set if they need to be */
	assert(split_entry->adj_end_right != 0  || buddy_entry->adj_end_right == 0);
	assert(buddy_entry->adj_start_left != 0 || split_entry->adj_start_left == 0);

	/** put buddy allocation on heap */
	availability_heap = &grid->availablity_heaps[buddy_entry->x_size_class][buddy_entry->y_size_class];
	sol_buddy_grid_entry_availability_heap_append(availability_heap, buddy_index, grid);

	/** mark the bit in availability mask to indicate there is a tile/entry of the split off buddies size available */
	assert((grid->availability_masks[buddy_entry->x_size_class] & (1u << buddy_entry->y_size_class)) == 0);
	grid->availability_masks[buddy_entry->x_size_class] |= (1u << buddy_entry->y_size_class);
}

static inline bool sol_buddy_grid_acquire_available_entry_of_size(struct sol_buddy_grid* grid, uint32_t required_x_size_class, uint32_t required_y_size_class, uint32_t* entry_index_ptr)
{
	struct sol_buddy_grid_entry_availability_heap* availability_heap;
	uint32_t entry_index, x_min, y_min, min_split_count, x, y, split_count, y_mask;
	uint16_t availiability_mask;
	bool acquired_available;

	min_split_count = UINT32_MAX;
	y_mask = -(1u << required_y_size_class);

	for(x = required_x_size_class; x <= grid->description.image_x_dimension_exponent; x++)
	{
		availiability_mask = grid->availability_masks[x] & y_mask;
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

	availability_heap = &grid->availablity_heaps[x_min][y_min];
	acquired_available = sol_buddy_grid_entry_availability_heap_withdraw(availability_heap, entry_index_ptr, grid);
	entry_index = *entry_index_ptr;
	/** mask should properly track availability */
	assert(acquired_available);

	/** if this was the last entry of the specified size; unset the bit in the availability masks to indicate as such  */
	if(sol_buddy_grid_entry_availability_heap_count(availability_heap) == 0)
	{
		grid->availability_masks[x_min] &= ~(1 << y_min);
	}


	/** validate entry really does look available */
	assert(sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index)->is_available);

	while(x_min != required_x_size_class || y_min != required_y_size_class)
	{
		/** check entry is currently of expected size */
		assert(sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index)->x_size_class == x_min);
		assert(sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index)->y_size_class == y_min);

		/** preferentially split vertically if possible */
		if(x_min == required_x_size_class || (y_min >= x_min && y_min != required_y_size_class))
		{
			assert(sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index)->y_size_class > required_y_size_class);
			sol_buddy_grid_entry_split_vertically(grid, entry_index);
			y_min--;
		}
		else
		{
			assert(sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index)->x_size_class > required_x_size_class);
			sol_buddy_grid_entry_split_horizontally(grid, entry_index);
			x_min--;
		}
	}

	sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index)->is_available = false;

	return true;
}


struct sol_buddy_grid* sol_buddy_grid_create(struct sol_buddy_grid_description description)
{
	uint32_t x_size_class, y_size_class, array_layer, entry_index;

	struct sol_buddy_grid* grid = malloc(sizeof(struct sol_buddy_grid));

	assert(description.image_x_dimension_exponent < SOL_BUDDY_GRID_SIZE_CLASS_COUNT);
	assert(description.image_y_dimension_exponent < SOL_BUDDY_GRID_SIZE_CLASS_COUNT);

	grid->description = description;

	sol_buddy_grid_entry_array_initialise(&grid->entry_array, 1024);

	/** need to reserve index 0 */
	sol_buddy_grid_entry_array_append_ptr(&grid->entry_array, &entry_index);
	assert(entry_index == 0);

	for(x_size_class = 0; x_size_class < SOL_BUDDY_GRID_SIZE_CLASS_COUNT; x_size_class++)
	{
		grid->availability_masks[x_size_class] = 0;
		for(y_size_class = 0; y_size_class < SOL_BUDDY_GRID_SIZE_CLASS_COUNT; y_size_class++)
		{
			sol_buddy_grid_entry_availability_heap_initialise(&grid->availablity_heaps[x_size_class][y_size_class], 0);
		}
	}

	/** populate availability with all encompassing entries */
	for(array_layer = 0; array_layer < description.image_array_dimension; array_layer++)
	{
		/** for every array layer make an entry filling the slive available */
		*sol_buddy_grid_entry_array_append_ptr(&grid->entry_array, &entry_index) = (struct sol_buddy_grid_entry)
		{
			.adj_start_left = 0,
			.adj_start_up   = 0,
			.adj_end_right  = 0,
			.adj_end_down   = 0,
			.heap_index = 0xFFFFFFFFu,//??
			.packed_location = array_layer << 24,
			.x_size_class = description.image_x_dimension_exponent,
			.y_size_class = description.image_y_dimension_exponent,
			.xy_offset = u16_vec2_set(0, 0),
			.array_layer = array_layer,
			.is_available = true,
		};
		/** put the entry in the heap and set availability mask */
		sol_buddy_grid_entry_availability_heap_append(&grid->availablity_heaps[description.image_x_dimension_exponent][description.image_y_dimension_exponent], entry_index, grid);
		grid->availability_masks[description.image_x_dimension_exponent] |= 1 << description.image_y_dimension_exponent;
	}

	return grid;
}


void sol_buddy_grid_destroy(struct sol_buddy_grid* grid)
{
	uint32_t x_size_class, y_size_class;
	uint32_t entry_index;
	struct sol_buddy_grid_entry* entry;

	x_size_class = grid->description.image_x_dimension_exponent;
	y_size_class = grid->description.image_y_dimension_exponent;

	/** should have released all entries before destroying grid */
	assert(sol_buddy_grid_entry_availability_heap_count(&grid->availablity_heaps[x_size_class][y_size_class]) == grid->description.image_array_dimension);

	while(sol_buddy_grid_entry_availability_heap_withdraw(&grid->availablity_heaps[x_size_class][y_size_class], &entry_index, grid))
	{
		entry = sol_buddy_grid_entry_array_withdraw_ptr(&grid->entry_array, entry_index);
		/** assert contents are as expected */
		assert(entry->x_size_class == x_size_class);
		assert(entry->y_size_class == y_size_class);
		assert((entry->packed_location & 0x00FFFFFFu) == 0u);
		assert(entry->adj_start_left == 0);
		assert(entry->adj_start_up   == 0);
		assert(entry->adj_end_right  == 0);
		assert(entry->adj_end_down   == 0);
		assert(entry->is_available);
	}

	/** needed to reserve index 0 */
	sol_buddy_grid_entry_array_withdraw_ptr(&grid->entry_array, 0);

	assert(sol_buddy_grid_entry_array_is_empty(&grid->entry_array));


	for(x_size_class = 0; x_size_class < SOL_BUDDY_GRID_SIZE_CLASS_COUNT; x_size_class++)
	{
		for(y_size_class = 0; y_size_class < SOL_BUDDY_GRID_SIZE_CLASS_COUNT; y_size_class++)
		{
			assert(sol_buddy_grid_entry_availability_heap_count(&grid->availablity_heaps[x_size_class][y_size_class]) == 0);
			sol_buddy_grid_entry_availability_heap_terminate(&grid->availablity_heaps[x_size_class][y_size_class]);
		}
	}

	sol_buddy_grid_entry_array_terminate(&grid->entry_array);

	free(grid);
}

bool sol_buddy_grid_acquire(struct sol_buddy_grid* grid, u16_vec2 size, uint32_t* index)
{
	uint32_t x_size_class = sol_u32_exp_ge(size.x);
	uint32_t y_size_class = sol_u32_exp_ge(size.y);

	return sol_buddy_grid_acquire_available_entry_of_size(grid, x_size_class, y_size_class, index);
}

void sol_buddy_grid_release(struct sol_buddy_grid* grid, uint32_t index)
{
	sol_buddy_grid_entry_make_available(grid, index);
}


struct sol_buddy_grid_location sol_buddy_grid_get_location(struct sol_buddy_grid* grid, uint32_t index)
{
	struct sol_buddy_grid_entry* entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, index);

	assert(sol_buddy_grid_packed_loc_get_layer(entry->packed_location) == entry->array_layer);
	assert(sol_buddy_grid_packed_loc_get_x(entry->packed_location) == entry->xy_offset.x);
	assert(sol_buddy_grid_packed_loc_get_y(entry->packed_location) == entry->xy_offset.y);

	return (struct sol_buddy_grid_location)
	{
		.array_layer = entry->array_layer,
		.xy_offset = entry->xy_offset,
	};
}

bool sol_buddy_grid_has_space(struct sol_buddy_grid* grid, u16_vec2 size)
{
	uint32_t x;

	const uint32_t x_size_class = sol_u32_exp_ge(size.x);
	const uint32_t y_size_class = sol_u32_exp_ge(size.y);
	const uint32_t y_mask = -(1u << y_size_class);

	for(x = x_size_class; x <= grid->description.image_x_dimension_exponent; x++)
	{
		if(grid->availability_masks[x] & y_mask)
		{
			return true;
		}
	}

	return false;
}