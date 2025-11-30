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

#include "overlay/enums.h"
#include "overlay/render.h"



struct cvm_overlay_frame_resources
{
    #warning does this require a last use moment!?
    /// copy of target image view, used as key to the framebuffer cache
    VkImageView image_view;
    cvm_vk_resource_identifier image_view_unique_identifier;

    /// data to cache
    VkFramebuffer framebuffer;
};

#define SOL_LIMITED_CACHE_ENTRY_TYPE struct cvm_overlay_frame_resources
#define SOL_LIMITED_CACHE_KEY_TYPE struct cvm_overlay_target*
#define SOL_LIMITED_CACHE_FUNCTION_PREFIX cvm_overlay_frame_resources_cache
#define SOL_LIMITED_CACHE_STRUCT_NAME cvm_overlay_frame_resources_cache
#define SOL_LIMITED_CACHE_CMP_EQ( entry , key ) (entry->image_view_unique_identifier == key->image_view_unique_identifier) && (entry->image_view == key->image_view)
#include "data_structures/limited_cache.h"

/// needs a better name
struct cvm_overlay_target_resources
{
    /// used for finding extant resources in cache
    VkExtent2D extent;
    VkFormat format;
    VkColorSpaceKHR color_space;
    VkImageLayout initial_layout;
    VkImageLayout final_layout;
    bool clear_image;

    /// data to cache
    VkRenderPass render_pass;
    VkPipeline pipeline;

    /// rely on all frame resources being deleted to ensure not in use
    struct cvm_overlay_frame_resources_cache frame_resources;

    /// moment when this cache entry is no longer in use and can thus be evicted
    struct sol_vk_timeline_semaphore_moment last_use_moment;
};

#define SOL_QUEUE_ENTRY_TYPE struct cvm_overlay_target_resources
#define SOL_QUEUE_FUNCTION_PREFIX cvm_overlay_target_resources_queue
#define SOL_QUEUE_STRUCT_NAME cvm_overlay_target_resources_queue
#include "data_structures/queue.h"
/// queue as is done above might not be best if it's desirable to maintain multiple targets as renderable

/// resources used in a per cycle/frame fashion
struct cvm_overlay_transient_resources
{
    struct sol_vk_command_pool command_pool;

    VkDescriptorSet descriptor_set;

    struct sol_vk_timeline_semaphore_moment last_use_moment;

    bool active;
};

#define SOL_LIMITED_QUEUE_ENTRY_TYPE struct cvm_overlay_transient_resources
#define SOL_LIMITED_QUEUE_FUNCTION_PREFIX cvm_overlay_transient_resources_queue
#define SOL_LIMITED_QUEUE_STRUCT_NAME cvm_overlay_transient_resources_queue
#include "data_structures/limited_queue.h"

/// fixed sized queue of these?
/// queue init at runtime? (custom size)
/// make cache init at runtime too? (not great but w/e)

struct cvm_overlay_renderer
{
    /** for uploading to images, is NOT locally owned*/
    #warning instead provide these on calling it?
    struct sol_overlay_rendering_resources* overlay_render_context;
    struct sol_vk_staging_buffer* staging_buffer;

    struct cvm_overlay_transient_resources_queue transient_resources_queue;

    struct sol_overlay_render_batch render_batch;

    struct sol_overlay_render_persistent_resources persistent_rendering_resources;

    struct cvm_overlay_target_resources_queue target_resources;
};














