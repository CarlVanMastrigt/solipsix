/**
Copyright 2020,2021,2022,2023,2025 Carl van Mastrigt

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
#include <assert.h>
#include <vulkan/vulkan_core.h>

#include "gui/object.h"
#include "overlay/render.h"


static VkResult sol_overlay_descriptor_pool_create(VkDescriptorPool* pool, struct cvm_vk_device * device, uint32_t active_render_count)
{
    *pool = VK_NULL_HANDLE;

    VkDescriptorPoolCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,///by not specifying individual free must reset whole pool (which is fine)
        .maxSets = active_render_count,
        .poolSizeCount = 2,
        .pPoolSizes=(VkDescriptorPoolSize[2])
        {
            {
                .type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,///VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER probably preferable here...
                .descriptorCount = active_render_count
            },
            {
                .type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT * active_render_count
            },
        }
    };

    return vkCreateDescriptorPool(device->device, &create_info, device->host_allocator, pool);
}


#warning why again was it that images got incorporated into a per-frame layout rather than being shared? (was it just for simplicity?)
static VkResult sol_overlay_descriptor_set_layout_create(VkDescriptorSetLayout* set_layout, struct cvm_vk_device * device)
{
    uint32_t i;
    VkSampler immutable_samplers[SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT];

    *set_layout = VK_NULL_HANDLE;

    for(i = 0; i < SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT; i++)
    {
        /** everything should use nearest/fetch sampler */
        immutable_samplers[i] = device->defaults.fetch_sampler;
    }

    VkDescriptorSetLayoutCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .bindingCount = 2,
        .pBindings = (VkDescriptorSetLayoutBinding[2])
        {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,///VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER probably preferable here...
                .descriptorCount = SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = immutable_samplers,
                #warning ^^ also test w/ null & setting samplers directly, probably MUCH preferred
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,///VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER probably preferable here...
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL,
            }
        },
    };

    return vkCreateDescriptorSetLayout(device->device, &create_info, device->host_allocator, set_layout);
}

static VkResult sol_overlay_pipeline_layout_create(VkPipelineLayout* pipeline_layout, struct cvm_vk_device * device, VkDescriptorSetLayout descriptor_set_layout)
{
    *pipeline_layout = VK_NULL_HANDLE;

    VkPipelineLayoutCreateInfo create_info=
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = (VkDescriptorSetLayout[1])
        {
            descriptor_set_layout,
        },
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = (VkPushConstantRange[1])
        {
            {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = sizeof(float)*2,
            }
        }
    };

    return vkCreatePipelineLayout(device->device, &create_info, device->host_allocator, pipeline_layout);
}

void sol_overlay_rendering_resources_default_initialise(struct sol_overlay_rendering_resources* overlay_rendering_resources, struct cvm_vk_device* device)
{
    const struct sol_image_atlas_description bc4_atlas_description =
    {
        .format = VK_FORMAT_BC4_UNORM_BLOCK,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .image_array_dimension = 1,
        .image_x_dimension_exponent = 11,
        .image_y_dimension_exponent = 11,
    };

    const struct sol_image_atlas_description r8_atlas_description =
    {
        .format = VK_FORMAT_R8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .image_array_dimension = 1,
        .image_x_dimension_exponent = 11,
        .image_y_dimension_exponent = 11,
    };

    const struct sol_image_atlas_description rgba8_atlas_description =
    {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .image_array_dimension = 1,
        .image_x_dimension_exponent = 11,
        .image_y_dimension_exponent = 11,
    };

    *overlay_rendering_resources = (struct sol_overlay_rendering_resources)
    {
        .atlases =
        {
            [SOL_OVERLAY_IMAGE_ATLAS_TYPE_BC4]         = sol_image_atlas_create(&bc4_atlas_description  , device),
            [SOL_OVERLAY_IMAGE_ATLAS_TYPE_R8_UNORM]    = sol_image_atlas_create(&r8_atlas_description   , device),
            [SOL_OVERLAY_IMAGE_ATLAS_TYPE_RGBA8_UNORM] = sol_image_atlas_create(&rgba8_atlas_description, device)
        }
    };
}
void sol_overlay_rendering_resources_terminate(struct sol_overlay_rendering_resources* overlay_rendering_resources, struct cvm_vk_device* device)
{
    sol_image_atlas_destroy(overlay_rendering_resources->atlases[SOL_OVERLAY_IMAGE_ATLAS_TYPE_BC4]        , device);
    sol_image_atlas_destroy(overlay_rendering_resources->atlases[SOL_OVERLAY_IMAGE_ATLAS_TYPE_R8_UNORM]   , device);
    sol_image_atlas_destroy(overlay_rendering_resources->atlases[SOL_OVERLAY_IMAGE_ATLAS_TYPE_RGBA8_UNORM], device);
}

