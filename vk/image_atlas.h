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


#warning these need better names
enum sol_image_atlas_result
{
    SOL_IMAGE_ATLAS_FAIL_FULL        = SOL_MAP_FAIL_FULL, /** either hash map is full or there are no remaining tiles, either way need to wait for space to be made */
    SOL_IMAGE_ATLAS_FAIL_ABSENT      = SOL_MAP_FAIL_ABSENT, /** if not forcing a dependency this may appear if a resource is written */
    SOL_IMAGE_ATLAS_SUCCESS_FOUND    = SOL_MAP_SUCCESS_FOUND, /** existing entry found; no need to initalise */
    SOL_IMAGE_ATLAS_SUCCESS_INSERTED = SOL_MAP_SUCCESS_INSERTED, /** existing entry not found; space was made but contents must be initialised */
};

struct sol_image_atlas_description
{
	// image details
	VkFormat format;
	VkImageUsageFlags usage;
	uint8_t image_x_dimension_exponent;
	uint8_t image_y_dimension_exponent;
	uint8_t image_array_dimension;
};

struct sol_image_atlas_location
{
	u16_vec2 offset;
	uint8_t array_layer;
};

struct sol_image_atlas* sol_image_atlas_create(const struct sol_image_atlas_description* description, struct cvm_vk_device* device);
void sol_image_atlas_destroy(struct sol_image_atlas* atlas, struct cvm_vk_device* device);

/** must wait on returned moment before doing anything with the atlas
 * (reading or writing accessed entries) */
struct sol_vk_timeline_semaphore_moment sol_image_atlas_acquire_access(struct sol_image_atlas* atlas);

/** moment must be signalled after all reads and writes to the atlas have completed */
struct sol_vk_timeline_semaphore_moment sol_image_atlas_release_access(struct sol_image_atlas* atlas);


/** acquire a unique identifier for accessing/indexing entries in the atlas
	`transient` entries may be released the moment they are no longer retained by an accessor and must be written every time they are used */
uint64_t sol_image_atlas_acquire_entry_identifier(struct sol_image_atlas* atlas, bool transient);

/** same as above but shortcuts returning its present location, useful when other stored detals aren't required */
enum sol_image_atlas_result sol_image_atlas_entry_find(struct sol_image_atlas* atlas, uint64_t entry_identifier, struct sol_image_atlas_location* entry_location);

/** if the entry didnt exist, create a slot for it, prefer this if entry will be created regardless (over calling find first)
	this must be used with write if its contents will be modified in any way
	if the same resource will be written and used over and over it is the callers responsibility to ensure any read-write-read chain is properly synchonised
	NOTE: transient resources obtained will be made available immediately when access is released */
enum sol_image_atlas_result sol_image_atlas_entry_obtain(struct sol_image_atlas* atlas, uint64_t entry_identifier, u16_vec2 size, bool write_access, struct sol_image_atlas_location* entry_location);


struct sol_vk_supervised_image* sol_image_atlas_acquire_supervised_image(struct sol_image_atlas* atlas);


#warning should readonly access be a viable option? (allowing only find operations) could also make this immediate in that incomplete writes are NOT vended

#warning make `write_access` and `transient` (in obtain) flags instead?

#warning if write access requested and there is a better slot to place the size of content desired (or a different size is requested) \
use that new spot and wait on the appropriate entry to be made free to clear it





// bool sol_image_atlas_defragment(struct sol_image_atlas* atlas, const struct cvm_vk_device* device, )