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
    VkSemaphoreCreateInfo timeline_semaphore_create_info=(VkSemaphoreCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext=(VkSemaphoreTypeCreateInfo[1])
        {
            {
                .sType=VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                .pNext=NULL,
                .semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE,
                .initialValue=0,
            }
        },
        .flags=0
    };

    VkResult result = vkCreateSemaphore(device->device, &timeline_semaphore_create_info, NULL, &timeline_semaphore->semaphore);
    assert(result == VK_SUCCESS);
    timeline_semaphore->value = 0;
}

void sol_vk_timeline_semaphore_terminate(struct sol_vk_timeline_semaphore* timeline_semaphore, const struct cvm_vk_device* device)
{
    vkDestroySemaphore(device->device, timeline_semaphore->semaphore, NULL);
}

struct sol_vk_timeline_semaphore_moment sol_vk_timeline_semaphore_generate_moment(struct sol_vk_timeline_semaphore* timeline_semaphore)
{
    timeline_semaphore->value++;

    return (struct sol_vk_timeline_semaphore_moment)
    {
        .semaphore = timeline_semaphore->semaphore,
        .value = timeline_semaphore->value,
    };
}

VkSemaphoreSubmitInfo sol_vk_timeline_semaphore_moment_submit_info(const struct sol_vk_timeline_semaphore_moment* moment, VkPipelineStageFlags2 stages)
{
    return (VkSemaphoreSubmitInfo)
    {
        .sType=VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext=NULL,
        .semaphore=moment->semaphore,
        .value=moment->value,
        .stageMask=stages,
        .deviceIndex=0
    };
}

void sol_vk_timeline_semaphore_moment_wait(const struct sol_vk_timeline_semaphore_moment* moment, const struct cvm_vk_device* device)
{
    VkResult result;
    /// should this check non-null?
    VkSemaphoreWaitInfo wait =
    {
        .sType=VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext=NULL,
        .flags=0,
        .semaphoreCount=1,
        .pSemaphores=&moment->semaphore,
        .pValues=&moment->value,
    };

    do
    {
        result = vkWaitSemaphores(device->device, &wait, CVM_VK_DEFAULT_TIMEOUT);
        if(result == VK_TIMEOUT)
        {
            fprintf(stderr,"timeline semaphore seems to be stalling");
        }
    }
    while(result == VK_TIMEOUT);
    assert(result == VK_SUCCESS);
}

bool sol_vk_timeline_semaphore_moment_query(const struct sol_vk_timeline_semaphore_moment* moment, const struct cvm_vk_device* device)
{
    VkResult result;
    /// should this check non-null?
    VkSemaphoreWaitInfo wait =
    {
        .sType=VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext=NULL,
        .flags=0,
        .semaphoreCount=1,
        .pSemaphores=&moment->semaphore,
        .pValues=&moment->value,
    };

    result = vkWaitSemaphores(device->device, &wait, 0);
    assert(result == VK_SUCCESS || result == VK_TIMEOUT);
    return result == VK_SUCCESS;
}