VkResult sol_overlay_render_persistent_resources_initialise(struct sol_overlay_render_persistent_resources* persistent_resources, struct cvm_vk_device* device, uint32_t active_render_count)
{
    VkResult result;


    result = sol_overlay_descriptor_pool_create(&persistent_resources->descriptor_pool, device, active_render_count);
    assert(result == VK_SUCCESS);

    result = sol_overlay_descriptor_set_layout_create(&persistent_resources->descriptor_set_layout, device);
    assert(result == VK_SUCCESS);

    result = sol_overlay_pipeline_layout_create(&persistent_resources->pipeline_layout, device, persistent_resources->descriptor_set_layout);
    assert(result == VK_SUCCESS);

    sol_vk_limited_descriptor_set_stack_initialise(&persistent_resources->descriptor_sets, active_render_count);


    if(device->feature_int_16_shader_types)
    {
        sol_vk_create_shader_stage_info(&persistent_resources->vertex_pipeline_stage,   device, "solipsix/shaders/overlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        sol_vk_create_shader_stage_info(&persistent_resources->fragment_pipeline_stage, device, "solipsix/shaders/overlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    else
    {
        sol_vk_create_shader_stage_info(&persistent_resources->vertex_pipeline_stage,   device, "solipsix/shaders/overlay_reference.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        sol_vk_create_shader_stage_info(&persistent_resources->fragment_pipeline_stage, device, "solipsix/shaders/overlay_reference.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    return result;
}

void sol_overlay_render_persistent_resources_terminate(struct sol_overlay_render_persistent_resources* persistent_resources, struct cvm_vk_device* device)
{
    VkResult result;

    sol_vk_destroy_shader_stage_info(&persistent_resources->vertex_pipeline_stage  , device);
    sol_vk_destroy_shader_stage_info(&persistent_resources->fragment_pipeline_stage, device);

    vkDestroyPipelineLayout(device->device, persistent_resources->pipeline_layout, device->host_allocator);

    /*result = vkFreeDescriptorSets(device->device, persistent_resources->descriptor_pool, persistent_resources->total_set_count, persistent_resources->descriptor_sets);
    assert(result == VK_SUCCESS);*/
    sol_vk_limited_descriptor_set_stack_terminate(&persistent_resources->descriptor_sets);

    vkDestroyDescriptorPool(device->device, persistent_resources->descriptor_pool, device->host_allocator);
    vkDestroyDescriptorSetLayout(device->device, persistent_resources->descriptor_set_layout, device->host_allocator);
}

VkDescriptorSet sol_overlay_render_descriptor_set_allocate(struct cvm_vk_device* device, struct sol_overlay_render_persistent_resources* persistent_resources)
{
    VkResult result;
    VkDescriptorSet descriptor_set;

    VkDescriptorSetAllocateInfo allocate_info=
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = persistent_resources->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &persistent_resources->descriptor_set_layout,
    };
    
    result = vkAllocateDescriptorSets(device->device, &allocate_info, &descriptor_set);

    assert(result == VK_SUCCESS);

    return descriptor_set;
}

VkPipeline sol_overlay_render_pipeline_create_static(struct cvm_vk_device* device, const struct sol_overlay_render_persistent_resources* persistent_resources, VkRenderPass render_pass, VkExtent2D extent, uint32_t subpass)
{
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result;

    VkGraphicsPipelineCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stageCount = 2,
        .pStages = (VkPipelineShaderStageCreateInfo[2])
        {
            persistent_resources->vertex_pipeline_stage,
            persistent_resources->fragment_pipeline_stage,
        },
        .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions =  (VkVertexInputBindingDescription[1])
            {
                {
                    .binding = 0,
                    .stride = sizeof(struct sol_overlay_render_element),
                    .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
                }
            },
            .vertexAttributeDescriptionCount = 4,
            .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[4])
            {
                {
                    .location = 0,
                    .binding = 0,
                    .format = VK_FORMAT_R16G16B16A16_UINT,
                    .offset = offsetof(struct sol_overlay_render_element,pos_rect)
                },
                {
                    .location = 1,
                    .binding = 0,
                    .format = VK_FORMAT_R16G16B16A16_UINT,
                    .offset = offsetof(struct sol_overlay_render_element, tex_coords)
                },
                {
                    .location = 2,
                    .binding = 0,
                    .format = VK_FORMAT_R16G16B16A16_UINT,
                    .offset = offsetof(struct sol_overlay_render_element, other_data)
                },
                {
                    .location = 3,
                    .binding = 0,
                    .format = VK_FORMAT_R16G16B16A16_UINT,
                    .offset = offsetof(struct sol_overlay_render_element, idk)
                },
            }
        },
        .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .primitiveRestartEnable = VK_FALSE
        },
        .pTessellationState = NULL,
        .pViewportState = &(VkPipelineViewportStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = (VkViewport[1])
            {
                {
                .x = 0.0,
                .y = 0.0,
                .width = (float)extent.width,
                .height = (float)extent.height,
                .minDepth = 0.0,
                .maxDepth = 1.0,
                },
            },
            .scissorCount = 1,
            .pScissors =  (VkRect2D[1])
            {
                {
                    .offset = (VkOffset2D){.x = 0,.y = 0},
                    .extent = extent,
                },
            },
        },
        .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0,
            .depthBiasClamp = 0.0,
            .depthBiasSlopeFactor = 0.0,
            .lineWidth = 1.0
        },
        .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0,
            .pSampleMask = NULL,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
        },
        .pDepthStencilState = NULL,
        .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,///must equal colorAttachmentCount in subpass
            .pAttachments =  (VkPipelineColorBlendAttachmentState[1])
            {
                {
                    .blendEnable = VK_TRUE,
                    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
                },
            },
            .blendConstants = {0.0, 0.0, 0.0, 0.0},
        },
        .pDynamicState = NULL,
        .layout = persistent_resources->pipeline_layout,
        .renderPass = render_pass,
        .subpass = subpass,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    result = vkCreateGraphicsPipelines(device->device, device->pipeline_cache.cache, 1, &create_info, device->host_allocator, &pipeline);
    assert(result == VK_SUCCESS);

    return pipeline;
}

