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

#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>



#define SOL_FIXED_SIZE_QUEUE(type, struct_name, function_prefix)                                               \
                                                                                                               \
struct struct_name                                                                                             \
{                                                                                                              \
    type* data;                                                                                                \
    uint32_t space;                                                                                            \
    uint32_t count;                                                                                            \
    uint32_t front;                                                                                            \
};                                                                                                             \
                                                                                                               \
                                                                                                               \
static inline void function_prefix##_initialise(struct struct_name* q, uint32_t size)                          \
{                                                                                                              \
    assert((size & (size - 1 )) == 0);                                                                         \
    q->data = malloc(sizeof(type) * size);                                                                     \
    q->space = size;                                                                                           \
    q->count = 0;                                                                                              \
    q->front = 0;                                                                                              \
}                                                                                                              \
                                                                                                               \
static inline void function_prefix##_terminate(struct struct_name* q)                                          \
{                                                                                                              \
    free(q->data);                                                                                             \
}                                                                                                              \
                                                                                                               \
static inline type* function_prefix##_access_entry(struct struct_name* q, uint32_t index)                      \
{                                                                                                              \
    assert(index < q->front+q->count && index >= q->front);                                                    \
    return q->data + (index & (q->space - 1));                                                                 \
}                                                                                                              \
                                                                                                               \
static inline bool function_prefix##_enqueue_index(struct struct_name* q, uint32_t* index_ptr)                 \
{                                                                                                              \
    uint32_t front_offset, move_count;                                                                         \
    type* src;                                                                                                 \
    if(q->count == q->space)                                                                                   \
    {                                                                                                          \
        return false;                                                                                          \
    }                                                                                                          \
    if(index_ptr)                                                                                              \
    {                                                                                                          \
        *index_ptr = q->front + q->count++;                                                                    \
    }                                                                                                          \
    return true;                                                                                               \
}                                                                                                              \
                                                                                                               \
static inline bool function_prefix##_enqueue_ptr(struct struct_name* q, type** location, uint32_t* index_ptr)  \
{                                                                                                              \
    uint32_t index;                                                                                            \
    if(function_prefix##_enqueue_index(q, &index))                                                             \
    {                                                                                                          \
        if(index_ptr)                                                                                          \
        {                                                                                                      \
            *index_ptr = index;                                                                                \
        }                                                                                                      \
        if(location)                                                                                           \
        {                                                                                                      \
            *location = q->data + (index & (q->space - 1));                                                    \
        }                                                                                                      \
        return true;                                                                                           \
    }                                                                                                          \
    return false;                                                                                              \
}                                                                                                              \
                                                                                                               \
static inline bool function_prefix##_enqueue(struct struct_name* q, type value, uint32_t* index_ptr)           \
{                                                                                                              \
    uint32_t index;                                                                                            \
    if(function_prefix##_enqueue_index(q, &index))                                                             \
    {                                                                                                          \
        if(index_ptr)                                                                                          \
        {                                                                                                      \
            *index_ptr = index;                                                                                \
        }                                                                                                      \
        q->data[index & (q->space - 1)] = value;                                                               \
        return true;                                                                                           \
    }                                                                                                          \
    return false;                                                                                              \
}                                                                                                              \
                                                                                                               \
static inline bool function_prefix##_dequeue_ptr(struct struct_name* q, type** location)                       \
{                                                                                                              \
    type* ptr;                                                                                                 \
    if(q->count == 0)                                                                                          \
    {                                                                                                          \
        return false;                                                                                          \
    }                                                                                                          \
    if(location)                                                                                               \
    {                                                                                                          \
        *location = q->data + (q->front & (q->space - 1));                                                     \
    }                                                                                                          \
    q->front++;                                                                                                \
    q->count--;                                                                                                \
    return true;                                                                                               \
}                                                                                                              \
                                                                                                               \
static inline bool function_prefix##_dequeue(struct struct_name* q, type* value)                               \
{                                                                                                              \
    if(q->count == 0)                                                                                          \
    {                                                                                                          \
        return false;                                                                                          \
    }                                                                                                          \
    if(value)                                                                                                  \
    {                                                                                                          \
        *value = q->data[ q->front & (q->space - 1) ];                                                         \
    }                                                                                                          \
    q->front++;                                                                                                \
    q->count--;                                                                                                \
    return true;                                                                                               \
}                                                                                                              \
                                                                                                               \
static inline type* function_prefix##_access_front(struct struct_name* q)                                      \
{                                                                                                              \
    if(q->count == 0)                                                                                          \
    {                                                                                                          \
        return NULL;                                                                                           \
    }                                                                                                          \
    return q->data + (q->front & (q->space - 1));                                                              \
}                                                                                                              \
                                                                                                               \
static inline type* function_prefix##_access_back(struct struct_name* q)                                       \
{                                                                                                              \
    if(q->count == 0)                                                                                          \
    {                                                                                                          \
        return NULL;                                                                                           \
    }                                                                                                          \
    return q->data + ((q->front + q->count - 1) & (q->space - 1));                                             \
}                                                                                                              \




