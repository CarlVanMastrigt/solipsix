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



#include "vk/image_atlas.h"





/** should the image atlas vend monotonic identifiers which are then mapped externally?
 	that way fonts can track their contents with a map as they would likely need to anyway (to store glyph specific data)

 	how to defragment? acquire access and release with semaphore of completion on both
 	acquire -> semaphore to wait on before using
 	release <- semaphore of last use for these contents (or array of semaphores if used across queues)
 		^ acquire/release would need an index/slot associated (so that prior releases can be ignored)
 			^ NO - copies/defragments can wait on prior defragments, allowing correct global order
 	doing so would mark all resources before and after usage and allow defragmentation to be managed internally?

 	fill / copies would need to be handled externally; this is good if atlas is being used for compositing
 		^ compositing would want transient allocations that don't retain their contents (and thus don't require a copy op to defrag)

 	accessors hold a RC of each entry? add to LRU after being released? automatic release if created as "transient"? (no fixed size &c.)
 	RC should be decremented after all moments are released

 	release last triggers defragmentation?
*/



/** each accessor needs a reference to all entries used in it's execution, can easuly have multiple completely independent accesses to entries that don't complete in order
	however, this is too much overhead

	single usage queue (linked list) with an associated point in time for each accessor
		^ start of accessor: mark point in time in queue
		^ release of accessor: apply moment restrictions to progressing past that point in time

		-------|------|-----|------
		       ^ anything after here must have waited on all prior entries in order to be releasable (including defragment based copies)

	could/should literally have something in the linked list/queue to identify these thresholds (variant index bit to identify threshold?)
	
	the above approach does mean long lived dispatches block release of all *subsequent* entries, but this is probably fine as dispatches should be *relatively* short lived
*/


/** for now: atlas is singularly ordered for all individual resources (all resources must be externally synchronized between init and use)
		^ this handles [write -> use -> use] by putting onus on caller
	really need a plan for defragmentation though, would like it to run completely asynchronously (only update locations when CPU is notified of copy op completing)
	current access complete useful for this, don't defragment until first use is complete (technically unnecessary wait as write *likely* happens before read use)
	perhaps its better to not to "deframent" at all and instead force resource recreation by invalidating entries when thier placement becomes particularly egregious
	possibly block access to these resources so they leave the active section of the queue? or put onus on caller to copy as part of request (where it would otherwise have to load if it were invalidated)
	this avoids recreation while allowing access to the old location for other uses

	similar approach could also be used for ensuring value is initialised; write access (either implicitly on obtain because missing or explicitly on request)
	forces subsequent uses to wait on that accesses request (should they wish to use it, make optional part of request that forces a dependency)
	similar could then be used to handle defragmentation without shifting onus on caller to copy
		^ though: start with no defrag; just invalidate

	important to allow external writing of resources, this MAY be paired with immediate invalidation (only transient resources written externally) or locked in accessor "owner" for resources
	important to allow immediate/controlled instantiation of resources even if that means access cannot be multi-threaded/multi-accessor <- this may actually be a (massive) benefit ??
		^ allows internal abstracted defragmentation controlled CPU side in simple fashion(?)

		need a map of which re
	*/

#warning the single wait on latest moment when acquiring is sufficient for ensuring defragmentation works (need to ensure latest location is vended), \
but not great for having dedicated loaders or really any good mechanism for sharing resources between accessors, \
at the moment a novel resource must not be used by another accessor as there is no way to guarantee synchronization of filling its contents (leading to UB)
// ^ MAYBE find for texture resources, but definitely bad for buffer resources


	/** is it worth breaking the timeline semaphore into read and write??
	 * read must wait on prior writes
	 * write must wait on prior writes
	 * write must wait on prior reads
	 * read needn't wait on prior reads though...
	 * so separating them would allow disorder read only frames, I'm unsure if this is desirable though*/




#define SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT 2u
#define SOL_IMAGE_ATLAS_MIN_TILE_SIZE 4u
/** note: SOL_IMAGE_ATLAS_MIN_TILE_SIZE must be power of 2 */
#define SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT 13u
/** [4,16384] inclusive ^^ */

struct sol_image_atlas_entry
{
	/** hash map key; used for hash map lookup upon defragmentation eviction */
	uint64_t identifier;

//== 64 ==

	/** prev/next indices in linked list of entries my order of use, 16M is more than enough entries
	 * NOTE: 0 is reserved for the dummy start index
	 * SOL_U32_INVALID indicates an entry is not in the linked list, though this can also be inferred by other properties */
	uint32_t prev_entry_index;
	uint32_t next_entry_index;

//== 64 ==

	/** links in 2d structure
	 *  index zero is invalid, which should be the index of root node in the linked list
	 *  names reftlect direction from a corner;
	 * start corner is towards zero (top left)
	 * end corner is away from zero (bottom right) */
	uint32_t adj_start_left;
	uint32_t adj_start_up;
//== 64 ==
	uint32_t adj_end_right;
	uint32_t adj_end_down;

//== 64 ==

