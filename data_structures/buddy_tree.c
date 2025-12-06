/**
Copyright 2025 Carl van Mastrigt

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

#include "buddy_tree.h"
#include "sol_utils.h"

#define SOL_BUDDY_TREE_U32_MASK_MIN_BIT 0x00000100u

/** per location in the tree; is a mask of available slots at or beneath this location
		0 indicates this section/branch of the tree is completely occupied
		note: can actually have more than 32768 times smallest allocation total, as long as desired size is NEVER over that relative size
			^ just need a metadata tree with u32 backing for the first few indices/layers */

void sol_buddy_tree_initialise(struct sol_buddy_tree* tree, uint32_t total_size_exponent)
{
	assert(total_size_exponent < (CHAR_BIT * sizeof(uint32_t) - 1));
	assert(total_size_exponent >= 8);
	/**  assume more than 256 allocations manageable, makes code simpler and seems unreasonable to have fewer */

	uint8_t* availablity_masks_8_real_start;
	size_t bytes_to_allocate;
	uint32_t* metadata_allocation;

	uint32_t u32_mask_exponent = total_size_exponent - 8;
	uint32_t u32_mask_array_size = 1u << (u32_mask_exponent + 1);
	uint32_t u8_mask_array_size = 1u << (total_size_exponent + 1);

	tree->total_size_exponent = total_size_exponent;

	bytes_to_allocate = (sizeof(uint32_t) * u32_mask_array_size) + (sizeof(uint8_t) * (u8_mask_array_size - u32_mask_array_size));

	metadata_allocation = malloc(bytes_to_allocate);
	availablity_masks_8_real_start = (void*)(metadata_allocation + u32_mask_array_size);

	tree->availablity_masks_32 = metadata_allocation;
	/** set this pointer to the virtual start to avoid having to offset its memory */
	tree->availablity_masks_8 = availablity_masks_8_real_start - u32_mask_array_size;

	tree->availablity_masks_32[1] = 1u << total_size_exponent;
	/** this is a slight optimization that improves addition, avoiding an inxex check when scanning parents siblings for splittable size.
		this nodes neighbour(buddy) is the top of the tree and can thus we can idicate nothing more needs to be propagated "up the tree" if all of its bits are set */
	tree->availablity_masks_32[0] = -1;
}

void sol_buddy_tree_terminate(struct sol_buddy_tree* tree)
{
	/** all allocations should be freed before terminating */
	assert(tree->availablity_masks_32[1] == ((size_t)1 << tree->total_size_exponent));

	/** note: this allocation also includes the availablity_masks_8 */
	free(tree->availablity_masks_32);
}

