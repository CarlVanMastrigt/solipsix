/**
Copyright 2024 Carl van Mastrigt

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

#ifndef CVM_VK_H
#include "cvm_vk.h"
#endif


#ifndef CVM_VK_COMMAND_POOL_H
#define CVM_VK_COMMAND_POOL_H

#warning move this somewhere more appropriate


/** single threaded data structure! */
typedef struct cvm_vk_command_pool
{
    uint32_t device_queue_family_index;
    uint32_t device_queue_index;

    VkCommandPool pool;

    VkCommandBuffer* buffers;/// this almost certainly wants to be a list/stack and init as necessary
    #warning REALLY need to handle this better... use the fixed pool to manage command buffers instead and vend pointers??
    struct sol_vk_semaphore_submit_list* signal_lists;
    struct sol_vk_semaphore_submit_list* wait_lists;

    uint32_t acquired_buffer_count;
    uint32_t submitted_buffer_count;
    uint32_t total_buffer_count;
}
cvm_vk_command_pool;

typedef struct cvm_vk_command_buffer
{
    /** nothing in this is owned */
    const cvm_vk_command_pool* parent_pool;
    VkCommandBuffer buffer;

    struct sol_vk_semaphore_submit_list signal_list;
    struct sol_vk_semaphore_submit_list wait_list;
}
cvm_vk_command_buffer;

void cvm_vk_command_pool_initialise(cvm_vk_command_pool* pool, const struct cvm_vk_device* device, uint32_t device_queue_family_index, uint32_t device_queue_index);
void cvm_vk_command_pool_terminate(cvm_vk_command_pool* pool, const struct cvm_vk_device* device);
void cvm_vk_command_pool_reset(cvm_vk_command_pool* pool, const struct cvm_vk_device* device);

void cvm_vk_command_pool_acquire_command_buffer(cvm_vk_command_pool* pool, const struct cvm_vk_device* device, cvm_vk_command_buffer* command_buffer);
/** note: submit is also "release" - perhaps a better name then acquire is in order; perpare? */
/** this must be synchronised with device/ the queue, submission must be well ordered */
struct sol_vk_timeline_semaphore_moment cvm_vk_command_pool_submit_command_buffer(cvm_vk_command_pool* pool, const struct cvm_vk_device* device, cvm_vk_command_buffer* command_buffer, VkPipelineStageFlags2 completion_signal_stages);



#endif