	/** can be derived from packed location, but provided her for quick access (may be worth removing id doing so would better size this struct) */
	u16_vec2 pixel_location;

	/** z-tile location is in terms of minimum entry pixel dimension (4)
	 * packed in such a way to order entries by layer first then top left -> bottom right
	 * i.e. packed like: | array layer (8) | z-tile location (24 == 12x + 12y) |
	 * this is used to sort/order entries (lower value means better placed) */
	uint32_t packed_location;

//== 64 ==

	/** size class is power of 2 times minimum entry pixel dimension (4)
	 * so: 4 * 2 ^ (0:15) is more than enough (4 << 15 = 128k, is larger than max texture size)*/
	uint64_t x_size_class : 4;
	uint64_t y_size_class : 4;

	/** array slice of image this entry is present in */
	uint64_t array_slice : 8;

	/** index in heap of size class (x,y) only 16M entries supported so this is enough (22 bits would be enough) */
	uint64_t heap_index : 24;
	#warning is heap warranted? items on the heap should be free, which means identifier and prev/next in linked list will be unused...
		/** ^ should be very few free items at any time though */


	/** index in queue of accessors (does need to be a u32 for compatibility)
	 * used to ensure an entry is only moved forward in the least recently used queue when necessary and to a point that correctly reflects its usage
	 * (should be put just after the accessor marker)  */
	uint64_t access_index : 16;
	/** ^ just making sure this matches index held in sol_image_atlas_queue_index (shouldn't matter given queue implementation though) */

	/** must be set appropriately on "creation" this indicates that the contents should NOT be copied when this tile is deframented
	 * (possibly just immediately free this location instead of deframenting it)
	 * (and) the location should be discarded/relinquished immediately after accessor release ?
	 * if request differs from this property then the old location should be treated as different (possibly same goes with size?)
	 * only applicable when is_tile_entry */
	uint64_t is_transient : 1;

	/** the start of each accessor and the start/end of the queue itself use (unreferenced) atlas entries in the linked list to designate ranges
	 * the following are mutually exlusive; is_accessor indicating an accessor threshold entry and is_tile_entry indicating a pixel grid (real) entry
	 * if neither are set that is the root of the linked list held by the atlas itself and used for insertion */
	uint64_t is_accessor : 1;
	uint64_t is_tile_entry : 1;

	/** is this a free/available tile, if so identifier should be ignored */
	uint64_t is_available : 1;

	/** 4 reserved bytes */
};

struct sol_image_atlas_accessor
{
	/** the index of the entry within the linked list marking the start/threshold of this accessor */
	uint32_t threshold_entry_index;

	struct sol_vk_timeline_semaphore_moment complete_moment;

	#warning entries can back-refernce this by index, possibly good to do this to allow dependencies between accesses?
	/** a more reliable way to accomplish this is to have dedicated loading for */
};

#define SOL_ARRAY_ENTRY_TYPE struct sol_image_atlas_entry
#define SOL_ARRAY_FUNCTION_PREFIX sol_image_atlas_entry_array
#define SOL_ARRAY_STRUCT_NAME sol_image_atlas_entry_array
#include "data_structures/array.h"



#define SOL_QUEUE_ENTRY_TYPE struct sol_image_atlas_accessor
#define SOL_QUEUE_FUNCTION_PREFIX sol_image_atlas_accessor_queue
#define SOL_QUEUE_STRUCT_NAME sol_image_atlas_accessor_queue
#include "data_structures/queue.h"


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
#define SOL_HASH_MAP_ENTRY_HASH(E, CTX) sol_image_atlas_entry_identifier_get(E, CTX)
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

#warning could binary heap benefit from custom allocator (169 allocations here, that could all come from a single alloc)
#warning binary heap could have custom size to use when 0 initial size is provided


struct sol_image_atlas
{
	/** provided description */
	struct sol_image_atlas_description description;



	/** this is the management semaphore for accesses in the image atlas, it will signal availability and write completion
	 * users of the image atlas must signal moments vended to them to indicate reads and writes have completed
	 * users must also wait on the moment vended upon acquisition in order to ensure all prior modifications are visible */
	struct sol_vk_timeline_semaphore timeline_semaphore;


	/** all entry indices reference indices in this array, contains the actual information regarding vended tiles */
	struct sol_image_atlas_entry_array entry_array;

	/** queue of all accessors vended/used to make queries to the atlas and time their moments of completion (i.e. when they're no longer in use) */
	struct sol_image_atlas_accessor_queue accessor_queue;

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

	/** the (reserved/unused) entry delineating the start and end of the linked list of entries */
	uint32_t root_entry_index;

	/** index in queue of present accessor, only one may be active at a time in the current scheme */
	uint32_t current_accessor_index;

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
	/** the root entry should not be removed in this way */
	assert(entry->is_accessor || entry->is_tile_entry);
	assert(entry->next_entry_index != SOL_U32_INVALID);
	assert(entry->prev_entry_index != SOL_U32_INVALID);

