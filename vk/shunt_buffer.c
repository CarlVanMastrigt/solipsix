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

#include <assert.h>

#include "vk/shunt_buffer.h"
#include "sol_utils.h"
#include "vk/utils.h"


void sol_vk_shunt_buffer_initialise(struct sol_vk_shunt_buffer* buffer, VkDeviceSize alignment, VkDeviceSize max_size, bool multithreaded)
{
    assert((alignment & (alignment-1)) == 0);///alignment must be a power of 2
    buffer->alignment = alignment;
    buffer->multithreaded = multithreaded;
    buffer->max_size = sol_vk_align(max_size, alignment);
    if(multithreaded)
    {
        atomic_init(&buffer->atomic_offset, 0);
        buffer->size = buffer->max_size;
    }
    else
    {
        buffer->offset=0;
        buffer->size = SOL_MAX(16384, alignment * 4);
        assert(buffer->size <= buffer->max_size);/// specified size too small
    }
    buffer->backing = malloc(buffer->size);
}

void sol_vk_shunt_buffer_terminate(struct sol_vk_shunt_buffer* buffer)
{
    free(buffer->backing);
}


void sol_vk_shunt_buffer_reset(struct sol_vk_shunt_buffer* buffer)
{
    if(buffer->multithreaded)
    {
        atomic_store_explicit(&buffer->atomic_offset, 0, memory_order_relaxed);
    }
    else
    {
        buffer->offset=0;
    }
}

void * sol_vk_shunt_buffer_reserve_bytes(struct sol_vk_shunt_buffer* buffer, VkDeviceSize byte_count, VkDeviceSize* offset)
{
    uint_fast64_t current_offset;
    byte_count = sol_vk_align(byte_count, buffer->alignment);

    if(buffer->multithreaded)
    {
        /// this implementation is a little more expensive but ensures that anything that would consume the buffer can actually use it
        current_offset = atomic_load_explicit(&buffer->atomic_offset, memory_order_relaxed);
        do
        {
            if(current_offset + byte_count > buffer->size)
            {
                return NULL;
            }
        }
        while (atomic_compare_exchange_weak_explicit(&buffer->atomic_offset, &current_offset, current_offset+byte_count, memory_order_relaxed, memory_order_relaxed));
        *offset = current_offset;
    }
    else
    {
        *offset = buffer->offset;
        buffer->offset += byte_count;

        if(buffer->offset > buffer->size)
        {
            if(buffer->offset > buffer->max_size)
            {
                /// allocation cannot fit!
                buffer->offset -= byte_count;
                return NULL;
            }

            do buffer->size *= 2;
            while(buffer->offset > buffer->size);

            buffer->size = SOL_MIN(buffer->size, buffer->max_size);

            buffer->backing = realloc(buffer->backing, buffer->size);
        }
    }

    return buffer->backing + *offset;
}

VkDeviceSize sol_vk_shunt_buffer_get_space_used(struct sol_vk_shunt_buffer* buffer)
{
    if(buffer->multithreaded)
    {
        return atomic_load_explicit(&buffer->atomic_offset, memory_order_relaxed);
    }
    else
    {
        return buffer->offset;
    }
}

void sol_vk_shunt_buffer_copy(struct sol_vk_shunt_buffer* buffer, void* dst)
{
    memcpy(dst, buffer->backing, sol_vk_shunt_buffer_get_space_used(buffer));
}