static VkRenderPass cvm_overlay_render_pass_create(const cvm_vk_device * device,VkFormat target_format, VkImageLayout initial_layout, VkImageLayout final_layout, bool clear)
{
    VkRenderPass render_pass;
    VkResult created;

    VkRenderPassCreateInfo create_info=
    {
        .sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .attachmentCount=1,
        .pAttachments=(VkAttachmentDescription[1])
        {
            {
                .flags=0,
                .format=target_format,
                .samples=VK_SAMPLE_COUNT_1_BIT,
                .loadOp=clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp=VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout=initial_layout,
                .finalLayout=final_layout,
            },
        },
        .subpassCount=1,
        .pSubpasses=(VkSubpassDescription[1])
        {
            {
                .flags=0,
                .pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount=0,
                .pInputAttachments=NULL,
                .colorAttachmentCount=1,
                .pColorAttachments=(VkAttachmentReference[1])
                {
                    {
                        .attachment=0,
                        .layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    }
                },
                .pResolveAttachments=NULL,
                .pDepthStencilAttachment=NULL,
                .preserveAttachmentCount=0,
                .pPreserveAttachments=NULL
            },
        },
        .dependencyCount=2,
        .pDependencies=(VkSubpassDependency[2])
        {
            {
                .srcSubpass=VK_SUBPASS_EXTERNAL,
                .dstSubpass=0,
                .srcStageMask=VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask=VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask=VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask=VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT
            },
            {
                .srcSubpass=0,
                .dstSubpass=VK_SUBPASS_EXTERNAL,
                .srcStageMask=VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask=VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask=VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask=VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT
            },
        },
    };

    created = vkCreateRenderPass(device->device, &create_info, device->host_allocator, &render_pass);
    assert(created == VK_SUCCESS);

    return render_pass;
}

static VkFramebuffer cvm_overlay_framebuffer_create(const cvm_vk_device * device, VkExtent2D extent, VkRenderPass render_pass, VkImageView swapchain_image_view)
{
    VkFramebuffer framebuffer;
    VkResult created;

    VkFramebufferCreateInfo create_info=
    {
        .sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .renderPass=render_pass,
        .attachmentCount=1,
        .pAttachments=&swapchain_image_view,
        .width=extent.width,
        .height=extent.height,
        .layers=1
    };

    created=vkCreateFramebuffer(device->device, &create_info, device->host_allocator, &framebuffer);
    assert(created == VK_SUCCESS);

    return framebuffer;
}





static inline void cvm_overlay_frame_resources_initialise(struct cvm_overlay_frame_resources* frame_resources, const cvm_vk_device* device,
    const struct cvm_overlay_target_resources* target_resources, const struct cvm_overlay_target * target)
{
    frame_resources->image_view = target->image_view;
    frame_resources->image_view_unique_identifier = target->image_view_unique_identifier;

    frame_resources->framebuffer = cvm_overlay_framebuffer_create(device, target->extent, target_resources->render_pass, target->image_view);
}

static inline void cvm_overlay_frame_resources_terminate(struct cvm_overlay_frame_resources* frame_resources, const cvm_vk_device* device)
{
    vkDestroyFramebuffer(device->device, frame_resources->framebuffer, device->host_allocator);
}

static inline struct cvm_overlay_frame_resources* cvm_overlay_frame_resources_acquire(struct cvm_overlay_target_resources* target_resources, const cvm_vk_device * device, const struct cvm_overlay_target * target)
{
    struct cvm_overlay_frame_resources* frame_resources;
    enum sol_cache_result obtain_result;

    /// acquire framebuffer backing, framebuffer backing and mutable resources are intrinsically linked
    obtain_result = cvm_overlay_frame_resources_cache_obtain(&target_resources->frame_resources, target, &frame_resources);
    switch(obtain_result)
    {
        /** NOTE: intentional fallthrough */
    case SOL_CACHE_SUCCESS_REPLACED:
        cvm_overlay_frame_resources_terminate(frame_resources, device);
    case SOL_CACHE_SUCCESS_INSERTED:
        cvm_overlay_frame_resources_initialise(frame_resources, device, target_resources, target);
    default:;
    }

    return frame_resources;
}

static inline void cvm_overlay_frame_resources_release(struct cvm_overlay_frame_resources* frame_resources, struct sol_vk_timeline_semaphore_moment completion_moment)
{
    /// still exists in the cache, needn't do anything
}



static inline void cvm_overlay_target_resources_initialise(struct cvm_overlay_target_resources* target_resources, struct cvm_vk_device* device,
    const struct sol_overlay_render_persistent_resources* persistent_rendering_resources, const struct cvm_overlay_target * target)
{
    /// set cache key information
    target_resources->extent = target->extent;
    target_resources->format = target->format;
    target_resources->color_space = target->color_space;
    target_resources->initial_layout = target->initial_layout;
    target_resources->final_layout = target->final_layout;
    target_resources->clear_image = target->clear_image;

    target_resources->render_pass = cvm_overlay_render_pass_create(device, target->format, target->initial_layout, target->final_layout, target->clear_image);
    target_resources->pipeline = sol_overlay_render_pipeline_create(device, persistent_rendering_resources, target_resources->render_pass, target->extent, 0);/// subpass=0, b/c is only subpass

    cvm_overlay_frame_resources_cache_initialise(&target_resources->frame_resources, 8);

    target_resources->last_use_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;
}