VkPipeline sol_overlay_render_pipeline_create_dynamic(struct cvm_vk_device* device, const struct sol_overlay_render_persistent_resources* persistent_resources, VkFormat target_format)
{
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result;

    ;

    VkGraphicsPipelineCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stageCount = 2,
        .pStages = (VkPipelineShaderStageCreateInfo[2])
        {
            persistent_resources->vertex_pipeline_stage,
            persistent_resources->fragment_pipeline_stage,
        },
        .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions =  (VkVertexInputBindingDescription[1])
            {
                {
                    .binding = 0,
                    .stride = sizeof(struct sol_overlay_render_element),
                    .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
                }
            },
            .vertexAttributeDescriptionCount = 4,
            .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[4])
            {
                {
                    .location = 0,
                    .binding = 0,
                    .format = VK_FORMAT_R16G16B16A16_UINT,
                    .offset = offsetof(struct sol_overlay_render_element,pos_rect)
                },
                {
                    .location = 1,
                    .binding = 0,
                    .format = VK_FORMAT_R16G16B16A16_UINT,
                    .offset = offsetof(struct sol_overlay_render_element, tex_coords)
                },
                {
                    .location = 2,
                    .binding = 0,
                    .format = VK_FORMAT_R16G16B16A16_UINT,
                    .offset = offsetof(struct sol_overlay_render_element, other_data)
                },
                {
                    .location = 3,
                    .binding = 0,
                    .format = VK_FORMAT_R16G16B16A16_UINT,
                    .offset = offsetof(struct sol_overlay_render_element, idk)
                },
            }
        },
        .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .primitiveRestartEnable = VK_FALSE
        },
        .pTessellationState = NULL,
        .pViewportState = &(VkPipelineViewportStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            /** viewport and scissor are dynamic **/
            .viewportCount = 0,
            .pViewports = NULL,
            .scissorCount = 0,
            .pScissors = NULL,
        },
        .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0,
            .depthBiasClamp = 0.0,
            .depthBiasSlopeFactor = 0.0,
            .lineWidth = 1.0
        },
        .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0,
            .pSampleMask = NULL,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
        },
        .pDepthStencilState = NULL,
        .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments =  (VkPipelineColorBlendAttachmentState[1])
            {
                {
                    .blendEnable = VK_TRUE,
                    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                    .colorBlendOp = VK_BLEND_OP_ADD,
                    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                    .alphaBlendOp = VK_BLEND_OP_ADD,
                    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
                },
            },
            .blendConstants = {0.0, 0.0, 0.0, 0.0},
        },
        .pDynamicState = &(VkPipelineDynamicStateCreateInfo) 
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .dynamicStateCount = 2,
            .pDynamicStates = (VkDynamicState[2])
            {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
            },
        },
        .layout = persistent_resources->pipeline_layout,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    result = vkCreateGraphicsPipelines(device->device, device->pipeline_cache.cache, 1, &create_info, device->host_allocator, &pipeline);
    assert(result == VK_SUCCESS);

    return pipeline;
}




