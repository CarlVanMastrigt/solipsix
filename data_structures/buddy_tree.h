
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


#include <inttypes.h>

struct sol_buddy_tree
{
	/** it is the callers responsibility to size/scale provided offsets 
	 * which will always be in single units from the buddy tree's perspective */
	uint32_t* availablity_masks;
	
	/** the power of 2 that includes all addressible offsets in the tree, is also double the size of the */
	uint32_t encompasing_bit;

	/** this marks the size of the allocation managed in cases where the allocation didnt start out as a power of 2 or long lived allocations were taken from the end */
	uint32_t size;
};

void sol_buddy_tree_initialise(struct sol_buddy_tree* tree, uint32_t size);
void sol_buddy_tree_terminate(struct sol_buddy_tree* tree);

bool sol_buddy_tree_acquire(struct sol_buddy_tree* tree, uint32_t desired_size_exponent, uint32_t* allocation_offset);
void sol_buddy_tree_release(struct sol_buddy_tree* tree, uint32_t allocation_offset);

uint32_t sol_buddy_tree_query_allocation_size_exponent(const struct sol_buddy_tree* tree, uint32_t allocation_offset);

static inline bool sol_buddy_tree_has_space(struct sol_buddy_tree* tree, uint32_t desired_size_exponent)
{
	return tree->availablity_masks[1] >= (1u << desired_size_exponent);
}

/** tree has NO allocations */
static inline bool sol_buddy_tree_empty(struct sol_buddy_tree* tree)
{
	return tree->availablity_masks[1] == tree->size;
}


/** allowing dynamic resizing of total space, while possible, is a bad idea.
 * the slots opened up by any such resizing will be preferentially used/split
 * thus any subsequent resizing is very unlikely to work as the space it would try to take is the most likely region to have been allocated by any appropriately sized allocations */ 

/** possible to have a multilevel buddy allocator that uses u16 basis allocations at both (?) levels - puts some onus on caller
 * 
 * possible to allow non- Po2 allocations that provide space for smaller allocations to fill the gap (put "real size", in high bits with effective size class in lowe bits as is done now)
 * 
 * */

