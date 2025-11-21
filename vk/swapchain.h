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

#ifndef CVM_VK_H
#include "cvm_vk.h"
#endif


#ifndef CVM_VK_SWAPCHAIN_H
#define CVM_VK_SWAPCHAIN_H


enum cvm_vk_presentable_image_state
{
    CVM_VK_PRESENTABLE_IMAGE_STATE_READY = 0,/// basically uninitialised
    CVM_VK_PRESENTABLE_IMAGE_STATE_ACQUIRED = 1,
    CVM_VK_PRESENTABLE_IMAGE_STATE_STARTED = 2,/// have waited on acquire semaphore; basically is being rendered to
    // CVM_VK_PRESENTABLE_IMAGE_STATE_TRANSFERRED = 3, /// for QFOT
    CVM_VK_PRESENTABLE_IMAGE_STATE_COMPLETE = 4,
    CVM_VK_PRESENTABLE_IMAGE_STATE_PRESENTED = 5,
};

typedef struct cvm_vk_swapchain_setup
{
    VkSurfaceKHR surface;

    VkExtent2D intended_extent;

    uint32_t min_image_count;///0=use system minimum
    VkImageUsageFlagBits usage_flags;
//    VkCompositeAlphaFlagsKHR compositing_mode;

    VkSurfaceFormatKHR preferred_surface_format;
    VkPresentModeKHR preferred_present_mode;
}
cvm_vk_swapchain_setup;

typedef struct cvm_vk_swapchain_instance cvm_vk_swapchain_instance;

typedef struct cvm_vk_swapchain_presentable_image
{
    VkImage image;///theese are provided by the WSI, need access to this for synchronization purposes
    VkImageView image_view;
    cvm_vk_resource_identifier image_view_unique_identifier;

    VkFence presentation_fence;// used if swapchain maintainence is present
    bool presentation_fence_active;

    uint32_t index;

    VkSemaphore acquire_semaphore;///held temporarily by this struct, not owner, not created or destroyed as part of it

    cvm_vk_swapchain_instance * parent_swapchain_instance;/// use with care

    VkSemaphore present_semaphore;///needed by VkPresentInfoKHR, which doesn not accept timeline semaphores

    enum cvm_vk_presentable_image_state state;
    VkImageLayout layout;
    #warning make this a managed image? that way layout tracking can be handled

    // uint32_t latest_queue_family;// latest queue family where this image was used (useful for QFOT)
    struct sol_vk_timeline_semaphore_moment latest_moment;// used to order uses of this image across different submodules
    // ^ this being signalled by the presenting command buffer shouldn't communicate whether present has fully competed, only WSI managed primitives can do that
    // so waiting on this (as is presently done) for an existing presented image accomplishes nothing (or at least that should be the case)
}
cvm_vk_swapchain_presentable_image;

struct cvm_vk_swapchain_instance
{
    /// its assumed these can change as the surface changes, probably not the case but whatever
    VkExtent2D extent;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;

    VkSwapchainKHR swapchain;

    uint32_t fallback_present_queue_family;///if we can't present on the current queue family we'll present on the fallback (lowest indexed queue family that supports present)
    uint64_t queue_family_presentable_mask;

    VkSurfaceCapabilitiesKHR surface_capabilities;// contains info about the surface, should really be used

    cvm_vk_swapchain_presentable_image* presentable_images; // generated/initialised/set_up upon acquisition of swapchain image from WSI
    uint32_t image_count;/// this is also the number of swapchain images
    uint32_t acquired_image_count;/// just for debug tracking

    bool out_of_date;/// if true should be recreated ASAP

//    struct /// pregenerated defaults for use in creating other state
//    {
//        VkRect2D scissor;
//        VkViewport viewport;
//        VkPipelineViewportStateCreateInfo pipeline_viewport_state;
////        VkExtent3D ??
//    }
//    defaults;
};

#define SOL_QUEUE_ENTRY_TYPE cvm_vk_swapchain_instance
#define SOL_QUEUE_FUNCTION_PREFIX cvm_vk_swapchain_instance_queue
#define SOL_QUEUE_STRUCT_NAME cvm_vk_swapchain_instance_queue
#include "data_structures/queue.h"

/// all the data associated with a window and rendering to a surface(usually a window)
typedef struct cvm_vk_surface_swapchain
{
    cvm_vk_swapchain_setup setup_info;

    VkFence metering_fence;///wait till previous fence is acquired before acquiring another
    bool metering_fence_active;

    struct cvm_vk_swapchain_instance_queue swapchain_queue;/// need to preserve out of date/invalid swapchains while they're still in use
}
cvm_vk_surface_swapchain;


void cvm_vk_swapchain_initialse(cvm_vk_surface_swapchain* swapchain, const struct cvm_vk_device* device, const cvm_vk_swapchain_setup* setup);
void cvm_vk_swapchain_terminate(cvm_vk_surface_swapchain* swapchain, const struct cvm_vk_device* device);

/** may return NULL, e.g. when minimized, so calling code should avoid rendering when NULL */
cvm_vk_swapchain_presentable_image* cvm_vk_surface_swapchain_acquire_presentable_image(cvm_vk_surface_swapchain* swapchain, const struct cvm_vk_device* device, VkExtent2D extent);

void cvm_vk_surface_swapchain_present_image(const cvm_vk_device* device, cvm_vk_swapchain_presentable_image* presentable_image, cvm_vk_device_queue* present_queue);


#endif

