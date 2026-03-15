/**
Copyright 2025,2026 Carl van Mastrigt

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


#include "stdio.h"
#warning remove above

#include <assert.h>

#include "data_structures/buddy_grid.h"
#include "stdlib.h"

/** maximum xy dimension classes; the maximum number of sizes a tile can be (0-12 inclusive) */
#define SOL_BUDDY_GRID_SIZE_CLASS_COUNT 13

// #define USE_LINKED_LIST
#define USE_RB_TREE

struct sol_buddy_grid_entry
{
	#ifdef USE_RB_TREE
	uint32_t left_child;
	uint32_t right_child;
	bool is_marked;
	#endif

	#ifdef USE_LINKED_LIST
	uint32_t next;
	uint32_t dummy_a;
	uint8_t dummy_b;
	#endif
	


	uint8_t x_size_class;// : 4;
	uint8_t y_size_class;// : 4;

	uint8_t array_layer;
	u16_vec2 xy_offset;

	/** z-tile location of the tile within its size class, with array layer in top 8 bits */
	uint32_t packed_location;
};

#define SOL_ARRAY_ENTRY_TYPE struct sol_buddy_grid_entry
#define SOL_ARRAY_STRUCT_NAME sol_buddy_grid_entry_array
#include "data_structures/array.h"


/** underlying type sufficiently complex as to not expose its internals here (so reqire allocation of struct) */
struct sol_buddy_grid_2
{
	/** provided description */
	struct sol_buddy_grid_description description;

	/** array of entries, both available and in use */
	struct sol_buddy_grid_entry_array entry_array;

	/** [W][H] size class 2d array of binary tree heads **/
	uint32_t available_tree_heads[SOL_BUDDY_GRID_SIZE_CLASS_COUNT][SOL_BUDDY_GRID_SIZE_CLASS_COUNT];

	/** array index is x, bit index is y */
	uint16_t availability_masks[SOL_BUDDY_GRID_SIZE_CLASS_COUNT];
};


#define SOL_BUDDY_GRID_PACKED_X_MASK     0x00555555u
#define SOL_BUDDY_GRID_PACKED_Y_MASK     0x00AAAAAAu
#define SOL_BUDDY_GRID_PACKED_NOT_X_MASK 0xFFAAAAAAu
#define SOL_BUDDY_GRID_PACKED_NOT_Y_MASK 0xFF555555u
#define SOL_BUDDY_GRID_PACKED_LAYER_MASK 0xFF000000u

/** x on even bits, y on odd bits */
#define SOL_BUDDY_GRID_PACKED_X_BASE     0x00000001u
#define SOL_BUDDY_GRID_PACKED_Y_BASE     0x00000002u





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

static inline uint32_t sol_buddy_grid_packed_loc_right_shift_x(uint32_t packed_location)
{
	return (packed_location & SOL_BUDDY_GRID_PACKED_NOT_X_MASK) | (packed_location & SOL_BUDDY_GRID_PACKED_X_MASK) >> 2;
}

static inline uint32_t sol_buddy_grid_packed_loc_right_shift_y(uint32_t packed_location)
{
	return (packed_location & SOL_BUDDY_GRID_PACKED_NOT_Y_MASK) | (packed_location & SOL_BUDDY_GRID_PACKED_Y_MASK) >> 2;
}

static inline uint32_t sol_buddy_grid_packed_loc_left_shift_x(uint32_t packed_location)
{
	return (packed_location & SOL_BUDDY_GRID_PACKED_NOT_X_MASK) | (packed_location & SOL_BUDDY_GRID_PACKED_X_MASK) << 2;
}

static inline uint32_t sol_buddy_grid_packed_loc_left_shift_y(uint32_t packed_location)
{
	return (packed_location & SOL_BUDDY_GRID_PACKED_NOT_Y_MASK) | (packed_location & SOL_BUDDY_GRID_PACKED_Y_MASK) << 2;
}



#warning better to recycle the extracted withdraw code, even when guarantees can be made for one branch?


#ifdef USE_RB_TREE
/** caller must decide what to do with the index 
 * note: this function should ONLY affect tree structure */
