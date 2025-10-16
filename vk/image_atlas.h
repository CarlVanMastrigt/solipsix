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

/** external syncrronization of contents must be performed */

struct cvm_vk_device;

struct sol_image_atlas;

struct sol_vk_supervised_image;

#warning these need better names
enum sol_image_atlas_result
{
    SOL_IMAGE_ATLAS_FAIL_IMAGE_FULL, /** there are no remaining tiles, need to wait for space to be made */
    SOL_IMAGE_ATLAS_FAIL_MAP_FULL, /** hash map is full, need to wait for space to be made */
    SOL_IMAGE_ATLAS_FAIL_UPLOAD_FULL, /** desire to upload contents for this tile, but there is no remaining space in upload buffer */
    SOL_IMAGE_ATLAS_FAIL_ABSENT, /** if not forcing a dependency this may appear if a resource is written */
    SOL_IMAGE_ATLAS_SUCCESS_FOUND, /** existing entry found; no need to initalise */
    SOL_IMAGE_ATLAS_SUCCESS_INSERTED, /** existing entry not found; space was made but contents must be initialised */
};

#define SOL_IMAGE_ATLAS_OBTAIN_FLAG_WRITE  0x00000001u

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

#warning add notes on new access pattern
struct sol_vk_timeline_semaphore_moment sol_image_atlas_access_scope_setup_begin(struct sol_image_atlas* atlas);

/** the `access_scope` set as part of this function describes the scope of the callers (i.e. external) permissions and responsibilities
 * NOTE: this will fully initialise `access_scope` which may be discarded after all its responsibilities are fulfilled
 * any accesses (entry read or write or supervised image state mutation) must be synchronised externally using the semaphore moments assigned in the `access_scope`
 * the assigned acquire semaphore must be waited on before any accesses of atlas
 * the assigned release remaphore must be signalled after any accesses of atlas
 * NOTE: its important to consider ordering of submissions to queues (erlier queue submissions should not depend on later submissions) when submitting work that uses the provided moments
 *      ^ the same goes for supervided image state tracking, as such its likely best to "single thread" gpu work */
struct sol_vk_timeline_semaphore_moment sol_image_atlas_access_scope_setup_end(struct sol_image_atlas* atlas);

/** acquire a unique identifier for accessing/indexing entries in the atlas in the setup phase
 * `transient` entries may be released the moment they are no longer retained by an accessor and must be written every time they are used */
uint64_t sol_image_atlas_acquire_entry_identifier(struct sol_image_atlas* atlas, bool transient);

/** if the entry didnt exist, create a slot for it, prefer this if entry will be created regardless (over calling find first)
 * this must be used with write if its contents will be modified in any way
 * if the same resource will be written and used over and over it is the callers responsibility to ensure any read-write-read chain is properly synchonised
 * NOTE: transient resources obtained will be made available immediately when access is released */
enum sol_image_atlas_result sol_image_atlas_entry_obtain(struct sol_image_atlas* atlas, uint64_t entry_identifier, u16_vec2 size, uint32_t flags, struct sol_image_atlas_location* entry_location);

/** same as above but shortcuts returning its present location, useful when other stored detals aren't required (cannot be used when write is intended) */
enum sol_image_atlas_result sol_image_atlas_entry_find(struct sol_image_atlas* atlas, uint64_t entry_identifier, struct sol_image_atlas_location* entry_location);


/** if insertion happens but external systems fail to initialise an entry this should be called to release it */
void sol_image_atlas_entry_release(struct sol_image_atlas* atlas, uint64_t entry_identifier);

#warning if write access requested and there is a better slot to place the size of content desired (or a different size is requested) \
use that new spot (consider allowing image->image copy list to facilitate this type of behaviour)


struct sol_vk_supervised_image* sol_image_atlas_acquire_supervised_image(struct sol_image_atlas* atlas);


// bool sol_image_atlas_defragment(struct sol_image_atlas* atlas, const struct cvm_vk_device* device, )


