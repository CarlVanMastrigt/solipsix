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

VkResult sol_vk_staging_buffer_initialise(struct sol_vk_staging_buffer* staging_buffer, const struct cvm_vk_device* device, VkBufferUsageFlags usage, VkDeviceSize buffer_size, VkDeviceSize reserved_high_priority_space)
{
    VkResult result;
    const VkDeviceSize alignment = cvm_vk_buffer_alignment_requirements(device, usage);

    buffer_size = cvm_vk_align(buffer_size, alignment);/// round to multiple of alignment

    struct sol_vk_backed_buffer_description backing_description =
    {
        .size = buffer_size,
        .usage = usage,
        .required_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        .desired_properties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    result = sol_vk_backed_buffer_create(device, &backing_description, &staging_buffer->backing);

    if(result == VK_SUCCESS)
    {
        staging_buffer->threads_waiting_on_semaphore_setup = false;
        staging_buffer->terminating = false;

        staging_buffer->buffer_size = buffer_size;
        staging_buffer->alignment = alignment;
        staging_buffer->reserved_high_priority_space = reserved_high_priority_space;

        staging_buffer->current_offset = 0;
        staging_buffer->remaining_space = buffer_size;

        mtx_init(&staging_buffer->access_mutex, mtx_plain);
        cnd_init(&staging_buffer->setup_stall_condition);

        sol_vk_staging_buffer_segment_queue_initialise(&staging_buffer->segment_queue, 16);
    }

    return result;
}

/// cannot acquire allocations after this has been called
void sol_vk_staging_buffer_terminate(struct sol_vk_staging_buffer* staging_buffer, const struct cvm_vk_device* device)
{
    struct sol_vk_staging_buffer_segment * oldest_active_segment;

    mtx_lock(&staging_buffer->access_mutex);
    staging_buffer->terminating = true;
    /** wait for all memory uses to complete, must be externally synchronised to ensure nothing is still attempting to allocate from staging buffer at this point */
    while(sol_vk_staging_buffer_segment_queue_dequeue_ptr(&staging_buffer->segment_queue, &oldest_active_segment))
    {
        if(oldest_active_segment->release_moments_set)
        {
            assert(oldest_active_segment->size);/** segment must occupy space */

            sol_vk_timeline_semaphore_moment_wait_multiple(oldest_active_segment->release_moments, oldest_active_segment->release_moment_count, true, device);

            /** this checks that the start of this segment is the end of the available space */
            assert(oldest_active_segment->offset == (staging_buffer->current_offset + staging_buffer->remaining_space) % staging_buffer->buffer_size);/** out of order free for some reason, or offset mismatch */

            staging_buffer->remaining_space += oldest_active_segment->size;/** relinquish this segments space */
        }
        else
        {
            /** this segment has been reserved but not completed, need to wait for release moments(condition) to be set */
            staging_buffer->threads_waiting_on_semaphore_setup = true;
            cnd_wait(&staging_buffer->setup_stall_condition, &staging_buffer->access_mutex);
        }
    }

    mtx_unlock(&staging_buffer->access_mutex);

    assert(staging_buffer->remaining_space == staging_buffer->buffer_size);

    mtx_destroy(&staging_buffer->access_mutex);
    cnd_destroy(&staging_buffer->setup_stall_condition);
    sol_vk_staging_buffer_segment_queue_terminate(&staging_buffer->segment_queue);

    sol_vk_backed_buffer_destroy(device, &staging_buffer->backing);
}


static inline void sol_vk_staging_buffer_prune_allocations(struct sol_vk_staging_buffer* staging_buffer, const struct cvm_vk_device* device)
{
    struct sol_vk_staging_buffer_segment * oldest_active_segment;

    while(sol_vk_staging_buffer_segment_queue_access_front(&staging_buffer->segment_queue, &oldest_active_segment))
    {
        if(!oldest_active_segment->release_moments_set)
        {
            /** oldest segment has not had its release condition set (release function called) so it cannot be pruned */
            return;
        }

        assert(oldest_active_segment->size);/** segment must occupy space */

        if(!sol_vk_timeline_semaphore_moment_query_multiple(oldest_active_segment->release_moments, oldest_active_segment->release_moment_count, true, device))
        {
            /** oldest segment is still in use by some command_buffer/queue */
            return;
        }

        /** this checks that the start of this segment is the end of the available space */
        assert(oldest_active_segment->offset == (staging_buffer->current_offset + staging_buffer->remaining_space) % staging_buffer->buffer_size);/** out of order free for some reason, or offset mismatch */

        staging_buffer->remaining_space += oldest_active_segment->size;/** relinquish this segments space */

        sol_vk_staging_buffer_segment_queue_prune_front(&staging_buffer->segment_queue);/** remove oldest_active_segment from the queue */

        if(staging_buffer->remaining_space == staging_buffer->buffer_size)
        {
            assert(staging_buffer->segment_queue.count==0);
            /** should the whole buffer become available, reset offset to 0 so that we can use the buffer more efficiently (try to avoid wrap) */
            staging_buffer->current_offset=0;
        }
    }
}

struct sol_vk_staging_buffer_allocation sol_vk_staging_buffer_allocation_acquire(struct sol_vk_staging_buffer* staging_buffer, const struct cvm_vk_device * device, VkDeviceSize requested_space, bool high_priority)
{
    #warning this functions structure is a bit fucky; cleanup would be good
    VkDeviceSize required_space;
    bool wrap;
    bool existing_segment;
    struct sol_vk_staging_buffer_segment* oldest_active_segment;
    struct sol_vk_staging_buffer_segment* new_segment;
    struct sol_vk_staging_buffer_segment oldest_active_segment_copy;
    uint32_t segment_index,segment_count,masked_first_segment_index;
    VkDeviceSize acquired_offset;
    char* mapping;

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

        assert(required_space < staging_buffer->buffer_size);

        /** check that the reserved high priority space will be preserved when processing low priority requests */
        if(required_space + (high_priority ? 0 : staging_buffer->reserved_high_priority_space) <= staging_buffer->remaining_space)
        {
            /**  request will fit, we're done checking for space*/
            break;
        }

        /** otherwise; more space required; need to wait for space to become free */
        existing_segment = sol_vk_staging_buffer_segment_queue_access_front(&staging_buffer->segment_queue, &oldest_active_segment);
        assert(existing_segment); /** should not need more space if there are no active segments */

        if(oldest_active_segment->release_moments_set)
        {
            /// wait on semaphore, many threads may actually do this, but that should be fine as long as timeline semaphore waiting on many threads isnt an issue
            /// need to retrieve all mutex controlled data used outside mutex, before unlocking mutex (the pointer may become invalid after the mutex is unlocked)
            oldest_active_segment_copy = *oldest_active_segment;

            /// isn't ideal b/c we will have to check this moment again after the mutex is locked again
            mtx_unlock(&staging_buffer->access_mutex);
            /// wait for semaphore outside of mutex so as to not block inside the mutex
            sol_vk_timeline_semaphore_moment_wait_multiple(oldest_active_segment_copy.release_moments, oldest_active_segment_copy.release_moment_count, true, device);

            mtx_lock(&staging_buffer->access_mutex);
        }
        else
        {
            /**
            this segment has been reserved but not completed, need to wait for release moments(condition) to be set
            this is a particularly bad situation
            must be while loop to handle spurrious wakeup
            must be signalled by thread that sets this semaphore moment
            */
            staging_buffer->threads_waiting_on_semaphore_setup = true;
            cnd_wait(&staging_buffer->setup_stall_condition, &staging_buffer->access_mutex);
            /** when this actually regains the lock the actual first segment may have progressed, thus we need to start again (don't try to wait here) */
        }
    }

    sol_vk_staging_buffer_segment_queue_enqueue_ptr(&staging_buffer->segment_queue, &new_segment, &segment_index);
    *new_segment = (struct sol_vk_staging_buffer_segment)
    {
        .release_moment_count = 0,
        .release_moments_set = false,
        .offset = staging_buffer->current_offset,
        .size = required_space,
    };

    acquired_offset = wrap ? 0 : staging_buffer->current_offset;
    mapping = staging_buffer->backing.mapping + acquired_offset;

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
        .acquired_offset = acquired_offset,
        .mapping = mapping,
        .segment_index = segment_index,
        .flushed = false,
    };
}