static inline void sol_buddy_grid_withdraw(struct sol_buddy_grid_2* grid, uint32_t** traversal_references, uint64_t traversal_mask, uint32_t withdrawn_depth)
{
	struct sol_buddy_grid_entry* withdrawn_entry;
	struct sol_buddy_grid_entry* d0;
	struct sol_buddy_grid_entry* d1;
	struct sol_buddy_grid_entry* d2;
	uint32_t d0_index, d1_index, d2_index, subtree_head_index, withdrawn_index, depth;
	bool removed_was_marked;

	withdrawn_index = *traversal_references[withdrawn_depth];
	withdrawn_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, withdrawn_index);

	depth = withdrawn_depth;

	if(withdrawn_entry->left_child)
	{
		/** should fill this slot with the replacement entry */
		depth++;
		traversal_references[depth] = &withdrawn_entry->left_child;
		d0_index = withdrawn_entry->left_child;
		d0 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d0_index);

		while(d0->right_child)
		{
			/** note: is always going right here, so `traversal_mask` bit is zero */
			depth++;
			traversal_references[depth] = &d0->right_child;
			d0_index = d0->right_child;
			d0 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d0_index);
		}
			
		if(d0->left_child)
		{
			*traversal_references[depth] = d0->left_child;
			d1 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d0->left_child);
			assert(!d1->is_marked && d1->left_child == 0 && d1->right_child == 0);
			d1->is_marked = true;
			removed_was_marked = false;
		}
		else
		{
			removed_was_marked = d0->is_marked;
			*traversal_references[depth] = 0;

			/** reference has changed along with replacement being moved */
			traversal_references[withdrawn_depth + 1] = &d0->left_child;
			traversal_mask |= (uint64_t)1 << withdrawn_depth;
		}

		*traversal_references[withdrawn_depth] = d0_index;

		d0->left_child = withdrawn_entry->left_child;
		d0->right_child = withdrawn_entry->right_child;
		d0->is_marked = withdrawn_entry->is_marked;
	}
	else if(withdrawn_entry->right_child)
	{
		*traversal_references[depth] = withdrawn_entry->right_child;
		d0 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, withdrawn_entry->right_child);
		assert(!d0->is_marked && d0->left_child == 0 && d0->right_child == 0);
		d0->is_marked = true;
		removed_was_marked = false;
	}
	else
	{
		*traversal_references[depth] = 0;
		removed_was_marked = withdrawn_entry->is_marked;
	}

	if(!removed_was_marked)
	{
		 return;
	}

	while(depth)
	{
		depth--;
		d0_index = *traversal_references[depth];
		d0 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d0_index);

		if((traversal_mask >> depth) & (uint64_t)1)/** LHS shortened */
		{
			/** removed one marked from left, ergo right must exist */
			d1_index = d0->right_child;
			assert(d1_index);
			d1 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d1_index);
			if(d1->is_marked)
			{
				d2_index = d1->left_child;
				d2 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d2_index);
				if(d2_index && !d2->is_marked)
				{
					d0->right_child = d2->left_child;
					d1->left_child = d2->right_child;

					d2->is_marked = d0->is_marked;
					d0->is_marked = true;

					d2->left_child = d0_index;
					d2->right_child = d1_index;

					*traversal_references[depth] = d2_index;
					return;
				}
				else if(!d0->is_marked)
				{
					/** following would (otherwise) introduce an unmarked chain */
					assert(d0->left_child == 0 || sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d0->left_child)->is_marked);

					d0->right_child = d2_index;/** d2_index == d1->left_child */
					d1->left_child = d0_index;

					*traversal_references[depth] = d1_index;
					return;
				}
				else
				{
					d2_index = d1->right_child;
					d2 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d2_index);
					if(d2_index && !d2->is_marked)
					{
						/** following would (otherwise) introduce an unmarked chain */
						assert(d0->left_child == 0 || sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d0->left_child)->is_marked);

						/** d2->is_marked = d0->is_marked; // this is required in the general case of this rotation, but in this scenario we know d0 is marked (higher priority handling) */
						assert(d0->is_marked);
						d0->right_child = d1->left_child;
						d1->left_child = d0_index;
						d2->is_marked = true;

						*traversal_references[depth] = d1_index;
						return;
					}
					else
					{
						d1->is_marked = false;
						/** continue */
					}
				}
			}
			else
			{
				/** following would (otherwise) introduce an unmarked chain */
				assert(d0->left_child == 0 || sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d0->left_child)->is_marked);

				d0->right_child = d1->left_child;
				d0->is_marked = false;

				d1->left_child = d0_index;
				d1->is_marked = true;

				*traversal_references[depth] = d1_index;

				traversal_references[depth + 1] = &d1->left_child;
				/** traversal_mask |= (uint64_t)1 << depth; // this has already been set (this is the side we are on!) */
				traversal_mask |= (uint64_t)2 << depth;
				depth += 2;
			}
		}
		else /** RHS shortened */
		{
			/** removed one marked from right, ergo left must exist */
			d1_index = d0->left_child;
			assert(d1_index);
			d1 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d1_index);
			if(d1->is_marked)
			{
				d2_index = d1->right_child;
				d2 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d2_index);
				if(d2_index && !d2->is_marked)
				{
					d0->left_child = d2->right_child;
					d1->right_child = d2->left_child;

					d2->is_marked = d0->is_marked;
					d0->is_marked = true;

					d2->right_child = d0_index;
					d2->left_child = d1_index;

					*traversal_references[depth] = d2_index;
					return;
				}
				else if(!d0->is_marked)
				{
					/** following would (otherwise) introduce an unmarked chain */
					assert(d0->left_child == 0 || sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d0->right_child)->is_marked);

					d0->left_child = d2_index;/** d2_index == d1->left_child */
					d1->right_child = d0_index;

					*traversal_references[depth] = d1_index;
					return;
				}
				else
				{
					d2_index = d1->left_child;
					d2 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d2_index);
					if(d2_index && !d2->is_marked)
					{
						/** following would (otherwise) introduce an unmarked chain */
						assert(d0->right_child == 0 || sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d0->right_child)->is_marked);

						/** d2->is_marked = d0->is_marked; // this is required in the general case of this rotation, but in this scenario we know d0 is marked */
						assert(d0->is_marked);
						d0->left_child = d1->right_child;
						d1->right_child = d0_index;
						d2->is_marked = true;

						*traversal_references[depth] = d1_index;
						return;
					}
					else
					{
						d1->is_marked = false;
						/** continue */
					}
				}
			}
			else
			{
				/** following would (otherwise) introduce an unmarked chain */
				assert(d0->right_child == 0 || sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d0->right_child)->is_marked);

				d0->left_child = d1->right_child;
				d0->is_marked = false;

				d1->right_child = d0_index;
				d1->is_marked = true;

				*traversal_references[depth] = d1_index;

				traversal_references[depth + 1] = &d1->right_child; 
				/** traversal_mask &= ~((uint64_t)1 << depth); // this has already been set (this is the side we are on!) */
				traversal_mask &= ~((uint64_t)2 << depth);
				depth += 2;
			}
		}
	}
}

