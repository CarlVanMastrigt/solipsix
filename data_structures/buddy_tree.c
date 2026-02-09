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


#include <stdlib.h>
#include <assert.h>
#include <limits.h>


#include <stdio.h>

#include "buddy_tree.h"
#include "sol_utils.h"


void sol_buddy_tree_initialise(struct sol_buddy_tree* tree, uint32_t size)
{
	uint32_t offset, current_size_bit, remaining_bits, size_exponent;
	assert(size > 1);
	
	size_exponent = sol_u32_exp_ge(size);

	assert(size_exponent < (CHAR_BIT * sizeof(uint32_t) - 1));

	tree->encompasing_bit = 1u << size_exponent;;
	tree->size = size;

	tree->availablity_masks = malloc((sizeof(uint32_t) << (size_exponent + 1)));
	// memset(tree->availablity_masks, 0xFF, (sizeof(uint32_t) << (size_exponent + 1)));// use to verify bit structure is correct

	/** this is a slight optimization that improves addition, avoiding an index check when scanning parents siblings for splittable size.
		this nodes neighbour(buddy) is the top of the tree and can thus we can idicate nothing more needs to be propagated "up the tree" if all of its bits are set */
	tree->availablity_masks[0] = ~0u;

	offset = 1;
	current_size_bit = 1u << size_exponent;
	remaining_bits = size;

	/** behaviour / control-flow is slightly weird here if size == current_size_bit, but it still works */
	while(remaining_bits)
	{
		if(remaining_bits & current_size_bit)
		{
			tree->availablity_masks[offset] = current_size_bit;
			offset++;
			remaining_bits ^= current_size_bit;
		}
		else
		{
			/** is *technically* correct without this, but it is perhaps very difficult to see why */
			tree->availablity_masks[offset+1] = 0;
		}
		tree->availablity_masks[offset] = remaining_bits;
		offset <<= 1;
		current_size_bit >>= 1;
	}

	assert(tree->availablity_masks[1] == size);
}

void sol_buddy_tree_terminate(struct sol_buddy_tree* tree)
{
	/** all allocations should be freed before terminating */
	assert(tree->availablity_masks[1] == tree->size);
	assert(tree->availablity_masks[0] == ~0u);

	free(tree->availablity_masks);
}

// returns offset
// bool sol_buddy_tree_acquire(struct sol_buddy_tree* tree, size_t allocation_size, struct sol_buddy_tree_allocation* allocation)
bool sol_buddy_tree_acquire(struct sol_buddy_tree* tree, uint32_t desired_size_exponent, uint32_t* allocation_offset)
{
	uint32_t desired_size_bit, current_size_bit, splitable_size_bit, introduced_sizes_mask, offset, parent_offset;

	desired_size_bit = 1u << desired_size_exponent;

	if(tree->availablity_masks[1] < desired_size_bit)
	{
		/** no allocations of desired size available; all available allocations smaller */
		return false;
	}

	offset = 1;

	/** find the smallest size bit of all entries available that can be split to get the desired size bit (size >= desired size as bit)
	 * 		note: this may not actually require anything to be split, as the smallest allocation that may satisfy the request may be exactly the right size */
	splitable_size_bit = tree->availablity_masks[1] & (-desired_size_bit);
	// splitable_size_bit &= ~(splitable_size_bit-1);
	// splitable_size_bit &= -splitable_size_bit;
	splitable_size_bit = 1u << sol_u32_ctz(splitable_size_bit);


	/** start searching at the root node */
	current_size_bit = tree->encompasing_bit;

	/** by splitting `introduced_sizes_mask` sizes will become available 
	 * 		note: these bits shouldn't be present anywhere in the tree as of yet, 
	 * 		otherwise the allocation that offending bit signifies should be the one being split */
	introduced_sizes_mask = splitable_size_bit - desired_size_bit;
	assert((tree->availablity_masks[1] & introduced_sizes_mask) == 0);

	while(current_size_bit != splitable_size_bit)
	{
		/** splitable bit must be present */
		assert(tree->availablity_masks[offset] & splitable_size_bit);

		/** smaller sizes that are still larger than or equal to the desired size should also not be present, 
		 * otherwise they would have been split/used, ergo tree is in an invalid state */
		assert((tree->availablity_masks[offset] & introduced_sizes_mask) == 0);

		tree->availablity_masks[offset] |= introduced_sizes_mask;

		offset <<= 1;
		current_size_bit >>= 1;

		if((tree->availablity_masks[offset] & splitable_size_bit) == 0)
		{
			offset ++;
		}
	}

	/** need to remove splitable bit from tree; this cannot be done on way down as the splitable bit being set may reference MANY children
		note: splitable may actually just be desired here */
	parent_offset = offset >> 1;

	/** this WILL hit the root of the tree (offset/index 1) assuming the size_delta_mask still has bits to change */
	while(parent_offset)
	{
		assert(tree->availablity_masks[parent_offset] & splitable_size_bit);

		/** note: on first iteration this will (and should) remove spliable size from this (the first parent) entry */
		tree->availablity_masks[parent_offset] ^= splitable_size_bit;

		if(tree->availablity_masks[parent_offset ^ 1] & splitable_size_bit)
		{
			/** parents sibling has this bit set, so don't need to remove from subsequent ancestors (as there is an allocation of splitable_size_bit)
				note: this cannot be true on first iteration; ergo structure to perform this operation automatically for first ancestor */
			break;
		}

		parent_offset >>= 1;
	}


	while(current_size_bit != desired_size_bit)
	{
		/** this branch of tree will have all child sizes between current (non-inclusive) and desired size (inclusive) */
		tree->availablity_masks[offset] = current_size_bit - desired_size_bit;
		current_size_bit >>= 1;
		offset <<= 1;
		/** set sibling */
		tree->availablity_masks[offset + 1] = current_size_bit;
	}

	/** this sector has no available allocations (the whole sector is being allocated here) */
	tree->availablity_masks[offset] = 0;


	/** move offset to be in terms of minimum allocation size */
	offset <<= desired_size_exponent;

	/** this stores the size class/exponent for a given allocation at the maximum layer (one for highest granularity allocations)
		importantly: when there is information regarding allocation availibility that will actually be read overlapping this write the values will always match (0) */
	tree->availablity_masks[offset] = desired_size_exponent;
	/** need to remove top bit (relative max exponent) from the offset to get allocation offset in terms of minimum allocation size, and it MUST be present */
	assert(offset & tree->encompasing_bit);

	*allocation_offset = offset ^ tree->encompasing_bit;

	assert(tree->availablity_masks[0] == ~0u);

	/** could store marker that this specific location was allocated here (needed on free if not returning/storing sol_buddy_tree_allocation)
	 	i.e. need some more data if just storing/returning byte offset/pointer */

	return true;
}

