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



#ifndef SOL_BINARY_HEAP_ENTRY_TYPE
#error must define SOL_BINARY_HEAP_ENTRY_TYPE
#define SOL_BINARY_HEAP_ENTRY_TYPE int
#endif

#ifndef SOL_BINARY_HEAP_STRUCT_NAME
#error must define SOL_BINARY_HEAP_STRUCT_NAME
#define SOL_BINARY_HEAP_STRUCT_NAME placeholder_binary_heap
#endif

#ifndef SOL_BINARY_HEAP_FUNCTION_PREFIX
#define SOL_BINARY_HEAP_FUNCTION_PREFIX SOL_BINARY_HEAP_STRUCT_NAME
#endif

#ifndef SOL_BINARY_HEAP_DEFAULT_STARTING_SIZE
#define SOL_BINARY_HEAP_DEFAULT_STARTING_SIZE 64
#endif

#ifndef SOL_BINARY_HEAP_ENTRY_CMP_LT
#error must define bool SOL_BINARY_HEAP_ENTRY_CMP_LT(const SOL_BINARY_HEAP_ENTRY_TYPE* A, const SOL_BINARY_HEAP_ENTRY_TYPE* B) returning (A < B) with context as param if provided
#define SOL_BINARY_HEAP_ENTRY_CMP_LT(A,B) ((*A)<(*B))
#endif


#ifdef SOL_BINARY_HEAP_CONTEXT_TYPE
    #ifndef SOL_BINARY_HEAP_SET_ENTRY_INDEX
    #define SOL_BINARY_HEAP_SET_ENTRY_INDEX(E, IDX, CTX)
    #endif
    #define SOL_BINARY_HEAP_ENTRY_CMP_LT_CONTEXT(A, B) SOL_BINARY_HEAP_ENTRY_CMP_LT(A, B, context)
    #define SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT(E, IDX) SOL_BINARY_HEAP_SET_ENTRY_INDEX(E, IDX, context)
#else
    #ifndef SOL_BINARY_HEAP_SET_ENTRY_INDEX
    #define SOL_BINARY_HEAP_SET_ENTRY_INDEX(E, IDX)
    #endif
    #define SOL_BINARY_HEAP_ENTRY_CMP_LT_CONTEXT(A, B) SOL_BINARY_HEAP_ENTRY_CMP_LT(A, B)
    #define SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT(E, IDX) SOL_BINARY_HEAP_SET_ENTRY_INDEX(E, IDX)
#endif


struct SOL_BINARY_HEAP_STRUCT_NAME
{
    SOL_BINARY_HEAP_ENTRY_TYPE* heap;
    uint32_t space;
    uint32_t count;
};

static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_initialise)(struct SOL_BINARY_HEAP_STRUCT_NAME* heap, uint32_t initial_space)
{
    heap->heap = initial_space ? malloc(sizeof(SOL_BINARY_HEAP_ENTRY_TYPE) * initial_space) : NULL;
    heap->space = initial_space;
    heap->count = 0;
}

static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_terminate)(struct SOL_BINARY_HEAP_STRUCT_NAME* heap)
{
    free(heap->heap);
}

#ifdef SOL_BINARY_HEAP_CONTEXT_TYPE
static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_append)(struct SOL_BINARY_HEAP_STRUCT_NAME* restrict heap, SOL_BINARY_HEAP_ENTRY_TYPE entry, SOL_BINARY_HEAP_CONTEXT_TYPE context)
#else
static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_append)(struct SOL_BINARY_HEAP_STRUCT_NAME* restrict heap, SOL_BINARY_HEAP_ENTRY_TYPE entry)
#endif
{
    uint32_t next_index, index;

    if(heap->count == heap->space)
    {
        if(heap->space)
        {
            heap->space *= 2;
        }
        else
        {
            heap->space = SOL_BINARY_HEAP_DEFAULT_STARTING_SIZE;
        }
        heap->heap = realloc(heap->heap, sizeof(SOL_BINARY_HEAP_ENTRY_TYPE) * heap->space);
    }

    index = heap->count++;

    while(index)
    {
        next_index = (index - 1) >> 1;

        if(SOL_BINARY_HEAP_ENTRY_CMP_LT_CONTEXT((heap->heap + next_index), (&entry)))
        {
            break;
        }

        heap->heap[index] = heap->heap[next_index];
        SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT((heap->heap + index), index);
        index = next_index;
    }

    heap->heap[index] = entry;
    SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT((heap->heap + index), index);
}

static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_clear)(struct SOL_BINARY_HEAP_STRUCT_NAME* heap)
{
    heap->count = 0;
}

static inline uint32_t SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_count)(struct SOL_BINARY_HEAP_STRUCT_NAME* heap)
{
    return heap->count;
}

static inline const SOL_BINARY_HEAP_ENTRY_TYPE* SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_access_top)(struct SOL_BINARY_HEAP_STRUCT_NAME* heap)
{
    return heap->heap;
}

