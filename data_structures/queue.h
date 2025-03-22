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
#include <string.h>
#include <assert.h>

/**
brief on queue resizing:
need to move the correct part of the buffer to maintain (modulo) indices after resizing:
|+++o+++|
|---o+++++++----|
vs
        |+++o+++|
|+++--------o+++|
^ realloced segment array with alignment of relative (intended) indices/offsets


iterating a queue
for(i=0;i<q->count;i++) queue_get_ptr(q, q->front + i)
*/

#define SOL_QUEUE(type, struct_name, function_prefix)                                                            \
                                                                                                                 \
struct struct_name                                                                                               \
{                                                                                                                \
    type* data;                                                                                                  \
    uint32_t space;                                                                                              \
    uint32_t count;                                                                                              \
    uint32_t front;                                                                                              \
};                                                                                                               \
                                                                                                                 \
                                                                                                                 \
static inline void function_prefix##_initialise(struct struct_name* q, uint32_t initial_size)                    \
{                                                                                                                \
    assert((initial_size & (initial_size - 1)) == 0);                                                            \
    q->data = malloc(sizeof(type) * initial_size);                                                               \
    q->space = initial_size;                                                                                     \
    q->count = 0;                                                                                                \
    q->front = 0;                                                                                                \
}                                                                                                                \
                                                                                                                 \
static inline void function_prefix##_terminate(struct struct_name* q)                                            \
{                                                                                                                \
    free(q->data);                                                                                               \
}                                                                                                                \
                                                                                                                 \
static inline type* function_prefix##_access_entry(struct struct_name* q, uint32_t index)                        \
{                                                                                                                \
    assert(index < q->front+q->count && index >= q->front);                                                      \
    return q->data + (index & (q->space - 1));                                                                   \
}                                                                                                                \
                                                                                                                 \
static inline void function_prefix##_enqueue_index(struct struct_name* q, uint32_t* index_ptr)                   \
{                                                                                                                \
    uint32_t front_offset, move_count;                                                                           \
    type * src;                                                                                                  \
    assert(index_ptr);                                                                                           \
    if(q->count == q->space)                                                                                     \
    {                                                                                                            \
        q->data = realloc(q->data, sizeof(type) * q->count * 2);                                                 \
        front_offset = q->front & (q->space - 1);                                                                \
        if(q->front & q->space)                                                                                  \
        {                                                                                                        \
            src = q->data + front_offset;                                                                        \
            move_count = q->space - front_offset;                                                                \
        }                                                                                                        \
        else                                                                                                     \
        {                                                                                                        \
            src = q->data;                                                                                       \
            move_count = front_offset;                                                                           \
        }                                                                                                        \
        memcpy(src + q->space, src, sizeof(type) * move_count);                                                  \
        q->space *= 2;                                                                                           \
    }                                                                                                            \
    *index_ptr = q->front + q->count;                                                                            \
    q->count++;                                                                                                  \
}                                                                                                                \
                                                                                                                 \
static inline void function_prefix##_enqueue_ptr(struct struct_name* q, type** entry_ptr, uint32_t* index_ptr)   \
{                                                                                                                \
    uint32_t index;                                                                                              \
    assert(entry_ptr);                                                                                           \
    function_prefix##_enqueue_index(q, &index);                                                                  \
    if(index_ptr) *index_ptr = index;                                                                            \
    *entry_ptr = q->data + (index & (q->space - 1));                                                             \
}                                                                                                                \
                                                                                                                 \
static inline void function_prefix##_enqueue(struct struct_name* q, type value, uint32_t* index_ptr)             \
{                                                                                                                \
    uint32_t index;                                                                                              \
    function_prefix##_enqueue_index(q, &index);                                                                  \
    if(index_ptr) *index_ptr = index;                                                                            \
    q->data[index & (q->space - 1)] = value;                                                                     \
}                                                                                                                \
                                                                                                                 \
static inline bool function_prefix##_dequeue_ptr(struct struct_name* q, type** entry_ptr)                        \
{                                                                                                                \
    assert(entry_ptr);                                                                                           \
    if(q->count == 0)                                                                                            \
    {                                                                                                            \
        *entry_ptr = NULL;                                                                                       \
        return false;                                                                                            \
    }                                                                                                            \
    *entry_ptr = q->data + (q->front & (q->space - 1));                                                          \
    q->front++;                                                                                                  \
    q->count--;                                                                                                  \
    return true;                                                                                                 \
}                                                                                                                \
                                                                                                                 \
static inline bool function_prefix##_dequeue(struct struct_name* q, type* value)                                 \
{                                                                                                                \
    assert(value);                                                                                               \
    if(q->count == 0) return false;                                                                              \
    *value = q->data[q->front & (q->space - 1)];                                                                 \
    q->front++;                                                                                                  \
    q->count--;                                                                                                  \
    return true;                                                                                                 \
}                                                                                                                \
                                                                                                                 \
/** remove the front of the queue without accessing its contents */                                              \
/** should only be used after access_front has been used to check the entry that will be pruned */               \
static inline void function_prefix##_prune_front(struct struct_name* q)                                          \
{                                                                                                                \
    assert(q->count > 0);                                                                                        \
    q->front++;                                                                                                  \
    q->count--;                                                                                                  \
}                                                                                                                \
                                                                                                                 \
static inline bool function_prefix##_access_front(struct struct_name* q, type** entry_ptr)                       \
{                                                                                                                \
    assert(entry_ptr);                                                                                           \
    if(q->count == 0)                                                                                            \
    {                                                                                                            \
        *entry_ptr = NULL;                                                                                       \
        return false;                                                                                            \
    }                                                                                                            \
    *entry_ptr = q->data + (q->front & (q->space - 1));                                                          \
    return true;                                                                                                 \
}                                                                                                                \
                                                                                                                 \
static inline bool function_prefix##_access_back(struct struct_name* q, type** entry_ptr)                        \
{                                                                                                                \
    assert(entry_ptr);                                                                                           \
    if(q->count == 0)                                                                                            \
    {                                                                                                            \
        *entry_ptr = NULL;                                                                                       \
        return false;                                                                                            \
    }                                                                                                            \
    *entry_ptr = q->data + ((q->front + q->count - 1) & (q->space - 1));                                         \
    return true;                                                                                                 \
}                                                                                                                \




