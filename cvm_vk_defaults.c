/**
Copyright 2022 Carl van Mastrigt

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

static VkRect2D cvm_vk_default_screen_rectangle;
static VkViewport cvm_vk_default_viewport;
static VkExtent3D cvm_vk_default_framebuffer_extent;

static VkPipelineInputAssemblyStateCreateInfo cvm_vk_default_input_assembly_state;
static VkPipelineViewportStateCreateInfo cvm_vk_default_viewport_state;
static VkPipelineRasterizationStateCreateInfo cvm_vk_default_unculled_raster_state;
static VkPipelineRasterizationStateCreateInfo cvm_vk_default_culled_raster_state;
static VkPipelineMultisampleStateCreateInfo cvm_vk_default_multisample_state;
static VkPipelineDepthStencilStateCreateInfo cvm_vk_default_greater_depth_stencil_write_state;
static VkPipelineDepthStencilStateCreateInfo cvm_vk_default_greater_depth_stencil_read_state;
static VkPipelineDepthStencilStateCreateInfo cvm_vk_default_equal_depth_stencil_state;
static VkPipelineDepthStencilStateCreateInfo cvm_vk_default_null_depth_stencil_state;
static VkPipelineColorBlendAttachmentState cvm_vk_default_null_blend_state;
static VkPipelineColorBlendAttachmentState cvm_vk_default_alpha_blend_state;
static VkPipelineColorBlendAttachmentState cvm_vk_default_additive_blend_state;

static VkImageCreateInfo cvm_vk_default_framebuffer_image_creation_info;
static VkImageViewCreateInfo cvm_vk_default_framebuffer_image_view_creation_info;

///implement more variants when mesh supports vertex normas and tex-coords
static VkVertexInputBindingDescription cvm_vk_default_pos_vertex_input_binding;
static VkVertexInputAttributeDescription cvm_vk_default_pos_vertex_input_attribute;
static VkPipelineVertexInputStateCreateInfo cvm_vk_default_pos_vertex_input_state;

VkPipelineVertexInputStateCreateInfo cvm_vk_default_empty_vertex_input_state;





#warning have 2 variants of below, 1 that ouputs depth at the near plane and 1 at the far, for use with post processing algorithms that interface with depth in some way
static VkPipelineShaderStageCreateInfo cvm_vk_default_fullscreen_vertex_stage;



static void cvm_vk_create_default_vertex_bindings(void)
{
    cvm_vk_default_pos_vertex_input_binding= (VkVertexInputBindingDescription)
    {
        .binding=0,
        .stride=sizeof(cvm_mesh_data_pos),
        .inputRate=VK_VERTEX_INPUT_RATE_VERTEX
    };

    cvm_vk_default_pos_vertex_input_attribute=(VkVertexInputAttributeDescription)
    {
        .location=0,
        .binding=0,
        .format=cvm_mesh_get_pos_format(),
        .offset=offsetof(cvm_mesh_data_pos,pos)
    };

    cvm_vk_default_pos_vertex_input_state=(VkPipelineVertexInputStateCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .vertexBindingDescriptionCount=1,
        .pVertexBindingDescriptions=&cvm_vk_default_pos_vertex_input_binding,
        .vertexAttributeDescriptionCount=1,
        .pVertexAttributeDescriptions=&cvm_vk_default_pos_vertex_input_attribute
    };

    cvm_vk_default_empty_vertex_input_state=(VkPipelineVertexInputStateCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .vertexBindingDescriptionCount=0,
        .pVertexBindingDescriptions=NULL,
        .vertexAttributeDescriptionCount=0,
        .pVertexAttributeDescriptions=NULL
    };
}

static void cvm_vk_destroy_default_vertex_bindings(void)
{
}

void cvm_vk_create_defaults_old(void)
{
    #warning can make these macros??

    cvm_vk_create_default_vertex_bindings();

    cvm_vk_default_input_assembly_state=(VkPipelineInputAssemblyStateCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable=VK_FALSE
    };

    cvm_vk_default_culled_raster_state=cvm_vk_default_unculled_raster_state=(VkPipelineRasterizationStateCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .depthClampEnable=VK_FALSE,
        .rasterizerDiscardEnable=VK_FALSE,
        .polygonMode=VK_POLYGON_MODE_FILL,
        //.cullMode=,
        .frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable=VK_FALSE,
        .depthBiasConstantFactor=0.0,
        .depthBiasClamp=0.0,
        .depthBiasSlopeFactor=0.0,
        .lineWidth=1.0
    };
    cvm_vk_default_unculled_raster_state.cullMode=VK_CULL_MODE_NONE;
    cvm_vk_default_culled_raster_state.cullMode=VK_CULL_MODE_BACK_BIT;

    cvm_vk_default_multisample_state=(VkPipelineMultisampleStateCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .rasterizationSamples=VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable=VK_FALSE,
        .minSampleShading=1.0,
        .pSampleMask=NULL,
        .alphaToCoverageEnable=VK_FALSE,
        .alphaToOneEnable=VK_FALSE
    };

    cvm_vk_default_greater_depth_stencil_write_state=cvm_vk_default_greater_depth_stencil_read_state=(VkPipelineDepthStencilStateCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .depthTestEnable=VK_TRUE,
        //.depthWriteEnable,
        .depthCompareOp=VK_COMPARE_OP_GREATER,
        .depthBoundsTestEnable=VK_FALSE,
        .stencilTestEnable=VK_FALSE,
        .front=(VkStencilOpState)
        {
            .failOp=VK_STENCIL_OP_KEEP,
            .passOp=VK_STENCIL_OP_KEEP,
            .depthFailOp=VK_STENCIL_OP_KEEP,
            .compareOp=VK_COMPARE_OP_NEVER,
            .compareMask=0x00,
            .writeMask=0x00,
            .reference=0x00
        },
        .back=(VkStencilOpState)
        {
            .failOp=VK_STENCIL_OP_KEEP,
            .passOp=VK_STENCIL_OP_KEEP,
            .depthFailOp=VK_STENCIL_OP_KEEP,
            .compareOp=VK_COMPARE_OP_NEVER,
            .compareMask=0x00,
            .writeMask=0x00,
            .reference=0x00
        },
        .minDepthBounds=0.0,
        .maxDepthBounds=1.0,
    };
    cvm_vk_default_greater_depth_stencil_write_state.depthWriteEnable=VK_TRUE;
    cvm_vk_default_greater_depth_stencil_read_state.depthWriteEnable=VK_FALSE;


    cvm_vk_default_equal_depth_stencil_state=(VkPipelineDepthStencilStateCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .depthTestEnable=VK_TRUE,
        .depthWriteEnable=VK_FALSE,
        .depthCompareOp=VK_COMPARE_OP_EQUAL,
        .depthBoundsTestEnable=VK_FALSE,
        .stencilTestEnable=VK_FALSE,
        .front=(VkStencilOpState)
        {
            .failOp=VK_STENCIL_OP_KEEP,
            .passOp=VK_STENCIL_OP_KEEP,
            .depthFailOp=VK_STENCIL_OP_KEEP,
            .compareOp=VK_COMPARE_OP_NEVER,
            .compareMask=0x00,
            .writeMask=0x00,
            .reference=0x00
        },
        .back=(VkStencilOpState)
        {
            .failOp=VK_STENCIL_OP_KEEP,
            .passOp=VK_STENCIL_OP_KEEP,
            .depthFailOp=VK_STENCIL_OP_KEEP,
            .compareOp=VK_COMPARE_OP_NEVER,
            .compareMask=0x00,
            .writeMask=0x00,
            .reference=0x00
        },
        .minDepthBounds=0.0,
        .maxDepthBounds=1.0,
    };

    cvm_vk_default_null_depth_stencil_state=(VkPipelineDepthStencilStateCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .depthTestEnable=VK_FALSE,
        .depthWriteEnable=VK_FALSE,
        .depthCompareOp=VK_COMPARE_OP_NEVER,
        .depthBoundsTestEnable=VK_FALSE,
        .stencilTestEnable=VK_FALSE,
        .front=(VkStencilOpState)
        {
            .failOp=VK_STENCIL_OP_KEEP,
            .passOp=VK_STENCIL_OP_KEEP,
            .depthFailOp=VK_STENCIL_OP_KEEP,
            .compareOp=VK_COMPARE_OP_NEVER,
            .compareMask=0x00,
            .writeMask=0x00,
            .reference=0x00
        },
        .back=(VkStencilOpState)
        {
            .failOp=VK_STENCIL_OP_KEEP,
            .passOp=VK_STENCIL_OP_KEEP,
            .depthFailOp=VK_STENCIL_OP_KEEP,
            .compareOp=VK_COMPARE_OP_NEVER,
            .compareMask=0x00,
            .writeMask=0x00,
            .reference=0x00
        },
        .minDepthBounds=0.0,
        .maxDepthBounds=1.0,
    };


    cvm_vk_default_null_blend_state=(VkPipelineColorBlendAttachmentState)
    {
        .blendEnable=VK_FALSE,
        .srcColorBlendFactor=VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor=VK_BLEND_FACTOR_ZERO,
        .colorBlendOp=VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp=VK_BLEND_OP_ADD,
        .colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
    };

    cvm_vk_default_alpha_blend_state=(VkPipelineColorBlendAttachmentState)
    {
        .blendEnable=VK_TRUE,
        .srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp=VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,
        .dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp=VK_BLEND_OP_ADD,
        .colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
    };

    cvm_vk_default_additive_blend_state=(VkPipelineColorBlendAttachmentState)
    {
        .blendEnable=VK_TRUE,
        .srcColorBlendFactor=VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor=VK_BLEND_FACTOR_ONE,
        .colorBlendOp=VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,
        .dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp=VK_BLEND_OP_ADD,
        .colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
    };

    cvm_vk_create_shader_stage_info(&cvm_vk_default_fullscreen_vertex_stage, NULL, "solipsix/shaders/fullscreen.vert.spv",VK_SHADER_STAGE_VERTEX_BIT);
}

void cvm_vk_destroy_defaults_old(void)
{
    cvm_vk_destroy_default_vertex_bindings();

    cvm_vk_destroy_shader_stage_info(&cvm_vk_default_fullscreen_vertex_stage, NULL);
}

void cvm_vk_create_swapchain_dependent_defaults(uint32_t width,uint32_t height)
{
    cvm_vk_default_screen_rectangle=(VkRect2D)
    {
        .offset=(VkOffset2D){.x=0,.y=0},
        .extent=(VkExtent2D){.width=width,.height=height}
    };

    cvm_vk_default_viewport=(VkViewport)
    {
        .x=0.0,
        .y=0.0,
        .width=(float)width,
        .height=(float)height,
        .minDepth=0.0,
        .maxDepth=1.0
    };

    cvm_vk_default_framebuffer_extent=(VkExtent3D)
    {
        .width=width,
        .height=height,
        .depth=1
    };

    cvm_vk_default_viewport_state=(VkPipelineViewportStateCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .viewportCount=1,
        .pViewports=&cvm_vk_default_viewport,
        .scissorCount=1,
        .pScissors= &cvm_vk_default_screen_rectangle
    };


    cvm_vk_default_framebuffer_image_creation_info=(VkImageCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .imageType=VK_IMAGE_TYPE_2D,
        .format=VK_FORMAT_UNDEFINED,///set later
        .extent=cvm_vk_default_framebuffer_extent,
        .mipLevels=1,
        .arrayLayers=1,
        .samples=0,///set later
        .tiling=VK_IMAGE_TILING_OPTIMAL,
        .usage=0,///set later
        .sharingMode=VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount=0,
        .pQueueFamilyIndices=NULL,
        .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED
    };

    cvm_vk_default_framebuffer_image_view_creation_info=(VkImageViewCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .image=VK_NULL_HANDLE,///set later
        .viewType=VK_IMAGE_VIEW_TYPE_2D,
        .format=VK_FORMAT_UNDEFINED,///set later
        .components=(VkComponentMapping)
        {
            .r=VK_COMPONENT_SWIZZLE_IDENTITY,
            .g=VK_COMPONENT_SWIZZLE_IDENTITY,
            .b=VK_COMPONENT_SWIZZLE_IDENTITY,
            .a=VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange=(VkImageSubresourceRange)
        {
            .aspectMask=0,///set later
            .baseMipLevel=0,
            .levelCount=1,
            .baseArrayLayer=0,
            .layerCount=1
        }
    };
}

void cvm_vk_destroy_swapchain_dependent_defaults(void)
{
}

VkRect2D cvm_vk_get_default_render_area(void)
{
    return cvm_vk_default_screen_rectangle;
}

VkExtent3D cvm_vk_get_default_framebuffer_extent(void)
{
    return cvm_vk_default_framebuffer_extent;
}

VkPipelineInputAssemblyStateCreateInfo * cvm_vk_get_default_input_assembly_state(void)
{
    return &cvm_vk_default_input_assembly_state;
}

VkPipelineViewportStateCreateInfo * cvm_vk_get_default_viewport_state(void)
{
    return &cvm_vk_default_viewport_state;
}

VkPipelineRasterizationStateCreateInfo * cvm_vk_get_default_raster_state(bool culled)
{
    return culled ? &cvm_vk_default_culled_raster_state : &cvm_vk_default_unculled_raster_state;
}

VkPipelineMultisampleStateCreateInfo * cvm_vk_get_default_multisample_state(void)
{
    return &cvm_vk_default_multisample_state;
}

VkPipelineDepthStencilStateCreateInfo * cvm_vk_get_default_greater_depth_stencil_state(bool write)
{
    return write ? &cvm_vk_default_greater_depth_stencil_write_state : &cvm_vk_default_greater_depth_stencil_read_state;
}

VkPipelineDepthStencilStateCreateInfo * cvm_vk_get_default_equal_depth_stencil_state(bool write)
{
    assert(!write);
    return &cvm_vk_default_equal_depth_stencil_state;
}

VkPipelineDepthStencilStateCreateInfo * cvm_vk_get_default_null_depth_stencil_state(void)
{
    return &cvm_vk_default_null_depth_stencil_state;
}

VkPipelineColorBlendAttachmentState cvm_vk_get_default_null_blend_state(void)
{
    return cvm_vk_default_null_blend_state;
}

VkPipelineColorBlendAttachmentState cvm_vk_get_default_alpha_blend_state(void)
{
    return cvm_vk_default_alpha_blend_state;
}

VkPipelineColorBlendAttachmentState cvm_vk_get_default_additive_blend_state(void)
{
    return cvm_vk_default_additive_blend_state;
}

void cvm_vk_create_default_framebuffer_images(VkImage * images,VkFormat format,uint32_t count,VkSampleCountFlagBits samples,VkImageUsageFlags usage)
{
    cvm_vk_default_framebuffer_image_creation_info.format=format;
    cvm_vk_default_framebuffer_image_creation_info.samples=samples;
    cvm_vk_default_framebuffer_image_creation_info.usage=usage;

//    if(format==VK_FORMAT_D32_SFLOAT_S8_UINT)
//    {
//        VkImageStencilUsageCreateInfo su=
//        {
//            .sType=VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO,
//            .pNext=NULL,
//            .stencilUsage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
//        };
//
//        cvm_vk_default_framebuffer_image_creation_info.pNext=&su;
//    }

    while(count--)cvm_vk_create_image(images+count,&cvm_vk_default_framebuffer_image_creation_info);

//    cvm_vk_default_framebuffer_image_creation_info.pNext=NULL;
}

void cvm_vk_create_default_framebuffer_image_views(VkImageView * views,VkImage * images,VkFormat format,VkImageAspectFlags aspects,uint32_t count)
{
    cvm_vk_default_framebuffer_image_view_creation_info.format=format;
    cvm_vk_default_framebuffer_image_view_creation_info.subresourceRange.aspectMask=aspects;

    while(count--)
    {
        cvm_vk_default_framebuffer_image_view_creation_info.image=images[count];
        cvm_vk_create_image_view(views+count,&cvm_vk_default_framebuffer_image_view_creation_info);
    }
}


VkPipelineVertexInputStateCreateInfo * cvm_vk_get_mesh_vertex_input_state(uint16_t flags)
{
    if(flags & (CVM_MESH_VERTEX_NORMALS|CVM_MESH_TEXTURE_COORDS))
    {
        fprintf(stderr,"MESH NORMALS AND TEX-COORDS VERTEX STATE NYI\n");
        exit(-1);
    }

    return &cvm_vk_default_pos_vertex_input_state;
}

VkPipelineVertexInputStateCreateInfo * cvm_vk_get_empty_vertex_input_state(void)
{
    return &cvm_vk_default_empty_vertex_input_state;
}



VkPipelineShaderStageCreateInfo cvm_vk_get_default_fullscreen_vertex_stage(void)
{
    return cvm_vk_default_fullscreen_vertex_stage;
}

void cvm_vk_render_fullscreen_pass(VkCommandBuffer cb)
{
    vkCmdDraw(cb,3,1,0,0);
}

VkAttachmentDescription cvm_vk_get_default_colour_attachment(VkFormat format,VkSampleCountFlagBits sample_count,bool clear,bool load,bool store)
{
    assert(!clear || !load);///CANNOT CLEAR AND LOAD COLOUR ATTACHMENT UPON INPUT

    return (VkAttachmentDescription)
    {
        .flags=0,
        .format=format,
        .samples=sample_count,
        .loadOp=load?VK_ATTACHMENT_LOAD_OP_LOAD:(clear?VK_ATTACHMENT_LOAD_OP_CLEAR:VK_ATTACHMENT_LOAD_OP_DONT_CARE),
        .storeOp=store?VK_ATTACHMENT_STORE_OP_STORE:VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout=load?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
}

VkAttachmentDescription cvm_vk_get_default_depth_stencil_attachment(VkFormat format,VkSampleCountFlagBits sample_count,bool depth_clear,bool depth_load,bool depth_store,bool stencil_clear,bool stencil_load,bool stencil_store)
{
    assert(!depth_clear || !depth_load);///CANNOT CLEAR AND LOAD DEPTH ATTACHMENT UPON INPUT

    assert(!stencil_clear || !stencil_load);///CANNOT CLEAR AND LOAD DEPTH ATTACHMENT UPON INPUT

    return (VkAttachmentDescription)
    {
        .flags=0,
        .format=format,
        .samples=sample_count,
        .loadOp=depth_load?VK_ATTACHMENT_LOAD_OP_LOAD:(depth_clear?VK_ATTACHMENT_LOAD_OP_CLEAR:VK_ATTACHMENT_LOAD_OP_DONT_CARE),
        .storeOp=depth_store?VK_ATTACHMENT_STORE_OP_STORE:VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp=stencil_load?VK_ATTACHMENT_LOAD_OP_LOAD:(stencil_clear?VK_ATTACHMENT_LOAD_OP_CLEAR:VK_ATTACHMENT_LOAD_OP_DONT_CARE),
        .stencilStoreOp=stencil_store?VK_ATTACHMENT_STORE_OP_STORE:VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout=(stencil_load||depth_load)?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };
}

VkSubpassDependency cvm_vk_get_default_colour_attachment_dependency(uint32_t src_subpass,uint32_t dst_subpass)
{
    return (VkSubpassDependency)
    {
        .srcSubpass=src_subpass,
        .dstSubpass=dst_subpass,
        .srcStageMask=VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        /// ^ image attachment needs shader stage, see VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT
        .srcAccessMask=VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .dstAccessMask=VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT
    };
}

VkSubpassDependency cvm_vk_get_default_depth_stencil_attachment_dependency(uint32_t src_subpass,uint32_t dst_subpass)
{
    return (VkSubpassDependency)
    {
        .srcSubpass=src_subpass,
        .dstSubpass=dst_subpass,
        .srcStageMask=VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask=VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask=VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        .dstAccessMask= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        .dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT
    };
}
