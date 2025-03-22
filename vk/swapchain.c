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

#include "solipsix.h"



static inline void cvm_vk_swapchain_presentable_image_initialise(cvm_vk_swapchain_presentable_image * presentable_image, const cvm_vk_device * device, VkImage image, uint32_t index, cvm_vk_swapchain_instance * parent_swapchain_instance)
{
    uint32_t i;

    presentable_image->image = image;
    presentable_image->index = index;
    presentable_image->parent_swapchain_instance = parent_swapchain_instance;

    presentable_image->present_semaphore = cvm_vk_create_binary_semaphore(device);
    presentable_image->qfot_semaphore = cvm_vk_create_binary_semaphore(device);

    presentable_image->acquire_semaphore=VK_NULL_HANDLE;///set using one of the available image_acquisition_semaphores
    presentable_image->state=CVM_VK_PRESENTABLE_IMAGE_STATE_READY;

    presentable_image->latest_queue_family = CVM_INVALID_U32_INDEX;
    presentable_image->latest_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;

    presentable_image->present_acquire_command_buffers = malloc(sizeof(VkCommandBuffer) * device->queue_family_count);
    for(i=0;i<device->queue_family_count;i++)
    {
        presentable_image->present_acquire_command_buffers[i] = VK_NULL_HANDLE;
    }

    presentable_image->presentation_fence = cvm_vk_create_fence(device, false);
    presentable_image->presentation_fence_active = false;

    ///image view
    presentable_image->image_view = VK_NULL_HANDLE;
    presentable_image->image_view_unique_identifier = cvm_vk_resource_unique_identifier_acquire(device);
    #warning move above into handled function that atomically increments, ideally in a way that doesnt require a non-const device
    VkImageViewCreateInfo image_view_create_info=(VkImageViewCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .image=image,
        .viewType=VK_IMAGE_VIEW_TYPE_2D,
        .format=parent_swapchain_instance->surface_format.format,
        .components=(VkComponentMapping)
        {
            .r=VK_COMPONENT_SWIZZLE_IDENTITY,
            .g=VK_COMPONENT_SWIZZLE_IDENTITY,
            .b=VK_COMPONENT_SWIZZLE_IDENTITY,
            .a=VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange=(VkImageSubresourceRange)
        {
            .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel=0,
            .levelCount=1,
            .baseArrayLayer=0,
            .layerCount=1
        }
    };
    CVM_VK_CHECK(vkCreateImageView(device->device, &image_view_create_info, NULL, &presentable_image->image_view));
    #warning above may need to move to have a mutex lock, at the very least for the resource identifier
}

static inline void cvm_vk_swapchain_presentable_image_terminate(cvm_vk_swapchain_presentable_image * presentable_image, const cvm_vk_device * device, const cvm_vk_swapchain_instance * parent_swapchain_instance)
{
    uint32_t i;

    if(presentable_image->presentation_fence_active)
    {
        cvm_vk_wait_on_fence_and_reset(device, presentable_image->presentation_fence);
        presentable_image->presentation_fence_active = false;
    }

    vkDestroyFence(device->device, presentable_image->presentation_fence, device->host_allocator);

    vkDestroyImageView(device->device,presentable_image->image_view,NULL);

    assert(presentable_image->acquire_semaphore == VK_NULL_HANDLE);

    vkDestroySemaphore(device->device, presentable_image->present_semaphore, NULL);
    vkDestroySemaphore(device->device, presentable_image->qfot_semaphore, NULL);

    ///queue ownership transfer stuff
    for(i=0;i<device->queue_family_count;i++)
    {
        if(presentable_image->present_acquire_command_buffers[i]!=VK_NULL_HANDLE)
        {
            /// FUUUUCK, this BS probably means each instance wants its own set of command pools, freeing command pools can cause fragmentation! WTF should I do!?
            vkFreeCommandBuffers(device->device, device->queue_families[parent_swapchain_instance->fallback_present_queue_family].internal_command_pool, 1, presentable_image->present_acquire_command_buffers+i);
            #warning requires synchronization! (uses shared command pool, ergo not thread safe)
        }
    }
    free(presentable_image->present_acquire_command_buffers);
}

static inline int cvm_vk_swapchain_instance_initialise(cvm_vk_swapchain_instance * instance, const cvm_vk_device * device, const cvm_vk_surface_swapchain * swapchain, VkSwapchainKHR old_swapchain)
{
    uint32_t i,j,fallback_present_queue_family,format_count,present_mode_count,width,height;
    VkBool32 surface_supported;
    VkSurfaceFormatKHR * formats;
    VkPresentModeKHR * present_modes;
    VkImage * images;

    instance->out_of_date = false;
    instance->acquired_image_count = 0;

    /// select format (image format and colourspace)
    CVM_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, swapchain->setup_info.surface, &format_count, NULL));
    formats = malloc(sizeof(VkSurfaceFormatKHR)*format_count);
    CVM_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, swapchain->setup_info.surface, &format_count, formats));

    instance->surface_format = formats[0];///fallback in case preferred isnt found, required to have at least 1 at this point
    for(i=0;i<format_count;i++)
    {
        if(swapchain->setup_info.preferred_surface_format.colorSpace==formats[i].colorSpace && swapchain->setup_info.preferred_surface_format.format==formats[i].format)
        {
            ///preferred format exists
            instance->surface_format = swapchain->setup_info.preferred_surface_format;
            break;
        }
    }
    free(formats);


    ///select screen present mode
    CVM_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, swapchain->setup_info.surface, &present_mode_count, NULL));
    present_modes = malloc(sizeof(VkPresentModeKHR)*present_mode_count);
    CVM_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, swapchain->setup_info.surface, &present_mode_count, present_modes));

    instance->present_mode = VK_PRESENT_MODE_FIFO_KHR;/// this is required to be supported
    for(i=0;i<present_mode_count;i++)
    {
        if(present_modes[i] == swapchain->setup_info.preferred_present_mode)
        {
            instance->present_mode = swapchain->setup_info.preferred_present_mode;
            break;
        }
    }
    free(present_modes);


    /// search for presentable queue families
    instance->queue_family_presentable_mask = 0;
    instance->fallback_present_queue_family = CVM_INVALID_U32_INDEX;
    i = device->queue_family_count;
    while(i--)/// search last to first so that fallback is minimum supported
    {
        CVM_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device->physical_device, i, swapchain->setup_info.surface, &surface_supported));
        if(surface_supported)
        {
            instance->fallback_present_queue_family = i;
            instance->queue_family_presentable_mask |= 1<<i;
        }
    }
    if(instance->queue_family_presentable_mask==0) return -1;///cannot present to this surface!
    assert(instance->fallback_present_queue_family != CVM_INVALID_U32_INDEX);

    /// check surface capabilities and create a the new swapchain
    CVM_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, swapchain->setup_info.surface, &instance->surface_capabilities));

    if((instance->surface_capabilities.supportedUsageFlags & swapchain->setup_info.usage_flags) != swapchain->setup_info.usage_flags) return -1;///intened usage not supported
    if((instance->surface_capabilities.maxImageCount != 0) && (instance->surface_capabilities.maxImageCount < swapchain->setup_info.min_image_count)) return -1;///cannot suppirt minimum image count
    if((instance->surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) == 0)return -1;///compositing not supported
    /// would like to support different compositing, need to extend to allow this

    VkSwapchainCreateInfoKHR swapchain_create_info=(VkSwapchainCreateInfoKHR)
    {
        .sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext=NULL,
        .flags=0,
        .surface=swapchain->setup_info.surface,
        .minImageCount=CVM_MAX(instance->surface_capabilities.minImageCount, swapchain->setup_info.min_image_count),
        .imageFormat=instance->surface_format.format,
        .imageColorSpace=instance->surface_format.colorSpace,
        .imageExtent=instance->surface_capabilities.currentExtent,
        .imageArrayLayers=1,
        .imageUsage=swapchain->setup_info.usage_flags,
        .imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,///means QFOT is necessary, thus following should not be prescribed
        .queueFamilyIndexCount=0,
        .pQueueFamilyIndices=NULL,
        .preTransform=instance->surface_capabilities.currentTransform,
        .compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode=instance->present_mode,
        .clipped=VK_TRUE,
        .oldSwapchain=old_swapchain,
    };

    instance->swapchain = VK_NULL_HANDLE;
    CVM_VK_CHECK(vkCreateSwapchainKHR(device->device,&swapchain_create_info,NULL,&instance->swapchain));


    /// get images and set up image data
    CVM_VK_CHECK(vkGetSwapchainImagesKHR(device->device, instance->swapchain, &instance->image_count, NULL));
    images = malloc(sizeof(VkImage) * instance->image_count);
    CVM_VK_CHECK(vkGetSwapchainImagesKHR(device->device, instance->swapchain, &instance->image_count, images));

    instance->presentable_images = malloc(sizeof(cvm_vk_swapchain_presentable_image) * instance->image_count);
    for(i=0;i<instance->image_count;i++)
    {
        cvm_vk_swapchain_presentable_image_initialise(instance->presentable_images + i, device, images[i], i, instance);
    }

    free(images);


    cvm_vk_create_swapchain_dependent_defaults(instance->surface_capabilities.currentExtent.width,instance->surface_capabilities.currentExtent.height);
    #warning make these defaults part of the swapchain_instance!?

    return 0;
}

