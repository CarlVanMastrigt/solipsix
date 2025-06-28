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

#pragma once

#include <stdbool.h>
#include <vulkan/vulkan.h>

struct cvm_vk_device;

struct sol_vk_timeline_semaphore
{
    VkSemaphore semaphore;
    uint64_t value;
};

struct sol_vk_timeline_semaphore_moment
{
    VkSemaphore semaphore;
    uint64_t value;
};

#define SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL ((struct sol_vk_timeline_semaphore_moment){.semaphore=VK_NULL_HANDLE,.value=0})

void sol_vk_timeline_semaphore_initialise(struct sol_vk_timeline_semaphore* timeline_semaphore, const struct cvm_vk_device* device);
void sol_vk_timeline_semaphore_terminate(struct sol_vk_timeline_semaphore* timeline_semaphore, const struct cvm_vk_device* device);

struct sol_vk_timeline_semaphore_moment sol_vk_timeline_semaphore_generate_moment(struct sol_vk_timeline_semaphore* timeline_semaphore);

VkSemaphoreSubmitInfo sol_vk_timeline_semaphore_moment_submit_info(const struct sol_vk_timeline_semaphore_moment* moment, VkPipelineStageFlags2 stages);

void sol_vk_timeline_semaphore_moment_wait(const struct sol_vk_timeline_semaphore_moment* moment, const struct cvm_vk_device* device);
bool sol_vk_timeline_semaphore_moment_query(const struct sol_vk_timeline_semaphore_moment* moment, const struct cvm_vk_device* device);
/// ^ returns true if this moment has elapsed

#define SOL_VK_TIMELINE_SEMAPHORE_MOMENT_MAX_WAIT_COUNT 8

void sol_vk_timeline_semaphore_moment_wait_multiple(const struct sol_vk_timeline_semaphore_moment* moments, uint32_t moment_count, bool wait_on_all, const struct cvm_vk_device* device);
bool sol_vk_timeline_semaphore_moment_query_multiple(const struct sol_vk_timeline_semaphore_moment* moments, uint32_t moment_count, bool wait_on_all, const struct cvm_vk_device* device);
