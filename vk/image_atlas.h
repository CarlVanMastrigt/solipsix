/**
Copyright 2021,2022,2024,2025,2026 Carl van Mastrigt

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
    SOL_IMAGE_ATLAS_FAIL_ABSENT, /** if using find rather than obtain this will be returned if the entry is not already present */
    SOL_IMAGE_ATLAS_SUCCESS_FOUND, /** existing entry found; no need to initalise */
    SOL_IMAGE_ATLAS_SUCCESS_INSERTED, /** existing entry not found; space was made but contents must be initialised */
};

#define SOL_IMAGE_ATLAS_OBTAIN_FLAG_WRITE  0x00000001u

struct sol_image_atlas_description
{
	/** image details */
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

/** Note: cannot realistically provide the capabilities offered by buffer table because of layout & queue family ownership considerations associated with images in vulkan */

/** TODO: it may be desirable to allow external synchronization, as is done by buffer tables, 
 * this would probably be the preferred mecahanism and would involve begin not providing a moment and end requiring an extrnal moment 
 * (though; external synchonization can ensure scheduled writes occurr after reads, invalidating the need for any moments at all) */

struct sol_image_atlas* sol_image_atlas_create(const struct sol_image_atlas_description* description, struct cvm_vk_device* device);
void sol_image_atlas_destroy(struct sol_image_atlas* atlas, struct cvm_vk_device* device);

/** returned moment must be waited on before performing any reads or writes to the texture, this ensures;
 * all prior reads have completed (avoiding errant read after write)
 * all prior writes have completed (avoiding errant read before write) */
struct sol_vk_timeline_semaphore_moment sol_image_atlas_access_range_begin(struct sol_image_atlas* atlas);

/** returned moment must be signalled after all writes and reads of resources accessed in this access range have completed, this permits;
 * subsequent reads to wait on writes performed in this access range
 * subsequent writes to happen after all reads in this access range have completed
 * (basically pairing with `sol_image_atlas_access_range_begin`) */
struct sol_vk_timeline_semaphore_moment sol_image_atlas_access_range_end(struct sol_image_atlas* atlas);

/** should ONLY be used for validation, not for decision making */
bool sol_image_atlas_access_range_is_active(struct sol_image_atlas* atlas);

/** acquire a unique identifier for accessing/indexing entries in the atlas in the setup phase
 * `transient` entries may be released the moment they are no longer retained by an accessor and must be written every time they are used */
uint64_t sol_image_atlas_generate_entry_identifier(struct sol_image_atlas* atlas, bool transient);

/** if the entry didnt exist, create a slot for it, prefer this if entry will be created regardless (over calling find first)
 * this must be used with write if its contents will be modified in any way
 * if the same resource will be written and used over and over it is the callers responsibility to ensure any read-write-read chain is properly synchonised
 * NOTE: transient resources obtained will be made available immediately when access is released */
enum sol_image_atlas_result sol_image_atlas_entry_obtain(struct sol_image_atlas* atlas, uint64_t entry_identifier, u16_vec2 size, uint32_t flags, struct sol_image_atlas_location* entry_location);

/** same as above but shortcuts returning its present location, useful when other stored detals aren't required (cannot be used when write is intended) */
enum sol_image_atlas_result sol_image_atlas_entry_find(struct sol_image_atlas* atlas, uint64_t entry_identifier, struct sol_image_atlas_location* entry_location);




/** if insertion happens but external systems fail to initialise an entry this should be called to release it
 * this must not be called on in use resources 
 * 
 * this has been removed from the API because this behaviour should generally be avoided:
 * 	^ it makes the worst case behaviour worse (add to hash map and then remove immediately over and over again, which can be non-performant)
 * 	^ it causes a mismatch with what is possible/desired in the buffer table algorithm
 * 	^ it is also incompatible with any attempts at the use of this algorithm in a multitheaded fashion (other threads may see an entry as created/initialised and then it gets removed)
 * >>  if REALLY needed: it can be uncommented out here and in `image_atlas.c` - it will *work* in the single threaded use case... */
// bool sol_image_atlas_entry_release(struct sol_image_atlas* atlas, uint64_t entry_identifier);

#warning if write access requested and there is a better slot to place the size of content desired (or a different size is requested) \
use that new spot (consider allowing image->image copy list to facilitate this type of behaviour)


struct sol_vk_supervised_image* sol_image_atlas_access_supervised_image(struct sol_image_atlas* atlas);
VkImageView sol_image_atlas_access_image_view(struct sol_image_atlas* atlas);


// bool sol_image_atlas_defragment(struct sol_image_atlas* atlas, const struct cvm_vk_device* device, )