static inline void cvm_vk_swapchain_instance_terminate(cvm_vk_swapchain_instance * instance, const cvm_vk_device * device)
{
    uint32_t i;
    VkSemaphore semaphore;

    // need to wait for presentation to finish using the present semaphore before deleting it
    // without swapchain maintainence there is no better way to do this
    // vkDeviceWaitIdle doesn't *guarantee* it will be finished with either, but it works on a plethora of platforms
    if(!device->feature_swapchain_maintainence)
    {
        vkDeviceWaitIdle(device->device);
    }

    assert(instance->acquired_image_count == 0);///MUST WAIT TILL ALL IMAGES ARE RETURNED BEFORE TERMINATING SWAPCHAIN

    cvm_vk_destroy_swapchain_dependent_defaults();
    #warning make above defaults part of the swapchain_instance


    for(i=0;i<instance->image_count;i++)
    {
        cvm_vk_swapchain_presentable_image_terminate(instance->presentable_images+i, device, instance);
    }
    free(instance->presentable_images);

    vkDestroySwapchainKHR(device->device, instance->swapchain, NULL);
}



void cvm_vk_swapchain_initialse(const cvm_vk_device * device, cvm_vk_surface_swapchain * swapchain, const cvm_vk_swapchain_setup * setup)
{
    swapchain->setup_info = *setup;

    swapchain->metering_fence = cvm_vk_create_fence(device,false);
    swapchain->metering_fence_active = false;

    if(setup->preferred_surface_format.format == VK_FORMAT_UNDEFINED)
    {
        swapchain->setup_info.preferred_surface_format=(VkSurfaceFormatKHR){.format=VK_FORMAT_B8G8R8A8_SRGB,.colorSpace=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    cvm_vk_swapchain_instance_queue_initialise(&swapchain->swapchain_queue, 16);
}

void cvm_vk_swapchain_terminate(const cvm_vk_device * device, cvm_vk_surface_swapchain * swapchain)
{
    cvm_vk_swapchain_instance * instance;
    cvm_vk_swapchain_presentable_image * presentable_image;
    uint32_t i;

    #warning move metering fence to the instance???
    if(swapchain->metering_fence_active)
    {
        cvm_vk_wait_on_fence_and_reset(device, swapchain->metering_fence);
        swapchain->metering_fence_active=false;
    }
    cvm_vk_destroy_fence(device,swapchain->metering_fence);


    while(cvm_vk_swapchain_instance_queue_dequeue_ptr(&swapchain->swapchain_queue, &instance))
    {
        for(i=0;i<instance->image_count;i++)
        {
            presentable_image = instance->presentable_images+i;

            if(presentable_image->state == CVM_VK_PRESENTABLE_IMAGE_STATE_READY)
            {
                assert(presentable_image->latest_moment.semaphore == VK_NULL_HANDLE);
                assert(presentable_image->acquire_semaphore == VK_NULL_HANDLE);
            }
            else
            {
                assert(presentable_image->state == CVM_VK_PRESENTABLE_IMAGE_STATE_PRESENTED);
                assert(presentable_image->latest_moment.semaphore != VK_NULL_HANDLE);
                assert(presentable_image->acquire_semaphore != VK_NULL_HANDLE);

                sol_vk_timeline_semaphore_moment_wait(&presentable_image->latest_moment, device);

                assert(instance->acquired_image_count > 0);
                instance->acquired_image_count--;
                sol_vk_device_object_pool_semaphore_release(device, presentable_image->acquire_semaphore);
                presentable_image->latest_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;
                presentable_image->acquire_semaphore = VK_NULL_HANDLE;
                presentable_image->state = CVM_VK_PRESENTABLE_IMAGE_STATE_READY;
            }
        }
        assert(instance->acquired_image_count == 0);
        cvm_vk_swapchain_instance_terminate(instance, device);
    }
    cvm_vk_swapchain_instance_queue_terminate(&swapchain->swapchain_queue);
}




static inline void cvm_vk_swapchain_cleanup_out_of_date_instances(cvm_vk_surface_swapchain * swapchain, cvm_vk_device * device)
{
    cvm_vk_swapchain_presentable_image * presentable_image;
    cvm_vk_swapchain_instance * instance;
    uint32_t i;

    /// get the front of the deletion queue
    while(cvm_vk_swapchain_instance_queue_access_front(&swapchain->swapchain_queue, &instance) && instance->out_of_date)
    {
        for(i=0;i<instance->image_count;i++)
        {
            presentable_image = instance->presentable_images+i;

            if(presentable_image->state == CVM_VK_PRESENTABLE_IMAGE_STATE_READY)
            {
                assert(presentable_image->latest_moment.semaphore == VK_NULL_HANDLE);
                assert(presentable_image->acquire_semaphore == VK_NULL_HANDLE);
            }
            else
            {
                assert(presentable_image->state == CVM_VK_PRESENTABLE_IMAGE_STATE_PRESENTED);
                assert(presentable_image->latest_moment.semaphore != VK_NULL_HANDLE);
                assert(presentable_image->acquire_semaphore != VK_NULL_HANDLE);

                if(sol_vk_timeline_semaphore_moment_query(&presentable_image->latest_moment, device))
                {
                    assert(instance->acquired_image_count > 0);
                    instance->acquired_image_count--;
                    sol_vk_device_object_pool_semaphore_release(device, presentable_image->acquire_semaphore);
                    presentable_image->latest_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;
                    presentable_image->acquire_semaphore = VK_NULL_HANDLE;
                    presentable_image->state = CVM_VK_PRESENTABLE_IMAGE_STATE_READY;
                }
            }
        }

        if(instance->acquired_image_count == 0)
        {
            cvm_vk_swapchain_instance_terminate(instance, device);
            cvm_vk_swapchain_instance_queue_prune_front(&swapchain->swapchain_queue);
        }
        else
        {
            break;
        }
    }
}


cvm_vk_swapchain_presentable_image * cvm_vk_surface_swapchain_acquire_presentable_image(cvm_vk_surface_swapchain * swapchain, cvm_vk_device * device)
{
    bool existing_instance;
    cvm_vk_swapchain_presentable_image * presentable_image;
    cvm_vk_swapchain_instance * instance, * new_instance;
    uint32_t image_index;
    VkSemaphore acquire_semaphore;
    VkResult acquire_result;

    presentable_image = NULL;

    do
    {
        existing_instance = cvm_vk_swapchain_instance_queue_access_back(&swapchain->swapchain_queue, &instance);

        /// create new instance if necessary
        if(!existing_instance || instance==NULL || instance->out_of_date)
        {
            cvm_vk_swapchain_instance_queue_enqueue_ptr(&swapchain->swapchain_queue, &new_instance, NULL);

            cvm_vk_swapchain_instance_initialise(new_instance, device, swapchain, instance ? instance->swapchain : VK_NULL_HANDLE);
            instance = new_instance;
        }

        cvm_vk_swapchain_cleanup_out_of_date_instances(swapchain, device);/// must be called after replacing the instance (require the out of date instances swapchain for recreation)


        if(swapchain->metering_fence_active)
        {
            /// wait for image to actually be acquired before acquiring another
            cvm_vk_wait_on_fence_and_reset(device, swapchain->metering_fence);/// should stall here
            swapchain->metering_fence_active=false;
        }


        acquire_semaphore = sol_vk_device_object_pool_semaphore_acquire(device);
        acquire_result = vkAcquireNextImageKHR(device->device, instance->swapchain, CVM_VK_DEFAULT_TIMEOUT, acquire_semaphore, swapchain->metering_fence, &image_index);
        instance->acquired_image_count++;

        if(acquire_result==VK_SUCCESS || acquire_result==VK_SUBOPTIMAL_KHR)
        {
            swapchain->metering_fence_active=true;

            if(acquire_result==VK_SUBOPTIMAL_KHR)
            {
                instance->out_of_date = true;
            }

            presentable_image = instance->presentable_images + image_index;

            if(presentable_image->state != CVM_VK_PRESENTABLE_IMAGE_STATE_READY)
            {
                // recycle the presented image (presented image existed, it needs to be cleaned up)
                assert(presentable_image->state == CVM_VK_PRESENTABLE_IMAGE_STATE_PRESENTED);
                instance->acquired_image_count--;
                sol_vk_device_object_pool_semaphore_release(device, presentable_image->acquire_semaphore);
            }

            presentable_image->acquire_semaphore = acquire_semaphore;
            presentable_image->state = CVM_VK_PRESENTABLE_IMAGE_STATE_ACQUIRED;
            presentable_image->layout = VK_IMAGE_LAYOUT_UNDEFINED;
            presentable_image->latest_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;
            presentable_image->latest_queue_family=CVM_INVALID_U32_INDEX;
        }
        else
        {
            instance->acquired_image_count--;
            sol_vk_device_object_pool_semaphore_release(device, acquire_semaphore);
            if(acquire_result != VK_TIMEOUT)
            {
                fprintf(stderr,"acquire_presentable_image failed -- unknown reason (%u)\n", acquire_result);
                instance->out_of_date = true;
            }
            fprintf(stderr,"acquire_presentable_image failed -- timeout\n");
        }
    }
    while(presentable_image == NULL);

    return presentable_image;
}





static VkCommandBuffer cvm_vk_swapchain_create_image_qfot_command_buffer(const cvm_vk_device * device, VkImage image, uint32_t src_queue_family, uint32_t dst_queue_family)
{
    VkCommandBuffer command_buffer;
    #warning mutex lock on device internal command pool (device no longer const?)
    #warning instead move command pool to the swapchain instance? only need 1 command pool! for the fallback queue family!
    VkCommandBufferAllocateInfo command_buffer_allocate_info=(VkCommandBufferAllocateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext=NULL,
        .commandPool=device->queue_families[dst_queue_family].internal_command_pool,
        .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount=1
    };

    CVM_VK_CHECK(vkAllocateCommandBuffers(device->device,&command_buffer_allocate_info,&command_buffer));

    VkCommandBufferBeginInfo command_buffer_begin_info=(VkCommandBufferBeginInfo)
    {
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext=NULL,
        .flags=0,///not one time use
        .pInheritanceInfo=NULL
    };

    VkDependencyInfo present_acquire_dependencies=
    {
        .sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext=NULL,
        .dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
        .memoryBarrierCount=0,
        .pMemoryBarriers=NULL,
        .bufferMemoryBarrierCount=0,
        .pBufferMemoryBarriers=NULL,
        .imageMemoryBarrierCount=1,
        .pImageMemoryBarriers=(VkImageMemoryBarrier2[1])
        {
            {
                .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext=NULL,
                .srcStageMask=0,/// from examles: no srcStage/AccessMask or dstStage/AccessMask is needed, waiting for a semaphore does that automatically.
                .srcAccessMask=0,
                .dstStageMask=VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,/// ??? what stage even is present? (stage and access can probably be 0, just being overly safe here)
                .dstAccessMask=VK_ACCESS_2_MEMORY_READ_BIT,
                .oldLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamilyIndex=src_queue_family,
                .dstQueueFamilyIndex=dst_queue_family,
                .image=image,
                .subresourceRange=(VkImageSubresourceRange)
                {
                    .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel=0,
                    .levelCount=1,
                    .baseArrayLayer=0,
                    .layerCount=1
                }
            }
        }
    };


    CVM_VK_CHECK(vkBeginCommandBuffer(command_buffer,&command_buffer_begin_info));

    vkCmdPipelineBarrier2(command_buffer,&present_acquire_dependencies);

    CVM_VK_CHECK(vkEndCommandBuffer(command_buffer));

    return command_buffer;
}

void cvm_vk_surface_swapchain_present_image(const cvm_vk_surface_swapchain * swapchain, const cvm_vk_device * device, cvm_vk_swapchain_presentable_image* presentable_image)
{
    #warning needs massive cleanup
    VkSemaphoreSubmitInfo wait_semaphores[1];
    VkSemaphoreSubmitInfo signal_semaphores[2];
    cvm_vk_device_queue_family * present_queue_family;
    cvm_vk_device_queue * present_queue;
    cvm_vk_swapchain_instance * swapchain_instance;
    const void* prev_vk_struct;
    VkResult result;


    swapchain_instance = presentable_image->parent_swapchain_instance;

    assert(presentable_image->latest_queue_family != CVM_INVALID_U32_INDEX);
    assert(presentable_image->layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    if(presentable_image->state == CVM_VK_PRESENTABLE_IMAGE_STATE_TRANSFERRED)
    {
        if(presentable_image->present_acquire_command_buffers[presentable_image->latest_queue_family]==VK_NULL_HANDLE)
        {
            #warning could allocate the command buffers upfront and only reset upon swapchain recreation...
            presentable_image->present_acquire_command_buffers[presentable_image->latest_queue_family] =
                cvm_vk_swapchain_create_image_qfot_command_buffer(device, presentable_image->image, presentable_image->latest_queue_family, swapchain_instance->fallback_present_queue_family);
        }


        present_queue_family = device->queue_families + swapchain_instance->fallback_present_queue_family;
        present_queue = present_queue_family->queues+0;/// use queue 0

        ///fixed count and layout of wait and signal semaphores here
        wait_semaphores[0]=cvm_vk_binary_semaphore_submit_info(presentable_image->qfot_semaphore,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

        presentable_image->latest_moment = sol_vk_timeline_semaphore_generate_moment(&present_queue->timeline);
        /// presentable_image->present_semaphore triggered either here or above when CVM_VK_PAYLOAD_LAST_SWAPCHAIN_USE, this path being taken when present queue != graphics queue
        signal_semaphores[0] = cvm_vk_binary_semaphore_submit_info(presentable_image->present_semaphore, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        signal_semaphores[1] = sol_vk_timeline_semaphore_moment_submit_info(&presentable_image->latest_moment, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        #warning REMOVE THIS ^^ (but... these both seem necessary?)
        #warning is waiting on the latest moment nonsense? i.e. is it invalid to ever actually orchistrate timing by waiting on this after present?

        VkSubmitInfo2 submit_info=(VkSubmitInfo2)
        {
            .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext=NULL,
            .flags=0,
            .waitSemaphoreInfoCount=1,///fixed number, set above
            .pWaitSemaphoreInfos=wait_semaphores,
            .commandBufferInfoCount=1,
            .pCommandBufferInfos=(VkCommandBufferSubmitInfo[1])
            {
                {
                    .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                    .pNext=NULL,
                    .commandBuffer=presentable_image->present_acquire_command_buffers[presentable_image->latest_queue_family],
                    .deviceMask=0
                }
            },
            .signalSemaphoreInfoCount=2,///fixed number, set above
            .pSignalSemaphoreInfos=signal_semaphores
        };

        CVM_VK_CHECK(vkQueueSubmit2(present_queue->queue, 1, &submit_info, VK_NULL_HANDLE));

        presentable_image->state = CVM_VK_PRESENTABLE_IMAGE_STATE_COMPLETE;
    }
    else
    {
        assert(swapchain_instance->queue_family_presentable_mask | (1 << presentable_image->latest_queue_family));///must be presentable on last used queue family
        present_queue_family = device->queue_families + presentable_image->latest_queue_family;
        present_queue = present_queue_family->queues + 0;/// use queue 0
    }

    assert(presentable_image->state == CVM_VK_PRESENTABLE_IMAGE_STATE_COMPLETE);



    /// wait on fence for PRIOR present if it has happened
    if(presentable_image->presentation_fence_active)
    {
        cvm_vk_wait_on_fence_and_reset(device, presentable_image->presentation_fence);
        presentable_image->presentation_fence_active = false;
    }



    prev_vk_struct = NULL;

    const VkSwapchainPresentFenceInfoEXT present_fence_info =
    {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
        .pNext = NULL,
        .swapchainCount = 1,
        .pFences = &presentable_image->presentation_fence,
    };

    if(device->feature_swapchain_maintainence)
    {
        prev_vk_struct = &present_fence_info;
    }

    const VkPresentInfoKHR present_info =
    {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = prev_vk_struct,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &presentable_image->present_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain_instance->swapchain,
        .pImageIndices = &presentable_image->index,
        .pResults = NULL
    };

    #warning present should be synchronised (check this is true) -- does present (or regular submission for that matter) need to be externally synchronised!?

    result = vkQueuePresentKHR(present_queue->queue, &present_info);

    presentable_image->state = CVM_VK_PRESENTABLE_IMAGE_STATE_PRESENTED;// this is fine right??

    switch(result)
    {
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
    case VK_ERROR_SURFACE_LOST_KHR:
        // above are technically errors, could do something different?
        fprintf(stderr,"PRESENTATION SUCCEEDED WITH RESULT : %d\n", result);
    case VK_SUBOPTIMAL_KHR:
        // for all above; the swapchain should be rebuilt
        swapchain_instance->out_of_date = true;
    case VK_SUCCESS:
        // for all above; the present actually happened and we need to wait on the fence
        presentable_image->presentation_fence_active = device->feature_swapchain_maintainence;
        break;
    default:
        fprintf(stderr,"PRESENTATION FAILED WITH ERROR : %d\n", result);
        assert(false);
        break;
    }
}













