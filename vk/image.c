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

#include <assert.h>

#include "vk/image.h"
#include "vk/image_utils.h"

#include "cvm_vk.h"






VkResult sol_vk_image_create(struct sol_vk_image* image, struct cvm_vk_device* device, const VkImageCreateInfo* image_create_info, const VkImageViewCreateInfo* default_view_create_info)
{
    VkResult result;
    uint32_t memory_type_index;
    VkImageViewCreateInfo view_create_info_internal;

    VkMemoryDedicatedRequirements dedicated_requirements =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
        .pNext = NULL,
    };
    VkMemoryRequirements2 memory_requirements =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .pNext = &dedicated_requirements,
    };


    #warning merge/move this concept with managed image?
    *image = (struct sol_vk_image)
    {
        .properties =
        {
            .flags       = image_create_info->flags,
            .imageType   = image_create_info->imageType,
            .format      = image_create_info->format,
            .extent      = image_create_info->extent,
            .mipLevels   = image_create_info->mipLevels,
            .arrayLayers = image_create_info->arrayLayers,
            .samples     = image_create_info->samples,
            .tiling      = image_create_info->tiling,
            .usage       = image_create_info->usage,
            .sharingMode = image_create_info->sharingMode,
        },
        .image     = VK_NULL_HANDLE,
        .base_view = VK_NULL_HANDLE,
        .memory    = VK_NULL_HANDLE,
    };


    result = vkCreateImage(device->device, image_create_info, device->host_allocator, &image->image);

    if(result == VK_SUCCESS)
    {
        const VkImageMemoryRequirementsInfo2 image_requirements_info =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
            .pNext = NULL,
            .image = image->image,
        };

        vkGetImageMemoryRequirements2(device->device, &image_requirements_info, &memory_requirements);

        printf("sol_vk_image_create: %d : %d @ %lu\n", dedicated_requirements.prefersDedicatedAllocation, dedicated_requirements.requiresDedicatedAllocation,  memory_requirements.memoryRequirements.size);

        memory_type_index = sol_vk_find_appropriate_memory_type(device, memory_requirements.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if(memory_type_index == CVM_INVALID_U32_INDEX)
        {
            memory_type_index = sol_vk_find_appropriate_memory_type(device, memory_requirements.memoryRequirements.memoryTypeBits, 0);
        }

        if(memory_type_index == CVM_INVALID_U32_INDEX)
        {
            result = VK_ERROR_UNKNOWN;
        }
    }

    if(result == VK_SUCCESS)
    {
        #warning unless dedicated allocation is required; can probably use an existing memory allocation with some offset (same w/ binding)
        #warning VkMemoryDedicatedAllocateInfo in next!
        const VkMemoryAllocateInfo memory_allocate_info =
        {
            .sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext=NULL,
            .allocationSize = memory_requirements.memoryRequirements.size,
            .memoryTypeIndex = memory_type_index
        };

        result = vkAllocateMemory(device->device, &memory_allocate_info, device->host_allocator, &image->memory);
    }

    if(result == VK_SUCCESS)
    {
        result = vkBindImageMemory(device->device, image->image, image->memory, 0);
    }

    if(result == VK_SUCCESS)
    {
        if(default_view_create_info == NULL)
        {
            sol_vk_default_view_for_image(&view_create_info_internal, image_create_info, image->image);
        }
        else
        {
            view_create_info_internal = *default_view_create_info;
            view_create_info_internal.image = image->image;
        }

        result = vkCreateImageView(device->device, &view_create_info_internal, device->host_allocator, &image->base_view);
    }

    if(result != VK_SUCCESS)
    {
        sol_vk_image_destroy(image, device);
    }

    return result;
}


void sol_vk_image_destroy(struct sol_vk_image* image, struct cvm_vk_device* device)
{
    if(image->image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device->device, image->image, device->host_allocator);
        image->image = VK_NULL_HANDLE;
    }
    if(image->base_view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->device, image->base_view, device->host_allocator);
        image->base_view = VK_NULL_HANDLE;
    }
    if(image->memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->device, image->memory, device->host_allocator);
        image->memory = VK_NULL_HANDLE;
    }
}


#warning convert this to use sol_buffer instead of shunt buffer (which should be removed)