static inline void cvm_overlay_target_resources_terminate(struct cvm_overlay_target_resources* target_resources, struct cvm_vk_device* device)
{
    struct cvm_overlay_frame_resources* frame_resources;

    assert(target_resources->last_use_moment.semaphore != VK_NULL_HANDLE);
    sol_vk_timeline_semaphore_moment_wait(&target_resources->last_use_moment, device);

    while((frame_resources = cvm_overlay_frame_resources_cache_evict_oldest(&target_resources->frame_resources)))
    {
        cvm_overlay_frame_resources_terminate(frame_resources, device);
    }
    cvm_overlay_frame_resources_cache_terminate(&target_resources->frame_resources);

    vkDestroyRenderPass(device->device, target_resources->render_pass, device->host_allocator);
    vkDestroyPipeline(device->device, target_resources->pipeline, device->host_allocator);
}

static inline bool cvm_overlay_target_resources_compatible_with_target(const struct cvm_overlay_target_resources* target_resources, const struct cvm_overlay_target * target)
{
    return target_resources != NULL &&
        target_resources->extent.width == target->extent.width &&
        target_resources->extent.height == target->extent.height &&
        target_resources->format == target->format &&
        target_resources->color_space == target->color_space &&
        target_resources->initial_layout == target->initial_layout &&
        target_resources->final_layout == target->final_layout &&
        target_resources->clear_image == target->clear_image;
}

static inline void cvm_overlay_target_resources_prune(struct cvm_overlay_renderer* renderer, struct cvm_vk_device * device)
{
    bool existing_resources;
    struct cvm_overlay_target_resources* target_resources;
    /// prune out of date resources
    while(renderer->target_resources.count > 1)
    {
        ///deletion queue, get the first entry ready to be deleted
        existing_resources = cvm_overlay_target_resources_queue_access_front(&renderer->target_resources, &target_resources);
        assert(existing_resources);
        assert(target_resources->last_use_moment.semaphore != VK_NULL_HANDLE);
        if(sol_vk_timeline_semaphore_moment_query(&target_resources->last_use_moment, device))
        {
            cvm_overlay_target_resources_terminate(target_resources, device);
            cvm_overlay_target_resources_queue_prune_front(&renderer->target_resources);
        }
        else break;
    }
}

#warning "target_resources" needs a better name
static inline struct cvm_overlay_target_resources* cvm_overlay_target_resources_acquire(struct cvm_overlay_renderer * renderer, struct cvm_vk_device* device, const struct cvm_overlay_target* target)
{
    bool existing_resources;
    struct cvm_overlay_target_resources* target_resources;

    existing_resources = cvm_overlay_target_resources_queue_access_back(&renderer->target_resources, &target_resources);
    if(!existing_resources || !cvm_overlay_target_resources_compatible_with_target(target_resources, target))
    {
        cvm_overlay_target_resources_queue_enqueue_ptr(&renderer->target_resources, &target_resources, NULL);

        cvm_overlay_target_resources_initialise(target_resources, device, &renderer->persistent_rendering_resources, target);
    }

    cvm_overlay_target_resources_prune(renderer, device);

    return target_resources;
}

static inline void cvm_overlay_target_resources_release(struct cvm_overlay_target_resources* target_resources, struct sol_vk_timeline_semaphore_moment completion_moment)
{
    target_resources->last_use_moment = completion_moment;
    /// still exists in the cache, needn't do anything more
}