	prev_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry->prev_entry_index);
	next_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry->next_entry_index);

	prev_entry->next_entry_index = entry->next_entry_index;
	next_entry->prev_entry_index = entry->prev_entry_index;

	entry->next_entry_index = SOL_U32_INVALID;
	entry->prev_entry_index = SOL_U32_INVALID;
}

static inline void sol_image_atlas_remove_available_entry(struct sol_image_atlas* atlas, struct sol_image_atlas_entry* entry)
{
	struct sol_image_atlas_entry_availability_heap* availability_heap;
	uint32_t validation_index;

	/** validate entry really does look available */
	assert(entry->is_available);
	assert(entry->is_tile_entry);
	assert(entry->identifier == 0);
	assert(entry->prev_entry_index == SOL_U32_INVALID);
	assert(entry->next_entry_index == SOL_U32_INVALID);

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
	uint16_t coalesced_end_x;
	bool odd_offset;

	coalesce_index = *entry_index_ptr;
	coalesce_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, *entry_index_ptr);

	/** check for coalescable buddy */
	odd_offset = coalesce_entry->pixel_location.x & (SOL_IMAGE_ATLAS_MIN_TILE_SIZE << coalesce_entry->x_size_class);
	buddy_index = odd_offset ? coalesce_entry->adj_start_left : coalesce_entry->adj_end_right;
	if (buddy_index == 0)
	{
		/** root entry index (zero) indicated this entry has no valid neighbours in this direction
		 * must fill entire image to not have a valid buddy though */
		assert(coalesce_entry->x_size_class + SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT == atlas->description.image_x_dimension_exponent);
		return false;
	}
	buddy_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, buddy_index);

	if ( ! buddy_entry->is_available || buddy_entry->x_size_class != coalesce_entry->x_size_class && buddy_entry->y_size_class != coalesce_entry->y_size_class)
	{
		return false;
	}

	/** remove buddy from availability heap */
	sol_image_atlas_remove_available_entry(atlas, buddy_entry);

	/** coalesce into the entry closer to zero, so swap entry and it's buddy if necessary to make this happen
	 * this makes the code that follows simpler/invariant and preserves the packed location value */
	if (odd_offset)
	{
		*entry_index_ptr = buddy_index;
		SOL_SWAP(coalesce_entry, buddy_entry);
		SOL_SWAP(coalesce_index, buddy_index);
	}

	/** make sure buddy is in the expected location */
	assert(coalesce_entry->pixel_location.y == buddy_entry->pixel_location.y);
	assert(coalesce_entry->pixel_location.x + (SOL_IMAGE_ATLAS_MIN_TILE_SIZE << coalesce_entry->x_size_class) == buddy_entry->pixel_location.x);

	/** combine buddy with entry*/
	coalesce_entry->x_size_class++;

	coalesce_entry->adj_end_right = buddy_entry->adj_end_right;
	coalesce_entry->adj_end_down   = buddy_entry->adj_end_down;

	/** this combines the buddy (right tile of the 2) into the left tile
	 * so need to set any link that referenced the buddy to now reference the coalesced tile
	 * iterate all edges and replace references to buddy(index) with reference to entry(index) */

	/** traverse right side */
	adjacent_index = coalesce_entry->adj_end_right;
	while (adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if (adjacent_entry->adj_start_left != buddy_index)
		{
			assert(adjacent_entry->pixel_location.y < buddy_entry->pixel_location.y);
			break;
		}
		adjacent_entry->adj_start_left = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_up;
	}

	/** traverse bottom side */
	adjacent_index = coalesce_entry->adj_end_down;
	while (adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if (adjacent_entry->adj_start_up != buddy_index)
		{
			assert(adjacent_entry->pixel_location.x < buddy_entry->pixel_location.x);
			break;
		}
		adjacent_entry->adj_start_up = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_left;
	}

	/** traverse top side, first half may already reference coalesced index and so muct be skipped */
	coalesced_end_x = coalesce_entry->pixel_location.x + (SOL_IMAGE_ATLAS_MIN_TILE_SIZE << coalesce_entry->x_size_class);
	adjacent_index = coalesce_entry->adj_start_up;
	while (adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		/** when starting from top left may already be referencing correct entry (index), so should skip this if encountered */
		if (adjacent_entry->adj_end_down != coalesce_index)
		{
			if (adjacent_entry->adj_end_down != buddy_index)
			{
				assert(adjacent_entry->pixel_location.x >= coalesced_end_x || adjacent_entry->x_size_class > coalesce_entry->x_size_class);
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
	uint16_t coalesced_end_y;
	bool odd_offset;

	coalesce_index = *entry_index_ptr;
	coalesce_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, *entry_index_ptr);

	/** check for coalescable buddy */
	odd_offset = coalesce_entry->pixel_location.y & (SOL_IMAGE_ATLAS_MIN_TILE_SIZE << coalesce_entry->y_size_class);
	buddy_index = odd_offset ? coalesce_entry->adj_start_up : coalesce_entry->adj_end_down;
	if (buddy_index == 0)
	{
		/** root entry index (zero) indicated this entry has no valid neighbours in this direction
		 * must fill entire image to not have a valid buddy though */
		assert(coalesce_entry->y_size_class + SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT == atlas->description.image_y_dimension_exponent);
		return false;
	}
	buddy_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, buddy_index);

	if ( ! buddy_entry->is_available || buddy_entry->x_size_class != coalesce_entry->x_size_class && buddy_entry->y_size_class != coalesce_entry->y_size_class)
	{
		return false;
	}

	/** remove buddy from availability heap */
	sol_image_atlas_remove_available_entry(atlas, buddy_entry);

	/** coalesce into the entry closer to zero, so swap entry and it's buddy if necessary to make this happen
	 * this makes the code that follows simpler/invariant and preserves the packed location value */
	if (odd_offset)
	{
		*entry_index_ptr = buddy_index;
		SOL_SWAP(coalesce_entry, buddy_entry);
		SOL_SWAP(coalesce_index, buddy_index);
	}

	/** make sure buddy is in the expected location */
	assert(coalesce_entry->pixel_location.x == buddy_entry->pixel_location.x);
	assert(coalesce_entry->pixel_location.y + (SOL_IMAGE_ATLAS_MIN_TILE_SIZE << coalesce_entry->y_size_class) == buddy_entry->pixel_location.y);

	/** combine buddy with entry*/
	coalesce_entry->y_size_class++;

	coalesce_entry->adj_end_right = buddy_entry->adj_end_right;
	coalesce_entry->adj_end_down   = buddy_entry->adj_end_down;

	/** this combines the buddy (right tile of the 2) into the left tile
	 * so need to set any link that referenced the buddy to now reference the coalesced tile
	 * iterate all edges and replace references to buddy(index) with reference to entry(index) */

	/** traverse bottom side */
	adjacent_index = coalesce_entry->adj_end_down;
	while (adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if (adjacent_entry->adj_start_up != buddy_index)
		{
			assert(adjacent_entry->pixel_location.x < buddy_entry->pixel_location.x);
			break;
		}
		adjacent_entry->adj_start_up = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_left;
	}

	/** traverse right side */
	adjacent_index = coalesce_entry->adj_end_right;
	while (adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if (adjacent_entry->adj_start_left != buddy_index)
		{
			assert(adjacent_entry->pixel_location.y < buddy_entry->pixel_location.y);
			break;
		}
		adjacent_entry->adj_start_left = coalesce_index;

		adjacent_index = adjacent_entry->adj_start_up;
	}

	/** traverse left side, first half may already reference coalesced index and so muct be skipped */
	coalesced_end_y = coalesce_entry->pixel_location.y + (SOL_IMAGE_ATLAS_MIN_TILE_SIZE << coalesce_entry->y_size_class);
	adjacent_index = coalesce_entry->adj_start_left;
	while (adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		/** when starting from top left may already be referencing correct entry (index), so should skip this if encountered */
		if (adjacent_entry->adj_end_right != coalesce_index)
		{
			if (adjacent_entry->adj_end_right != buddy_index)
			{
				assert(adjacent_entry->pixel_location.y >= coalesced_end_y || adjacent_entry->y_size_class > coalesce_entry->y_size_class);
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

	/** entry must have been removed from the map before calling this function */
	assert(entry->identifier == 0);
	assert(entry->is_tile_entry);

	sol_image_atlas_entry_remove_from_queue(atlas, entry);
	entry->is_available = true;

	/** check to see if this entry can be coalesced with its neighbours */
	do
	{
		/** note: more agressively combine vertically, because also more agressively split vertically */
		if (entry->x_size_class < entry->y_size_class)
		{
			coalesced = sol_image_atlas_entry_try_coalesce_horizontal(atlas, &entry_index);
			if (!coalesced)
			{
				coalesced = sol_image_atlas_entry_try_coalesce_vertical(atlas, &entry_index);
			}
		}
		else
		{
			coalesced = sol_image_atlas_entry_try_coalesce_vertical(atlas, &entry_index);
			if (!coalesced)
			{
				coalesced = sol_image_atlas_entry_try_coalesce_horizontal(atlas, &entry_index);
			}
		}

		entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index);
	}
	while (coalesced);

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

	map_remove_result = sol_image_atlas_map_remove(&atlas->itentifier_entry_map, entry->identifier, &map_entry_index);
	assert(map_remove_result == SOL_MAP_SUCCESS_REMOVED);
	assert(map_entry_index == entry_index);
	entry->identifier = 0;

	sol_image_atlas_entry_make_available(atlas, entry_index);
}

/** NOTE: will always maintain the index of the split location, creating a new entry (and index with it) for the adjacent tile created by splitting
 * this should also only be called on available entries */
static inline void sol_image_atlas_entry_split_horizontally(struct sol_image_atlas* atlas, struct sol_image_atlas_entry* split_entry, uint32_t split_index)
{
	struct sol_image_atlas_entry_availability_heap* availability_heap;
	struct sol_image_atlas_entry* buddy_entry;
	struct sol_image_atlas_entry* adjacent_entry;
	uint32_t buddy_index, adjacent_index;
	uint16_t buddy_end_x;

	/** validate entry to split really does look available */
	assert(split_entry->is_available);
	assert(split_entry->is_tile_entry);
	assert(split_entry->identifier == 0);
	assert(split_entry->prev_entry_index == SOL_U32_INVALID);
	assert(split_entry->next_entry_index == SOL_U32_INVALID);

	/** in order to split entry must be larger than the min size */
	assert(split_entry->x_size_class > 0);
	split_entry->x_size_class--;

	/* check the split entries offset is valid for it's size class */
	assert((split_entry->packed_location & (1u << (split_entry->x_size_class * 2 + 0))) == 0);
	assert((split_entry->pixel_location.x & (SOL_IMAGE_ATLAS_MIN_TILE_SIZE << split_entry->x_size_class)) == 0);

	buddy_entry = sol_image_atlas_entry_array_append_ptr(&atlas->entry_array, &buddy_index);

	*buddy_entry = (struct sol_image_atlas_entry)
	{
		.identifier = 0,
		.next_entry_index = SOL_U32_INVALID,
		.prev_entry_index = SOL_U32_INVALID,
		.adj_start_left = split_index,
		.adj_start_up = 0,/** must be set */
		.adj_end_right = split_entry->adj_end_right,
		.adj_end_down  = split_entry->adj_end_down,
		.pixel_location = u16_vec2_add(split_entry->pixel_location, u16_vec2_set(SOL_IMAGE_ATLAS_MIN_TILE_SIZE << split_entry->x_size_class, 0)),
		.packed_location = split_entry->packed_location | (1u << (split_entry->x_size_class * 2 + 0)),
		.x_size_class = split_entry->x_size_class,
		.y_size_class = split_entry->y_size_class,
		.array_slice = split_entry->array_slice,
		// .heap_index
		.is_accessor = false,
		.is_tile_entry = true,
		.is_available = true,
	};


	split_entry->adj_end_right = buddy_index;
	split_entry->adj_end_down = 0;/** must be set */

	/** traverse right side */
	adjacent_index = buddy_entry->adj_end_right;
	while (adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if (adjacent_entry->adj_start_left != split_index)
		{
			assert(adjacent_entry->pixel_location.y < buddy_entry->pixel_location.y);
			break;
		}
		adjacent_entry->adj_start_left = buddy_index;

		adjacent_index = adjacent_entry->adj_start_up;
	}

	/** traverse bottom side */
	adjacent_index = buddy_entry->adj_end_down;
	while (adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if (adjacent_entry->pixel_location.x < buddy_entry->pixel_location.x)
		{
			assert(split_entry->adj_end_down == 0);
			split_entry->adj_end_down = adjacent_index;
			break;
		}
		adjacent_entry->adj_start_up = buddy_index;

		adjacent_index = adjacent_entry->adj_start_left;
	}

	/** traverse top side */
	buddy_end_x = buddy_entry->pixel_location.x + (SOL_IMAGE_ATLAS_MIN_TILE_SIZE << buddy_entry->x_size_class);
	adjacent_index = split_entry->adj_start_up;
	while (adjacent_entry)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if (adjacent_entry->x_size_class > buddy_entry->x_size_class || adjacent_entry->pixel_location.x == buddy_entry->pixel_location.x)
		{
			assert(buddy_entry->adj_start_up == 0);
			buddy_entry->adj_start_up = adjacent_index;
		}

		if (adjacent_entry->adj_end_down != split_index)
		{
			assert(adjacent_entry->pixel_location.x >= buddy_end_x || adjacent_entry->x_size_class > (split_entry->x_size_class + 1));
			break;
		}

		if (adjacent_entry->pixel_location.x >= buddy_entry->pixel_location.x)
		{
			adjacent_entry->adj_end_down = buddy_index;
		}

		adjacent_index = adjacent_entry->adj_end_right;
	}

	assert(split_entry->adj_end_down != 0);
	assert(buddy_entry->adj_start_up != 0);

	/** put buddy allocation on heap */
	availability_heap = &atlas->availablity_heaps[buddy_entry->x_size_class][buddy_entry->y_size_class];
	sol_image_atlas_entry_availability_heap_append(availability_heap, buddy_index, atlas);

	/** mark the bit in availability mask to indicate there is a tile/entry of the split off buddies size available */
	atlas->availability_masks[buddy_entry->x_size_class] |= (1 << buddy_entry->y_size_class);
}

/** NOTE: will always maintain the index of the split location, creating a new entry (and index with it) for the adjacent tile created by splitting
 * this should also only be called on available entries */
static inline void sol_image_atlas_entry_split_vertically(struct sol_image_atlas* atlas, struct sol_image_atlas_entry* split_entry, uint32_t split_index)
{
	struct sol_image_atlas_entry_availability_heap* availability_heap;
	struct sol_image_atlas_entry* buddy_entry;
	struct sol_image_atlas_entry* adjacent_entry;
	uint32_t buddy_index, adjacent_index;
	uint16_t buddy_end_y;

	/** validate entry to split really does look available */
	assert(split_entry->is_available);
	assert(split_entry->is_tile_entry);
	assert(split_entry->identifier == 0);
	assert(split_entry->prev_entry_index == SOL_U32_INVALID);
	assert(split_entry->next_entry_index == SOL_U32_INVALID);

	/** in order to split entry must be larger than the min size */
	assert(split_entry->y_size_class > 0);
	split_entry->y_size_class--;

	/* check the split entries offset is valid for it's size class */
	assert((split_entry->packed_location & (1u << (split_entry->y_size_class * 2 + 1))) == 0);
	assert((split_entry->pixel_location.y & (SOL_IMAGE_ATLAS_MIN_TILE_SIZE << split_entry->y_size_class)) == 0);

	buddy_entry = sol_image_atlas_entry_array_append_ptr(&atlas->entry_array, &buddy_index);

	*buddy_entry = (struct sol_image_atlas_entry)
	{
		.identifier = 0,
		.next_entry_index = SOL_U32_INVALID,
		.prev_entry_index = SOL_U32_INVALID,
		.adj_start_left = 0,/** must be set */
		.adj_start_up = split_index,
		.adj_end_right = split_entry->adj_end_right,
		.adj_end_down  = split_entry->adj_end_down,
		.pixel_location = u16_vec2_add(split_entry->pixel_location, u16_vec2_set(0, SOL_IMAGE_ATLAS_MIN_TILE_SIZE << split_entry->x_size_class)),
		.packed_location = split_entry->packed_location | (1u << (split_entry->x_size_class * 2 + 1)),
		.x_size_class = split_entry->x_size_class,
		.y_size_class = split_entry->y_size_class,
		.array_slice = split_entry->array_slice,
		// .heap_index
		.is_accessor = false,
		.is_tile_entry = true,
		.is_available = true,
	};


	split_entry->adj_end_right = 0;/** must be set */
	split_entry->adj_end_down = buddy_index;

	/** traverse bottom side */
	adjacent_index = buddy_entry->adj_end_down;
	while (adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if (adjacent_entry->adj_start_up != split_index)
		{
			assert(adjacent_entry->pixel_location.x < buddy_entry->pixel_location.x);
			break;
		}
		adjacent_entry->adj_start_up = buddy_index;

		adjacent_index = adjacent_entry->adj_start_left;
	}

	/** traverse right side */
	adjacent_index = buddy_entry->adj_end_right;
	while (adjacent_index)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if (adjacent_entry->pixel_location.y < buddy_entry->pixel_location.y)
		{
			assert(split_entry->adj_end_right == 0);
			split_entry->adj_end_right = adjacent_index;
			break;
		}
		adjacent_entry->adj_start_left = buddy_index;

		adjacent_index = adjacent_entry->adj_start_up;
	}

	/** traverse left side */
	buddy_end_y = buddy_entry->pixel_location.y + (SOL_IMAGE_ATLAS_MIN_TILE_SIZE << buddy_entry->y_size_class);
	adjacent_index = split_entry->adj_start_left;
	while (adjacent_entry)
	{
		adjacent_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, adjacent_index);
		if (adjacent_entry->y_size_class > buddy_entry->y_size_class || adjacent_entry->pixel_location.y == buddy_entry->pixel_location.y)
		{
			assert(buddy_entry->adj_start_left == 0);
			buddy_entry->adj_start_left = adjacent_index;
		}

		if (adjacent_entry->adj_end_right != split_index)
		{
			assert(adjacent_entry->pixel_location.y >= buddy_end_y || adjacent_entry->y_size_class > (split_entry->y_size_class + 1));
			break;
		}

		if (adjacent_entry->pixel_location.y >= buddy_entry->pixel_location.y)
		{
			adjacent_entry->adj_end_right = buddy_index;
		}

		adjacent_index = adjacent_entry->adj_end_down;
	}

	assert(split_entry->adj_end_right != 0);
	assert(buddy_entry->adj_start_left != 0);

	/** put buddy allocation on heap */
	availability_heap = &atlas->availablity_heaps[buddy_entry->x_size_class][buddy_entry->y_size_class];
	sol_image_atlas_entry_availability_heap_append(availability_heap, buddy_index, atlas);

	/** mark the bit in availability mask to indicate there is a tile/entry of the split off buddies size available */
	atlas->availability_masks[buddy_entry->x_size_class] |= (1 << buddy_entry->y_size_class);
}

