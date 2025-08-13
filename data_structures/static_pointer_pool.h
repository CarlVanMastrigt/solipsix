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

#warning this could be typeless! just pass in size, but doing so removes strict typing which is desired

#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#ifdef SOL_STATIC_POINTER_POOL_MULTITHREADED
#include <threads.h>
#define SOL_STATIC_POINTER_POOL_MTX mtx_t mutex
#define SOL_STATIC_POINTER_POOL_MTX_INIT mtx_init(&pool->mutex,mtx_plain)
#define SOL_STATIC_POINTER_POOL_MTX_DESTROY mtx_destroy(&pool->mutex)
#define SOL_STATIC_POINTER_POOL_MTX_LOCK mtx_lock(&pool->mutex)
#define SOL_STATIC_POINTER_POOL_MTX_UNLOCK mtx_unlock(&pool->mutex)
#undef SOL_STATIC_POINTER_POOL_MULTITHREADED
#else
#define SOL_STATIC_POINTER_POOL_MTX
#define SOL_STATIC_POINTER_POOL_MTX_INIT
#define SOL_STATIC_POINTER_POOL_MTX_DESTROY
#define SOL_STATIC_POINTER_POOL_MTX_LOCK
#define SOL_STATIC_POINTER_POOL_MTX_UNLOCK
#endif

#include "sol_utils.h"
#include "data_structures/available_pointer_stack.h"



#ifndef SOL_STATIC_POINTER_POOL_ENTRY_TYPE
#error must define SOL_STATIC_POINTER_POOL_ENTRY_TYPE
#define SOL_STATIC_POINTER_POOL_ENTRY_TYPE int
#endif

#ifndef SOL_STATIC_POINTER_POOL_FUNCTION_PREFIX
#error must define SOL_STATIC_POINTER_POOL_FUNCTION_PREFIX
#define SOL_STATIC_POINTER_POOL_FUNCTION_PREFIX placeholder_static_pointer_pool
#endif

#ifndef SOL_STATIC_POINTER_POOL_STRUCT_NAME
#error must define SOL_STATIC_POINTER_POOL_STRUCT_NAME
#define SOL_STATIC_POINTER_POOL_STRUCT_NAME placeholder_static_pointer_pool
#endif

#ifndef SOL_STATIC_POINTER_POOL_SLAB_SIZE
#define SOL_STATIC_POINTER_POOL_SLAB_SIZE 1024
#endif

struct SOL_STATIC_POINTER_POOL_STRUCT_NAME
{
    struct sol_available_pointer_stack available_pointers;
    struct sol_available_pointer_stack allocations;
    SOL_STATIC_POINTER_POOL_ENTRY_TYPE* active_allocation;
    uint32_t active_slab_remaining;

    SOL_STATIC_POINTER_POOL_MTX;
};

static inline void SOL_CONCATENATE(SOL_STATIC_POINTER_POOL_FUNCTION_PREFIX,_initialise)(struct SOL_STATIC_POINTER_POOL_STRUCT_NAME* pool)
{
    sol_available_pointer_stack_initialise(&pool->available_pointers, SOL_STATIC_POINTER_POOL_SLAB_SIZE);
    sol_available_pointer_stack_initialise(&pool->allocations, 64);
    pool->active_allocation = NULL;
    pool->active_slab_remaining = 0;

    SOL_STATIC_POINTER_POOL_MTX_INIT;
}

static inline void SOL_CONCATENATE(SOL_STATIC_POINTER_POOL_FUNCTION_PREFIX,_terminate)(struct SOL_STATIC_POINTER_POOL_STRUCT_NAME* pool)
{
    void* allocation;
    /** make sure all entries have been released */
    assert(pool->active_slab_remaining + sol_available_pointer_stack_count(&pool->available_pointers) ==
        sol_available_pointer_stack_count(&pool->allocations) * SOL_STATIC_POINTER_POOL_SLAB_SIZE);

    /** free all allocations */
    while(sol_available_pointer_stack_remove(&pool->allocations, &allocation))
    {
        free(allocation);
    }
    sol_available_pointer_stack_terminate(&pool->available_pointers);
    sol_available_pointer_stack_terminate(&pool->allocations);

    SOL_STATIC_POINTER_POOL_MTX_DESTROY;
}

static inline SOL_STATIC_POINTER_POOL_ENTRY_TYPE* SOL_CONCATENATE(SOL_STATIC_POINTER_POOL_FUNCTION_PREFIX,_acquire)(struct SOL_STATIC_POINTER_POOL_STRUCT_NAME* pool)
{
    void* ptr;

    SOL_STATIC_POINTER_POOL_MTX_LOCK;
    if(!sol_available_pointer_stack_remove(&pool->available_pointers, &ptr))
    {
        if(pool->active_slab_remaining > 0)
        {
            pool->active_slab_remaining--;
            ptr = pool->active_allocation + pool->active_slab_remaining;
        }
        else
        {
            pool->active_allocation = malloc(sizeof(SOL_STATIC_POINTER_POOL_ENTRY_TYPE) * SOL_STATIC_POINTER_POOL_SLAB_SIZE);
            sol_available_pointer_stack_append(&pool->allocations, pool->active_allocation);

            pool->active_slab_remaining = SOL_STATIC_POINTER_POOL_SLAB_SIZE - 1;
            ptr = pool->active_allocation + (SOL_STATIC_POINTER_POOL_SLAB_SIZE - 1);
        }
    }
    SOL_STATIC_POINTER_POOL_MTX_UNLOCK;
    return ptr;
}

static inline void SOL_CONCATENATE(SOL_STATIC_POINTER_POOL_FUNCTION_PREFIX,_release)(struct SOL_STATIC_POINTER_POOL_STRUCT_NAME* pool, SOL_STATIC_POINTER_POOL_ENTRY_TYPE* entry)
{
    SOL_STATIC_POINTER_POOL_MTX_LOCK;
    sol_available_pointer_stack_append(&pool->available_pointers, entry);
    SOL_STATIC_POINTER_POOL_MTX_UNLOCK;
}


#undef SOL_STATIC_POINTER_POOL_ENTRY_TYPE
#undef SOL_STATIC_POINTER_POOL_FUNCTION_PREFIX
#undef SOL_STATIC_POINTER_POOL_STRUCT_NAME
#undef SOL_STATIC_POINTER_POOL_SLAB_SIZE

#undef SOL_STATIC_POINTER_POOL_MTX
#undef SOL_STATIC_POINTER_POOL_MTX_INIT
#undef SOL_STATIC_POINTER_POOL_MTX_DESTROY
#undef SOL_STATIC_POINTER_POOL_MTX_LOCK
#undef SOL_STATIC_POINTER_POOL_MTX_UNLOCK

