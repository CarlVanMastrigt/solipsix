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

#warning prefer to use only 20 bits for accessor index...


enum sol_image_atlas_queue_index_type
{
	SOL_IMAGE_ATLAS_QUEUE_INDEX_TYPE_BASE                = 0,/** index is irrelevant in this case */
	SOL_IMAGE_ATLAS_QUEUE_INDEX_TYPE_ENTRY               = 1,
	SOL_IMAGE_ATLAS_QUEUE_INDEX_TYPE_ACCESS_MARKER_READ  = 2,
	SOL_IMAGE_ATLAS_QUEUE_INDEX_TYPE_ACCESS_MARKER_WRITE = 3,
};

struct sol_image_atlas_queue_index
{
	/** note: this will reference accessors in a queue by index, it IS valid to use fewer bits to store the index than the full 32
		so long as fewer than that many are actually used at once (half that many in this case because checking order with modular arithmetic)
		supporting 10^9 max elements should be enough... */
	uint32_t index:30;// only 20 bits used for access_marker
	uint32_t type:2;/** enum sol_image_atlas_queue_index_type */
};


#define SOL_IMAGE_ATLAS_QUEUE_INDEX_NULL (struct sol_image_atlas_queue_index) {.index = 0x3FFFFFFFu, .type = 0}
bool sol_image_atlas_queue_index_is_null(struct sol_image_atlas_queue_index queue_index)
{
	return (queue_index.type == 0) && (queue_index.index == 0x3FFFFFFFu);
}

/** how best to pack? */
struct sol_image_atlas_entry
{
	/** prev/next indices in linked list of entries my order of use */
	struct sol_image_atlas_queue_index prev;
	struct sol_image_atlas_queue_index next;

	/** can be derived from packed location, but provided her for quick access (may be worth removing id doing so would better size this struct) */
	u16_vec2 entry_location;

	/** z-tile location is in terms of minimum entry pixel dimension (4)
		packed in such a way to order entries by layer first then top left -> bottom right
		i.e. packed like: | array layer (8) | z-tile location (24) |
		this is used to sort/order entries (lower value means better placed) */
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
	uint32_t access_index : 20;
	/** ^ just making sure this matches index held in sol_image_atlas_queue_index (shouldn't matter given queue implementation though) */

	/** must be set appropriately on "creation" this indicates that the contents should NOT be copied when this tile is deframented
		(possibly just immediately free this location instead of deframenting it)
		(and) the location should be discarded/relinquished immediately after accessor release ?
		if request differs from this property then the old location should be treated as different (possibly same goes with size?) */
	uint32_t transient_contents : 1;
};

/** marker in queue of accessors vended, basically needs to be used in a queue */
struct sol_image_atlas_access_marker
{
	struct sol_image_atlas_queue_index prev;
	struct sol_image_atlas_queue_index next;

	// uint8_t access_complete_moment_count;
	// struct sol_vk_timeline_semaphore_moment access_complete_moments[SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT];
	struct sol_vk_timeline_semaphore_moment access_complete_moment;

	#warning entries can back-refernce this by index, possibly good to do this to allow dependencies between accesses?
	/** a more reliable way to accomplish this is to have dedicated loading for */
};

struct sol_image_atlas_map_entry
{
	uint64_t identifier;/** hash map key */
	uint32_t entry_index;/** must be an index because its index is used to navigate the linked list */
	/* wasted space... */
};

#define SOL_ARRAY_TYPE struct sol_image_atlas_entry
#define SOL_ARRAY_FUNCTION_PREFIX sol_image_atlas_entry_array
#define SOL_ARRAY_STRUCT_NAME sol_image_atlas_entry_array
#include "solipsix/data_structures/array.h"

struct sol_image_atlas
{
	struct sol_image_atlas_queue_index first;
	struct sol_image_atlas_queue_index last;

	struct sol_image_atlas_entry_array entries;
};