static inline bool sol_image_atlas_acquire_available_entry_of_size(struct sol_image_atlas* atlas, u16_vec2 size, uint32_t* entry_index_ptr)
{
	struct sol_image_atlas_entry_availability_heap* availability_heap;
	struct sol_image_atlas_entry* entry;
	uint32_t entry_index, x_req, y_req, x_min, y_min, split_min, x, y, split;
	uint16_t availiability_mask;
	bool acquired_available;

	x_req = sol_u32_exp_ge(size.x >> SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT);
	y_req = sol_u32_exp_ge(size.y >> SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT);

	split_min = UINT32_MAX;

	for (x = x_req; x < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; x++)
	{
		availiability_mask = atlas->availability_masks[x] >> y_req;
		if (availiability_mask)
		{
			y = sol_u32_ctz(availiability_mask);
			split = y + x;
			if (split <= split_min)
			{
				split_min = split;
				x_min = x;
				y_min = y;
			}
		}
	}

	if (split_min == UINT32_MAX)
	{
		return false;
	}

	/** y_min was calculated relative to y_req, x_min was not */
	y_min += y_req;

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

	entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry_index);

	/** validate entry really does look available */
	assert(entry->is_available);
	assert(entry->is_tile_entry);
	assert(entry->identifier == 0);
	assert(entry->prev_entry_index == SOL_U32_INVALID);
	assert(entry->next_entry_index == SOL_U32_INVALID);

	assert(entry->x_size_class == x_min);
	assert(entry->y_size_class == y_min);

	while(entry->x_size_class != x_req || entry->y_size_class != y_req)
	{
		/** preferentially split vertically if possible */
		if(entry->y_size_class >= entry->x_size_class && entry->y_size_class != y_req)
		{
			sol_image_atlas_entry_split_vertically(atlas, entry, entry_index);
		}
		else
		{
			assert(entry->x_size_class != x_req);
			sol_image_atlas_entry_split_horizontally(atlas, entry, entry_index);
		}
	}

	return true;
}






