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

#include "vk/sync_manager.h"
#include "cvm_vk.h"

#warning how to remove sol_vk as a requiremed include here?


static int sol_vk_sync_thread_function(void* in)
{
    struct sol_vk_sync_manager* manager = in;
    VkResult result;
    uint32_t entry_index;


    VkDevice vk_device = manager->device->device;

    VkSemaphoreWaitInfo wait_info =
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext = NULL,
        .flags = VK_SEMAPHORE_WAIT_ANY_BIT,
    };

    mtx_lock(&manager->mutex);

    while(manager->running || manager->entry_count > 1)
    {
        manager->semaphores[0] = manager->alteration_semaphore.semaphore;
        manager->values    [0] = manager->alteration_semaphore.value;

        wait_info.semaphoreCount = manager->entry_count;
        wait_info.pSemaphores    = manager->semaphores;
        wait_info.pValues        = manager->values;

        mtx_unlock(&manager->mutex);

        /** timeout is perfectly fine; will just wake the thread temporarily (no checking of semaphores will be performed)
            having semaphores signalled before now is also perfectly fine, wait will just return immediately */
        result = vkWaitSemaphores(vk_device, &wait_info, SOL_VK_DEFAULT_TIMEOUT);// UINT64_MAX ??

        mtx_lock(&manager->mutex);

        if(result == VK_SUCCESS)
        {
            /** wait on all entries individually; signal and remove as necessary */
            entry_index = 1;
            while(entry_index < manager->entry_count)
            {
                wait_info.semaphoreCount = 1;
                wait_info.pSemaphores    = manager->semaphores + entry_index;
                wait_info.pValues        = manager->values     + entry_index;

                result = vkWaitSemaphores(vk_device, &wait_info, 0);

                /** signal the primitive and remove it from the list */
                if(result == VK_SUCCESS)
                {
                    assert(manager->entry_count > 1);
                    sol_sync_primitive_signal_condition(manager->primitives[entry_index]);

                    manager->entry_count--;
                    manager->primitives[entry_index] = manager->primitives[manager->entry_count];
                    manager->semaphores[entry_index] = manager->semaphores[manager->entry_count];
                    manager->values    [entry_index] = manager->values    [manager->entry_count];
                }
                else
                {
                    assert(result == VK_TIMEOUT);
                    entry_index++;
                }
            }

            /** about to unlock mutex so require a change to kill the wait early (but only do this when not timing out) */
            manager->alteration_semaphore.value++;
        }
        else
        {
            assert(result == VK_TIMEOUT);
        }
    }

    mtx_unlock(&manager->mutex);

    return 0;
}

static inline void sol_vk_sync_manager_signal_wakeup(struct sol_vk_sync_manager* manager)
{
    VkSemaphoreSignalInfo wakeup_signal_info =
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .pNext = NULL,
        .semaphore = manager->alteration_semaphore.semaphore,
        .value = manager->alteration_semaphore.value,
    };

    vkSignalSemaphore(manager->device->device, &wakeup_signal_info);
}

void sol_vk_sync_manager_initialise(struct sol_vk_sync_manager* manager, const struct cvm_vk_device* device)
{
    const uint32_t initial_space = 1024;

    manager->device = device;

    manager->entry_space = initial_space;
    manager->entry_count = 1;// the manager semaphore

    manager->primitives = malloc(sizeof(struct sol_sync_primitive*) * initial_space);
    manager->semaphores = malloc(sizeof(VkSemaphore)                * initial_space);
    manager->values     = malloc(sizeof(uint64_t)                   * initial_space);

    sol_vk_timeline_semaphore_initialise(&manager->alteration_semaphore, device);

    manager->running = true;

    mtx_init(&manager->mutex, mtx_plain);
    thrd_create(&manager->thread, sol_vk_sync_thread_function, manager);
}

void sol_vk_sync_manager_terminate(struct sol_vk_sync_manager* manager, const struct cvm_vk_device* device)
{
    assert(device == manager->device);
    /** shut down the management thread, ensuring all primitives have been signalled */
    mtx_lock(&manager->mutex);
    manager->running = false;
    sol_vk_sync_manager_signal_wakeup(manager);
    mtx_unlock(&manager->mutex);
    thrd_join(manager->thread, NULL);

    sol_vk_timeline_semaphore_terminate(&manager->alteration_semaphore, manager->device);

    free(manager->primitives);
    free(manager->semaphores);
    free(manager->values);
}

void sol_vk_sync_manager_impose_timeline_semaphore_moment_condition(struct sol_vk_sync_manager* manager, struct sol_vk_timeline_semaphore_moment moment, struct sol_sync_primitive* successor)
{
    /** test before even locking mutex (in which case no need to even impose a condition, just return) */
    if (sol_vk_timeline_semaphore_moment_query(&moment, manager->device))
    {
        return;
    }


    sol_sync_primitive_impose_condition(successor);

    mtx_lock(&manager->mutex);
    assert(manager->running);

    if(manager->entry_count == manager->entry_space)
    {
        manager->entry_space *= 2;
        manager->primitives = realloc(manager->primitives, sizeof(struct sol_sync_primitive*) * manager->entry_space);
        manager->semaphores = realloc(manager->semaphores, sizeof(VkSemaphore)                * manager->entry_space);
        manager->values     = realloc(manager->values    , sizeof(uint64_t)                   * manager->entry_space);
    }

    manager->primitives[manager->entry_count] = successor;
    manager->semaphores[manager->entry_count] = moment.semaphore;
    manager->values    [manager->entry_count] = moment.value;
    manager->entry_count++;

    sol_vk_sync_manager_signal_wakeup(manager);

    mtx_unlock(&manager->mutex);
}