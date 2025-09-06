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

struct sol_buffer;
struct sol_buffer_allocation;

struct sol_vk_buf_img_copy_list;

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
#define SOL_IMAGE_ATLAS_OBTAIN_FLAG_UPLOAD 0x00000002u

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

/** this is largely a utility intended for setting up device side work/bindings/state changes after an access has been completed
 * it is only safe to schedule mutation of the supervised images state between the acquire and release moments
 * NOTE: due to accesses to the supervised images internal state it's necessary to externally synchronise modifications of the supervised image
 * NOTE: upload list is the one provided to `sol_image_atlas_acquire_access` and should not be accessed externally */
struct sol_overlay_atlas_access_scope
{
    struct sol_vk_supervised_image* supervised_image;

    /** the upload list will be set here only for convenience, it is the same one provided to `sol_image_atlas_acquire_access` so could be tracked externally */
    struct sol_vk_buf_img_copy_list* upload_list;

    struct sol_vk_timeline_semaphore_moment acquire_moment;
    struct sol_vk_timeline_semaphore_moment release_moment;
};

struct sol_image_atlas* sol_image_atlas_create(const struct sol_image_atlas_description* description, struct cvm_vk_device* device);
void sol_image_atlas_destroy(struct sol_image_atlas* atlas, struct cvm_vk_device* device);

#warning may wish to provide a staging buffer here and manage uploads with an entirely different mechanism -- like a system of signalled moments that connects to a linked(?) list of staging buffer allocations to be made available
#warning ^ this could/would fully opacify ALL staging buffer usages across the whole application
/** `upload_buffer` and `upload_list` may be NULL, in which case no calls to obtain should be passed SOL_IMAGE_ATLAS_OBTAIN_FLAG_UPLOAD
 * `upload buffer` will only be used within the range between the beginning and end of the scope setup and its contents must be staged and provided to `sol_overlay_atlas_access_scope_execute_copies` with the scope assigned on release
 * NOTE: it's necessary to provide a copy list (rather than have one internal to the atlas) because setting up operations that will be executed after access release is an intended use case
 * NOTE: its necessary to provide this such that an obtained entry can have guarantees that it's contents can be initialised (otherwise there is no way to guarantee that there is space to stage the upload) */
void sol_image_atlas_access_scope_setup_begin(struct sol_image_atlas* atlas, struct sol_buffer* upload_buffer, struct sol_vk_buf_img_copy_list* upload_list);

/** the `access_scope` set as part of this function describes the scope of the callers (i.e. external) permissions and responsibilities
 * NOTE: this will fully initialise `access_scope` which may be discarded after all its responsibilities are fulfilled
 * any accesses (entry read or write or supervised image state mutation) must be synchronised externally using the semaphore moments assigned in the `access_scope`
 * the assigned acquire semaphore must be waited on before any accesses of atlas
 * the assigned release remaphore must be signalled after any accesses of atlas
 * NOTE: its important to consider ordering of submissions to queues (erlier queue submissions should not depend on later submissions) when submitting work that uses the provided moments
 *      ^ the same goes for supervided image state tracking, as such its likely best to "single thread" gpu work */
void sol_image_atlas_access_scope_setup_end(struct sol_image_atlas* atlas, struct sol_overlay_atlas_access_scope* access_scope);

/** acquire a unique identifier for accessing/indexing entries in the atlas in the setup phase
 * `transient` entries may be released the moment they are no longer retained by an accessor and must be written every time they are used */
uint64_t sol_image_atlas_acquire_entry_identifier(struct sol_image_atlas* atlas, bool transient);

/** if the entry didnt exist, create a slot for it, prefer this if entry will be created regardless (over calling find first)
 * this must be used with write if its contents will be modified in any way
 * if the same resource will be written and used over and over it is the callers responsibility to ensure any read-write-read chain is properly synchonised
 * NOTE: transient resources obtained will be made available immediately when access is released */
enum sol_image_atlas_result sol_image_atlas_entry_obtain(struct sol_image_atlas* atlas, uint64_t entry_identifier, u16_vec2 size, uint32_t flags, struct sol_image_atlas_location* entry_location, struct sol_buffer_allocation* upload_allocation);

/** same as above but shortcuts returning its present location, useful when other stored detals aren't required (cannot be used when write is intended) */
enum sol_image_atlas_result sol_image_atlas_entry_find(struct sol_image_atlas* atlas, uint64_t entry_identifier, struct sol_image_atlas_location* entry_location);


/** NOTE: this is mostly just a utility function with limited value (it could be implemented externally quite trivially)
 * if a non-null `upload_buffer` & `upload_list` were provided to `sol_image_atlas_acquire_access` then this MUST be called on a command buffer between the acquire and release semaphores assigned at the end of scope setup
 * provided staging buffer at provided staging offset must contain the contents of the `upload_buffer`
 * provided to `sol_image_atlas_acquire_access` AFTER `sol_image_atlas_release_access` has been called
 * NOTE: an inmplementation may wish to share a single upload buffer across multiple atlases and other use cases, this must be externally synchronised
 * NOTE: the `upload_list` provided to `sol_image_atlas_acquire_access` will become available again after this has been called */
void sol_overlay_atlas_access_scope_execute_copies(struct sol_overlay_atlas_access_scope* access_scope, VkCommandBuffer command_buffer, VkBuffer staging_buffer, VkDeviceSize staging_offset);


#warning if write access requested and there is a better slot to place the size of content desired (or a different size is requested) \
use that new spot (consider allowing image->image copy list to facilitate this type of behaviour)





// bool sol_image_atlas_defragment(struct sol_image_atlas* atlas, const struct cvm_vk_device* device, )


