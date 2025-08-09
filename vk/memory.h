/**
Copyright 2021,2022 Carl van Mastrigt

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

#ifndef solipsix_H
#include "solipsix.h"
#endif


#ifndef CVM_VK_MEMORY_H
#define CVM_VK_MEMORY_H

#warning NIX THIS

typedef struct cvm_vk_staging_buffer_region
{
    uint32_t start;
    uint32_t end;
}
cvm_vk_staging_buffer_region;

typedef struct cvm_vk_staging_buffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;

    VkBufferUsageFlags usage;

    uint32_t total_space;
    uint32_t alignment_size_factor;

    cvm_vk_staging_buffer_region active_region;///region selected for this frame
    atomic_uint_fast32_t active_offset;///need to test atomic version isn't (significatly) slower than non-atomic for single threaded applications

    cvm_vk_staging_buffer_region * acquired_regions;///one per swapchain image
    cvm_vk_staging_buffer_region * available_regions;///will always be less than swapchain image count of these
    uint32_t available_region_count;

    uint8_t * mapping;
    bool mapping_coherent;
}
cvm_vk_staging_buffer;

void cvm_vk_staging_buffer_create(cvm_vk_staging_buffer * sb,VkBufferUsageFlags usage);
void cvm_vk_staging_buffer_update(cvm_vk_staging_buffer * sb,uint32_t space_per_frame,uint32_t frame_count);
void cvm_vk_staging_buffer_destroy(cvm_vk_staging_buffer * sb);

uint32_t cvm_vk_staging_buffer_get_rounded_allocation_size(cvm_vk_staging_buffer * sb,uint32_t allocation_size);

///these are not thread safe, if its desirable to make staging thread safe a decent amount of work is necessary, perhaps involving pre-staging data, with copy instructions updated as necessary, which isnt the worst idea... especially if the total buffer size bigger than the pre-staging region...
void cvm_vk_staging_buffer_begin(cvm_vk_staging_buffer * sb);
void cvm_vk_staging_buffer_end(cvm_vk_staging_buffer * sb,uint32_t frame_index);

void * cvm_vk_staging_buffer_acquire_space(cvm_vk_staging_buffer * sb,uint32_t allocation_size,VkDeviceSize * acquired_offset);

void cvm_vk_staging_buffer_release_space(cvm_vk_staging_buffer * sb,uint32_t frame_index);





///16 bytes, 4 to a cacheline
typedef struct cvm_vk_temporary_buffer_allocation_data
{
    uint32_t prev;
    uint32_t next;

    uint64_t offset:36;/// actual offset = base_size<<offset_factor
    uint64_t heap_index:22;///4M frees supported should be more than enough
    uint64_t size_factor:5;///32 different sizes is definitely enough,
    uint64_t available:1;///does not have contents, can be retrieved by new allocation or recombined into
    ///get bit for "is_right_buffer" by 1 & (offset >> size_factor) during recombination calculations
}
cvm_vk_temporary_buffer_allocation_data;


/// could use more complicated allocator that can recombine sections of arbitrary size with no power of 2 superstructure, may work reasonably well, especially if memory is grouped by expected lifetime
///     ^ this will definitely require a defragger!
///     ^ use expected lifetime to prioritise ordering of sections? will that happen naturally as chunks are allocated/deallocated
///     ^ profile an implementation and only use it if its substantially better than the extant implementation in terms of alloc/dealloc time
///     ^ require all chunks to meet the largest supported types alignment requirements


///cannot use generic heap for this as a bunch of specialised things are done with this heap (randomised deletions, and tracked indices to support that)
typedef struct cvm_vk_available_temporary_allocation_heap
{
    uint32_t * heap;///indices of allocations
    uint32_t space;
    uint32_t count;
}
cvm_vk_available_temporary_allocation_heap;

struct cvm_vk_managed_buffer
{
    atomic_uint_fast32_t allocation_management_spinlock;

    VkBuffer buffer;
    VkDeviceMemory memory;

    ///VkBufferUsageFlags usage;/// for use if buffer ends up needing recreation

    uint64_t total_space;///actually DO need to track total buffer size so that buffer can be cleaned (all permanent and temporary allocations removed in one go)
    uint64_t permanent_offset;/// taken from end of buffer
    uint32_t temporary_offset;/// taken from start of buffer, is in terms of base offset


    cvm_vk_temporary_buffer_allocation_data * temporary_allocation_data;
    uint32_t temporary_allocation_data_space;

    uint32_t first_unused_temporary_allocation;///singly linked list of allocations to assign space to
    uint32_t unused_temporary_allocation_count;

    uint32_t last_used_allocation;///used for setting left/right of new allocations

    ///for various reasons can't/shouldn't use generic heap data structure here
    cvm_vk_available_temporary_allocation_heap available_temporary_allocations[32];
    uint32_t available_temporary_allocation_bitmask;
    uint32_t base_temporary_allocation_size_factor;



    uint8_t * mapping;///can copy data in directly (no need for staging) on UMA systems, also operates as flag as to whether staging is necessary and how to handle release/free operations
    bool mapping_coherent;///indicated whether flushing ranges is necessary when this buffer is mapped

    cvm_vk_staging_buffer * staging_buffer;///for when mapping is not available

    ///copies can/should get built up withing frame at same time as dismissal list (create vs destroy essentially), dismissal list just needs to hold onto data over time
    atomic_uint_fast32_t copy_spinlock;
    uint16_t copy_update_counter;
    struct cvm_vk_buffer_copy_stack pending_copies;
    struct cvm_vk_buffer_barrier_stack copy_release_barriers;
    uint32_t copy_queue_bitmask;///queues that above copy will affect
};

/**
from the spec:

If memory dependencies are correctly expressed between uses of such a resource between two
queues in different families, but no ownership transfer is defined, the contents of that resource are
undefined for any read accesses performed by the second queue family.

Note
If an application does not need the contents of a resource to remain valid when
transferring from one queue family to another, then the ownership transfer
SHOULD be skipped.


fuck. yes.   confirmation
*/