struct sol_image_atlas* sol_image_atlas_create(const struct sol_image_atlas_description* description)
{
	uint32_t x_size_class, y_size_class, array_slice, entry_index;
	struct sol_image_atlas* atlas = malloc(sizeof(struct sol_image_atlas));
	struct sol_image_atlas_entry* root_entry;
	struct sol_image_atlas_entry* entry;

	assert(description->image_x_dimension_exponent >= 8 && description->image_x_dimension_exponent <= 16);
	assert(description->image_y_dimension_exponent >= 8 && description->image_y_dimension_exponent <= 16);

	atlas->description = *description;

	sol_vk_timeline_semaphore_initialise(&atlas->timeline_semaphore, atlas->description.device);

	#warning initialise (tracked?) image

	sol_image_atlas_entry_array_initialise(&atlas->entry_array, 1024);
	sol_image_atlas_accessor_queue_initialise(&atlas->accessor_queue, 64);
	struct sol_hash_map_descriptor map_descriptor = {
		.entry_space_exponent_initial = 12,
		.entry_space_exponent_limit = 24,
		.resize_fill_factor = 160,
		.limit_fill_factor = 192,
	};
	sol_image_atlas_map_initialise(&atlas->itentifier_entry_map, map_descriptor, atlas);

	/** NOTE: this will get updated/iterated before being vended, ergo 0 will not be vended for the first 2^64 identifiers, so may be treated as invalid */
	atlas->current_identifier = 0;
	atlas->accessor_active = false;

