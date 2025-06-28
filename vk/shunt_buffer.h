/**
Copyright 2024,2025 Carl van Mastrigt

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

#include <stdatomic.h>
#include <stdbool.h>
#include <vulkan/vulkan.h>

/** this is a super simple structure for building up data to copy into a staging buffer, mostly just utility
note: even if multithreaded requires external synchronization to ensure
    no reservations or modifications to the buffers contents are made after the buffer is copied (presumably to a staging buffer)
*/

struct sol_vk_shunt_buffer
{
    bool multithreaded;
    char* backing;
    VkDeviceSize alignment;
    VkDeviceSize size;
    union
    {
        VkDeviceSize offset;/** non-multithreaded */
        atomic_uint_fast64_t atomic_offset;/** multithreaded */
    };
};

void sol_vk_shunt_buffer_initialise(struct sol_vk_shunt_buffer* buffer, VkDeviceSize alignment, VkDeviceSize size, bool multithreaded);
void sol_vk_shunt_buffer_terminate(struct sol_vk_shunt_buffer* buffer);

void sol_vk_shunt_buffer_reset(struct sol_vk_shunt_buffer* buffer);

/** returned pointer is valid until shunt_buffer is reset, will return NULL if no space remains */
void * sol_vk_shunt_buffer_reserve_bytes(struct sol_vk_shunt_buffer* buffer, VkDeviceSize byte_count, VkDeviceSize* offset);

VkDeviceSize sol_vk_shunt_buffer_get_space_used(struct sol_vk_shunt_buffer* buffer);
void sol_vk_shunt_buffer_copy(struct sol_vk_shunt_buffer* buffer, void* dst);

