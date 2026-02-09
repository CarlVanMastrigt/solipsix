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

#include "cvm_vk.h"

#include "vk/staging_buffer.h"

VkResult sol_vk_staging_buffer_initialise(struct sol_vk_staging_buffer* staging_buffer, struct cvm_vk_device* device, VkBufferUsageFlags usage, VkDeviceSize buffer_size)
{
    VkResult result;
    struct sol_vk_buffer backing_buffer;

    const VkDeviceSize alignment = sol_vk_buffer_alignment_requirements(device, usage);

    buffer_size = cvm_vk_align(buffer_size, alignment);/// round to multiple of alignment

    const VkBufferCreateInfo buffer_create_info =
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = buffer_size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
    };

    result = sol_vk_buffer_initialise(&backing_buffer, device, &buffer_create_info, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if(result == VK_SUCCESS)
    {
        *staging_buffer = (struct sol_vk_staging_buffer)
        {
            .backing_buffer = backing_buffer,
            .threads_waiting_on_semaphore_setup = false,
            .terminating = false,

            .buffer_size = buffer_size,
            .alignment = alignment,

            .current_offset = 0,
            .remaining_space = buffer_size,
        };

        mtx_init(&staging_buffer->access_mutex, mtx_plain);
        cnd_init(&staging_buffer->setup_stall_condition);

        sol_vk_staging_buffer_segment_queue_initialise(&staging_buffer->segment_queue, 32);
        sol_vk_timeline_semaphore_moment_queue_initialise(&staging_buffer->release_moment_queue, 32);
    }

    return result;
}

/// cannot acquire allocations after this has been called
void sol_vk_staging_buffer_terminate(struct sol_vk_staging_buffer* staging_buffer, struct cvm_vk_device* device)
{
    struct sol_vk_staging_buffer_segment * oldest_active_segment;
    struct sol_vk_timeline_semaphore_moment release_moments[SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT];
    uint32_t release_moment_count;

    mtx_lock(&staging_buffer->access_mutex);
    staging_buffer->terminating = true;
    /** wait for all memory uses to complete, must be externally synchronised to ensure nothing is still attempting to allocate from staging buffer at this point */
    while(sol_vk_staging_buffer_segment_queue_dequeue_ptr(&staging_buffer->segment_queue, &oldest_active_segment))
    {
        if(oldest_active_segment->retain_count)
        {
            /** this segment has been reserved but not completed, need to wait for release moments(condition) to be set */
            staging_buffer->threads_waiting_on_semaphore_setup = true;
            cnd_wait(&staging_buffer->setup_stall_condition, &staging_buffer->access_mutex);
        }
        else
        {
            assert(oldest_active_segment->size);/** segment must occupy space */
            assert(oldest_active_segment->release_moment_count <= SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT);
            assert(sol_vk_timeline_semaphore_moment_queue_index_is_front(&staging_buffer->release_moment_queue, oldest_active_segment->release_moment_queue_index));

            release_moment_count = sol_vk_timeline_semaphore_moment_queue_copy_many_front(&staging_buffer->release_moment_queue, release_moments, oldest_active_segment->release_moment_count);
            assert(release_moment_count == oldest_active_segment->release_moment_count);

            sol_vk_timeline_semaphore_moment_wait_multiple(release_moments, release_moment_count, true, device);

            /** remove all reserved moments from queue, note: this includes reserved byt unused moments */
            sol_vk_timeline_semaphore_moment_queue_prune_many_front(&staging_buffer->release_moment_queue, oldest_active_segment->reserved_moment_count);

            /** this checks that the start of this segment is the end of the available space */
            assert(oldest_active_segment->offset == (staging_buffer->current_offset + staging_buffer->remaining_space) % staging_buffer->buffer_size);/** out of order free for some reason, or offset mismatch */

            staging_buffer->remaining_space += oldest_active_segment->size;/** relinquish this segments space */
        }
    }

    mtx_unlock(&staging_buffer->access_mutex);

    assert(staging_buffer->remaining_space == staging_buffer->buffer_size);

    mtx_destroy(&staging_buffer->access_mutex);
    cnd_destroy(&staging_buffer->setup_stall_condition);
    sol_vk_timeline_semaphore_moment_queue_terminate(&staging_buffer->release_moment_queue);
    sol_vk_staging_buffer_segment_queue_terminate(&staging_buffer->segment_queue);

    sol_vk_buffer_terminate(&staging_buffer->backing_buffer, device);
}

