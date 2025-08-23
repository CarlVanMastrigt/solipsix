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

#include "cvm_vk.h"


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