void cvm_vk_managed_buffer_create(cvm_vk_managed_buffer * mb,uint32_t buffer_size,uint32_t min_size_factor,VkBufferUsageFlags usage,bool host_visible);
///dismissal_cycle_count should be initialised with swapchain image count in the simple graphics based paradigm, should essentially be used to ensure a frame delay sufficient to prevent re-use of "deleted" resources that are still in flight
void cvm_vk_managed_buffer_destroy(cvm_vk_managed_buffer * mb);

bool cvm_vk_managed_buffer_acquire_temporary_allocation(cvm_vk_managed_buffer * mb,uint64_t size,uint32_t * allocation_index,uint64_t * allocation_offset);
void cvm_vk_managed_buffer_dismiss_temporary_allocation(cvm_vk_managed_buffer * mb,uint32_t allocation_index);///schedule for deletion, internally synchronised
void cvm_vk_managed_buffer_release_temporary_allocation(cvm_vk_managed_buffer * mb,uint32_t allocation_index);///delete immediately, must be correctly synchronised

uint64_t cvm_vk_managed_buffer_acquire_permanent_allocation(cvm_vk_managed_buffer * mb,uint64_t size,uint64_t alignment);///cannot be released, exists until entire buffer is cleared or deleted

/// FUCK following may be a reason to separate out copy data -- this effectively ties a managed buffer to a single batch (and module with it) by submitting ALL buffer copies to a particular batch!
void cvm_vk_managed_buffer_submit_all_batch_copy_actions(cvm_vk_managed_buffer * mb,cvm_vk_module_batch * batch);///includes necessary barriers
/// for the love of god only call this once per buffer per frame, transfer and graphics cb necessary for case of QFOT (process acquisitions for previous frames releases)

void cvm_vk_managed_buffer_bind_as_vertex(VkCommandBuffer cmd_buf,cvm_vk_managed_buffer * mb,uint32_t binding);
void cvm_vk_managed_buffer_bind_as_index(VkCommandBuffer cmd_buf,cvm_vk_managed_buffer * mb,VkIndexType type);

///low priority versions
void * cvm_vk_managed_buffer_acquire_temporary_allocation_with_mapping(cvm_vk_managed_buffer * mb,uint64_t size,
            queue_transfer_synchronization_data * transfer_data,VkPipelineStageFlags2 use_stage_mask,VkAccessFlags2 use_access_mask,
            uint32_t * allocation_index,uint64_t * allocation_offset,uint16_t * availability_token,uint32_t dst_queue_id);///perhaps move some/all of this to a unified structure

void * cvm_vk_managed_buffer_acquire_permanent_allocation_with_mapping(cvm_vk_managed_buffer * mb,uint64_t size,uint64_t alignment,
            queue_transfer_synchronization_data * transfer_data,VkPipelineStageFlags2 use_stage_mask,VkAccessFlags2 use_access_mask,
            uint64_t * allocation_offset,uint16_t * availability_token,uint32_t dst_queue_id);

