/**
Copyright 2024 Carl van Mastrigt

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

#ifndef solipsix_H
#include "solipsix.h"
#endif

#ifndef CVM_INDEPENDENT_OVERLAY_H
#define CVM_INDEPENDENT_OVERLAY_H

/// for rendering independently of any particular render pass or rendering pipeline
/// can provide enough for a UI only application, especially simple if rendering to a presentable image
/// can act as a starting point for using the general ui/rendering capabilities of the library
/// can be a drop in renderer after other rendering and simply render over the top of other work







/// target information and synchronization requirements, provides all necessary control, not required if using a presentable image
struct cvm_overlay_target
{
    /// framebuffer depends on these
    VkImageView image_view;
    cvm_vk_resource_identifier image_view_unique_identifier;
    /// render pass depends on these
    VkExtent2D extent;
    VkFormat format;
    // start respecting colour space, to do so must support different input colour space (i believe) and must have information on which space blending is done in and how
    VkColorSpaceKHR color_space;/// respecting the colour space is NYI

    VkImageLayout initial_layout;
    VkImageLayout final_layout;
    bool clear_image;

    /// in / out synchronization setup data
    uint32_t wait_semaphore_count;
    uint32_t acquire_barrier_count;
    VkSemaphoreSubmitInfo wait_semaphores[4];
    VkImageMemoryBarrier2 acquire_barriers[4];

    uint32_t signal_semaphore_count;
    uint32_t release_barrier_count;
    VkSemaphoreSubmitInfo signal_semaphores[4];
    VkImageMemoryBarrier2 release_barriers[4];
};

typedef struct cvm_overlay_renderer cvm_overlay_renderer;




struct cvm_overlay_renderer* cvm_overlay_renderer_create(const struct cvm_vk_device* device, struct sol_vk_staging_buffer* staging_buffer, uint32_t active_render_count);
void cvm_overlay_renderer_destroy(struct cvm_overlay_renderer* renderer, const struct cvm_vk_device* device);

struct sol_vk_timeline_semaphore_moment cvm_overlay_render_to_target(const cvm_vk_device* device, cvm_overlay_renderer* renderer, struct cvm_overlay_image_atlases* image_atlases, widget* root_widget, const struct cvm_overlay_target* target);

struct sol_vk_timeline_semaphore_moment cvm_overlay_render_to_presentable_image(const cvm_vk_device* device, cvm_overlay_renderer* renderer, struct cvm_overlay_image_atlases* image_atlases, widget* root_widget, cvm_vk_swapchain_presentable_image* presentable_image, bool last_use);



#endif