static inline void sol_buddy_grid_append(struct sol_buddy_grid_2* grid, uint32_t** traversal_references, uint64_t traversal_mask, uint32_t appended_depth)
{
	struct sol_buddy_grid_entry* d2;
	struct sol_buddy_grid_entry* d1;
	struct sol_buddy_grid_entry* d1_adj;
	struct sol_buddy_grid_entry* d0;
	uint32_t d0_index, d1_index, d1_adj_index, d2_index, subtree_head_index;
	uint32_t depth;

	depth = appended_depth;
	d2_index = *traversal_references[depth];
	d2 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d2_index);

	while(depth)
	{
		depth--;
		d1_index = *traversal_references[depth];
		d1 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d1_index);
		if(d1->is_marked)
		{
			return;
		}
		if(depth == 0)
		{
			/** set top to tree to marked */
			d1->is_marked = true;
			return;
		}

		depth--;
		d0_index = *traversal_references[depth];

		d0 = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d0_index);
		assert(d0->is_marked);/** d0 must be marked if d1 is not */

		if((traversal_mask >> depth) & (uint64_t)1)
		{
			d1_adj_index = d0->right_child;
			d1_adj = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d1_adj_index);
			if(d1_adj->is_marked) /** note: dummy entry occupying index zero MUST be marked */
			{
				if((traversal_mask >> depth) & (uint64_t)2)
				{
					/** d1 now top of subtree */
					d0->left_child = d1->right_child;
					d0->is_marked = false;

					d1->right_child = d0_index;
					d1->is_marked = true;

					*traversal_references[depth] = d1_index;
				}
				else
				{
					/** d2 now top of subtree */
					d1->right_child = d2->left_child;

					d0->left_child = d2->right_child;
					d0->is_marked = false;

					d2->left_child  = d1_index;
					d2->right_child = d0_index;
					d2->is_marked = true;

					*traversal_references[depth] = d2_index;
				}
				return;
			}
		}
		else
		{
			d1_adj_index = d0->left_child;
			d1_adj = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, d1_adj_index);
			if(d1_adj->is_marked) /** note: dummy entry occupying index zero MUST be marked */ 
			{
				if((traversal_mask >> depth) & (uint64_t)2)
				{
					/** d2 now top of subtree */
					d1->left_child = d2->right_child;

					d0->right_child = d2->left_child;
					d0->is_marked = false;

					d2->left_child  = d0_index;
					d2->right_child = d1_index;
					d2->is_marked = true;

					*traversal_references[depth] = d2_index;
				}
				else
				{
					/** d1 now top of subtree */
					d0->right_child = d1->left_child;
					d0->is_marked = false;

					d1->left_child = d0_index;
					d1->is_marked = true;

					*traversal_references[depth] = d1_index;
				}
				return;
			}
		}

		d0->is_marked = false;
		d1->is_marked = true;
		d1_adj->is_marked = true;

		d2_index = d0_index;
		d2 = d0;
	}

	/** set the top of the tree to marked */
	d2->is_marked = true;
}



