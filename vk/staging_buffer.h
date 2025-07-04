/**
Copyright 2024 Carl van Mastrigt

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

#include <stdbool.h>
#include <inttypes.h>
#include <threads.h>
#include <vulkan/vulkan.h>

#include "data_structures/queue.h"
#include "vk/timeline_semaphore.h"
#include "cvm_vk.h"

#warning separate out backed buffer from cvm_vk

struct cvm_vk_device;

/// for when its desirable to support a simple staging buffer or a more complex staging manager backing
struct sol_vk_staging_buffer_allocation
{
    VkDeviceSize acquired_offset;/** byte offset in staging buffer, used for submission to vulkan functions */
    char* mapping;/** mapped location in staging buffer to write to, has already been offset */
    uint32_t segment_index;/** for internal use */
    bool flushed;/** internal debugging use */
};

#define SOL_STAGING_BUFFER_MAX_RELEASE_MOMENTS 8

struct sol_vk_staging_buffer_segment
{
    struct sol_vk_timeline_semaphore_moment release_moments[SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT];
    uint32_t release_moment_count;
    bool release_moments_set;

    VkDeviceSize offset;
    VkDeviceSize size;
};

SOL_QUEUE(struct sol_vk_staging_buffer_segment, sol_vk_staging_buffer_segment_queue, sol_vk_staging_buffer_segment_queue)

/** TODO: should have a way to wait on CPU-side if the allocation was written GPU-side */

struct sol_vk_staging_buffer
{
    struct sol_vk_backed_buffer backing;

    bool threads_waiting_on_semaphore_setup;

    bool terminating;/** for debug */

    VkDeviceSize buffer_size;/** also present in backing description, but more convenient to access like this */
    VkDeviceSize alignment;
    VkDeviceSize reserved_high_priority_space;

    VkDeviceSize current_offset;
    VkDeviceSize remaining_space;

    mtx_t access_mutex;
    cnd_t setup_stall_condition;

    struct sol_vk_staging_buffer_segment_queue segment_queue;
};

VkResult sol_vk_staging_buffer_initialise(struct sol_vk_staging_buffer* staging_buffer, const struct cvm_vk_device* device, VkBufferUsageFlags usage, VkDeviceSize buffer_size, VkDeviceSize reserved_high_priority_space);
void sol_vk_staging_buffer_terminate(struct sol_vk_staging_buffer* staging_buffer, const struct cvm_vk_device* device);

/** TODO: at present this will stall (mutex lock) until space is made, this design can (and should) be improved to allow task system integration (take/signal a sync primitive)*/
struct sol_vk_staging_buffer_allocation sol_vk_staging_buffer_allocation_acquire(struct sol_vk_staging_buffer* staging_buffer, const struct cvm_vk_device* device, VkDeviceSize requested_space, bool high_priority);

void sol_vk_staging_buffer_allocation_flush_range(const struct sol_vk_staging_buffer* staging_buffer, const struct cvm_vk_device* device, struct sol_vk_staging_buffer_allocation* allocation, VkDeviceSize relative_offset, VkDeviceSize size);

/** the release moments are all the moments required to wait on for this allocation/segment to no longer be in use (i.e. moments after all separate uses) */
void sol_vk_staging_buffer_allocation_release(struct sol_vk_staging_buffer* staging_buffer, struct sol_vk_staging_buffer_allocation* allocation, struct sol_vk_timeline_semaphore_moment* release_moments, uint32_t release_moment_count);

VkDeviceSize sol_vk_staging_buffer_allocation_align_offset(const struct sol_vk_staging_buffer* staging_buffer, VkDeviceSize offset);