void sol_buddy_tree_release(struct sol_buddy_tree* tree, uint32_t offset)
{
	uint32_t exponent, released_size_bit, delta_size_bits;

	/** check allocation offset is a valid value for this allocator */
	assert(offset < tree->size);

	/** check offset isn't too big */
	assert(offset < tree->encompasing_bit);

	/** indices in tree have (relative) high bit set */
	offset |= tree->encompasing_bit;

	/** allocate stores size class in last layer for any given offset */
	exponent = tree->availablity_masks[offset];

	/** ensure the alignment of the allocation offset is correct given the expected (stored) size class */
	assert((offset & (((size_t)1 << exponent) - 1)) == 0);
	/** move to the size appropriate level of the tree (i.e. where the allocation mist have been made from) */
	offset >>= exponent;


	released_size_bit = 1u << exponent;

	assert(tree->availablity_masks[offset] == 0);

	/** try to coalesce available allocations */
	while(tree->availablity_masks[offset ^ 1] == released_size_bit)
	{
		assert(offset > 1);
		released_size_bit <<= 1;
		offset >>= 1;
	}

	assert(offset > 0);

	/** add the newly released, post-coalescion allocation to the tree */

	tree->availablity_masks[offset] = released_size_bit;

	/** the changes that must be propogated up the tree are the newly added allocation, and the removal of all coalesced allocation sizes
	 * the removed allocation sizes are everything between the initially released location size (invlusive) and the post-coalescion size (not inclusive) */
	delta_size_bits = released_size_bit | (released_size_bit - (1u << exponent));

	/** the coalesced-released bit should always be larger than its sibling (buddy)
		the coalesced size must be the exact largest for this layer of the tree and if its sibling (buddy) were the same they would coalesce */
	assert(tree->availablity_masks[offset ^ 1] < released_size_bit || offset == 1);

	/** need to mark (final) released_size_bit as present in rest of tree and all other (coalesced) as removed
	 * parent will already have bits present in sibling set
	 * thus removing all bits present in sibling from the delta will mean only necessary *changes* are preserved while navigating tree 
	 * 		note: because this is only *changes* to bit structure XOR is appropriate 
	 * 			  and when *changes* becomes a zero mask there is nothing left to do */
	while(delta_size_bits)
	{
		/** don't alter bits already present in sibling (buddy) */
		delta_size_bits &= ~tree->availablity_masks[offset ^ 1];
		offset >>= 1;

		assert((tree->availablity_masks[offset] & delta_size_bits) == (delta_size_bits & (~released_size_bit)));

		tree->availablity_masks[offset] ^= delta_size_bits;
	}

	assert(tree->availablity_masks[0] == ~0u);
}

uint32_t sol_buddy_tree_query_allocation_size_exponent(const struct sol_buddy_tree* tree, uint32_t offset)
{
	uint32_t exponent;

	/** check allocation offset is a valid value for this allocator */
	assert(offset < tree->size);

	/** check offset isn't too big */
	assert(offset < tree->encompasing_bit);

	/** indices in tree have (relative) high bit set */
	offset |= tree->encompasing_bit;

	/** allocate stores size class in last layer for any given offset */
	exponent = tree->availablity_masks[offset];

	/** ensure the alignment of the allocation offset is correct given the expected (stored) size class */
	assert((offset & (((size_t)1 << exponent) - 1)) == 0);

	return exponent;
}


