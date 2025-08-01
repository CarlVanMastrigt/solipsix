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




#define SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT 2
#define SOL_IMAGE_ATLAS_MIN_TILE_SIZE 4
#define SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT 13
/** [4,16384] inclusive ^^ */

struct sol_image_atlas_entry
{
	/** hash map key; used for hash map lookup upon defragmentation eviction */
	uint64_t identifier;

//== 64 ==

	/** prev/next indices in linked list of entries my order of use, 16M is more than enough entries
	 * NOTE: 0 is reserved for the dummy start index*/
	uint32_t prev_entry_index;
	uint32_t next_entry_index;

//== 64 ==

	uint32_t top_left_adjacent_vertical;
	uint32_t top_left_adjacent_horizontal;
//== 64 ==
	uint32_t bottom_right_adjacent_vertical;
	uint32_t hottom_adjacent_horizontal;

//== 64 ==

	/** can be derived from packed location, but provided her for quick access (may be worth removing id doing so would better size this struct) */
	u16_vec2 entry_location;

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
	uint64_t access_index : 20;
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
	uint16_t availibility_masks[SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT];


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
	/** the root entry should not be removed in this way */
	assert(entry->is_accessor || entry->is_tile_entry);
	struct sol_image_atlas_entry* prev_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry->prev_entry_index);
	struct sol_image_atlas_entry* next_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, entry->next_entry_index);

	prev_entry->next_entry_index = entry->next_entry_index;
	next_entry->prev_entry_index = entry->prev_entry_index;
}





struct sol_image_atlas* sol_image_atlas_create(const struct sol_image_atlas_description* description)
{
	uint32_t x_dim, y_dim, i, entry_index;
	struct sol_image_atlas* atlas = malloc(sizeof(struct sol_image_atlas));
	struct sol_image_atlas_entry* root_entry;
	struct sol_image_atlas_entry* entry;

	assert(description->image_x_dimension_exponent >= 8 && description->image_x_dimension_exponent<=16);
	assert(description->image_y_dimension_exponent >= 8 && description->image_y_dimension_exponent<=16);

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


	for(x_dim = 0; x_dim < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; x_dim++)
	{
		atlas->availibility_masks[x_dim] = 0;
		for(y_dim = 0; y_dim < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; y_dim++)
		{
			sol_image_atlas_entry_availability_heap_initialise(&atlas->availablity_heaps[x_dim][y_dim], 0);
		}
	}

	/** populate availability for atlas image sized entries */
	x_dim = atlas->description.image_x_dimension_exponent - SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT;
	y_dim = atlas->description.image_y_dimension_exponent - SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT;

	for(i = 0; i < atlas->description.image_array_dimension; i++)
	{
		entry = sol_image_atlas_entry_array_append_ptr(&atlas->entry_array, &entry_index);
	}

	return atlas;
}

void sol_image_atlas_destroy(struct sol_image_atlas* atlas)
{
	uint32_t x_dim, y_dim;
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

	/** free all entries in the enytry linked list queue */
	root_entry = sol_image_atlas_entry_array_access_entry(&atlas->entry_array, atlas->root_entry_index);
	while(root_entry->next_entry_index != atlas->root_entry_index)
	{
		entry = sol_image_atlas_entry_array_remove_ptr(&atlas->entry_array, root_entry->next_entry_index);
		assert(entry->is_tile_entry);

		#warning make entry available: i.e. evict it, remove from map, queue, &c. put in available state
	}
	assert(root_entry->next_entry_index == atlas->root_entry_index);
	assert(root_entry->prev_entry_index == atlas->root_entry_index);
	sol_image_atlas_entry_array_remove(&atlas->entry_array, atlas->root_entry_index);



	x_dim = atlas->description.image_x_dimension_exponent - SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT;
	y_dim = atlas->description.image_y_dimension_exponent - SOL_IMAGE_ATLAS_MIN_TILE_SIZE_EXPONENT;
	assert(sol_image_atlas_entry_availability_heap_count(&atlas->availablity_heaps[x_dim][y_dim]) == atlas->description.image_array_dimension);

	while(sol_image_atlas_entry_availability_heap_remove(&atlas->availablity_heaps[x_dim][y_dim], &entry_index, atlas))
	{
		entry = sol_image_atlas_entry_array_remove_ptr(&atlas->entry_array, entry_index);
		/** assert contents are as expected */
		assert(entry->x_size_class == x_dim);
		assert(entry->y_size_class == y_dim);
		assert(entry->entry_location.x == 0);
		assert(entry->entry_location.y == 0);
		assert(entry->next_entry_index == SOL_U32_INVALID);
		assert(entry->prev_entry_index == SOL_U32_INVALID);
		assert(entry->identifier == 0);
		assert(entry->is_tile_entry);
		assert(entry->is_available);
	}

	sol_image_atlas_entry_array_remove(&atlas->entry_array, atlas->root_entry_index);
	assert(sol_image_atlas_entry_array_count(&atlas->entry_array) == 0);

	for(x_dim = 0; x_dim < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; x_dim++)
	{
		for(y_dim = 0; y_dim < SOL_IMAGE_ATLAS_AVAILIABILITY_HEAP_COUNT; y_dim++)
		{
			assert(sol_image_atlas_entry_availability_heap_count(&atlas->availablity_heaps[x_dim][y_dim]) == 0);
			sol_image_atlas_entry_availability_heap_terminate(&atlas->availablity_heaps[x_dim][y_dim]);
		}
	}

	sol_image_atlas_map_terminate(&atlas->itentifier_entry_map);
	sol_image_atlas_accessor_queue_terminate(&atlas->accessor_queue);
	sol_image_atlas_entry_array_terminate(&atlas->entry_array);

	sol_vk_timeline_semaphore_terminate(&atlas->timeline_semaphore, atlas->description.device);

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