void sol_overlay_render_batch_initialise(struct sol_overlay_render_batch* batch, struct cvm_vk_device* device, VkDeviceSize upload_buffer_size)
{
    uint32_t i;
    VkDeviceSize upload_buffer_alignment = sol_vk_buffer_alignment_requirements(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    sol_overlay_render_element_list_initialise(&batch->elements, 64);

    /** note: fixed/limited size lends itself well to buddy allocator use */
    sol_buffer_initialise(&batch->upload_buffer, upload_buffer_size, upload_buffer_alignment);

    for(i = 0; i< SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT; i++)
    {
        sol_vk_buf_img_copy_list_initialise(batch->atlas_copy_lists + i, 64);
    }
}

void sol_overlay_render_batch_terminate(struct sol_overlay_render_batch* batch)
{
    uint32_t i;

    for(i = 0; i< SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT; i++)
    {
        sol_vk_buf_img_copy_list_terminate(batch->atlas_copy_lists + i);
    }

    sol_buffer_terminate(&batch->upload_buffer);

    sol_overlay_render_element_list_terminate(&batch->elements);
}


void sol_overlay_render_step_compose_elements(struct sol_overlay_render_batch* batch, struct sol_gui_context* gui_context, struct sol_overlay_rendering_resources* rendering_resources, VkExtent2D target_extent)
{
    /** copy actions should be reset when copied, this must have been done before resetting the batch
     * overlay system relies on these entries having been staged and uploaded */
    uint32_t i;

    batch->rendering_resources = rendering_resources;
    batch->bounds = s16_rect_set(0, 0, target_extent.width, target_extent.height);
    batch->target_extent = target_extent;
    #warning rather than tracking the context in this way, it's very likely better to reference resources used by it

    for(i = 0; i< SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT; i++)
    {
        assert(sol_vk_buf_img_copy_list_count(batch->atlas_copy_lists + i) == 0);
        assert(sol_image_atlas_access_range_is_active(rendering_resources->atlases[i]));
    }
    assert(sol_buffer_used_space(&batch->upload_buffer) == 0);
    assert(sol_overlay_render_element_list_count(&batch->elements) == 0);


    bool gui_fits = sol_gui_context_update_screen_size(gui_context, s16_vec2_set(target_extent.width, target_extent.height));
    if(!gui_fits)
    {
        fprintf(stderr, "overlay doesn't fit on screen\n");
    }

    sol_gui_context_render(gui_context, batch);
}

void sol_overlay_render_step_write_descriptors(struct sol_overlay_render_batch* batch, struct cvm_vk_device* device, struct sol_vk_staging_buffer* staging_buffer, const float* colour_array, VkDescriptorSet descriptor_set)
{
    VkDeviceSize upload_offset, elements_offset, uniform_offset, staging_space;
    uint32_t i;
    VkDescriptorImageInfo atlas_descriptor_image_info[SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT];

    /** upload all staged resources needed by this frame (uniforms, uploaded data, elements),
     * track the offset progressively including total required space */
    uniform_offset  = 0;

    #warning if a non-ring buffer implementation (buddy allocator) for staging is preferred it may be worthwhile to break this into chunks (by comparing required size across approaches?)
    #warning need to make sure upload alignment here is a multiple of any possible texture upload format
    #warning FUCK, would have to know required specialised alignment for textures that require non power of 2 pixel alignmenment!

    /* very ad-hoc use of staging (for now) */
    upload_offset   = sol_vk_staging_buffer_allocation_align_offset(staging_buffer, uniform_offset  + sizeof(float) * 4 * OVERLAY_NUM_COLOURS);
    elements_offset = sol_vk_staging_buffer_allocation_align_offset(staging_buffer, upload_offset   + sol_buffer_used_space(&batch->upload_buffer));
    staging_space   = sol_vk_staging_buffer_allocation_align_offset(staging_buffer, elements_offset + sol_overlay_render_element_list_size(&batch->elements));

    batch->staging_buffer_allocation = sol_vk_staging_buffer_allocation_acquire(staging_buffer, device, staging_space, 1);

    batch->staging_buffer = staging_buffer;

    char* const staging_mapping = batch->staging_buffer_allocation.mapping;
    const VkDeviceSize staging_offset = batch->staging_buffer_allocation.acquired_offset;

    batch->element_offset = staging_offset + elements_offset;
    batch->upload_offset  = staging_offset + upload_offset;

    /** copy uniforms */
    memcpy(staging_mapping + uniform_offset, colour_array, sizeof(float) * 4 * OVERLAY_NUM_COLOURS);

    /** copy upload buffer */
    sol_buffer_copy(&batch->upload_buffer, staging_mapping + upload_offset);

    /** copy render elements (instances) */
    sol_overlay_render_element_list_copy(&batch->elements, staging_mapping + elements_offset);

    /** flush all uploads */
    sol_vk_staging_buffer_allocation_flush_range(staging_buffer, device, &batch->staging_buffer_allocation, 0, staging_space);

    // this is super strange to put here... could/should go closer to writes
    for(i = 0; i < SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT; i++)
    {
        atlas_descriptor_image_info[i] = (VkDescriptorImageInfo)
        {
            .sampler = device->defaults.fetch_sampler,
            .imageView = sol_image_atlas_access_image_view(batch->rendering_resources->atlases[i]),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
    }

    VkWriteDescriptorSet writes[2] =
    {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = atlas_descriptor_image_info,
            .pBufferInfo = NULL,
            .pTexelBufferView = NULL
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = descriptor_set,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,///VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER probably preferable here... then use either RGBA8 unorm colour or possibly RGBA16
            .pImageInfo = NULL,
            .pBufferInfo = (VkDescriptorBufferInfo[1])
            {
                {
                    .buffer = batch->staging_buffer_allocation.acquired_buffer,
                    .offset = batch->staging_buffer_allocation.acquired_offset + uniform_offset,
                    .range = OVERLAY_NUM_COLOURS * 4 * sizeof(float),
                    #warning make above a struct maybe?
                }
            },
            .pTexelBufferView = NULL
        }
    };

    vkUpdateDescriptorSets(device->device, 2, writes, 0, NULL);

    batch->descriptor_set = descriptor_set;
}

void sol_overlay_render_step_submit_vk_transfers(struct sol_overlay_render_batch* batch, VkCommandBuffer command_buffer)
{
    struct sol_vk_supervised_image* atlas_supervised_image;
    struct sol_vk_buf_img_copy_list* atlas_copy_list;
    VkBuffer     staging_buffer;
    VkDeviceSize staging_offset;
    uint32_t i;

    staging_buffer = batch->staging_buffer_allocation.acquired_buffer;
    staging_offset = batch->upload_offset;

    for(i = 0; i < SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT; i++)
    {
        atlas_copy_list = batch->atlas_copy_lists + i;
        atlas_supervised_image = sol_image_atlas_access_supervised_image(batch->rendering_resources->atlases[i]);
        sol_vk_supervised_image_execute_copies(atlas_supervised_image, atlas_copy_list, command_buffer, staging_buffer, staging_offset);
    }
}

void sol_overlay_render_step_insert_vk_barriers(struct sol_overlay_render_batch* batch, VkCommandBuffer command_buffer)
{
    struct sol_vk_supervised_image* atlas_supervised_image;
    uint32_t i;

    for(i = 0; i < SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT; i++)
    {
        atlas_supervised_image = sol_image_atlas_access_supervised_image(batch->rendering_resources->atlases[i]);
        sol_vk_supervised_image_barrier(atlas_supervised_image, command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    }
}

/**
pipeline_layout: completely static, singular
pipeline: changes with target, singular
descriptor set (from batch): must come from managed per-frame resources, dynamic and numerous
*/
void sol_overlay_render_step_draw_elements(struct sol_overlay_render_batch* batch, struct sol_overlay_render_persistent_resources* persistent_resources, VkPipeline pipeline, VkCommandBuffer command_buffer)
{
    struct sol_vk_supervised_image* atlas_supervised_image;
    uint32_t i;

    const uint32_t element_count = sol_overlay_render_element_list_count(&batch->elements);
    VkBuffer draw_buffer = batch->staging_buffer_allocation.acquired_buffer;

    for(i = 0; i < SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT; i++)
    {
        atlas_supervised_image = sol_image_atlas_access_supervised_image(batch->rendering_resources->atlases[i]);
        /** because `sol_overlay_render_step_insert_vk_barriers` is optional; make sure the image has been correctly barriered for the intended reads */
        assert(sol_vk_supervised_image_validate_state(atlas_supervised_image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
    }
    

    float push_constants[2]={2.0/(float)batch->target_extent.width, 2.0/(float)batch->target_extent.height};
    vkCmdPushConstants(command_buffer, persistent_resources->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 2, push_constants);

    /** set index (firstSet) is defined in pipeline creation (index in array) */
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, persistent_resources->pipeline_layout, 0, 1, &batch->descriptor_set, 0, NULL);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &draw_buffer, &batch->element_offset);
    vkCmdDraw(command_buffer, 4, element_count, 0, 0);

    // printf("overlay element render count %u\n",element_count);
}

void sol_overlay_render_step_completion(struct sol_overlay_render_batch* batch, struct sol_vk_timeline_semaphore_moment completion_moment)
{
    uint32_t i;

    const bool last_release = sol_vk_staging_buffer_allocation_release(batch->staging_buffer, &batch->staging_buffer_allocation, &completion_moment);
    assert(last_release);

    for(i = 0; i< SOL_OVERLAY_IMAGE_ATLAS_TYPE_COUNT; i++)
    {
        sol_vk_buf_img_copy_list_reset(batch->atlas_copy_lists + i);
    }

    sol_buffer_reset(&batch->upload_buffer);
    sol_overlay_render_element_list_reset(&batch->elements);
}





