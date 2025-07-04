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

#pragma once

#include <inttypes.h>
#include <threads.h>
#include <vulkan/vulkan.h>

#include "sync/primitive.h"
#include "vk/timeline_semaphore.h"

/**
 This provides a way to connect GPU (vulkan) execution to other (CPU) primitives
 it will require a thread that can wake up to handle new messages
 another pimitive is also defined to manage the reverse

 TODO: should this wholly be moved inside the sol_vk_device ?
*/

struct cvm_vk_device;

struct sol_vk_sync_manager
{
	const struct cvm_vk_device* device;

	thrd_t thread;
	mtx_t mutex;
	bool running;

	// always goes first in semaphore list
	struct sol_vk_timeline_semaphore alteration_semaphore;

	// semaphore and kept separate to allow easy integration with Vulkans interface (i.e. can just pass in the semaphore/value arrays)
	struct sol_sync_primitive** primitives;
	VkSemaphore* semaphores;
	uint64_t* values;
	uint32_t entry_space;
	uint32_t entry_count;
};

void sol_vk_sync_manager_initialise(struct sol_vk_sync_manager* manager, const struct cvm_vk_device* device);
void sol_vk_sync_manager_terminate(struct sol_vk_sync_manager* manager, const struct cvm_vk_device* device);

/** adds the sol_vk_timeline_semaphore_moment as a condition to the primitive i.e. the sol_sync_primitive becomes a successor of the moment */
void sol_vk_sync_manager_impose_timeline_semaphore_moment_condition(struct sol_vk_sync_manager* manager, struct sol_vk_timeline_semaphore_moment moment, struct sol_sync_primitive* successor);