void* sol_vk_image_prepare_copy(struct sol_vk_image* image, struct sol_vk_buf_img_copy_list* copy_list, struct sol_vk_shunt_buffer* shunt_buffer, VkOffset3D offset, VkExtent3D extent, VkImageSubresourceLayers subresource)
{
    struct sol_vk_format_block_properties block_properties;
    VkDeviceSize byte_offset;
    VkDeviceSize byte_count;
    VkDeviceSize w, h;
    void* buffer_bytes;

    block_properties = sol_vk_format_block_properties(image->properties.format);

    /** offset and extent must be aligned to block texel size */
    assert(offset.x      % block_properties.texel_width  == 0);
    assert(offset.y      % block_properties.texel_height == 0);
    assert(extent.width  % block_properties.texel_width  == 0);
    assert(extent.height % block_properties.texel_height == 0);

    w = (VkDeviceSize)extent.width  / (VkDeviceSize)block_properties.texel_width;
    h = (VkDeviceSize)extent.height / (VkDeviceSize)block_properties.texel_height;
    byte_count = w * h * (VkDeviceSize)block_properties.bytes;

    buffer_bytes = sol_vk_shunt_buffer_reserve_bytes(shunt_buffer, byte_count, &byte_offset);

    *sol_vk_buf_img_copy_list_append_ptr(copy_list) = (VkBufferImageCopy)
    {
        .bufferOffset = byte_offset,
        /** 0 indicates tightly packed */
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = subresource,
        .imageOffset = offset,
        .imageExtent = extent,
    };

    return buffer_bytes;
}

void* sol_vk_image_prepare_copy_simple(struct sol_vk_image* image, struct sol_vk_buf_img_copy_list* copy_list, struct sol_vk_shunt_buffer* shunt_buffer, u16_vec2 offset, u16_vec2 extent, uint32_t array_layer)
{
    VkImageSubresourceLayers vk_subresource =
    {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = array_layer,
        .layerCount = 1,
    };
    VkOffset3D vk_offset =
    {
        .x = offset.x,
        .y = offset.y,
        .z = 0,
    };
    VkExtent3D vk_extent =
    {
        .width = extent.x,
        .height = extent.y,
        .depth = 1,
    };

    return sol_vk_image_prepare_copy(image, copy_list, shunt_buffer, vk_offset, vk_extent, vk_subresource);
}

void sol_vk_image_execute_copies(struct sol_vk_image* image, struct sol_vk_buf_img_copy_list* copy_list, VkCommandBuffer command_buffer, VkBuffer src_buffer, VkDeviceSize src_buffer_offset)
{
    uint32_t i, copy_count;
    VkBufferImageCopy* copy_actions;

    copy_count = sol_vk_buf_img_copy_list_count(copy_list);
    copy_actions = sol_vk_buf_img_copy_list_data(copy_list);

    if(copy_count)
    {
        for(i = 0; i < copy_count; i++)
        {
            copy_actions[i].bufferOffset += src_buffer_offset;
        }
        vkCmdCopyBufferToImage(command_buffer, src_buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy_count, copy_actions);

        sol_vk_buf_img_copy_list_reset(copy_list);
    }
}








void sol_vk_supervised_image_initialise(struct sol_vk_supervised_image* supervised_image, struct cvm_vk_device* device, const VkImageCreateInfo* image_create_info, const VkImageViewCreateInfo* view_create_info)
{
    sol_vk_image_create(&supervised_image->image, device, image_create_info, view_create_info);

    supervised_image->current_layout = image_create_info->initialLayout;

    supervised_image->write_stage_mask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    supervised_image->write_access_mask = VK_ACCESS_2_NONE;

    supervised_image->read_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    supervised_image->read_access_mask = VK_ACCESS_2_NONE;
}

void sol_vk_supervised_image_terminate(struct sol_vk_supervised_image* supervised_image, struct cvm_vk_device* device)
{
    sol_vk_image_destroy(&supervised_image->image, device);
}