	root_entry = sol_image_atlas_entry_array_append_ptr(&atlas->entry_array, &atlas->root_entry_index);
	assert(atlas->root_entry_index == 0);

	*root_entry = (struct sol_image_atlas_entry)
	{
		.identifier = 0, /** "invalid" identifier */
		.next_entry_index = atlas->root_entry_index,
		.prev_entry_index = atlas->root_entry_index,
		/** following 2 values being false indicate this entry is the root */
		.is_tile_entry = false,
		.is_accessor = false,
	};


	for(x_size_class = 0; x_size_class < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; x_size_class++)
	{
		atlas->availability_masks[x_size_class] = 0;
		for(y_size_class = 0; y_size_class < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; y_size_class++)
		{
			sol_image_atlas_entry_availability_heap_initialise(&atlas->availablity_heaps[x_size_class][y_size_class], 0);
		}
	}

	/** populate availability for atlas image sized entries */
	x_size_class = atlas->description.image_x_dimension_exponent - SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT;
	y_size_class = atlas->description.image_y_dimension_exponent - SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT;

	for(array_slice = 0; array_slice < atlas->description.image_array_dimension; array_slice++)
	{
		/** for every array slice make an entry filling the slive available */
		entry = sol_image_atlas_entry_array_append_ptr(&atlas->entry_array, &entry_index);
		*entry = (struct sol_image_atlas_entry)
		{
			.identifier = 0, /** "invalid" identifier */
			.next_entry_index = SOL_U32_INVALID,
			.prev_entry_index = SOL_U32_INVALID,
			.adj_start_left     = 0,
			.adj_start_up       = 0,
			.adj_end_right = 0,
			.adj_end_down   = 0,
			.pixel_location = u16_vec2_set(0, 0),
			.packed_location = array_slice << 24,
			.x_size_class = x_size_class,
			.y_size_class = y_size_class,
			.array_slice = array_slice,
			// .heap_index
			// .access_index
			// .is_transient
			.is_tile_entry = true,
			.is_accessor = false,
			.is_available = true,
		};
		/** put the entry in the heap and set availability mask */
		sol_image_atlas_entry_availability_heap_append(&atlas->availablity_heaps[x_size_class][y_size_class], entry_index, atlas);
		atlas->availability_masks[x_size_class] |= 1 << y_size_class;
	}

