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

#pragma once

#include <vulkan/vulkan.h>

#include "cvm_vk.h"
#warning instead: move sol_vk_image to its own header! possibly this one!
// struct cvm_vk_device;

struct sol_vk_supervised_image
{
    /** conceptually: this relies on write->write barriers being transitive
     * must be extrnally synchronised and match execution order of submitted work (ergo probably best to just use this in single threaded fashion) */

    struct sol_vk_image image;

    VkImageLayout current_layout;

    /** most recent writes (all prior writes will be dependency to this write) */
    VkPipelineStageFlagBits2 write_stage_mask;
    VkAccessFlagBits2 write_access_mask;

    /** acculumation of read barriuers that have occurred since last write, needs to be reset whenever a write is preformed,
     * will also be used as dependency to write ops though often not strictly necessary
     * (from spec: "Write-after-read hazards can be solved with just an execution dependency") */
    VkPipelineStageFlagBits2 read_stage_mask;
    VkAccessFlagBits2 read_access_mask;
};

void sol_vk_supervised_image_initialise(struct sol_vk_supervised_image* supervised_image, struct cvm_vk_device* device, const VkImageCreateInfo* image_create_info);
void sol_vk_supervised_image_terminate(struct sol_vk_supervised_image* supervised_image, struct cvm_vk_device* device);//does nothing

void sol_vk_supervised_image_barrier(struct sol_vk_supervised_image* supervised_image, VkCommandBuffer cb, VkImageLayout new_layout, VkPipelineStageFlagBits2 dst_stage_mask, VkAccessFlagBits2 dst_access_mask);
/** TODO: want a function that manages similar to above but in the context of render passes */