/** MUST only be called inside mutex locked region */
static inline void sol_vk_staging_buffer_prune_allocations(struct sol_vk_staging_buffer* staging_buffer, struct cvm_vk_device* device)
{
    struct sol_vk_staging_buffer_segment * oldest_active_segment;
    struct sol_vk_timeline_semaphore_moment release_moments[SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT];
    uint32_t release_moment_count;

    while(sol_vk_staging_buffer_segment_queue_access_front(&staging_buffer->segment_queue, &oldest_active_segment))
    {
        if(oldest_active_segment->retain_count)
        {
            /** oldest segment has not had its release condition set (release function called) so it cannot be pruned */
            return;
        }

        assert(oldest_active_segment->size);/** segment must occupy space */
        assert(oldest_active_segment->release_moment_count <= SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT);
        assert(sol_vk_timeline_semaphore_moment_queue_index_is_front(&staging_buffer->release_moment_queue ,oldest_active_segment->release_moment_queue_index)); /** queueus should line up/ be in sync */

        release_moment_count = sol_vk_timeline_semaphore_moment_queue_copy_many_front(&staging_buffer->release_moment_queue, release_moments, oldest_active_segment->release_moment_count);
        assert(release_moment_count == oldest_active_segment->release_moment_count);

        if(release_moment_count && !sol_vk_timeline_semaphore_moment_query_multiple(release_moments, release_moment_count, true, device))
        {
            /** oldest segment is still in use by some command_buffer/queue */
            return;
        }

        /** this checks that the start of this segment is the end of the available space */
        assert(oldest_active_segment->offset == (staging_buffer->current_offset + staging_buffer->remaining_space) % staging_buffer->buffer_size);/** out of order free for some reason, or offset mismatch */

        staging_buffer->remaining_space += oldest_active_segment->size;/** relinquish this segments space */

        /** remove all reserved moments from queue, note: this includes reserved byt unused moments */
        sol_vk_timeline_semaphore_moment_queue_prune_many_front(&staging_buffer->release_moment_queue, oldest_active_segment->reserved_moment_count);
        sol_vk_staging_buffer_segment_queue_prune_front(&staging_buffer->segment_queue);/** remove oldest_active_segment from the queue */

        if(staging_buffer->remaining_space == staging_buffer->buffer_size)
        {
            /** should the whole buffer become available, reset offset to 0 so that we can use the buffer more efficiently (try to avoid wrap) */
            assert(staging_buffer->segment_queue.count==0);
            staging_buffer->current_offset=0;
        }
    }
}

struct sol_vk_staging_buffer_allocation sol_vk_staging_buffer_allocation_acquire(struct sol_vk_staging_buffer* staging_buffer, struct cvm_vk_device * device, VkDeviceSize requested_space, uint32_t retain_count)
{
    #warning this functions structure is a bit fucky; cleanup would be good
    VkDeviceSize required_space;
    bool wrap;
    bool existing_segment;
    struct sol_vk_staging_buffer_segment* oldest_active_segment;
    struct sol_vk_staging_buffer_segment* new_segment;
    uint32_t segment_index,segment_count,masked_first_segment_index;
    VkDeviceSize acquired_offset;
    char* mapping;
    struct sol_vk_timeline_semaphore_moment release_moments[SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT];
    uint32_t release_moment_count;
    uint32_t release_moment_queue_index;

    /// this design may allow continuous small allocations to effectively stall a large allocation until the whole buffer is full...
    requested_space = cvm_vk_align(requested_space, staging_buffer->alignment);

    assert(requested_space < staging_buffer->buffer_size);/// REALLY want make max allocation 1/4 total space...

    mtx_lock(&staging_buffer->access_mutex);

    while(true)
    {
        assert(!staging_buffer->terminating);

        /** try to free up space */
        sol_vk_staging_buffer_prune_allocations(staging_buffer,device);

        wrap = (staging_buffer->current_offset + requested_space) > staging_buffer->buffer_size;

        /** if this request must wrap then we need to consume the unused section of the buffer that remains */
        required_space = requested_space + (wrap ? (staging_buffer->buffer_size - staging_buffer->current_offset) : 0);

        /** is an api violation to request more space than there is room for */
        assert(required_space < staging_buffer->buffer_size);

        if(required_space <= staging_buffer->remaining_space)
        {
            /**  request will fit, we're done checking for space*/
            break;
        }

        /** otherwise; more space required; need to wait for space to become free */
        existing_segment = sol_vk_staging_buffer_segment_queue_access_front(&staging_buffer->segment_queue, &oldest_active_segment);
        assert(existing_segment); /** should not need more space if there are no active segments */

        if(oldest_active_segment->retain_count)
        {
            /**
            this segment has been reserved but not completed, need to wait for release moments to be set
            (i.e. wait for the allocation to be released the number of times it was retained on creation)
            this is a particularly bad situation
            must be while loop to handle spurrious wakeup
            must be signalled by thread that sets this semaphore moment
            */
            staging_buffer->threads_waiting_on_semaphore_setup = true;
            cnd_wait(&staging_buffer->setup_stall_condition, &staging_buffer->access_mutex);
            /** when this actually regains the lock the actual first segment may have progressed, thus we need to start again (don't try to wait here) */
        }
        else
        {
            /** wait on semaphore, many threads may actually do this, but that should be fine as long as timeline semaphore waiting on many threads isnt an issue (it isn't)
             * note: need to retrieve all mutex controlled data used outside mutex, before unlocking mutex (the pointer may become invalid after the mutex is unlocked) */

            assert(oldest_active_segment->size);/** segment must occupy space */
            assert(oldest_active_segment->release_moment_count <= SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT);
            assert(sol_vk_timeline_semaphore_moment_queue_index_is_front(&staging_buffer->release_moment_queue ,oldest_active_segment->release_moment_queue_index)); /** queueus should line up/ be in sync */

            release_moment_count = sol_vk_timeline_semaphore_moment_queue_copy_many_front(&staging_buffer->release_moment_queue, release_moments, oldest_active_segment->release_moment_count);
            assert(release_moment_count == oldest_active_segment->release_moment_count);
            assert(release_moment_count > 0);

            /** isn't ideal b/c we will have to check this moment again after the mutex is locked again */
            mtx_unlock(&staging_buffer->access_mutex);
            /** wait for semaphore outside of mutex so as to not block inside the mutex */
            sol_vk_timeline_semaphore_moment_wait_multiple(release_moments, release_moment_count, true, device);

            mtx_lock(&staging_buffer->access_mutex);
        }
    }