static inline void cvm_overlay_add_target_acquire_instructions(struct sol_vk_command_buffer * command_buffer, const struct cvm_overlay_target * target)
{
    sol_vk_semaphore_submit_list_append_many(&command_buffer->wait_list, target->wait_semaphores, target->wait_semaphore_count);

    if(target->acquire_barrier_count)
    {
        VkDependencyInfo dependencies =
        {
            .sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext=NULL,
            .dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
            .memoryBarrierCount=0,
            .pMemoryBarriers=NULL,
            .bufferMemoryBarrierCount=0,
            .pBufferMemoryBarriers=NULL,
            .imageMemoryBarrierCount=target->acquire_barrier_count,
            .pImageMemoryBarriers=target->acquire_barriers,
        };

        vkCmdPipelineBarrier2(command_buffer->buffer, &dependencies);
    }
}

static inline void cvm_overlay_add_target_release_instructions(struct sol_vk_command_buffer * command_buffer, const struct cvm_overlay_target * target)
{
    sol_vk_semaphore_submit_list_append_many(&command_buffer->signal_list, target->signal_semaphores, target->signal_semaphore_count);

    if(target->release_barrier_count)
    {
        VkDependencyInfo dependencies =
        {
            .sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext=NULL,
            .dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
            .memoryBarrierCount=0,
            .pMemoryBarriers=NULL,
            .bufferMemoryBarrierCount=0,
            .pBufferMemoryBarriers=NULL,
            .imageMemoryBarrierCount=target->release_barrier_count,
            .pImageMemoryBarriers=target->release_barriers,
        };

        vkCmdPipelineBarrier2(command_buffer->buffer, &dependencies);
    }
}




/// may stall
static inline struct cvm_overlay_transient_resources* cvm_overlay_transient_resources_acquire(struct cvm_overlay_renderer* renderer, struct cvm_vk_device* device)
{
    struct cvm_overlay_transient_resources* transient_resources;
    if(cvm_overlay_transient_resources_queue_requeue_ptr(&renderer->transient_resources_queue, &transient_resources, NULL))
    {
        assert(!transient_resources->active);
        sol_vk_timeline_semaphore_moment_wait(&transient_resources->last_use_moment, device);

        /// reset resources
        sol_vk_command_pool_reset(&transient_resources->command_pool, device);
    }
    else
    {
        // enqueued, so need to init
        sol_vk_command_pool_initialise(&transient_resources->command_pool, device, device->graphics_queue_family_index, 0);
        transient_resources->descriptor_set = sol_overlay_render_descriptor_set_allocate(device, &renderer->persistent_rendering_resources);
    }
    transient_resources->last_use_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;
    transient_resources->active = true;

    return transient_resources;
}

static inline void cvm_overlay_transient_resources_release(struct cvm_overlay_transient_resources* transient_resources, struct sol_vk_timeline_semaphore_moment completion_moment)
{
    assert(transient_resources->active);
    assert(completion_moment.semaphore != VK_NULL_HANDLE);
    transient_resources->last_use_moment = completion_moment;
    transient_resources->active = false;
}







struct cvm_overlay_renderer* cvm_overlay_renderer_create(struct cvm_vk_device* device, struct sol_overlay_rendering_resources* overlay_render_context, struct sol_vk_staging_buffer* staging_buffer, uint32_t active_render_count)
{
    struct cvm_overlay_renderer* renderer;
    renderer = malloc(sizeof(struct cvm_overlay_renderer));

    sol_overlay_render_persistent_resources_initialise(&renderer->persistent_rendering_resources, device, active_render_count);

    /** 256k shunt buffer */
    sol_overlay_render_batch_initialise(&renderer->render_batch, device, 1<<18);

    cvm_overlay_transient_resources_queue_initialise(&renderer->transient_resources_queue, active_render_count);

    renderer->overlay_render_context = overlay_render_context;
    renderer->staging_buffer = staging_buffer;

    cvm_overlay_target_resources_queue_initialise(&renderer->target_resources, 16);

    return renderer;
}