/** will automatically coalesce entries 
 * returns true in append, false on coalesce (failed to insert for this size class because it had to coalesce) */
static inline bool sol_buddy_grid_append_available(struct sol_buddy_grid_2* grid, uint32_t appended_index)
{
	struct sol_buddy_grid_entry* append_entry;
	struct sol_buddy_grid_entry* entry;
	uint32_t entry_index, depth, h_coalescion_packed_location, v_coalescion_packed_location;
	uint64_t traversal_mask;
	uint8_t x_size_class, y_size_class;
	uint32_t* traversal_references[64];

	append_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, appended_index);
	h_coalescion_packed_location = append_entry->packed_location ^ SOL_BUDDY_GRID_PACKED_X_BASE;
	v_coalescion_packed_location = append_entry->packed_location ^ SOL_BUDDY_GRID_PACKED_Y_BASE;
	x_size_class = append_entry->x_size_class;
	y_size_class = append_entry->y_size_class;

	assert(append_entry->array_layer == sol_buddy_grid_packed_loc_get_layer(append_entry->packed_location));
	assert(append_entry->xy_offset.x == sol_buddy_grid_packed_loc_get_x(append_entry->packed_location) << append_entry->x_size_class);
	assert(append_entry->xy_offset.y == sol_buddy_grid_packed_loc_get_y(append_entry->packed_location) << append_entry->y_size_class);

	depth = 0;
	traversal_mask = 0;

	traversal_references[0] = &grid->available_tree_heads[x_size_class][y_size_class];
	entry_index = *traversal_references[0];


	while(entry_index)
	{
		entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index);
		assert(append_entry->packed_location != entry->packed_location);
		assert(entry->x_size_class == x_size_class && entry->y_size_class == y_size_class);

		/** note: while traversing the tree, it is guaranteed that the "read head" will pass through adjacent ordered numbers
		 * this is where potential coalescion candidates exist (`packed_location ^ 1` && `packed location ^ 2`, either of which will suffice) 
		 * unfortunately there is no simple way to be guaranteed to find BOTH the candidate coalescion entries here... */
		if(entry->packed_location == h_coalescion_packed_location)
		{
			append_entry->x_size_class++;
			append_entry->xy_offset.x &= -((uint16_t)2 << x_size_class);
			append_entry->packed_location = sol_buddy_grid_packed_loc_right_shift_x(append_entry->packed_location);
			goto WITHDRAW_COALESCABLE;
		}
		else if(entry->packed_location == v_coalescion_packed_location)
		{
			append_entry->y_size_class++;
			append_entry->xy_offset.y &= -((uint16_t)2 << y_size_class);
			append_entry->packed_location = sol_buddy_grid_packed_loc_right_shift_y(append_entry->packed_location);
			goto WITHDRAW_COALESCABLE;
		}

		if(append_entry->packed_location < entry->packed_location)
		{
			traversal_mask |= (uint64_t)1 << depth;
			depth++;
			traversal_references[depth] = &entry->left_child;
			entry_index = entry->left_child;
		}
		else
		{
			depth++;
			traversal_references[depth] = &entry->right_child;
			entry_index = entry->right_child;
		}
	}

	APPEND:
	{
		/** might be worth supporting this in a call like withdraw is to better allow an "active" tree for defragmentation purposes */ 
		grid->availability_masks[append_entry->x_size_class] |= (uint16_t)1 << append_entry->y_size_class;

		*traversal_references[depth] = appended_index;
		append_entry->left_child = 0;
		append_entry->right_child = 0;
		append_entry->is_marked = false;

		sol_buddy_grid_append(grid, traversal_references, traversal_mask, depth);
		return true;
	}

	WITHDRAW_COALESCABLE:
	{
		assert(*traversal_references[depth] == entry_index);
		sol_buddy_grid_withdraw(grid, traversal_references, traversal_mask, depth);
		if(*traversal_references[0] == 0)
		{
			grid->availability_masks[x_size_class] &= ~((uint16_t)1 << y_size_class);
		}

		sol_buddy_grid_entry_array_withdraw_ptr(&grid->entry_array, entry_index);

		return false;
	}
}