// returns offset
// bool sol_buddy_tree_acquire(struct sol_buddy_tree* tree, size_t allocation_size, struct sol_buddy_tree_allocation* allocation)
bool sol_buddy_tree_acquire(struct sol_buddy_tree* tree, uint32_t desired_exponent, uint32_t* allocation_offset)
{
	uint32_t desired_size_bit, current_size_bit, splitable_size_bit, introduced_sizes_mask, offset, parent_offset;
	assert(desired_exponent <= tree->total_size_exponent);

	uint32_t* availablity_masks_32 = tree->availablity_masks_32;
	uint8_t* availablity_masks_8 = tree->availablity_masks_8;
	const uint32_t mask_32_range = 1 << (tree->total_size_exponent - 8 + 1);

	desired_size_bit = 1u << desired_exponent;

	if(availablity_masks_32[1] < desired_size_bit)
	{
		/** no allocations of desired size available; all available allocations smaller */
		return false;
	}

	offset = 1;

	/** find the smallest size bit of all entries available that can be split to get the desired size bit (size >= desired size as bit)
	 	note: this may not actually require anything to be split, as the smallest allocation that may satisfy the request may be exactly the right size */
	splitable_size_bit = availablity_masks_32[1] & (-desired_size_bit);
	assert(splitable_size_bit);
	// splitable_size_bit &= -splitable_size_bit;
	splitable_size_bit = 1u << sol_u32_ctz(splitable_size_bit);

	/** start searching at the root node */
	current_size_bit = 1u << tree->total_size_exponent;

	/** by splitting these sizes will become available; note they should not be present anywhere in the tree */
	introduced_sizes_mask = splitable_size_bit - desired_size_bit;
	assert((availablity_masks_32[1] & introduced_sizes_mask) == 0);

	while(current_size_bit != splitable_size_bit)
	{
		assert(availablity_masks_32[offset] & splitable_size_bit);/** splitable bit must be present */
		assert((availablity_masks_32[offset] & introduced_sizes_mask) == 0);/** splitting must not introduce existing mask bits */
		assert(offset < mask_32_range);/** make sure to iterate only the 32 bit part of the tree here */

		availablity_masks_32[offset] |= introduced_sizes_mask;

		offset <<= 1;
		current_size_bit >>= 1;

		if(current_size_bit >= SOL_BUDDY_TREE_U32_MASK_MIN_BIT)
		{
			/** bit we're searching for is not in left branch of the tree, go down right (buddy) branch */
			offset += (availablity_masks_32[offset] & splitable_size_bit) == 0;
		}
		else
		{
			/** bit we're searching for is not in left branch of the tree, go down right (buddy) branch */
			offset += (availablity_masks_8[offset] & splitable_size_bit) == 0;
			break;
		}
	}

	/** iterate the u8 mask part of the tree, same code as 32 versions above and below */ 
	if(current_size_bit < SOL_BUDDY_TREE_U32_MASK_MIN_BIT)
	{
		while(current_size_bit != splitable_size_bit)
		{
			assert(availablity_masks_8[offset] & splitable_size_bit);/** splitable bit must be present */
			assert((availablity_masks_8[offset] & introduced_sizes_mask) == 0);/** splitting must not introduce existing mask bits */
			assert(offset >= mask_32_range);/** make sure to iterate only the 8 bit part of the tree here */

			availablity_masks_8[offset] |= introduced_sizes_mask;

			offset <<= 1;
			current_size_bit >>= 1;

			/** bit we're searching for is not in left branch of the tree, go down right (buddy) branch */
			offset += (availablity_masks_8[offset] & splitable_size_bit) == 0;
		}

		parent_offset = offset >> 1;

		while(parent_offset >= mask_32_range)
		{
			assert(availablity_masks_8[parent_offset] & splitable_size_bit);

			/** note: on first iteration this will (and should) remove spliable size from this (the first parent) entry */
			availablity_masks_8[parent_offset] ^= splitable_size_bit;

			#warning should always have taken left branch if it was valid, so need only check (access memory) if on right? (offset&1) `^ 1` becomes `& ~1u`

			if(availablity_masks_8[parent_offset ^ 1] & splitable_size_bit)
			{
				/** parents sibling has this bit set, so don't need to remove from subsequent ancestors (as there is an allocation of splitable_size_bit)
					note: this cannot be true on first iteration; ergo structure to perform this operation automatically for first ancestor */
				goto SPLITABLE_BIT_REMOVED;
			}

			parent_offset >>= 1;
		}
	}
	else
	{
		parent_offset = offset >> 1;
	}

	/** this WILL hit the root of the tree (offset/index 1) assuming the size_delta_mask still has bits to change */
	while(parent_offset)
	{
		assert(availablity_masks_32[parent_offset] & splitable_size_bit);

		/** note: on first iteration this will (and should) remove spliable size from this (the first parent) entry */
		availablity_masks_32[parent_offset] ^= splitable_size_bit;

		#warning should always have taken left branch if it was valid, so need only check (access memory) if on right? (offset&1) `^ 1` becomes `& ~1u`

		if(availablity_masks_32[parent_offset ^ 1] & splitable_size_bit)
		{
			/** parents sibling has this bit set, so don't need to remove from subsequent ancestors (as there is an allocation of splitable_size_bit)
				note: this cannot be true on first iteration; ergo structure to perform this operation automatically for first ancestor */
			goto SPLITABLE_BIT_REMOVED;
		}

		parent_offset >>= 1;

		/** shouldnt be possible to be here for offset zero */
		assert(parent_offset);
	}

	SPLITABLE_BIT_REMOVED:;

	while(current_size_bit >= SOL_BUDDY_TREE_U32_MASK_MIN_BIT && current_size_bit != desired_size_bit)
	{
		assert(offset < mask_32_range);/** make sure to iterate only the 32 bit part of the tree here */

		/** this branch of tree will have all child sizes between current (non-inclusive) and desired size (inclusive) */
		availablity_masks_32[offset] = current_size_bit - desired_size_bit;
		current_size_bit >>= 1;
		offset <<= 1;
		/** set sibling */
		availablity_masks_32[offset + 1] = current_size_bit;
	}

	while(current_size_bit != desired_size_bit)
	{
		assert(offset >= mask_32_range);/** make sure to iterate only the 32 bit part of the tree here */

		/** this branch of tree will have all child sizes between current (non-inclusive) and desired size (inclusive) */
		availablity_masks_8[offset] = current_size_bit - desired_size_bit;
		current_size_bit >>= 1;
		offset <<= 1;
		/** set sibling */
		availablity_masks_8[offset + 1] = current_size_bit;
	}


	/** this sector has no available allocations (the whole sector is being allocated here) */
	if(desired_size_bit >= SOL_BUDDY_TREE_U32_MASK_MIN_BIT)
	{
		availablity_masks_32[offset] = 0;
	}
	else
	{
		availablity_masks_8[offset] = 0;
	}

	/** move offset to be in terms of minimum allocation size */
	offset <<= desired_exponent;

	/** this stores the size class/exponent for a given allocation at the maximum layer (one for highest granularity allocations)
		importantly: when there is information regarding allocation availibility that will actually be read overlapping this write the values will always match (0) */
	availablity_masks_8[offset] = desired_exponent;

	/** need to remove top bit (relative max exponent) from the offset to get allocation offset in terms of minimum allocation size, and it MUST be present */
	assert(offset & (1 << (tree->total_size_exponent)));
	*allocation_offset = offset - (1u << tree->total_size_exponent);

	assert(availablity_masks_32[0] == ~0u);

	return true;
}

