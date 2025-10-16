/**
Copyright 2025 Carl van Mastrigt

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

#include <inttypes.h>

#include "math/s16_rect.h"
#include "data_structures/buffer.h"
#include "vk/image.h"
#include "vk/image_utils.h"
#include "vk/staging_buffer.h"
#include "vk/image_atlas.h"

#warning important to outline how bytes are used
/** note, 32 bytes total */
struct sol_overlay_render_element
{
    int16_t pos_rect[4];/// start(x,y), end(x,y)
    uint16_t tex_coords[4];/// base_tex(x,y), mask_tex(x,y)
    uint16_t other_data[4];// extra data - texture_id:2
    uint16_t idk[4];// extra data - texture_id:2
};

#define SOL_STACK_ENTRY_TYPE struct sol_overlay_render_element
#define SOL_STACK_FUNCTION_PREFIX sol_overlay_render_element_list
#define SOL_STACK_STRUCT_NAME sol_overlay_render_element_list
#include "data_structures/stack.h"

enum sol_overlay_image_atlas_type
{
    SOL_OVERLAY_IMAGE_ATLAS_TYPE_BC4,
    SOL_OVERLAY_IMAGE_ATLAS_TYPE_R8_UNORM,
    SOL_OVERLAY_IMAGE_ATLAS_TYPE_RGBA8_UNORM,
    SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT,
};


/** rename(?) static resources? base resources? */
/** these are the resources that need only be set up once */
struct sol_overlay_render_persistent_resources
{
    /** pool sized to fit the descriptor set layout that will be used, for as many sets as will be used */
    VkDescriptorPool descriptor_pool;

    VkDescriptorSet* descriptor_sets;
    uint32_t total_set_count;
    uint32_t available_set_count;

    /** for creating the render pipeline */
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineShaderStageCreateInfo vertex_pipeline_stage;
    VkPipelineShaderStageCreateInfo fragment_pipeline_stage;
};

/** this should only be called once at program start, only need one sol_overlay_render_resources */
VkResult sol_overlay_render_persistent_resources_initialise(struct sol_overlay_render_persistent_resources* persistent_resources, struct cvm_vk_device* device, uint32_t active_render_count);
void sol_overlay_render_persistent_resources_terminate(struct sol_overlay_render_persistent_resources* persistent_resources, struct cvm_vk_device* device);

VkDescriptorSet sol_overlay_render_descriptor_set_acquire(struct sol_overlay_render_persistent_resources* persistent_resources);
void sol_overlay_render_descriptor_set_release(struct sol_overlay_render_persistent_resources* persistent_resources, VkDescriptorSet set);

/** the pipeline is the only resource that needs to change when the window changes (may want to change this for compositing purposes!) */
#warning consider make resolution dynamic state for the purposes of compositing! - removes need for this AND allows compositing with different resolutions
/** maybe a hybrid approach is warranted, if dynamic pipelines cannot be guaranteed... or a solution that takes advantage of a potential compositing image atlas... */
struct sol_overlay_pipeline
{
    #warning remove this and make rendering dynamic, seriously
    VkPipeline pipeline;
    VkExtent2D extent;
};


/** render pass and subpass are intentionally not managed here */
VkResult sol_overlay_render_pipeline_initialise(struct sol_overlay_pipeline* pipeline, struct cvm_vk_device* device, const struct sol_overlay_render_persistent_resources* persistent_resources, VkRenderPass render_pass, VkExtent2D extent, uint32_t subpass);
void sol_overlay_render_pipeline_terminate(struct sol_overlay_pipeline* pipeline, struct cvm_vk_device* device);



#warning rename to sol_overlay_rendering_resources
struct sol_overlay_rendering_resources
{
    /** atlases used for storing backing information for verious purposes
     * some of these may be null depending on implementation details
     * these will, in order, match bind points in shader that renders overlay elements */
    // struct sol_image_atlas* bc4_atlas;
    // struct sol_image_atlas* r8_atlas;
    // struct sol_image_atlas* rgba8_atlas;
    struct sol_image_atlas* atlases[SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT];
};


/** batch is a bad name, need context, sub context stack/ranges for (potential) compositing passes
 * at that point is it maybe better to just handle the backing manually? */
