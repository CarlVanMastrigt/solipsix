/**
Copyright 2021,2022,2024,2025 Carl van Mastrigt

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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <threads.h>
#include <stdatomic.h>
#include <vulkan/vulkan.h>

#include "math/u16_vec2.h"

#include "vk/timeline_semaphore.h"
#include "data_structures/hash_map.h"

/** external syncrronization of contents must be performed
*/

struct cvm_vk_device;

struct sol_image_atlas;

enum sol_image_atlas_result
{
    SOL_IMAGE_ATLAS_FAIL_FULL        = SOL_MAP_FAIL_FULL, /** either hash map is full or there are no remaining tiles, either way need to wait for space to be made */
    SOL_IMAGE_ATLAS_FAIL_ABSENT      = SOL_MAP_FAIL_ABSENT,
    SOL_IMAGE_ATLAS_SUCCESS_FOUND    = SOL_MAP_SUCCESS_FOUND,
    SOL_IMAGE_ATLAS_SUCCESS_INSERTED = SOL_MAP_SUCCESS_INSERTED,
};

struct sol_image_atlas_description
{
	bool multithreaded;/** this structure may be accessed by multiple threads at once */
	bool deframenting;/** attempt to deframent the image atlas upon release */

	struct cvm_vk_device* device;
};

struct sol_image_atlas_location
{
	u16_vec2 xy;
	uint8_t array_layer;
};

void sol_image_atlas_initialise(struct sol_image_atlas* atlas, const struct sol_image_atlas_description* description);
void sol_image_atlas_terminate(struct sol_image_atlas* atlas);

/** before accessing entries its required to hold an accessor_slot
	it will return a moment that must be waited on to do any GPU operations (read or write) or any CPU operations writing a NON-novel entry
	(a NON novel entry being one for which sol_image_atlas_entry_obtain returned SOL_IMAGE_ATLAS_SUCCESS_FOUND) */
uint8_t sol_image_atlas_accessor_slot_acquire(struct sol_image_atlas* atlas, struct sol_vk_timeline_semaphore_moment* access_wait_moment);

/** after accessing some slots (read or write are considered the same)
	the slot should be released with a moment for all of the GPU operations where it was last used (all the leaves of a dependency graph)*/
void sol_image_atlas_accessor_slot_release(struct sol_image_atlas* atlas, uint8_t accessor_slot, const struct sol_vk_timeline_semaphore_moment* access_complete_moments, uint32_t moment_count);

/** acquire a unique identifier for accessing/indexing entries in the atlas
	`transient` entries will be released the moment they are no longer retained by an accessor, and should only be used / obtained once */
uint64_t sol_image_atlas_acquire_entry_identifier(struct sol_image_atlas* atlas, bool transient);

/** same as above but shortcuts returning its present location, useful when other stored detals aren't required */
bool sol_image_atlas_entry_find(struct sol_image_atlas* atlas, uint8_t accessor_slot, uint64_t entry_identifier, struct sol_image_atlas_location* entry_location);

/** if the entry didnt exist, create a slot for it, prefer this if entry will be created regardless (over calling find first) */
bool sol_image_atlas_entry_obtain(struct sol_image_atlas* atlas, uint8_t accessor_slot, uint64_t entry_identifier, struct sol_image_atlas_location* entry_location, u16_vec2 size);






// bool sol_image_atlas_defragment(struct sol_image_atlas* atlas, const struct cvm_vk_device* device, )
