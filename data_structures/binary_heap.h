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

#pragma once

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>




/**
 * SOL_BINARY_HEAP_CMP(A,B) must be declared, with true resulting in the value A moving UP the heap
 *     ^ A and B are type*
 *
 * TODO: make any inputs to heap_cmp pointers and const (access top, paired with NULL in remove, append_ptr)
 *
 * TODO: use test sort heap performance improvements
 *
 * TODO: may be worth looking into adding support for random location deletion and heap
 * location tracking to support that, though probably not as adaptable as might be desirable
 */

#define SOL_BINARY_HEAP(type, struct_name, function_prefix)                                                     \
                                                                                                                \
struct struct_name                                                                                              \
{                                                                                                               \
    type* heap;                                                                                                 \
    uint32_t space;                                                                                             \
    uint32_t count;                                                                                             \
};                                                                                                              \
                                                                                                                \
static inline void function_prefix##_initialise(struct struct_name* heap, uint32_t initial_space)               \
{                                                                                                               \
    heap->heap = malloc(sizeof(type) * initial_space);                                                          \
    heap->space = initial_space;                                                                                \
    heap->count = 0;                                                                                            \
}                                                                                                               \
                                                                                                                \
static inline void function_prefix##_terminate(struct struct_name* heap)                                        \
{                                                                                                               \
    free(heap->heap);                                                                                           \
}                                                                                                               \
                                                                                                                \
static inline void function_prefix##_append(struct struct_name* restrict heap, const type data)                 \
{                                                                                                               \
    uint32_t u,d;                                                                                               \
                                                                                                                \
    if(heap->count == heap->space)                                                                              \
    {                                                                                                           \
        heap->space *= 2;                                                                                       \
        heap->heap = realloc(heap->heap, sizeof(type) * heap->space);                                           \
    }                                                                                                           \
                                                                                                                \
    d = heap->count++;                                                                                          \
                                                                                                                \
    while(d)                                                                                                    \
    {                                                                                                           \
        u = (d-1) >> 1;                                                                                         \
                                                                                                                \
        if(SOL_BINARY_HEAP_CMP((heap->heap + u), (&data)))                                                      \
        {                                                                                                       \
            break;                                                                                              \
        }                                                                                                       \
                                                                                                                \
        heap->heap[d] = heap->heap[u];                                                                          \
        d = u;                                                                                                  \
    }                                                                                                           \
                                                                                                                \
    heap->heap[d] = data;                                                                                       \
}                                                                                                               \
                                                                                                                \
static inline void function_prefix##_append_ptr(struct struct_name* restrict heap, const type* restrict data)   \
{                                                                                                               \
    uint32_t u,d;                                                                                               \
                                                                                                                \
    if(heap->count == heap->space)                                                                              \
    {                                                                                                           \
        heap->space *= 2;                                                                                       \
        heap->heap = realloc(heap->heap, sizeof(type) * heap->space);                                           \
    }                                                                                                           \
                                                                                                                \
    d = heap->count++;                                                                                          \
                                                                                                                \
    while(d)                                                                                                    \
    {                                                                                                           \
        u = (d-1) >> 1;                                                                                         \
                                                                                                                \
        if(SOL_BINARY_HEAP_CMP((heap->heap + u), (data)))                                                       \
        {                                                                                                       \
            break;                                                                                              \
        }                                                                                                       \
                                                                                                                \
        heap->heap[d] = heap->heap[u];                                                                          \
        d = u;                                                                                                  \
    }                                                                                                           \
                                                                                                                \
    heap->heap[d] = *data;                                                                                      \
}                                                                                                               \
                                                                                                                \
                                                                                                                \
static inline void function_prefix##_clear(struct struct_name* heap)                                            \
{                                                                                                               \
    heap->count = 0;                                                                                            \
}                                                                                                               \
                                                                                                                \
static inline bool function_prefix##_remove(struct struct_name* restrict heap, type* restrict data )            \
{                                                                                                               \
    uint32_t u,d,count;                                                                                         \
    const type* r;                                                                                              \
                                                                                                                \
    if(heap->count == 0)                                                                                        \
    {                                                                                                           \
        return false;                                                                                           \
    }                                                                                                           \
                                                                                                                \
    *data = heap->heap[0];                                                                                      \
    count = --heap->count;                                                                                      \
    r = heap->heap + count;                                                                                     \
    u = 0;                                                                                                      \
    d = 2;                                                                                                      \
                                                                                                                \
    while(d < count)                                                                                            \
    {                                                                                                           \
        d -= SOL_BINARY_HEAP_CMP((heap->heap + d - 1), (heap->heap + d));                                       \
        if(SOL_BINARY_HEAP_CMP((r), (heap->heap + d)))                                                          \
        {                                                                                                       \
            break;                                                                                              \
        }                                                                                                       \
        heap->heap[u] = heap->heap[d];                                                                          \
        u = d;                                                                                                  \
        d = (d << 1) + 2;                                                                                       \
    }                                                                                                           \
    if(d == count && SOL_BINARY_HEAP_CMP((heap->heap + d - 1), r))                                              \
    {                                                                                                           \
        heap->heap[u] = heap->heap[d-1];                                                                        \
        u = d-1;                                                                                                \
    }                                                                                                           \
                                                                                                                \
    heap->heap[u] = *r;                                                                                         \
    return true;                                                                                                \
}






