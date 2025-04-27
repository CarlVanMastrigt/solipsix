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




struct cvm_overlay_frame_resources
{
    #warning does this require a last use moment!?
    /// copy of target image view, used as key to the framebuffer cache
    VkImageView image_view;
    cvm_vk_resource_identifier image_view_unique_identifier;

    /// data to cache
    VkFramebuffer framebuffer;
};

#define CVM_CACHE_CMP( entry , key ) (entry->image_view_unique_identifier == key->image_view_unique_identifier) && (entry->image_view == key->image_view)
CVM_CACHE(struct cvm_overlay_frame_resources, struct cvm_overlay_target*, cvm_overlay_frame_resources)
#undef CVM_CACHE_CMP

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
    struct cvm_overlay_pipeline pipeline_;

    /// rely on all frame resources being deleted to ensure not in use
    cvm_overlay_frame_resources_cache frame_resources;

    /// moment when this cache entry is no longer in use and can thus be evicted
    struct sol_vk_timeline_semaphore_moment last_use_moment;
};

SOL_QUEUE(struct cvm_overlay_target_resources, cvm_overlay_target_resources_queue, cvm_overlay_target_resources_queue)
/// queue as is done above might not be best if it's desirable to maintain multiple targets as renderable

/// resources used in a per cycle/frame fashion
struct cvm_overlay_transient_resources
{
    cvm_vk_command_pool command_pool;

    VkDescriptorSet descriptor_set;

    struct sol_vk_timeline_semaphore_moment last_use_moment;
};

SOL_QUEUE(struct cvm_overlay_transient_resources*, cvm_overlay_transient_resources_queue, cvm_overlay_transient_resources_queue)

/// fixed sized queue of these?
/// queue init at runtime? (custom size)
/// make cache init at runtime too? (not great but w/e)

struct cvm_overlay_renderer
{
    /// for uploading to images, is NOT locally owned
    struct sol_vk_staging_buffer* staging_buffer;


    uint32_t transient_count;
    uint32_t transient_count_initialised;
    struct cvm_overlay_transient_resources* transient_resources_backing;
    struct cvm_overlay_transient_resources_queue transient_resources_queue;

    struct cvm_overlay_render_batch render_batch;

    struct cvm_overlay_rendering_resources rendering_resources;

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
    bool evicted;

    frame_resources = cvm_overlay_frame_resources_cache_find(&target_resources->frame_resources, target);
    if(frame_resources==NULL)
    {
        frame_resources = cvm_overlay_frame_resources_cache_new(&target_resources->frame_resources, &evicted);
        if(evicted)
        {
            cvm_overlay_frame_resources_terminate(frame_resources, device);
        }
        cvm_overlay_frame_resources_initialise(frame_resources, device, target_resources, target);
    }

    return frame_resources;
}

static inline void cvm_overlay_frame_resources_release(struct cvm_overlay_frame_resources* frame_resources, struct sol_vk_timeline_semaphore_moment completion_moment)
{
    /// still exists in the cache, needn't do anything
}



static inline void cvm_overlay_target_resources_initialise(struct cvm_overlay_target_resources* target_resources, const cvm_vk_device* device,
    const struct cvm_overlay_rendering_resources* rendering_resources, const struct cvm_overlay_target * target)
{
    /// set cache key information
    target_resources->extent = target->extent;
    target_resources->format = target->format;
    target_resources->color_space = target->color_space;
    target_resources->initial_layout = target->initial_layout;
    target_resources->final_layout = target->final_layout;
    target_resources->clear_image = target->clear_image;

    target_resources->render_pass = cvm_overlay_render_pass_create(device, target->format, target->initial_layout, target->final_layout, target->clear_image);
    cvm_overlay_render_pipeline_initialise(&target_resources->pipeline_, device, rendering_resources, target_resources->render_pass, target->extent, 0);/// subpass=0, b/c is only subpass

    cvm_overlay_frame_resources_cache_initialise(&target_resources->frame_resources, 8);

    target_resources->last_use_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;
}