	return atlas;
}

void sol_image_atlas_destroy(struct sol_image_atlas* atlas)
{
	uint32_t x_size_class, y_size_class;
	struct sol_image_atlas_accessor* accessor;
	struct sol_image_atlas_entry* root_entry;
	uint32_t entry_index;
	struct sol_image_atlas_entry* entry;
	struct sol_image_atlas_entry* threshold_entry;

	/** wait on and then release all accessors */
	while(sol_image_atlas_accessor_queue_dequeue_ptr(&atlas->accessor_queue, &accessor))
	{
		sol_vk_timeline_semaphore_moment_wait(&accessor->complete_moment, atlas->description.device);

		threshold_entry = sol_image_atlas_entry_array_remove_ptr(&atlas->entry_array, accessor->threshold_entry_index);
		assert(threshold_entry->is_accessor);

		sol_image_atlas_entry_remove_from_queue(atlas, threshold_entry);
	}

	/** free all entries in the entry linked list queue */
	root_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, atlas->root_entry_index);
	while(root_entry->next_entry_index != atlas->root_entry_index)
	{
		sol_image_atlas_entry_evict(atlas, root_entry->next_entry_index);
	}
	assert(root_entry->next_entry_index == atlas->root_entry_index);
	assert(root_entry->prev_entry_index == atlas->root_entry_index);
	sol_image_atlas_entry_array_remove(&atlas->entry_array, atlas->root_entry_index);



	x_size_class = atlas->description.image_x_dimension_exponent - SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT;
	y_size_class = atlas->description.image_y_dimension_exponent - SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT;
	assert(sol_image_atlas_entry_availability_heap_count(&atlas->availablity_heaps[x_size_class][y_size_class]) == atlas->description.image_array_dimension);

	while(sol_image_atlas_entry_availability_heap_remove(&atlas->availablity_heaps[x_size_class][y_size_class], &entry_index, atlas))
	{
		entry = sol_image_atlas_entry_array_remove_ptr(&atlas->entry_array, entry_index);
		/** assert contents are as expected */
		assert(entry->x_size_class == x_size_class);
		assert(entry->y_size_class == y_size_class);
		assert(entry->pixel_location.x == 0);
		assert(entry->pixel_location.y == 0);
		assert(entry->next_entry_index == 0);
		assert(entry->prev_entry_index == 0);
		assert(entry->identifier == 0);
		assert(entry->is_tile_entry);
		assert(entry->is_available);
	}


	sol_image_atlas_entry_array_remove(&atlas->entry_array, atlas->root_entry_index);
	assert(sol_image_atlas_entry_array_count(&atlas->entry_array) == 0);


	for(x_size_class = 0; x_size_class < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; x_size_class++)
	{
		for(y_size_class = 0; y_size_class < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; y_size_class++)
		{
			assert(sol_image_atlas_entry_availability_heap_count(&atlas->availablity_heaps[x_size_class][y_size_class]) == 0);
			sol_image_atlas_entry_availability_heap_terminate(&atlas->availablity_heaps[x_size_class][y_size_class]);
		}
	}

	sol_image_atlas_map_terminate(&atlas->itentifier_entry_map);
	sol_image_atlas_accessor_queue_terminate(&atlas->accessor_queue);
	sol_image_atlas_entry_array_terminate(&atlas->entry_array);

	sol_vk_timeline_semaphore_terminate(&atlas->timeline_semaphore, atlas->description.device);

	#warning destroy image

	free(atlas);
}


uint64_t sol_image_atlas_acquire_entry_identifier(struct sol_image_atlas* atlas)
{
	/** copied from sol random */
	atlas->current_identifier = atlas->current_identifier * 0x5851F42D4C957F2Dlu + 0x7A4111AC0FFEE60Dlu;
	return atlas->current_identifier;
}


/** an access marker for writes may be necessary/important in order to track when an entry has finished being written?
 	can PROBABLY avoid injecting "accessor" entries into linked list altogether; instead just keeping accessor index in every entry
 		^ by moving whole sections of a linked list to another upon their "complete" moment passing the inactive list of entries can be easily maintained (their accessor list will remain invalid)
 	also of note:
 */