static inline void sol_buddy_grid_withdraw_available(struct sol_buddy_grid_2* grid, uint32_t* withdrawn_index, uint8_t x_size_class, uint8_t y_size_class)
{
	struct sol_buddy_grid_entry* entry;
	uint32_t entry_index;
	uint32_t depth;
	uint32_t* traversal_references[64];

	assert(grid->availability_masks[x_size_class] & (uint16_t)1 << y_size_class);

	depth = 0;
	traversal_references[0] = &grid->available_tree_heads[x_size_class][y_size_class];
	entry_index = *traversal_references[0];
	assert(entry_index);

	while(true)
	{
		entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index);
		if(entry->left_child)
		{
			depth++;
			traversal_references[depth] = &entry->left_child;
			entry_index = entry->left_child;
		}
		else
		{
			*withdrawn_index = entry_index;
			break;
		}
	}

	assert(*traversal_references[depth] == entry_index);
	sol_buddy_grid_withdraw(grid, traversal_references, ~(uint64_t)0, depth);
	assert(*traversal_references[depth] == entry->right_child);
	if(*traversal_references[0] == 0)
	{
		grid->availability_masks[x_size_class] &= ~((uint16_t)1 << y_size_class);
	}

	assert(entry->array_layer == sol_buddy_grid_packed_loc_get_layer(entry->packed_location));
	assert(entry->xy_offset.x == sol_buddy_grid_packed_loc_get_x(entry->packed_location) << entry->x_size_class);
	assert(entry->xy_offset.y == sol_buddy_grid_packed_loc_get_y(entry->packed_location) << entry->y_size_class);
}

#endif

#ifdef USE_LINKED_LIST
/** will automatically coalesce entries 
 * returns true in append, false on coalesce (failed to insert for this size class because it had to coalesce) */