void sol_vk_supervised_image_barrier(struct sol_vk_supervised_image* supervised_image, VkCommandBuffer command_buffer, VkImageLayout new_layout, VkPipelineStageFlagBits2 dst_stage_mask, VkAccessFlagBits2 dst_access_mask)
{
    /// thse definitions do restrict future use, but there isnt a good way around that, would be good to specify these values on the device...
    static const VkAccessFlagBits2 all_image_read_access_mask =
        VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT          |
        VK_ACCESS_2_SHADER_READ_BIT                    |
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT          |
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT  |
        VK_ACCESS_2_TRANSFER_READ_BIT                  |
        VK_ACCESS_2_HOST_READ_BIT                      |
        VK_ACCESS_2_MEMORY_READ_BIT                    |
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT            |
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT            |
        VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR          |
        VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR;

    static const VkAccessFlagBits2 all_image_write_access_mask =
        VK_ACCESS_2_SHADER_WRITE_BIT                   |
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT         |
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_TRANSFER_WRITE_BIT                 |
        VK_ACCESS_2_HOST_WRITE_BIT                     |
        VK_ACCESS_2_MEMORY_WRITE_BIT                   |
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT           |
        VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR         |
        VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR;



    /** need to wait on prior writes, waiting on reads (even in write case) is not necessary if there exists an execution dependency
     * (this barrier is the execution dependency) */
    const VkPipelineStageFlagBits2 src_stage_mask  = supervised_image->write_stage_mask;
    const VkAccessFlagBits2        src_access_mask = supervised_image->write_access_mask;

    /** make sure no unknown masks are used */
    assert((dst_access_mask & (all_image_read_access_mask | all_image_write_access_mask)) == dst_access_mask);
    assert(dst_stage_mask != VK_PIPELINE_STAGE_2_NONE);
    assert(dst_access_mask != VK_ACCESS_2_NONE);


    if(dst_access_mask & all_image_write_access_mask)
    {
        /** is a write op
         * reset all barriers, fresh slate, assumes this barrier transitively includes prior read & write scope */

        supervised_image->write_stage_mask  = dst_stage_mask;
        supervised_image->write_access_mask = dst_access_mask;

        supervised_image->read_stage_mask = VK_PIPELINE_STAGE_2_NONE;
        supervised_image->read_access_mask = VK_ACCESS_2_NONE;
    }
    else
    {
        /** is a read op */

        const bool stages_already_barriered   = (dst_stage_mask  & supervised_image->read_stage_mask ) == dst_stage_mask;
        const bool accesses_already_barriered = (dst_access_mask & supervised_image->read_access_mask) == dst_access_mask;

        if(stages_already_barriered && accesses_already_barriered && supervised_image->current_layout == new_layout)
        {
            /** barrier has already happened */
            return;
        }

        /** for ease of future checks on access-stage combinations we must barrier on the union of this and prior stage-access masks
         * is technically suboptimal, but shouldn't be an issue under most usage patterns */
        dst_stage_mask  |= supervised_image->read_stage_mask;
        dst_access_mask |= supervised_image->read_access_mask;

        supervised_image->read_stage_mask  = dst_stage_mask;
        supervised_image->read_access_mask = dst_access_mask;
    }


    VkDependencyInfo barrier_dependencies =
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = NULL,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = NULL,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = NULL,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = (VkImageMemoryBarrier2[1])
        {
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = NULL,
                .srcStageMask = src_stage_mask,
                .srcAccessMask = src_access_mask,
                .dstStageMask = dst_stage_mask,
                .dstAccessMask = dst_access_mask,
                .oldLayout = supervised_image->current_layout,
                .newLayout = new_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = supervised_image->image.image,
                .subresourceRange=(VkImageSubresourceRange)
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS
                }
            }
        }
    };

    vkCmdPipelineBarrier2(command_buffer, &barrier_dependencies);

    supervised_image->current_layout = new_layout;
}


void sol_vk_supervised_image_copy_regions_from_buffer(struct sol_vk_supervised_image* dst_image, struct sol_vk_buf_img_copy_list* copy_list, VkCommandBuffer command_buffer, VkBuffer src_buffer, VkDeviceSize src_buffer_offset)
{
    uint32_t i;
    VkBufferImageCopy* copy_actions = sol_vk_buf_img_copy_list_data(copy_list);
    uint32_t copy_count = sol_vk_buf_img_copy_list_count(copy_list);

    if(copy_count)
    {
        sol_vk_supervised_image_barrier(dst_image, command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

        for(i = 0; i < copy_count; i++)
        {
            copy_actions[i].bufferOffset += src_buffer_offset;
        }
        vkCmdCopyBufferToImage(command_buffer, src_buffer, dst_image->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy_count ,copy_actions);
    }

    sol_vk_buf_img_copy_list_reset(copy_list);
}


