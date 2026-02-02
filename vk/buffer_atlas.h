/**
Copyright 2026 Carl van Mastrigt

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

#pragma once

#include <inttypes.h>
#include <vulkan/vulkan.h>

struct cvm_vk_device;
struct sol_vk_buffer_atlas;
struct sol_vk_timeline_semaphore_moment;


enum sol_buffer_atlas_result
{
    SOL_BUFFER_TABLE_FAIL_FULL, /** there is no remaining space, need to wait for space to be made */
    SOL_BUFFER_TABLE_FAIL_MAP_FULL, /** hash map is full, need to wait for space to be made */
    SOL_BUFFER_TABLE_FAIL_ABSENT, /** if using find rather than obtain this will be returned if the entry is not already present */
    SOL_BUFFER_TABLE_FAIL_NOT_INITIALISED, /** this resource will be initialised by a different access slot, but has not yet become available(visible) from the accessor slot used */
    SOL_BUFFER_TABLE_SUCCESS_FOUND, /** existing entry found; no need to initalise */
    SOL_BUFFER_TABLE_SUCCESS_INSERTED, /** existing entry not found; space was made but contents must be initialised */
};

/** TODO: may wish to separate the moment where an access ranges regions are init (and thus available to other access ranges) from the moment an accessrange last refrences/uses that resource, this would simply mean providing another function to signal that moment */


/** re-writable resources require only one user at a time, same as obtaining a non-extant entry */
// #define SOL_BUFFER_TABLE_OBTAIN_FLAG_WRITE  0x00000001u

struct sol_vk_buffer_atlas_create_information
{
    VkBufferCreateInfo buffer_create_info;

    VkMemoryPropertyFlags required_properties;
    VkMemoryPropertyFlags desired_properties;

    VkDeviceSize base_allocation_size;

    /** slots should be externally referenced/managed, 
     * e.g. with an enum over uses: main render, compute, preload &c. 
     * only 255 available because we want to fit that data in (effectively) a u8 while having an invalid identifier */
    uint8_t accessor_slot_count;

    /** at the cost of locking a mutex each time; the buffer table can automatically manage multithreaded access to the table, this includes mltithreaded access within a single accessor
     * it is still required to perform external synchronization to guarantee ordering of per-accessor management (i.e. acquire, release & get_wait_moment with the same access slot) */
    bool multithreaded;
};

/** NOTE: due to being an automatically managed system: there is no guarantee that an entry that did exist previously will still exist; 
 * systems that use the buffer table must account for this */


/** note: buffer_create_info->size MUST be a multiple of base_allocation_size */
struct sol_vk_buffer_atlas* sol_vk_buffer_atlas_create(struct cvm_vk_device* device, const struct sol_vk_buffer_atlas_create_information* create_information);

void sol_vk_buffer_atlas_destroy(struct sol_vk_buffer_atlas* table, struct cvm_vk_device* device);


/** begin of range where reads and writes are able to be recorded for this buffer atlas (i.e. allowing access to its contents) 
 * note allows reading of immutable resources from other slots if they can be guaranteed to be initialised */
void sol_vk_buffer_atlas_access_range_begin(struct sol_vk_buffer_atlas* table, uint32_t accessor_slot, struct cvm_vk_device *device);

/** must pass in the moment where all reads and writes for this access range are known to have completed for the given slot 
 * TODO: need to investigate whether accesses from other slots necessitate knowledge of all prior write methods, or whether the CPU operation is enough for visibility to be guaranteed */
void sol_vk_buffer_atlas_access_range_end(struct sol_vk_buffer_atlas* table, uint32_t accessor_slot, const struct sol_vk_timeline_semaphore_moment* last_use_moment);

/** this can be used to access the moment where the most recent set of read and writes are known to have completed for the given slot
 * must be called in between begin and end, the returned moment is the most recent access range update moment
 * note: will return false on first use
 * TODO: could make this return a sequence by requiring it be called in a while loop 
 * this isn't strictly necessary if access begin-end ranges can guarantee ordering externally (which SHOULD be the normal use pattern)
 * (i.e. all work done in a range must be synchronised to preceed subsequent begin) */
bool sol_vk_buffer_atlas_access_range_wait_moment(struct sol_vk_buffer_atlas* table, uint32_t accessor_slot, struct sol_vk_timeline_semaphore_moment* wait_moment);



/** acquire a unique identifier for accessing/indexing entries in the table
 * note: because a buffer can be used by multiple command buffers at once (even across queues) the concept of a transient allocation cannot be applicable the same way it is to the image atlas 
 * note: a mutable region can only be accessed via one slot */
uint64_t sol_vk_buffer_atlas_generate_region_identifier(struct sol_vk_buffer_atlas* table);





/** `obtain` and `find` must only be called between `acquire` and `release` of the slot used */

/** this will get the region (really just the offset) in the buffer associated with an identifier (if it is present) 
 * size may be null, but onus is on caller to ensure size actually matches size that was provided on setup, so if non-null size will be set to the size of the region for validation */
enum sol_buffer_atlas_result sol_vk_buffer_atlas_find_identified_region(struct sol_vk_buffer_atlas* table, uint64_t region_identifier, uint32_t accessor_slot, VkDeviceSize* entry_offset, VkDeviceSize* size);

/** if this function fails to find an extant region associated the identifier, the function will (if possible) allocate backing memory (a region) to accomodate the requested size and return it
 * the onus then falls on the caller to ensure this memory is set up before the access scope's completion/release moment is signalled (obviously it must also be set up before bing used within the region too)
 * onus is also on the caller to ensure any other resources required to set up the contents of this region (e.g. staging) can be made avilable BEFORE calling obtain
 * note: if extant will assert size matches */
enum sol_buffer_atlas_result sol_vk_buffer_atlas_obtain_identified_region(struct sol_vk_buffer_atlas* table, uint64_t region_identifier, uint32_t accessor_slot, VkDeviceSize size, VkDeviceSize* entry_offset);


/** allocates a region without using an identifier, that will only be kept as long as the range is active (until the end range moment has been signalled)
 * can only fail if there is insufficient space */
enum sol_buffer_atlas_result sol_vk_buffer_atlas_obtain_transient_region(struct sol_vk_buffer_atlas* table, uint32_t accessor_slot, VkDeviceSize size, VkDeviceSize* entry_offset);

#warning add utility/helper function to perform copies that considers/acknowledges the buffer table may be host visible memory -- wrapper type?

struct sol_vk_buffer* sol_vk_buffer_atlas_access_buffer(struct sol_vk_buffer_atlas* table);