    sol_vk_timeline_semaphore_moment_queue_enqueue_many_index(&staging_buffer->release_moment_queue, retain_count, &release_moment_queue_index);
    sol_vk_staging_buffer_segment_queue_enqueue_ptr(&staging_buffer->segment_queue, &new_segment, &segment_index);

    *new_segment = (struct sol_vk_staging_buffer_segment)
    {
        .release_moment_count = 0,
        .release_moment_queue_index = release_moment_queue_index,
        .reserved_moment_count = retain_count,
        .retain_count = retain_count,
        .offset = staging_buffer->current_offset,
        .size = required_space,
    };

    acquired_offset = wrap ? 0 : staging_buffer->current_offset;
    mapping = staging_buffer->backing_buffer.mapping + acquired_offset;

    staging_buffer->remaining_space -= required_space;
    staging_buffer->current_offset  += required_space;

    if(staging_buffer->current_offset > staging_buffer->buffer_size)
    {
        /** wrap the current offset if necessary */
        staging_buffer->current_offset -= staging_buffer->buffer_size;
    }

    mtx_unlock(&staging_buffer->access_mutex);

    return (struct sol_vk_staging_buffer_allocation)
    {
        .acquired_buffer = staging_buffer->backing_buffer.buffer,
        .acquired_offset = acquired_offset,
        .mapping = mapping,
        .segment_index = segment_index,
    };
}

void sol_vk_staging_buffer_allocation_flush_range(struct sol_vk_staging_buffer* staging_buffer, struct cvm_vk_device* device, const struct sol_vk_staging_buffer_allocation* allocation, VkDeviceSize relative_offset, VkDeviceSize size)
{
    mtx_lock(&staging_buffer->access_mutex);

    sol_vk_buffer_flush_range(device, &staging_buffer->backing_buffer, allocation->acquired_offset + relative_offset, size);

    mtx_unlock(&staging_buffer->access_mutex);
}

bool sol_vk_staging_buffer_allocation_release(struct sol_vk_staging_buffer* staging_buffer, const struct sol_vk_staging_buffer_allocation* allocation, const struct sol_vk_timeline_semaphore_moment* release_moment)
{
    struct sol_vk_staging_buffer_segment* segment;
    struct sol_vk_timeline_semaphore_moment* release_moment_in_queue;
    bool index_valid, last_retain;

    mtx_lock(&staging_buffer->access_mutex);

    segment = sol_vk_staging_buffer_segment_queue_access_entry(&staging_buffer->segment_queue, allocation->segment_index);
    assert(segment->retain_count > 0);

    if(release_moment->semaphore != VK_NULL_HANDLE)
    {
        index_valid = sol_vk_timeline_semaphore_moment_queue_access_index(&staging_buffer->release_moment_queue, &release_moment_in_queue, segment->release_moment_queue_index + segment->release_moment_count);
        segment->release_moment_count++;
        *release_moment_in_queue = *release_moment;
    }
    segment->retain_count--;
    last_retain = segment->retain_count == 0;

    if(last_retain && staging_buffer->threads_waiting_on_semaphore_setup)
    {
        staging_buffer->threads_waiting_on_semaphore_setup = false;
        cnd_broadcast(&staging_buffer->setup_stall_condition);
    }

    mtx_unlock(&staging_buffer->access_mutex);

    return last_retain;
}

VkDeviceSize sol_vk_staging_buffer_allocation_align_offset(const struct sol_vk_staging_buffer* staging_buffer, VkDeviceSize offset)
{
    return cvm_vk_align(offset, staging_buffer->alignment);
}