static inline bool sol_buddy_grid_append_available(struct sol_buddy_grid_2* grid, uint32_t appended_index)
{
	struct sol_buddy_grid_entry* append_entry;
	struct sol_buddy_grid_entry* entry;
	uint32_t entry_index, h_coalescion_packed_location, v_coalescion_packed_location, scan_packed_location_limit;
	uint8_t x_size_class, y_size_class;
	uint32_t* append_reference;
	uint32_t* h_coalesce_reference;
	uint32_t* v_coalesce_reference;
	uint32_t* current_reference;
	uint32_t* base_reference;

	append_entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, appended_index);
	h_coalescion_packed_location = append_entry->packed_location ^ SOL_BUDDY_GRID_PACKED_X_BASE;
	v_coalescion_packed_location = append_entry->packed_location ^ SOL_BUDDY_GRID_PACKED_Y_BASE;
	x_size_class = append_entry->x_size_class;
	y_size_class = append_entry->y_size_class;

	scan_packed_location_limit = append_entry->packed_location | 3;

	assert(append_entry->array_layer == sol_buddy_grid_packed_loc_get_layer(append_entry->packed_location));
	assert(append_entry->xy_offset.x == sol_buddy_grid_packed_loc_get_x(append_entry->packed_location) << append_entry->x_size_class);
	assert(append_entry->xy_offset.y == sol_buddy_grid_packed_loc_get_y(append_entry->packed_location) << append_entry->y_size_class);

	base_reference = &grid->available_tree_heads[x_size_class][y_size_class];
	current_reference = base_reference;
	entry_index = *current_reference;

	// append_reference = current_reference;
	append_reference = NULL;
	h_coalesce_reference = NULL;
	v_coalesce_reference = NULL;

	while(entry_index)
	{
		entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index);
		assert(append_entry->packed_location != entry->packed_location);
		assert(entry->x_size_class == x_size_class && entry->y_size_class == y_size_class);

		if(entry->packed_location > scan_packed_location_limit)
		{
			break;
		}
		else if(entry->packed_location == h_coalescion_packed_location)
		{
			h_coalesce_reference = current_reference;
		}
		else if(entry->packed_location == v_coalescion_packed_location)
		{
			v_coalesce_reference = current_reference;
		}

		if(append_entry->packed_location < entry->packed_location)
		{
			append_reference = current_reference;
		}

		current_reference = &entry->next;
		entry_index = *current_reference;
	}

	if(h_coalesce_reference && (x_size_class > y_size_class || v_coalesce_reference == NULL))
	{
		entry_index = *h_coalesce_reference;
		entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index);
		*h_coalesce_reference = entry->next;

		sol_buddy_grid_entry_array_withdraw_ptr(&grid->entry_array, entry_index);

		append_entry->x_size_class++;
		append_entry->xy_offset.x &= -((uint16_t)2 << x_size_class);
		append_entry->packed_location = sol_buddy_grid_packed_loc_right_shift_x(append_entry->packed_location);

		if(*base_reference == 0)
		{
			grid->availability_masks[x_size_class] &= ~((uint16_t)1 << y_size_class);
		}

		return false;
	}
	else if(v_coalesce_reference)
	{
		entry_index = *v_coalesce_reference;
		entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index);
		*v_coalesce_reference = entry->next;

		sol_buddy_grid_entry_array_withdraw_ptr(&grid->entry_array, entry_index);

		append_entry->y_size_class++;
		append_entry->xy_offset.y &= -((uint16_t)2 << y_size_class);
		append_entry->packed_location = sol_buddy_grid_packed_loc_right_shift_y(append_entry->packed_location);

		if(*base_reference == 0)
		{
			grid->availability_masks[x_size_class] &= ~((uint16_t)1 << y_size_class);
		}

		return false;
	}
	

	if(append_reference == NULL)
	{
		append_reference = current_reference;
	}

	append_entry->next = *append_reference;
	*append_reference = appended_index;

	grid->availability_masks[x_size_class] |= (uint16_t)1 << y_size_class;

	return true;
}

static inline void sol_buddy_grid_withdraw_available(struct sol_buddy_grid_2* grid, uint32_t* withdrawn_index, uint8_t x_size_class, uint8_t y_size_class)
{
	struct sol_buddy_grid_entry* entry;
	uint32_t entry_index;
	uint32_t* base_reference;

	base_reference = &grid->available_tree_heads[x_size_class][y_size_class];
	entry_index = *base_reference;
	assert(entry_index);

	*withdrawn_index = entry_index;

	entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, entry_index);
	*base_reference = entry->next;

	assert(entry->array_layer == sol_buddy_grid_packed_loc_get_layer(entry->packed_location));
	assert(entry->xy_offset.x == sol_buddy_grid_packed_loc_get_x(entry->packed_location) << entry->x_size_class);
	assert(entry->xy_offset.y == sol_buddy_grid_packed_loc_get_y(entry->packed_location) << entry->y_size_class);

	if(entry->next == 0)
	{
		grid->availability_masks[x_size_class] &= ~((uint16_t)1 << y_size_class);
	}
}

