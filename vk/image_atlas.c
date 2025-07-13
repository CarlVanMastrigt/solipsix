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


#warning need to figure out if having reference to entries in accessor array is actually better (in terms of perf and size)
/** below requires a fuck ton of memory accesses that are almost guaranteed to be cache misses (e.g. prev/next linked list changes will likely cause cache misses) */

struct sol_image_atlas_queue_index
{
	#warning index needing to not be a full u32 is massively problematic; sol_queue requires u32 index to work properly; may need to add a mask to queue to permit this
	uint32_t index:31;
	uint32_t is_accessor_marker:1;
};

// #define SOL_IMAGE_ATLAS_QUEUE_INDEX_NULL (struct sol_image_atlas_queue_index) {.index = 0x7FFFFFFF, .is_accessor_marker = 0}
// bool sol_image_atlas_queue_index_is_null(struct sol_image_atlas_queue_index)
// {

// }

/** how best to pack? */
struct sol_image_atlas_entry
{
	#warning need to put identifier for whether this is an accessor marker along with prev/next index (really pack type with index...?)
	/** prev/next indices in linked list of entries my order of use */
	struct sol_image_atlas_queue_index prev;
	struct sol_image_atlas_queue_index next;

	/** can be derived from packed location, but provided her for quick access (may be worth removing id doing so would better size this struct) */
	u16_vec2 entry_location;

	/** z-tile location is in terms of minimum entry pixel dimension (4)
		packed in such a way to order entries by layer first then top left -> bottom right
		i.e. packed like: | array layer (8) | z-tile location (24) | */
	uint32_t packed_location;

	/** size class is power of 2 times entry pixel dimension (4)
		4 * 2 ^ (0:15) is more than enough (4 << 15 = 128k, is larger than max texture size)*/
	uint32_t x_size_class : 4;
	uint32_t y_size_class : 4;

	/** index in heap of size class (x,y)
		only applicable when free, so VERY inlikely 16m are available */
	uint32_t heap_index : 24;

#warning for the below we will potentially reference items in the accessor queue that have expired: as such queue should properly assert index is withing (wrapped) range
	/** index in queue of accessors (does need to be a u32 for compatibility)
		used to ensure an entry is only moved forward in the least recently used queue when necessary and to a point that correctly reflects its usage
		(should be put just after the accessor marker)  */
	uint32_t accessor_index;
};

/** marker in queue of accessors vended, basically needs to be used in a queue */
struct sol_image_atlas_accessor_marker
{
	struct sol_image_atlas_queue_index prev;
	struct sol_image_atlas_queue_index next;

	/** active then are still waiting on accessor to be released, so cannot wait on moments */
	bool active;

	uint8_t access_complete_moment_count;
	struct sol_vk_timeline_semaphore_moment access_complete_moments[SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT];
};

struct sol_image_atlas_map_entry
{
	uint64_t identifier;/** hash map key */
	uint32_t entry_index;
	/* wasted space... */
};

struct sol_image_atlas
{
	mtx_t mutex;
	bool multithreaded;

	// in flight accessors
	uint64_t active_accessor_mask;

	struct sol_image_atlas_entry* entries;
};






