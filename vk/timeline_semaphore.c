/**
Copyright 2024,2025 Carl van Mastrigt

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

#include "vk/timeline_semaphore.h"
#include "cvm_vk.h"


void sol_vk_timeline_semaphore_initialise(struct sol_vk_timeline_semaphore* timeline_semaphore, const struct cvm_vk_device* device)
{
    /** TODO: instead have a cache in device to pull from */
    VkSemaphoreCreateInfo timeline_semaphore_create_info=(VkSemaphoreCreateInfo)
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = (VkSemaphoreTypeCreateInfo[1])
        {
            {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .pNext = NULL,
                .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue = 0,
            }
        },
        .flags = 0,
    };

    VkResult result = vkCreateSemaphore(device->device, &timeline_semaphore_create_info, NULL, &timeline_semaphore->semaphore);
    assert(result == VK_SUCCESS);
    timeline_semaphore->value = 0;
}

void sol_vk_timeline_semaphore_terminate(struct sol_vk_timeline_semaphore* timeline_semaphore, const struct cvm_vk_device* device)
{
    vkDestroySemaphore(device->device, timeline_semaphore->semaphore, NULL);
}

struct sol_vk_timeline_semaphore_moment sol_vk_timeline_semaphore_generate_new_moment(struct sol_vk_timeline_semaphore* timeline_semaphore)
{
    timeline_semaphore->value++;

    return (struct sol_vk_timeline_semaphore_moment)
    {
        .semaphore = timeline_semaphore->semaphore,
        .value     = timeline_semaphore->value,
    };
}

struct sol_vk_timeline_semaphore_moment sol_vk_timeline_semaphore_get_current_moment(struct sol_vk_timeline_semaphore* timeline_semaphore)
{
    return (struct sol_vk_timeline_semaphore_moment)
    {
        .semaphore = timeline_semaphore->semaphore,
        .value     = timeline_semaphore->value,
    };
}

VkSemaphoreSubmitInfo sol_vk_timeline_semaphore_moment_submit_info(const struct sol_vk_timeline_semaphore_moment* moment, VkPipelineStageFlags2 stages)
{
    return (VkSemaphoreSubmitInfo)
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = NULL,
        .semaphore = moment->semaphore,
        .value = moment->value,
        .stageMask = stages,
        .deviceIndex = 0
    };
}

static inline bool sol_vk_timeline_semaphore_moment_wait_multiple_timed(const struct sol_vk_timeline_semaphore_moment* moments, uint32_t moment_count, bool wait_on_all, bool repeatedly_wait, uint64_t timeout, const struct cvm_vk_device* device)
{
    /** working in batches won't work when "wait on any" */
    uint32_t i;
    VkResult result;
    VkSemaphore semaphores[SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT];
    uint64_t values[SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT];

    assert(moment_count > 0);
    assert(moment_count <= SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT);

    /** split moments into semaphore and value arrays */
    for(i = 0; i < moment_count; i++)
    {
        assert(moments[i].semaphore != VK_NULL_HANDLE);
        semaphores[i] = moments[i].semaphore;
        values[i]     = moments[i].value;
    }

    VkSemaphoreWaitInfo wait =
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext = NULL,
        .flags = wait_on_all ? 0 : VK_SEMAPHORE_WAIT_ANY_BIT,
        .semaphoreCount = moment_count,
        .pSemaphores = semaphores,
        .pValues = values,
    };

    do
    {
        result = vkWaitSemaphores(device->device, &wait, timeout);
        if(result == VK_TIMEOUT && timeout)
        {
            fprintf(stderr, "timeline semaphore seems to be stalling");
        }
    }
    while(result == VK_TIMEOUT && repeatedly_wait);
    
    assert(result == VK_SUCCESS || result == VK_TIMEOUT);
    return result == VK_SUCCESS;
}

void sol_vk_timeline_semaphore_moment_wait(const struct sol_vk_timeline_semaphore_moment* moment, const struct cvm_vk_device* device)
{
    sol_vk_timeline_semaphore_moment_wait_multiple_timed(moment, 1, true, true, SOL_VK_DEFAULT_TIMEOUT, device);
}

bool sol_vk_timeline_semaphore_moment_query(const struct sol_vk_timeline_semaphore_moment* moment, const struct cvm_vk_device* device)
{
    return sol_vk_timeline_semaphore_moment_wait_multiple_timed(moment, 1, true, false, 0, device);
}

void sol_vk_timeline_semaphore_moment_signal(const struct sol_vk_timeline_semaphore_moment* moment, const struct cvm_vk_device* device)
{
    sol_vk_timeline_semaphore_moment_signal_multiple(moment, 1, device);
}


void sol_vk_timeline_semaphore_moment_wait_multiple(const struct sol_vk_timeline_semaphore_moment* moments, uint32_t moment_count, bool wait_on_all, const struct cvm_vk_device* device)
{
    sol_vk_timeline_semaphore_moment_wait_multiple_timed(moments, moment_count, wait_on_all, true, SOL_VK_DEFAULT_TIMEOUT, device);
}

bool sol_vk_timeline_semaphore_moment_query_multiple(const struct sol_vk_timeline_semaphore_moment* moments, uint32_t moment_count, bool wait_on_all, const struct cvm_vk_device* device)
{
    return sol_vk_timeline_semaphore_moment_wait_multiple_timed(moments, moment_count, wait_on_all, false, 0, device);
}

void sol_vk_timeline_semaphore_moment_signal_multiple(const struct sol_vk_timeline_semaphore_moment* moments, uint32_t moment_count, const struct cvm_vk_device* device)
{
    uint32_t i;
    VkResult result;
    VkSemaphoreSignalInfo signal_info =
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .pNext = NULL,
    };

    for (i = 0; i < moment_count; i++)
    {
        signal_info.semaphore = moments[i].semaphore;
        signal_info.value     = moments[i].value;

        result = vkSignalSemaphore(device->device, &signal_info);
        assert(result == VK_SUCCESS);
    }
}