#endif

struct sol_buddy_grid_2* sol_buddy_grid_2_create(struct sol_buddy_grid_description description)
{
	struct sol_buddy_grid_2* grid;
	uint32_t x_size_class, y_size_class, array_layer, entry_index;
	struct sol_buddy_grid_entry* zero_entry;
	bool inserted;

	grid = malloc(sizeof(struct sol_buddy_grid_2));

	assert(description.image_x_dimension_exponent < SOL_BUDDY_GRID_SIZE_CLASS_COUNT);
	assert(description.image_y_dimension_exponent < SOL_BUDDY_GRID_SIZE_CLASS_COUNT);
	assert(description.image_array_dimension > 0);

	grid->description = description;

	sol_buddy_grid_entry_array_initialise(&grid->entry_array, 1024);

	zero_entry = sol_buddy_grid_entry_array_append_ptr(&grid->entry_array, &entry_index);
	assert(entry_index == 0);
	*zero_entry = (struct sol_buddy_grid_entry)
	{
		#ifdef USE_RB_TREE
		/** only variable of the zero entry that should ever be read, red-black tree algorithm treats null/leaf nodes as marked (black) */
		.is_marked = true,
		#endif
	};

	for(x_size_class = 0; x_size_class < SOL_BUDDY_GRID_SIZE_CLASS_COUNT; x_size_class++)
	{
		for(y_size_class = 0; y_size_class < SOL_BUDDY_GRID_SIZE_CLASS_COUNT; y_size_class++)
		{
			grid->available_tree_heads[x_size_class][y_size_class] = 0;
		}
	}

	for(y_size_class = 0; y_size_class < SOL_BUDDY_GRID_SIZE_CLASS_COUNT; y_size_class++)
	{
		grid->availability_masks[y_size_class] = 0;
	}

	for(array_layer = 0; array_layer < description.image_array_dimension; array_layer++)
	{
		/** for every array layer make an entry filling the slive available */
		*sol_buddy_grid_entry_array_append_ptr(&grid->entry_array, &entry_index) = (struct sol_buddy_grid_entry)
		{
			.xy_offset = u16_vec2_set(0, 0),
			.array_layer = array_layer,
			.packed_location = array_layer << 24,
			.x_size_class = description.image_x_dimension_exponent,
			.y_size_class = description.image_y_dimension_exponent,
		};

		/** add the entry to the tree as available */
		inserted = sol_buddy_grid_append_available(grid, entry_index);

		/** initial entries for eash layer should not coalesce */
		assert(inserted);
	}

	return grid;
}


void sol_buddy_grid_2_destroy(struct sol_buddy_grid_2* grid)
{
	uint8_t x_size_class, y_size_class;
	uint32_t entry_index, array_layer_count;
	struct sol_buddy_grid_entry* entry;

	x_size_class = grid->description.image_x_dimension_exponent;
	y_size_class = grid->description.image_y_dimension_exponent;

	array_layer_count = 0;
	while(grid->available_tree_heads[x_size_class][y_size_class])
	{
		array_layer_count++;
		sol_buddy_grid_withdraw_available(grid, &entry_index, x_size_class, y_size_class);
		sol_buddy_grid_entry_array_withdraw_ptr(&grid->entry_array, entry_index);
	}

	assert(array_layer_count == grid->description.image_array_dimension);

	for(x_size_class = 0; x_size_class < SOL_BUDDY_GRID_SIZE_CLASS_COUNT; x_size_class++)
	{
		assert(grid->availability_masks[x_size_class] == 0);

		for(y_size_class = 0; y_size_class < SOL_BUDDY_GRID_SIZE_CLASS_COUNT; y_size_class++)
		{
			assert(grid->available_tree_heads[x_size_class][y_size_class] == 0);
		}
	}

	/** needed to reserve index 0 */
	sol_buddy_grid_entry_array_withdraw_ptr(&grid->entry_array, 0);

	assert(sol_buddy_grid_entry_array_is_empty(&grid->entry_array));

	sol_buddy_grid_entry_array_terminate(&grid->entry_array);

	free(grid);
}

