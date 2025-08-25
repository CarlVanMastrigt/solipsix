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

// #include "math/u16_vec2.h"

// #include "vk/shunt_buffer.h"

#define SOL_STACK_ENTRY_TYPE VkBufferImageCopy
#define SOL_STACK_FUNCTION_PREFIX sol_vk_buf_img_copy_list
#define SOL_STACK_STRUCT_NAME sol_vk_buf_img_copy_list
#include "data_structures/stack.h"


//void* sol_vk_image_util_upload_to_region_2D(struct sol_vk_buf_img_copy_list* copy_list, struct sol_vk_shunt_buffer* shunt_buffer, u16_vec2 offset, u16_vec2 extent, uint32_t array_layer, VkFormat format);