void sol_buddy_tree_release(struct sol_buddy_tree* tree, size_t allocation_offset)
{
	uint32_t exponent;
	uint32_t released_size_bit, delta_size_bits;
	size_t offset;

	uint32_t* availablity_masks_32 = tree->availablity_masks_32;
	uint8_t* availablity_masks_8 = tree->availablity_masks_8;
	const uint32_t mask_32_range = 1 << (tree->total_size_exponent - 8 + 1);

	offset = allocation_offset;

	/** check offset is a valid value for this allocator */
	assert(offset < ((size_t)1 << tree->total_size_exponent));




	/** put offset in correct location/index in tree */
	offset |= (1u << tree->total_size_exponent);

	/** allocate stores size class in last layer for any given offset */
	exponent = availablity_masks_8[offset];

	/** ensure the alignment of the allocation offset is correct given the expected (stored) size class */
	assert((offset & ((1u << exponent) - 1)) == 0);
	/** move to the size appropriate level of the tree (i.e. where the allocation mist have been made from) */
	offset >>= exponent;

	released_size_bit = 1u << exponent;

	if(offset < mask_32_range)
	{
		assert(availablity_masks_32[offset] == 0);
	}
	else
	{
		assert(availablity_masks_8[offset] == 0);
	}

	/** try to coalesce available allocations */
	delta_size_bits = released_size_bit;
	while(availablity_masks[offset ^ 1] == released_size_bit)
	{
		assert(offset > 1);
		released_size_bit <<= 1;
		offset >>= 1;
		delta_size_bits |= released_size_bit;
	}

	assert(offset > 0);

	availablity_masks[offset] = released_size_bit;

	/** the coalesced-released bit should always be larger than its neighbour (buddy)
		the coalesced size must be the exact largest for this layer of the tree and if its neighbour (buddy) were the same they would coalesce */
	assert(availablity_masks[offset ^ 1] < released_size_bit || offset == 1);

	/** need to mark (final) released_size_bit as present in rest of tree and all other (coalesced) as removed
		note: any bits set in delta_size_bits must be changed on this branch of the tree, and are only set in parent if set in sibling
		removing all bits in sibling will mean only necessary changes are preserved while navigating ancestor tree which will be CHANGES to bit structure, ergo xor
		example:
		removed bits in branch so far will be removed from parent only if not present in neighbour
		bits added by branch (only the released_size_bit) will only need to be added to parent if not present in neighbour
		if either set of bits are present in neighbour they shouldn't be changed in the parent */
	while(delta_size_bits)
	{
		/** don't alter bits already present in neighbour (buddy) */
		delta_size_bits &= ~availablity_masks[offset ^ 1];
		offset >>= 1;

		assert((availablity_masks[offset] & delta_size_bits) == (delta_size_bits & (~released_size_bit)));

		availablity_masks[offset] ^= delta_size_bits;
	}

	assert(availablity_masks[0] == ~0u);
}