/// need high priority and low priority variants of above? probably wouldn't hurt...
/// high priority would just copy inline immediately, taking command buffer or payload as parameters instead of queue_transfer_synchronization_data (which is essentially responsible for managing that copy and associated QFOT/sync)



#define SOL_STACK_ENTRY_TYPE uint32_t
#define SOL_STACK_FUNCTION_PREFIX u32_stack
#define SOL_STACK_STRUCT_NAME u32_stack
#include "data_structures/stack.h"

#warning remove above!

///splitting this in particular out from managed buffer allows different types of work to SHARE space in a managed buffer! (specifically when those types of work have differing "frames in flight")
struct cvm_vk_managed_buffer_dismissal_list
{
    ///elements scheduled for release, need to wait to make available until they are *definitely* deleted
    atomic_uint_fast32_t spinlock;
    ///should probably put all spinlocks in this file (and all uses of them) behind a "multithreaded" define guard

    struct u32_stack * allocation_indices;
    uint32_t frame_count;///same as swapchain image count used to initialise this
    uint32_t frame_index;

    ///make memory available again when its KNOWN to be unused, use swapchain image availability (same as other buffer management strategies)
    ///hmmm, as long as deletion(frame barrier) waits on transfer semaphore then this will also protect against same-frame copy and re-use
};


void cvm_vk_managed_buffer_dismissal_list_create(cvm_vk_managed_buffer_dismissal_list * dismissal_list);
void cvm_vk_managed_buffer_dismissal_list_update(cvm_vk_managed_buffer_dismissal_list * dismissal_list,uint32_t frame_count);///frame count should usually be swapchain image count
void cvm_vk_managed_buffer_dismissal_list_destroy(cvm_vk_managed_buffer_dismissal_list * dismissal_list);

void cvm_vk_managed_buffer_dismissal_list_begin_frame(cvm_vk_managed_buffer_dismissal_list * dismissal_list,uint32_t frame_index);
void cvm_vk_managed_buffer_dismissal_list_end_frame(cvm_vk_managed_buffer_dismissal_list * dismissal_list);
void cvm_vk_managed_buffer_dismissal_list_release_frame(cvm_vk_managed_buffer_dismissal_list * dismissal_list,cvm_vk_managed_buffer * managed_buffer,uint32_t frame_index);

void cvm_vk_managed_buffer_dismissal_list_enqueue_allocation(cvm_vk_managed_buffer_dismissal_list * dismissal_list,uint32_t allocation_index);///schedule for deletion with simple cycling of frames












///rename as transient upload buffer as transient workspace (GPU only) buffer would also be a useful paradigm to have access to
typedef struct cvm_vk_transient_buffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;

    VkBufferUsageFlags usage;

    uint32_t total_space;
    uint32_t space_per_frame;
    uint32_t max_offset;///fence, should be updated before multithreading becomes applicable this frame
    uint32_t alignment_size_factor;
    atomic_uint_fast32_t current_offset;
    ///need to test atomic version isn't (significatly) slower than non-atomic

    uint8_t * mapping;
    bool mapping_coherent;
}
cvm_vk_transient_buffer;

void cvm_vk_transient_buffer_create(cvm_vk_transient_buffer * tb,VkBufferUsageFlags usage);
void cvm_vk_transient_buffer_update(cvm_vk_transient_buffer * tb,uint32_t space_per_frame,uint32_t frame_count);
void cvm_vk_transient_buffer_destroy(cvm_vk_transient_buffer * tb);

uint32_t cvm_vk_transient_buffer_get_rounded_allocation_size(cvm_vk_transient_buffer * tb,uint32_t allocation_size,uint32_t alignment);
///having fixed locations wont work if non PO2 alignments are required, though this could be fixed by binding at a set offset

void cvm_vk_transient_buffer_begin(cvm_vk_transient_buffer * tb,uint32_t frame_index);
void cvm_vk_transient_buffer_end(cvm_vk_transient_buffer * tb);

///alignment 0 indicates to use base buffer alignment, likely because it will be bound with this offset rather than at 0 and indexed into
void * cvm_vk_transient_buffer_get_allocation(cvm_vk_transient_buffer * tb,uint32_t allocation_size,uint32_t alignment,VkDeviceSize * acquired_offset);

void cvm_vk_transient_buffer_bind_as_vertex(VkCommandBuffer cmd_buf,cvm_vk_transient_buffer * tb,uint32_t binding,VkDeviceSize offset);
void cvm_vk_transient_buffer_bind_as_index(VkCommandBuffer cmd_buf,cvm_vk_transient_buffer * tb,VkIndexType type,VkDeviceSize offset);




#endif

