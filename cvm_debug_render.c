/**
Copyright 2020,2021,2022 Carl van Mastrigt

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





static VkPipelineLayout debug_pipeline_layout;
static VkPipeline debug_pipeline;

static VkPipelineShaderStageCreateInfo debug_vert_stage;
static VkPipelineShaderStageCreateInfo debug_frag_stage;

void create_debug_render_data(VkDescriptorSetLayout * descriptor_set_layout)
{
    VkPipelineLayoutCreateInfo pipeline_layout_create_info=(VkPipelineLayoutCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .setLayoutCount=1,
        .pSetLayouts=(VkDescriptorSetLayout[1])
        {
            *descriptor_set_layout
        },
        .pushConstantRangeCount=0,
        .pPushConstantRanges=NULL
    };

    cvm_vk_create_pipeline_layout(&debug_pipeline_layout,&pipeline_layout_create_info);

    sol_vk_create_shader_stage_info(&debug_vert_stage, NULL, "shaders/debug/line.vert.spv",VK_SHADER_STAGE_VERTEX_BIT);
    sol_vk_create_shader_stage_info(&debug_frag_stage, NULL, "shaders/debug/line.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT);
}

void destroy_debug_render_data(void)
{
    sol_vk_destroy_shader_stage_info(&debug_vert_stage, NULL);
    sol_vk_destroy_shader_stage_info(&debug_frag_stage, NULL);

    cvm_vk_destroy_pipeline_layout(debug_pipeline_layout);
}

void create_debug_swapchain_dependent_render_data(VkRenderPass render_pass, uint32_t subpass, VkSampleCountFlagBits sample_count)
{

    float line_thickness=1.0;

    VkGraphicsPipelineCreateInfo pipeline_create_info=(VkGraphicsPipelineCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .stageCount=2,
        .pStages=(VkPipelineShaderStageCreateInfo[2])
        {
            debug_vert_stage,
            debug_frag_stage,
        },
        .pVertexInputState=(VkPipelineVertexInputStateCreateInfo[1])
        {
            {
                .sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .pNext=NULL,
                .flags=0,
                .vertexBindingDescriptionCount=1,
                .pVertexBindingDescriptions=(VkVertexInputBindingDescription[1])
                {
                    {
                        .binding=0,
                        .stride=sizeof(debug_vertex_data),
                        .inputRate=VK_VERTEX_INPUT_RATE_VERTEX
                    }
                },
                .vertexAttributeDescriptionCount=2,
                .pVertexAttributeDescriptions=(VkVertexInputAttributeDescription[2])
                {
                    {
                        .location=0,
                        .binding=0,
                        .format=VK_FORMAT_R32G32B32_SFLOAT,
                        .offset=offsetof(debug_vertex_data,pos)
                    },
                    {
                        .location=1,
                        .binding=0,
                        .format=VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                        .offset=offsetof(debug_vertex_data,col)
                    }
                }
            }
        },
        .pInputAssemblyState=(VkPipelineInputAssemblyStateCreateInfo[1])
        {
            {
                .sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .pNext=NULL,
                .flags=0,
                .topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                .primitiveRestartEnable=VK_FALSE
            }
        },
        .pTessellationState=NULL,
        .pViewportState=cvm_vk_get_default_viewport_state(),
        .pRasterizationState=(VkPipelineRasterizationStateCreateInfo[1])
        {
            {
                .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .pNext=NULL,
                .flags=0,
                .depthClampEnable=VK_FALSE,
                .rasterizerDiscardEnable=VK_FALSE,
                .polygonMode=VK_POLYGON_MODE_FILL,
                .cullMode=VK_CULL_MODE_NONE,
                .frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .depthBiasEnable=VK_FALSE,
                .depthBiasConstantFactor=0.0,
                .depthBiasClamp=0.0,
                .depthBiasSlopeFactor=0.0,
                .lineWidth=line_thickness
            }
        },
        .pMultisampleState=(VkPipelineMultisampleStateCreateInfo[1])
        {
            {
                .sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .pNext=NULL,
                .flags=0,
                .rasterizationSamples=sample_count,///this can be changed, requires rebuild all render passes, framebuffers &c.
                .sampleShadingEnable=VK_FALSE,
                .minSampleShading=0.0f,//probably don't want this
                .pSampleMask=NULL,
                .alphaToCoverageEnable=VK_FALSE,///VK_FALSE
                .alphaToOneEnable=VK_FALSE
            }
        },
        .pDepthStencilState=cvm_vk_get_default_greater_depth_stencil_state(false),
        .pColorBlendState=(VkPipelineColorBlendStateCreateInfo[1])
        {
            {
                .sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .pNext=NULL,
                .flags=0,
                .logicOpEnable=VK_FALSE,
                .logicOp=VK_LOGIC_OP_COPY,
                .attachmentCount=1,///must equal colorAttachmentCount in subpass
                .pAttachments= (VkPipelineColorBlendAttachmentState[1])
                {
                    cvm_vk_get_default_alpha_blend_state()
                },
                .blendConstants={0.0,0.0,0.0,0.0}
            }
        },
        .pDynamicState=NULL,
        .layout=debug_pipeline_layout,
        .renderPass=render_pass,
        .subpass=subpass,
        .basePipelineHandle=VK_NULL_HANDLE,
        .basePipelineIndex=-1
    };

    cvm_vk_create_graphics_pipeline(&debug_pipeline,&pipeline_create_info);
}

void destroy_debug_swapchain_dependent_render_data(void)
{
    cvm_vk_destroy_pipeline(debug_pipeline);
}

void prepare_debug_render(debug_render_data * data,cvm_vk_transient_buffer * transient_buffer,uint32_t vert_count)
{
    data->vertices = cvm_vk_transient_buffer_get_allocation(transient_buffer,sizeof(debug_vertex_data)*vert_count,sizeof(debug_vertex_data),&data->vertex_offset);
    data->vertex_count=0;
    data->transient_buffer=transient_buffer;
    data->vertex_space=vert_count;
}

void add_debug_flat_line(debug_render_data * data,vec3f start_pos,vec3f end_pos,uint32_t col)
{
    if(data->vertex_count+2<data->vertex_space)
    {
        data->vertices[data->vertex_count++]=(debug_vertex_data){.pos=start_pos,.col=col};
        data->vertices[data->vertex_count++]=(debug_vertex_data){.pos=end_pos,.col=col};
    }
}

void add_debug_gradient_line(debug_render_data * data,vec3f start_pos,vec3f end_pos,uint32_t start_col,uint32_t end_col)
{
    if(data->vertex_count+2<data->vertex_space)
    {
        data->vertices[data->vertex_count++]=(debug_vertex_data){.pos=start_pos,.col=start_col};
        data->vertices[data->vertex_count++]=(debug_vertex_data){.pos=end_pos,.col=end_col};
    }
}

void render_debug(VkCommandBuffer cb,VkDescriptorSet * descriptor_set,debug_render_data * data)
{
    vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,debug_pipeline_layout,0,1,descriptor_set,0,NULL);

    cvm_vk_transient_buffer_bind_as_vertex(cb,data->transient_buffer,0,0);

    vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,debug_pipeline);

    vkCmdDraw(cb,data->vertex_count,1,data->vertex_offset/sizeof(debug_vertex_data),0);
}