// returns offset
// bool sol_buddy_tree_acquire_2(struct sol_buddy_tree* tree, uint32_t desired_exponent, uint32_t* allocation_offset)
// {
// 	uint32_t desired_size_bit, current_size_bit, splitable_size_bit, introduced_sizes_mask, offset, parent_offset;
// 	assert(desired_exponent <= tree->total_size_exponent);

// 	uint32_t* availablity_masks_32 = tree->availablity_masks_32;
// 	uint8_t* availablity_masks_8 = tree->availablity_masks_8;
// 	uint32_t mask_32_range = 1 << (tree->total_size_exponent - 8 + 1);

// 	desired_size_bit = 1u << desired_exponent;

// 	if(availablity_masks_32[1] < desired_size_bit)
// 	{
// 		/** no allocations of desired size available; all available allocations smaller */
// 		return false;
// 	}

// 	offset = 1;

// 	/** find the smallest size bit of all entries available that can be split to get the desired size bit (size >= desired size as bit)
// 	 	note: this may not actually require anything to be split, as the smallest allocation that may satisfy the request may be exactly the right size */
// 	splitable_size_bit = availablity_masks_32[1] & (-desired_size_bit);
// 	assert(splitable_size_bit);
// 	// splitable_size_bit &= -splitable_size_bit;
// 	splitable_size_bit = 1u << sol_u32_ctz(splitable_size_bit);

// 	/** start searching at the root node */
// 	current_size_bit = 1u << tree->total_size_exponent;

// 	/** by splitting these sizes will become available; note they should not be present anywhere in the tree */
// 	introduced_sizes_mask = splitable_size_bit - desired_size_bit;
// 	assert((availablity_masks_32[1] & introduced_sizes_mask) == 0);

// 	while(current_size_bit != splitable_size_bit)
// 	{
// 		if(offset < mask_32_range)
// 		{
// 			assert(availablity_masks_32[offset] & splitable_size_bit);/** splitable bit must be present */
// 			assert((availablity_masks_32[offset] & introduced_sizes_mask) == 0);/** splitting must not introduce existing mask bits */
// 			assert(current_size_bit >= SOL_BUDDY_TREE_U32_MASK_MIN_BIT);/** make sure to iterate only the 32 bit part of the tree here */

// 			availablity_masks_32[offset] |= introduced_sizes_mask;

// 			offset <<= 1;
// 			current_size_bit >>= 1;

// 			#warning wrong! could need to look at u8
// 			// offset += (availablity_masks_32[offset] & splitable_size_bit) == 0;
// 		}
// 		else
// 		{
// 			assert(availablity_masks_8[offset] & splitable_size_bit);/** splitable bit must be present */
// 			assert((availablity_masks_8[offset] & introduced_sizes_mask) == 0);/** splitting must not introduce existing mask bits */
// 			assert(current_size_bit < SOL_BUDDY_TREE_U32_MASK_MIN_BIT);/** make sure to iterate only the 32 bit part of the tree here */

// 			availablity_masks_8[offset] |= introduced_sizes_mask;

// 			offset <<= 1;
// 			current_size_bit >>= 1;

// 			offset += (availablity_masks_8[offset] & splitable_size_bit) == 0;
// 		}
// 	}

// 	parent_offset = offset >> 1;

// 	/** this WILL hit the root of the tree (offset/index 1) assuming the size_delta_mask still has bits to change */
// 	while(parent_offset)
// 	{
// 		if(parent_offset < mask_32_range)
// 		{
// 			assert(availablity_masks_32[parent_offset] & splitable_size_bit);