#ifdef SOL_BINARY_HEAP_CONTEXT_TYPE
static inline bool SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_withdraw)(struct SOL_BINARY_HEAP_STRUCT_NAME* restrict heap, SOL_BINARY_HEAP_ENTRY_TYPE* restrict entry, SOL_BINARY_HEAP_CONTEXT_TYPE context)
#else
static inline bool SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_withdraw)(struct SOL_BINARY_HEAP_STRUCT_NAME* restrict heap, SOL_BINARY_HEAP_ENTRY_TYPE* restrict entry)
#endif
{
    uint32_t index, next_index, count;
    const SOL_BINARY_HEAP_ENTRY_TYPE* replacement;

    if(heap->count == 0)
    {
        return false;
    }

    if(entry)
    {
        *entry = heap->heap[0];
    }
    count = --heap->count;
    replacement = heap->heap + count;
    index = 0;
    next_index = 2;

    while(next_index < count)
    {
        next_index -= SOL_BINARY_HEAP_ENTRY_CMP_LT_CONTEXT((heap->heap + next_index - 1), (heap->heap + next_index));
        if(SOL_BINARY_HEAP_ENTRY_CMP_LT_CONTEXT((replacement), (heap->heap + next_index)))
        {
            break;
        }
        heap->heap[index] = heap->heap[next_index];
        SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT((heap->heap + index), index);
        index = next_index;
        next_index = (next_index + 1) << 1;
    }
    if(next_index == count && (SOL_BINARY_HEAP_ENTRY_CMP_LT_CONTEXT((heap->heap + next_index - 1), replacement)))
    {
        heap->heap[index] = heap->heap[next_index - 1];
        SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT((heap->heap + index), index);
        index = next_index - 1;
    }

    heap->heap[index] = *replacement;
    SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT((heap->heap + index), index);
    return true;
}

#ifdef SOL_BINARY_HEAP_CONTEXT_TYPE
static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_withdraw_index)(struct SOL_BINARY_HEAP_STRUCT_NAME* restrict heap, uint32_t index, SOL_BINARY_HEAP_ENTRY_TYPE* restrict entry, SOL_BINARY_HEAP_CONTEXT_TYPE context)
#else
static inline void SOL_CONCATENATE(SOL_BINARY_HEAP_FUNCTION_PREFIX,_withdraw_index)(struct SOL_BINARY_HEAP_STRUCT_NAME* restrict heap, uint32_t index, SOL_BINARY_HEAP_ENTRY_TYPE* restrict entry)
#endif
{
    uint32_t next_index, count;
    const SOL_BINARY_HEAP_ENTRY_TYPE* replacement;

    assert(index < heap->count);

    if(entry)
    {
        *entry = heap->heap[index];
    }
    count = --heap->count;
    replacement = heap->heap + count;

    /** attempt to move replacement up the heap */
    while(index)
    {
        next_index = (index - 1) >> 1;

        if(SOL_BINARY_HEAP_ENTRY_CMP_LT_CONTEXT((heap->heap + next_index), (entry)))
        {
            break;
        }

        heap->heap[index] = heap->heap[next_index];
        SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT((heap->heap + index), index);
        index = next_index;
    }

    /** attempt to move replacement down the hep */
    next_index = (index + 1) << 1;
    while(next_index < count)
    {
        next_index -= SOL_BINARY_HEAP_ENTRY_CMP_LT_CONTEXT((heap->heap + next_index - 1), (heap->heap + next_index));
        if(SOL_BINARY_HEAP_ENTRY_CMP_LT_CONTEXT((replacement), (heap->heap + next_index)))
        {
            break;
        }
        heap->heap[index] = heap->heap[next_index];
        SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT((heap->heap + index), index);
        index = next_index;
        next_index = (next_index + 1) << 1;
    }
    if(next_index == count && (SOL_BINARY_HEAP_ENTRY_CMP_LT_CONTEXT((heap->heap + next_index - 1), replacement)))
    {
        heap->heap[index] = heap->heap[next_index - 1];
        SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT((heap->heap + index), index);
        index = next_index - 1;
    }

    heap->heap[index] = *replacement;
    SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT((heap->heap + index), index);
}


#undef SOL_BINARY_HEAP_ENTRY_TYPE
#undef SOL_BINARY_HEAP_STRUCT_NAME
#undef SOL_BINARY_HEAP_FUNCTION_PREFIX
#undef SOL_BINARY_HEAP_DEFAULT_STARTING_SIZE
#undef SOL_BINARY_HEAP_ENTRY_CMP_LT
#undef SOL_BINARY_HEAP_SET_ENTRY_INDEX

#undef SOL_BINARY_HEAP_ENTRY_CMP_LT_CONTEXT
#undef SOL_BINARY_HEAP_SET_ENTRY_INDEX_CONTEXT

#ifdef SOL_BINARY_HEAP_CONTEXT_TYPE
#undef SOL_BINARY_HEAP_CONTEXT_TYPE
#endif
