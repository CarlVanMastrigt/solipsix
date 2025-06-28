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


void sol_vk_shunt_buffer_initialise(struct sol_vk_shunt_buffer* buffer, VkDeviceSize alignment, VkDeviceSize size, bool multithreaded)
{
    assert((alignment & (alignment-1)) == 0);/** alignment must be a power of 2 */
    assert((size & (alignment-1)) == 0 && size >= alignment);/** size must be a multiple of alignment */
    buffer->alignment = alignment;
    buffer->multithreaded = multithreaded;
    buffer->size = size;
    if(multithreaded)
    {
        atomic_init(&buffer->atomic_offset, 0);
    }
    else
    {
        buffer->offset=0;
    }
    buffer->backing = malloc(size);
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
        buffer->offset = 0;
    }
}

void * sol_vk_shunt_buffer_reserve_bytes(struct sol_vk_shunt_buffer* buffer, VkDeviceSize byte_count, VkDeviceSize* offset)
{
    uint_fast64_t current_offset;
    byte_count = sol_vk_align(byte_count, buffer->alignment);

    assert(byte_count < buffer->size);/** is invalid to request space greter than the shunt buffers size */

    if(buffer->multithreaded)
    {
        /** this implementation is a little more expensive but ensures that anything that would consume buffer space can actually use it (i.e. don't consume space only to fail) */
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
        if(buffer->offset + byte_count > buffer->size)
        {
            return NULL;
        }

        *offset = buffer->offset;
        buffer->offset += byte_count;
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

