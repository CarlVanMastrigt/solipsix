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
#include "data_structures/hash_map_defines.h"

/** external syncrronization of contents must be performed
*/

struct cvm_vk_device;

struct sol_image_atlas;

enum sol_image_atlas_result
{
    SOL_IMAGE_ATLAS_FAIL_FULL        = SOL_MAP_FAIL_FULL, /** either hash map is full or there are no remaining tiles, either way need to wait for space to be made */
    SOL_IMAGE_ATLAS_FAIL_ABSENT      = SOL_MAP_FAIL_ABSENT, /** if not forcing a dependency this may appear if a resource is written */
    SOL_IMAGE_ATLAS_SUCCESS_FOUND    = SOL_MAP_SUCCESS_FOUND,
    SOL_IMAGE_ATLAS_SUCCESS_INSERTED = SOL_MAP_SUCCESS_INSERTED,
};

struct sol_image_atlas_description
{
	struct cvm_vk_device* device;

	// image details
};

struct sol_image_atlas_location
{
	u16_vec2 xy;
	uint8_t array_layer;
};

void sol_image_atlas_initialise(struct sol_image_atlas* atlas, const struct sol_image_atlas_description* description);
void sol_image_atlas_terminate(struct sol_image_atlas* atlas);




void sol_image_atlas_acquire_access(struct sol_image_atlas* atlas);

/** after reading resources the slot should be released with the moment where it was last used (for now only one moment is supported) */
void sol_image_atlas_release_access(struct sol_image_atlas* atlas, const struct sol_vk_timeline_semaphore_moment* access_complete_moment);


/** acquire a unique identifier for accessing/indexing entries in the atlas
	`transient` entries may be released the moment they are no longer retained by an accessor and must be written every time they are used
	will always retun `SOL_IMAGE_ATLAS_SUCCESS_INSERTED` when `obtained` and `SOL_IMAGE_ATLAS_FAIL_ABSENT` when requested with `find` */
uint64_t sol_image_atlas_acquire_entry_identifier(struct sol_image_atlas* atlas);

/** same as above but shortcuts returning its present location, useful when other stored detals aren't required */
enum sol_image_atlas_result sol_image_atlas_entry_find(struct sol_image_atlas* atlas, uint64_t entry_identifier, struct sol_image_atlas_location* entry_location);

/** if the entry didnt exist, create a slot for it, prefer this if entry will be created regardless (over calling find first)
	this must be used with write if its contents will be modified in any way
	if the same resource will be written and used over and over it is the callers responsibility to ensure any read-write-read chain is properly synchonised */
enum sol_image_atlas_result sol_image_atlas_entry_obtain(struct sol_image_atlas* atlas, uint64_t entry_identifier, struct sol_image_atlas_location* entry_location, u16_vec2 size, bool write_access, bool transient);


#warning make `write_access` and `transient` in obtain flags?





// bool sol_image_atlas_defragment(struct sol_image_atlas* atlas, const struct cvm_vk_device* device, )