static inline void cvm_overlay_target_resources_terminate(struct cvm_overlay_target_resources* target_resources, const cvm_vk_device* device)
{
    struct cvm_overlay_frame_resources* frame_resources;

    assert(target_resources->last_use_moment.semaphore != VK_NULL_HANDLE);
    sol_vk_timeline_semaphore_moment_wait(&target_resources->last_use_moment, device);

    while((frame_resources = cvm_overlay_frame_resources_cache_evict(&target_resources->frame_resources)))
    {
        cvm_overlay_frame_resources_terminate(frame_resources, device);
    }
    cvm_overlay_frame_resources_cache_terminate(&target_resources->frame_resources);

    vkDestroyRenderPass(device->device, target_resources->render_pass, device->host_allocator);
    cvm_overlay_render_pipeline_terminate(&target_resources->pipeline_, device);
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

static inline void cvm_overlay_target_resources_prune(cvm_overlay_renderer * renderer, const cvm_vk_device * device)
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
static inline struct cvm_overlay_target_resources* cvm_overlay_target_resources_acquire(cvm_overlay_renderer * renderer, const cvm_vk_device * device, const struct cvm_overlay_target * target)
{
    bool existing_resources;
    struct cvm_overlay_target_resources* target_resources;

    existing_resources = cvm_overlay_target_resources_queue_access_back(&renderer->target_resources, &target_resources);
    if(!existing_resources || !cvm_overlay_target_resources_compatible_with_target(target_resources, target))
    {
        cvm_overlay_target_resources_queue_enqueue_ptr(&renderer->target_resources, &target_resources, NULL);

        cvm_overlay_target_resources_initialise(target_resources, device, &renderer->rendering_resources, target);
    }

    cvm_overlay_target_resources_prune(renderer, device);

    return target_resources;
}

static inline void cvm_overlay_target_resources_release(struct cvm_overlay_target_resources* target_resources, struct sol_vk_timeline_semaphore_moment completion_moment)
{
    target_resources->last_use_moment = completion_moment;
    /// still exists in the cache, needn't do anything more
}


static inline void cvm_overlay_add_target_acquire_instructions(cvm_vk_command_buffer * command_buffer, const struct cvm_overlay_target * target)
{
    cvm_vk_command_buffer_add_wait_info(command_buffer, target->wait_semaphores, target->wait_semaphore_count);

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

static inline void cvm_overlay_add_target_release_instructions(cvm_vk_command_buffer * command_buffer, const struct cvm_overlay_target * target)
{
    cvm_vk_command_buffer_add_signal_info(command_buffer, target->signal_semaphores, target->signal_semaphore_count);

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


static inline void cvm_overlay_transient_resources_initialise(struct cvm_overlay_transient_resources* transient_resources, const cvm_vk_device* device, const cvm_overlay_renderer * renderer)
{
    VkResult result;
    transient_resources->last_use_moment = SOL_VK_TIMELINE_SEMAPHORE_MOMENT_NULL;

    cvm_vk_command_pool_initialise(&transient_resources->command_pool, device, device->graphics_queue_family_index, 0);
    result = cvm_overlay_descriptor_set_fetch(device, &renderer->rendering_resources, &transient_resources->descriptor_set);
}

static inline void cvm_overlay_transient_resources_terminate(struct cvm_overlay_transient_resources* transient_resources, const cvm_vk_device* device)
{
    sol_vk_timeline_semaphore_moment_wait(&transient_resources->last_use_moment, device);
    cvm_vk_command_pool_terminate(&transient_resources->command_pool, device);
}

/// may stall
static inline struct cvm_overlay_transient_resources* cvm_overlay_transient_resources_acquire(cvm_overlay_renderer * renderer, const cvm_vk_device* device)
{
    struct cvm_overlay_transient_resources* transient_resources;
    bool dequeued;

    if(renderer->transient_count_initialised < renderer->transient_count)
    {
        transient_resources = renderer->transient_resources_backing + renderer->transient_count_initialised;

        cvm_overlay_transient_resources_initialise(transient_resources, device, renderer);

        renderer->transient_count_initialised++;
    }
    else
    {
        dequeued = cvm_overlay_transient_resources_queue_dequeue(&renderer->transient_resources_queue, &transient_resources);
        assert(dequeued);

        sol_vk_timeline_semaphore_moment_wait(&transient_resources->last_use_moment, device);

        /// reset resources
        cvm_vk_command_pool_reset(&transient_resources->command_pool, device);
    }

    return transient_resources;
}

static inline void cvm_overlay_transient_resources_release(cvm_overlay_renderer * renderer, struct cvm_overlay_transient_resources* transient_resources, struct sol_vk_timeline_semaphore_moment completion_moment)
{
    transient_resources->last_use_moment = completion_moment;
    cvm_overlay_transient_resources_queue_enqueue(&renderer->transient_resources_queue, transient_resources, NULL);
}







struct cvm_overlay_renderer* cvm_overlay_renderer_create(const struct cvm_vk_device* device, struct sol_vk_staging_buffer* staging_buffer, uint32_t active_render_count)
{
    cvm_overlay_renderer * renderer;
    renderer = malloc(sizeof(struct cvm_overlay_renderer));

    cvm_overlay_rendering_resources_initialise(&renderer->rendering_resources, device, active_render_count);

    cvm_overlay_render_batch_initialise(&renderer->render_batch, device, 1<<18);

    renderer->transient_count = active_render_count;
    renderer->transient_count_initialised = 0;
    renderer->transient_resources_backing = malloc(sizeof(struct cvm_overlay_transient_resources) * active_render_count);
    cvm_overlay_transient_resources_queue_initialise(&renderer->transient_resources_queue, 16);

    renderer->staging_buffer = staging_buffer;

    cvm_overlay_target_resources_queue_initialise(&renderer->target_resources, 16);

    return renderer;
}

void cvm_overlay_renderer_destroy(struct cvm_overlay_renderer* renderer, const struct cvm_vk_device* device)
{
    struct cvm_overlay_target_resources * target_resources;
    struct cvm_overlay_transient_resources* transient_resources;

    while( cvm_overlay_transient_resources_queue_dequeue(&renderer->transient_resources_queue, &transient_resources))
    {
        cvm_overlay_transient_resources_terminate(transient_resources, device);
    }
    cvm_overlay_transient_resources_queue_terminate(&renderer->transient_resources_queue);
    free(renderer->transient_resources_backing);


    while(cvm_overlay_target_resources_queue_dequeue_ptr(&renderer->target_resources, &target_resources))
    {
        cvm_overlay_target_resources_terminate(target_resources, device);
    }
    cvm_overlay_target_resources_queue_terminate(&renderer->target_resources);


    cvm_overlay_render_batch_terminate(&renderer->render_batch);
    cvm_overlay_rendering_resources_terminate(&renderer->rendering_resources, device);

    free(renderer);
}






struct sol_vk_timeline_semaphore_moment cvm_overlay_render_to_target(const cvm_vk_device * device, cvm_overlay_renderer * renderer, struct cvm_overlay_image_atlases* image_atlases, struct sol_gui_context* gui_context, const struct cvm_overlay_target* target)
{
    cvm_vk_command_buffer cb;
    struct sol_vk_timeline_semaphore_moment completion_moment;
//    sol_vk_staging_buffer_allocation staging_buffer_allocation;
//    VkDeviceSize upload_offset,instance_offset,uniform_offset,staging_space;
    struct cvm_overlay_render_batch* render_batch;
    struct sol_vk_staging_buffer* staging_buffer;
    struct cvm_overlay_target_resources* target_resources;
    struct cvm_overlay_frame_resources* frame_resources;
    struct cvm_overlay_transient_resources* transient_resources;
    VkDeviceSize staging_offset;
    char * staging_mapping;
    float screen_w,screen_h;

    staging_buffer = renderer->staging_buffer;

    render_batch = &renderer->render_batch;

    /// setup/reset the render batch
    cvm_overlay_render_batch_build(render_batch, gui_context, image_atlases, target->extent);



    /// move this somewhere?? theme data? allow non-destructive mutation based on these?
    #warning make it so this is used in uniform texel buffer, good to test that
    const float overlay_colours[OVERLAY_NUM_COLOURS*4]=
    {
        1.0,0.1,0.1,1.0,///OVERLAY_NO_COLOUR (error)
        0.24,0.24,0.6,0.9,///OVERLAY_BACKGROUND_COLOUR
        0.12,0.12,0.36,0.85,///OVERLAY_MAIN_COLOUR
        0.12,0.12,0.48,0.85,///OVERLAY_MAIN_ALTERNATE_COLOUR
        0.3,0.3,1.0,0.2,///OVERLAY_HIGHLIGHTING_COLOUR
        0.4,0.6,0.9,0.3,///OVERLAY_TEXT_HIGHLIGHT_COLOUR
        0.2,0.3,1.0,0.8,///OVERLAY_TEXT_COMPOSITION_COLOUR_0
        0.4,0.6,0.9,0.8,///OVERLAY_TEXT_COLOUR_0
    };


    /// acting on the shunt buffer directly in this way feels a little off

    transient_resources = cvm_overlay_transient_resources_acquire(renderer, device);
    target_resources = cvm_overlay_target_resources_acquire(renderer, device, target);
    frame_resources = cvm_overlay_frame_resources_acquire(target_resources, device, target);

    cvm_vk_command_pool_acquire_command_buffer(&transient_resources->command_pool, device, &cb);

    ///this uses the shunt buffer!

    cvm_overlay_render_batch_stage(render_batch, device, staging_buffer, overlay_colours, transient_resources->descriptor_set);
    cvm_overlay_render_batch_upload(render_batch, cb.buffer);

    cvm_overlay_render_batch_ready(render_batch, cb.buffer);




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

    cvm_overlay_render_batch_render(render_batch, &renderer->rendering_resources, &target_resources->pipeline_, cb.buffer);

    vkCmdEndRenderPass(cb.buffer);///================


    cvm_overlay_add_target_release_instructions(&cb, target);

    completion_moment = cvm_vk_command_pool_submit_command_buffer(&transient_resources->command_pool, device, &cb, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    cvm_overlay_render_batch_finish(render_batch, completion_moment);

    cvm_overlay_target_resources_release(target_resources, completion_moment);
    cvm_overlay_frame_resources_release(frame_resources, completion_moment);
    cvm_overlay_transient_resources_release(renderer, transient_resources, completion_moment);

    return completion_moment;
}


struct sol_vk_timeline_semaphore_moment cvm_overlay_render_to_presentable_image(const cvm_vk_device * device, cvm_overlay_renderer * renderer, struct cvm_overlay_image_atlases* image_atlases, struct sol_gui_context* gui_context, cvm_vk_swapchain_presentable_image * presentable_image, bool last_use)
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

    completion_moment = cvm_overlay_render_to_target(device, renderer, image_atlases, gui_context, &target);

    presentable_image->latest_moment = completion_moment;
    presentable_image->layout = target.final_layout;
    #warning may want to make above a function (like the whole block handling QFOT)
    /// must record changes made to layout

    return completion_moment;
}


