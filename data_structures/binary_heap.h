/**
Copyright 2020,2021,2022,2023,2024,2025 Carl van Mastrigt

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

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>

#include "sol_utils.h"


/**
 * SOL_BINARY_HEAP_ENTRY_CMP_LESS(A,B) must be declared, with true resulting in the value A moving UP the heap
 *     ^ A and B are SOL_BINARY_HEAP_ENTRY_TYPE*
 *
 * TODO: make any inputs to heap_cmp pointers and const (access top, paired with NULL in remove, append_ptr)
 *
 * TODO: use test sort heap performance improvements
 *
 * TODO: may be worth looking into adding support for random location deletion and heap
 * location tracking to support that, though probably not as adaptable as might be desirable
 */


#ifndef SOL_BINARY_HEAP_ENTRY_TYPE
#error must define SOL_BINARY_HEAP_ENTRY_TYPE
#define SOL_BINARY_HEAP_ENTRY_TYPE int
#endif

#ifndef SOL_BINARY_HEAP_STRUCT_NAME
#error must define SOL_BINARY_HEAP_STRUCT_NAME
#define SOL_BINARY_HEAP_STRUCT_NAME placeholder_hash_map
#endif

#ifndef SOL_BINARY_HEAP_FUNCTION_PREFIX
#error must define SOL_BINARY_HEAP_FUNCTION_PREFIX
#define SOL_BINARY_HEAP_FUNCTION_PREFIX placeholder_hash_map
#endif

#ifndef SOL_BINARY_HEAP_ENTRY_CMP_LESS
#error must define SOL_BINARY_HEAP_ENTRY_CMP_LESS(const SOL_BINARY_HEAP_ENTRY_TYPE*, const SOL_BINARY_HEAP_ENTRY_TYPE*) returning a bool
#define SOL_BINARY_HEAP_ENTRY_CMP_LESS(A,B) ((*A)<(*B))
#endif




struct SOL_BINARY_HEAP_STRUCT_NAME
{
    SOL_BINARY_HEAP_ENTRY_TYPE* heap;
    uint32_t space;
    uint32_t count;
};

static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_initialise)(struct SOL_BINARY_HEAP_STRUCT_NAME* heap, uint32_t initial_space)
{
    heap->heap = malloc(sizeof(SOL_BINARY_HEAP_ENTRY_TYPE) * initial_space);
    heap->space = initial_space;
    heap->count = 0;
}

static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_terminate)(struct SOL_BINARY_HEAP_STRUCT_NAME* heap)
{
    free(heap->heap);
}

static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_append)(struct SOL_BINARY_HEAP_STRUCT_NAME* restrict heap, const SOL_BINARY_HEAP_ENTRY_TYPE data)
{
    uint32_t u,d;

    if(heap->count == heap->space)
    {
        heap->space *= 2;
        heap->heap = realloc(heap->heap, sizeof(SOL_BINARY_HEAP_ENTRY_TYPE) * heap->space);
    }

    d = heap->count++;

    while(d)
    {
        u = (d-1) >> 1;

        if(SOL_BINARY_HEAP_ENTRY_CMP_LESS((heap->heap + u), (&data)))
        {
            break;
        }

        heap->heap[d] = heap->heap[u];
        d = u;
    }

    heap->heap[d] = data;
}

static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_append_ptr)(struct SOL_BINARY_HEAP_STRUCT_NAME* restrict heap, const SOL_BINARY_HEAP_ENTRY_TYPE* restrict data)
{
    uint32_t u,d;

    if(heap->count == heap->space)
    {
        heap->space *= 2;
        heap->heap = realloc(heap->heap, sizeof(SOL_BINARY_HEAP_ENTRY_TYPE) * heap->space);
    }

    d = heap->count++;

    while(d)
    {
        u = (d-1) >> 1;

        if(SOL_BINARY_HEAP_ENTRY_CMP_LESS((heap->heap + u), (data)))
        {
            break;
        }

        heap->heap[d] = heap->heap[u];
        d = u;
    }

    heap->heap[d] = *data;
}


static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_clear)(struct SOL_BINARY_HEAP_STRUCT_NAME* heap)
{
    heap->count = 0;
}

static inline bool SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_remove)(struct SOL_BINARY_HEAP_STRUCT_NAME* restrict heap, SOL_BINARY_HEAP_ENTRY_TYPE* restrict data )
{
    uint32_t u,d,count;
    const SOL_BINARY_HEAP_ENTRY_TYPE* r;

    if(heap->count == 0)
    {
        return false;
    }

    *data = heap->heap[0];
    count = --heap->count;
    r = heap->heap + count;
    u = 0;
    d = 2;

    while(d < count)
    {
        d -= SOL_BINARY_HEAP_ENTRY_CMP_LESS((heap->heap + d - 1), (heap->heap + d));
        if(SOL_BINARY_HEAP_ENTRY_CMP_LESS((r), (heap->heap + d)))
        {
            break;
        }
        heap->heap[u] = heap->heap[d];
        u = d;
        d = (d << 1) + 2;
    }
    if(d == count && SOL_BINARY_HEAP_ENTRY_CMP_LESS((heap->heap + d - 1), r))
    {
        heap->heap[u] = heap->heap[d-1];
        u = d-1;
    }

    heap->heap[u] = *r;
    return true;
}



#undef SOL_BINARY_HEAP_ENTRY_TYPE
#undef SOL_BINARY_HEAP_STRUCT_NAME
#undef SOL_BINARY_HEAP_FUNCTION_PREFIX
#undef SOL_BINARY_HEAP_ENTRY_CMP_LESS