struct sol_overlay_render_batch
{
    /** unowned, held only for the duration of `sol_overlay_render_step_compose_elements`
     * type safety here could be a good reason to provide a wrapped atlas type for access purposes */
    struct sol_overlay_rendering_resources* rendering_resources;

    /** bounds/limit of the current point in the render, this is almost a stack allocated value as it can change at any point in the render and that must then be carried forward */
    s16_rect bounds;

    /** actual UI element instance data */
    struct sol_overlay_render_element_list elements;

    /** this and count should be in an array per composite range */
    VkDeviceSize element_offset;


    /** miscellaneous inline upload buffer */
    struct sol_buffer upload_buffer;

    /** copy/upload lists that will be provided to the image atlas for scope setup */
    struct sol_vk_buf_img_copy_list atlas_copy_lists[SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT];
    /** offset applied to all copies */
    VkDeviceSize upload_offset;

    struct sol_vk_timeline_semaphore_moment atlas_acquire_moments[SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT];
    struct sol_vk_timeline_semaphore_moment atlas_release_moments[SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT];

    struct sol_vk_supervised_image* atlas_supervised_images[SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT];

    /** this is just a data container for when the staging allocayion is made */
    struct sol_vk_staging_buffer_allocation staging_buffer_allocation;

    /** following variables tracked from point where they become necessary to prevent needing to provide them multiple times */

    /** extent set at compose stage */
    VkExtent2D target_extent;
    VkDescriptorSet descriptor_set;
    /*staging buffer provided to `sol_overlay_render_step_write_descriptors`, used to track the release of this allocation properly */
    struct sol_vk_staging_buffer* staging_buffer;
};


void sol_overlay_render_batch_initialise(struct sol_overlay_render_batch* batch, struct cvm_vk_device* device, VkDeviceSize shunt_buffer_size);
void sol_overlay_render_batch_terminate(struct sol_overlay_render_batch* batch);


/** step : traverse the widget tree, creating the commands to render each element and loading resources (or creating instructions to load resources) when a requirement is encountered */
void sol_overlay_render_step_compose_elements(struct sol_overlay_render_batch* batch, struct sol_gui_context* gui_context, struct sol_overlay_rendering_resources* render_context, VkExtent2D target_extent);

/** step : move all resources (e.g. new image atlas pixel data) and draw instructions to staging and write the descriptors for resources rendering will use */
void sol_overlay_render_step_write_descriptors(struct sol_overlay_render_batch* batch, struct cvm_vk_device* device, struct sol_vk_staging_buffer* staging_buffer, const float* colour_array, VkDescriptorSet descriptor_set);

/** step : add the resource(atlas) management semaphores that must be waited on to the list that will be waited on before work is executed (likely the command buffer wait list) */
void sol_overlay_render_step_append_waits(struct sol_overlay_render_batch* batch, struct sol_vk_semaphore_submit_list* wait_list, VkPipelineStageFlags2 combined_stage_masks);

/** step : perform all copy operations (if any) necessary to set up data (e.g. copy image pixels from staging to the image atlas) */
void sol_overlay_render_step_submit_vk_transfers(struct sol_overlay_render_batch* batch, VkCommandBuffer command_buffer);

/** step : write barriers to put required resources (e.g. image atlases) in correct state to e read from while drawing overlay elements to the render target */
void sol_overlay_render_step_insert_vk_barriers(struct sol_overlay_render_batch* batch, VkCommandBuffer command_buffer);

/** step : encode the required draw commands to a command buffer, the render target/pass for which this applies must be set up externally */
void sol_overlay_render_step_draw_elements(struct sol_overlay_render_batch* batch, struct sol_overlay_render_persistent_resources* persistent_resources, struct sol_overlay_pipeline* pipeline, VkCommandBuffer command_buffer);

/** step : add the resource(atlas) management semaphores that must be signalled to the list that will be signalled when work is executed (likely the command buffer signal list) */
void sol_overlay_render_step_append_signals(struct sol_overlay_render_batch* batch, struct sol_vk_semaphore_submit_list* signal_list, VkPipelineStageFlags2 combined_stage_masks);

/** step : when rendering is known to have completed (e.g. after the render pass in which the render was submitted) use a semaphore moment to synchronise/signal the completion of rendering */
void sol_overlay_render_step_completion(struct sol_overlay_render_batch* batch, struct sol_vk_timeline_semaphore_moment completion_moment);