void cvm_overlay_renderer_destroy(struct cvm_overlay_renderer* renderer, struct cvm_vk_device* device)
{
    struct cvm_overlay_target_resources * target_resources;
    struct cvm_overlay_transient_resources* transient_resources;

    while( cvm_overlay_transient_resources_queue_dequeue_ptr(&renderer->transient_resources_queue, &transient_resources))
    {
        assert(!transient_resources->active);
        sol_vk_timeline_semaphore_moment_wait(&transient_resources->last_use_moment, device);
        sol_vk_command_pool_terminate(&transient_resources->command_pool, device);
    }
    cvm_overlay_transient_resources_queue_terminate(&renderer->transient_resources_queue);


    while(cvm_overlay_target_resources_queue_dequeue_ptr(&renderer->target_resources, &target_resources))
    {
        cvm_overlay_target_resources_terminate(target_resources, device);
    }
    cvm_overlay_target_resources_queue_terminate(&renderer->target_resources);


    sol_overlay_render_batch_terminate(&renderer->render_batch);
    sol_overlay_render_persistent_resources_terminate(&renderer->persistent_rendering_resources, device);

    free(renderer);
}






struct sol_vk_timeline_semaphore_moment cvm_overlay_render_to_target(struct cvm_vk_device * device, struct cvm_overlay_renderer* renderer, struct sol_gui_context* gui_context, const struct cvm_overlay_target* target)
{
    struct sol_vk_command_buffer cb;
    struct sol_vk_timeline_semaphore_moment completion_moment;
//    sol_vk_staging_buffer_allocation staging_buffer_allocation;
//    VkDeviceSize upload_offset,instance_offset,uniform_offset,staging_space;
    struct sol_overlay_render_batch* render_batch;
    struct cvm_overlay_target_resources* target_resources;
    struct cvm_overlay_frame_resources* frame_resources;
    struct cvm_overlay_transient_resources* transient_resources;
    float screen_w,screen_h;



    render_batch = &renderer->render_batch;

    /// setup/reset the render batch
    sol_overlay_render_step_compose_elements(render_batch, gui_context, renderer->overlay_render_context, target->extent);




    /// move this somewhere?? theme data? allow non-destructive mutation based on these?
    #warning make it so this is used in uniform texel buffer, good to test that
    const float overlay_colours[SOL_OVERLAY_COLOUR_COUNT*4]=
    {
        1.0,0.1,0.1,1.0,/// SOL_OVERLAY_COLOUR_ERROR | SOL_OVERLAY_COLOUR_DEFAULT
        0.24,0.24,0.6,0.9,/// SOL_OVERLAY_COLOUR_BACKGROUND
        0.12,0.12,0.36,0.85,/// SOL_OVERLAY_COLOUR_MAIN
        0.16,0.16,0.40,0.85,/// SOL_OVERLAY_COLOUR_HIGHLIGHTED
        0.16,0.16,0.48,0.85,/// SOL_OVERLAY_COLOUR_FOCUSED
        0.4,0.6,0.9,0.8,/// SOL_OVERLAY_COLOUR_STANDARD_TEXT
        0.2,0.3,1.0,0.8,/// SOL_OVERLAY_COLOUR_COMPOSITION_TEXT
        0.4,0.6,0.9,0.3,/// SOL_OVERLAY_COLOUR_TEXT_SELECTION
    };


    /// acting on the shunt buffer directly in this way feels a little off

    transient_resources = cvm_overlay_transient_resources_acquire(renderer, device);
    target_resources = cvm_overlay_target_resources_acquire(renderer, device, target);
    frame_resources = cvm_overlay_frame_resources_acquire(target_resources, device, target);

    sol_vk_command_pool_acquire_command_buffer(&transient_resources->command_pool, device, &cb);

    sol_overlay_render_step_append_waits(render_batch, &cb.wait_list, VK_PIPELINE_STAGE_2_NONE);

    sol_overlay_render_step_write_descriptors(render_batch, device, renderer->staging_buffer, overlay_colours, transient_resources->descriptor_set);
    sol_overlay_render_step_submit_vk_transfers(render_batch, cb.buffer);

    sol_overlay_render_step_insert_vk_barriers(render_batch, cb.buffer);




    ///start of graphics
    cvm_overlay_add_target_acquire_instructions(&cb, target);