// 			/** note: on first iteration this will (and should) remove spliable size from this (the first parent) entry */
// 			availablity_masks_32[parent_offset] ^= splitable_size_bit;

// 			// if((parent_offset & 1u) == 0 && availablity_masks_32[parent_offset | 1u] & splitable_size_bit)
// 			if(availablity_masks_32[parent_offset ^ 1] & splitable_size_bit)
// 			{
// 				/** parents sibling has this bit set, so don't need to remove from subsequent ancestors (as there is an allocation of splitable_size_bit)
// 					note: this cannot be true on first iteration; ergo structure to perform this operation automatically for first ancestor */
// 				break;
// 			}
// 		}
// 		else
// 		{
// 			assert(availablity_masks_8[parent_offset] & splitable_size_bit);

// 			/** note: on first iteration this will (and should) remove spliable size from this (the first parent) entry */
// 			availablity_masks_8[parent_offset] ^= splitable_size_bit;

// 			// if((parent_offset & 1u) == 0 && availablity_masks_8[parent_offset | 1u] & splitable_size_bit)
// 			if(availablity_masks_8[parent_offset ^ 1] & splitable_size_bit)
// 			{
// 				/** parents sibling has this bit set, so don't need to remove from subsequent ancestors (as there is an allocation of splitable_size_bit)
// 					note: this cannot be true on first iteration; ergo structure to perform this operation automatically for first ancestor */
// 				break;
// 			}
// 		}

// 		parent_offset >>= 1;

// 		/** shouldnt be possible to be here for offset zero */
// 		assert(parent_offset);
// 	}

// 	while(current_size_bit >= SOL_BUDDY_TREE_U32_MASK_MIN_BIT && current_size_bit != desired_size_bit)
// 	{
// 		if(offset < mask_32_range)
// 		{
// 			assert(current_size_bit >= SOL_BUDDY_TREE_U32_MASK_MIN_BIT);/** make sure to iterate only the 32 bit part of the tree here */

// 			/** this branch of tree will have all child sizes between current (non-inclusive) and desired size (inclusive) */
// 			availablity_masks_32[offset] = current_size_bit - desired_size_bit;
// 			current_size_bit >>= 1;
// 			offset <<= 1;
// 			/** set sibling */

// 			#warning wrong! could need to set u8
// 			// availablity_masks_32[offset + 1] = current_size_bit;
// 		}
// 		else
// 		{
// 			assert(current_size_bit < SOL_BUDDY_TREE_U32_MASK_MIN_BIT);/** make sure to iterate only the 32 bit part of the tree here */

// 			/** this branch of tree will have all child sizes between current (non-inclusive) and desired size (inclusive) */
// 			availablity_masks_8[offset] = current_size_bit - desired_size_bit;
// 			current_size_bit >>= 1;
// 			offset <<= 1;
// 			/** set sibling */

// 			availablity_masks_8[offset + 1] = current_size_bit;
// 		}
// 	}


// 	/** this sector has no available allocations (the whole sector is being allocated here) */
// 	if(desired_size_bit >= SOL_BUDDY_TREE_U32_MASK_MIN_BIT)
// 	{
// 		availablity_masks_32[offset] = 0;
// 	}
// 	else
// 	{
// 		availablity_masks_8[offset] = 0;
// 	}

// 	/** move offset to be in terms of minimum allocation size */
// 	offset <<= desired_exponent;

// 	/** this stores the size class/exponent for a given allocation at the maximum layer (one for highest granularity allocations)
// 		importantly: when there is information regarding allocation availibility that will actually be read overlapping this write the values will always match (0) */
// 	availablity_masks_8[offset] = desired_exponent;

// 	/** need to remove top bit (relative max exponent) from the offset to get allocation offset in terms of minimum allocation size, and it MUST be present */
// 	assert(offset & (1 << (tree->total_size_exponent)));
// 	*allocation_offset = offset - (1u << tree->total_size_exponent);

// 	assert(availablity_masks_32[0] == ~0u);

// 	return true;
// }