/** the acquired index must be released before destroying the buddy grid 
 * the index can be used as an index into an externally managed array 
 * (indices returned from an acquire call are guaranteed to not exceed the number of allocations at that time) */
bool sol_buddy_grid_2_acquire(struct sol_buddy_grid_2* grid, u16_vec2 size, uint32_t* entry_index)
{
	uint32_t x_min, y_min, min_split_count, x, y, split_count, buddy_entry_index;
	uint8_t x_size_class, y_size_class;
	uint16_t y_mask, availiability_mask;
	bool appended;
	struct sol_buddy_grid_entry* entry;
	struct sol_buddy_grid_entry* buddy_entry;

	x_size_class = sol_u32_exp_ge(size.x);
	y_size_class = sol_u32_exp_ge(size.y);

	min_split_count = UINT32_MAX;
	y_mask = -((uint16_t)1 << y_size_class);

	for(x = x_size_class; x <= grid->description.image_x_dimension_exponent; x++)
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

	sol_buddy_grid_withdraw_available(grid, entry_index, x_min, y_min);

	assert(x_min >= x_size_class && y_min >= y_size_class);

	while(x_min != x_size_class || y_min != y_size_class)
	{
		/** get new entry */
		buddy_entry = sol_buddy_grid_entry_array_append_ptr(&grid->entry_array, &buddy_entry_index);
		/** note: must reacquire pointer in case array was resized */
		entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, *entry_index);

		assert(entry->x_size_class == x_min && entry->y_size_class == y_min);

		if(x_min == x_size_class || (y_min >= x_min && y_min != y_size_class))
		{
			/** split vertically */
			y_min--;

			entry->packed_location = sol_buddy_grid_packed_loc_left_shift_y(entry->packed_location);
			entry->y_size_class = y_min;

			buddy_entry->array_layer = entry->array_layer;
			buddy_entry->xy_offset = entry->xy_offset;
			buddy_entry->xy_offset.y += (uint16_t)1 << y_min;
			buddy_entry->packed_location = entry->packed_location | SOL_BUDDY_GRID_PACKED_Y_BASE;
			buddy_entry->x_size_class = x_min; 
			buddy_entry->y_size_class = y_min; 
		}
		else
		{
			/** split hrizontally */
			x_min--;

			entry->packed_location = sol_buddy_grid_packed_loc_left_shift_x(entry->packed_location);
			entry->x_size_class = x_min;

			buddy_entry->array_layer = entry->array_layer;
			buddy_entry->xy_offset = entry->xy_offset;
			buddy_entry->xy_offset.x += (uint16_t)1 << x_min;
			buddy_entry->packed_location = entry->packed_location | SOL_BUDDY_GRID_PACKED_X_BASE;
			buddy_entry->x_size_class = x_min;
			buddy_entry->y_size_class = y_min;
		}

		appended = sol_buddy_grid_append_available(grid, buddy_entry_index);
		assert(appended);
	}

	return true;
}

void sol_buddy_grid_2_release(struct sol_buddy_grid_2* grid, uint32_t index)
{
	bool appended;
	do
	{
		appended = sol_buddy_grid_append_available(grid, index);
	}
	while(!appended);
}

struct sol_buddy_grid_location sol_buddy_grid_2_get_location(struct sol_buddy_grid_2* grid, uint32_t index)
{
	struct sol_buddy_grid_entry* entry;

	entry = sol_buddy_grid_entry_array_access_entry(&grid->entry_array, index);

	assert(entry->array_layer == sol_buddy_grid_packed_loc_get_layer(entry->packed_location));
	assert(entry->xy_offset.x == sol_buddy_grid_packed_loc_get_x(entry->packed_location) << entry->x_size_class);
	assert(entry->xy_offset.y == sol_buddy_grid_packed_loc_get_y(entry->packed_location) << entry->y_size_class);

	return (struct sol_buddy_grid_location)
	{
		.array_layer = entry->array_layer,
		.xy_offset = entry->xy_offset,
	};
}

bool sol_buddy_grid_2_has_space(struct sol_buddy_grid_2* grid, u16_vec2 size)
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