void sol_vk_staging_buffer_allocation_flush_range(const struct sol_vk_staging_buffer* staging_buffer, const struct cvm_vk_device* device, struct sol_vk_staging_buffer_allocation* allocation, VkDeviceSize relative_offset, VkDeviceSize size)
{
    sol_vk_backed_buffer_flush_range(device, &staging_buffer->backing, allocation->acquired_offset + relative_offset, size);

    assert(!allocation->flushed);/// only want to flush once
    allocation->flushed = true;
}

void sol_vk_staging_buffer_allocation_release(struct sol_vk_staging_buffer* staging_buffer, struct sol_vk_staging_buffer_allocation* allocation, struct sol_vk_timeline_semaphore_moment* release_moments, uint32_t release_moment_count)
{
    struct sol_vk_staging_buffer_segment* segment;
    
    assert(allocation->flushed);
    assert(release_moment_count > 0);
    assert(release_moment_count <= SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT);

    mtx_lock(&staging_buffer->access_mutex);

    segment = sol_vk_staging_buffer_segment_queue_access_entry(&staging_buffer->segment_queue, allocation->segment_index);

    memcpy(segment->release_moments, release_moments, sizeof(struct sol_vk_timeline_semaphore_moment) * release_moment_count);
    segment->release_moment_count = release_moment_count;
    segment->release_moments_set = true;

    if(staging_buffer->threads_waiting_on_semaphore_setup)
    {
        staging_buffer->threads_waiting_on_semaphore_setup = false;
        cnd_broadcast(&staging_buffer->setup_stall_condition);
    }

    mtx_unlock(&staging_buffer->access_mutex);
}

VkDeviceSize sol_vk_staging_buffer_allocation_align_offset(const struct sol_vk_staging_buffer* staging_buffer, VkDeviceSize offset)
{
    return cvm_vk_align(offset, staging_buffer->alignment);
}