    VkRenderPassBeginInfo render_pass_begin_info=(VkRenderPassBeginInfo)
    {
        .sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext=NULL,
        .renderPass=target_resources->render_pass,
        .framebuffer=frame_resources->framebuffer,
        .renderArea=cvm_vk_get_default_render_area(),
        .clearValueCount=1,
        .pClearValues=(VkClearValue[1]){{.color=(VkClearColorValue){.float32={0.0f,0.0f,0.0f,0.0f}}}},
        /// ^ do we even wat to clear? or should we assume all pixels will be rendered?
    };

    vkCmdBeginRenderPass(cb.buffer,&render_pass_begin_info,VK_SUBPASS_CONTENTS_INLINE);///================

    sol_overlay_render_step_draw_elements(render_batch, &renderer->persistent_rendering_resources, target_resources->pipeline, cb.buffer);

    vkCmdEndRenderPass(cb.buffer);///================

    sol_overlay_render_step_append_signals(render_batch, &cb.signal_list, VK_PIPELINE_STAGE_2_NONE);


    cvm_overlay_add_target_release_instructions(&cb, target);

    completion_moment = sol_vk_command_pool_submit_command_buffer(&transient_resources->command_pool, device, &cb, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    sol_overlay_render_step_completion(render_batch, completion_moment);

    cvm_overlay_target_resources_release(target_resources, completion_moment);
    cvm_overlay_frame_resources_release(frame_resources, completion_moment);
    cvm_overlay_transient_resources_release(transient_resources, completion_moment);

    return completion_moment;
}


struct sol_vk_timeline_semaphore_moment cvm_overlay_render_to_presentable_image(struct cvm_vk_device * device, struct cvm_overlay_renderer* renderer, struct sol_gui_context* gui_context, struct cvm_vk_swapchain_presentable_image* presentable_image, bool last_use)
{
    uint32_t overlay_queue_family_index;
    struct sol_vk_timeline_semaphore_moment completion_moment;

    bool first_use = (presentable_image->layout == VK_IMAGE_LAYOUT_UNDEFINED);

    struct cvm_overlay_target target =
    {
        .image_view = presentable_image->image_view,
        .image_view_unique_identifier = presentable_image->image_view_unique_identifier,

        .extent = presentable_image->parent_swapchain_instance->surface_capabilities.currentExtent,
        .format = presentable_image->parent_swapchain_instance->surface_format.format,
        .color_space = presentable_image->parent_swapchain_instance->surface_format.colorSpace,

        .initial_layout = presentable_image->layout,
        .final_layout = last_use ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .clear_image = first_use,

        .wait_semaphore_count = 0,
        .acquire_barrier_count = 0,

        .signal_semaphore_count = 0,
        .release_barrier_count = 0,
    };


    #warning genericize the following with a function (in swapchain)

    if(first_use)/// can figure out from state
    {
        assert(presentable_image->state == CVM_VK_PRESENTABLE_IMAGE_STATE_ACQUIRED);///also indicates first use
        presentable_image->state = CVM_VK_PRESENTABLE_IMAGE_STATE_STARTED;
        target.wait_semaphores[target.wait_semaphore_count++] = cvm_vk_binary_semaphore_submit_info(presentable_image->acquire_semaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    }
    else
    {
        #warning technically need to handle QFOT here! but thats quite painful, otherwise need to handle (wait on) any other use of this image
    }

    assert(presentable_image->state == CVM_VK_PRESENTABLE_IMAGE_STATE_STARTED);

    #warning presentable image management part of swapchain's functionality
    if(last_use)/// don't have a perfect way to figure out, but could infer from
    {
        overlay_queue_family_index = device->graphics_queue_family_index;

        assert(presentable_image->parent_swapchain_instance->queue_family_presentable_mask | (1<<overlay_queue_family_index));// must be able to present on this queue family

        presentable_image->state = CVM_VK_PRESENTABLE_IMAGE_STATE_COMPLETE;
        /// signal after any stage that could modify the image contents
        target.signal_semaphores[target.signal_semaphore_count++] = cvm_vk_binary_semaphore_submit_info(presentable_image->present_semaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    }

    completion_moment = cvm_overlay_render_to_target(device, renderer, gui_context, &target);

    presentable_image->latest_moment = completion_moment;
    presentable_image->layout = target.final_layout;
    #warning may want to make above a function (like the whole block handling QFOT)
    /// must record changes made to layout

    return completion_moment;
}


