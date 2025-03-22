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



#define SOL_FIXED_SIZE_QUEUE(type, struct_name, function_prefix)                                                                  \
                                                                                                                                  \
struct struct_name                                                                                                                \
{                                                                                                                                 \
    type* data;                                                                                                                   \
    uint32_t space;                                                                                                               \
    uint32_t count;                                                                                                               \
    uint32_t front;                                                                                                               \
};                                                                                                                                \
                                                                                                                                  \
                                                                                                                                  \
static inline void function_prefix##_initialise(struct struct_name* q, uint32_t size)                                             \
{                                                                                                                                 \
    q->data = malloc(sizeof(type) * size);                                                                                        \
    q->space = size;                                                                                                              \
    q->count = 0;                                                                                                                 \
    q->front = 0;                                                                                                                 \
}                                                                                                                                 \
                                                                                                                                  \
static inline void function_prefix##_terminate(struct struct_name* q)                                                             \
{                                                                                                                                 \
    free(q->data);                                                                                                                \
}                                                                                                                                 \
                                                                                                                                  \
static inline type* function_prefix##_access_entry(struct struct_name* q, uint32_t index)                                         \
{                                                                                                                                 \
    assert(index < q->space);                                                                                                     \
    return q->data + index;                                                                                                       \
}                                                                                                                                 \
                                                                                                                                  \
static inline bool function_prefix##_enqueue_index(struct struct_name* q, uint32_t* index_ptr)                                    \
{                                                                                                                                 \
    uint32_t index;                                                                                                               \
    assert(index_ptr);                                                                                                            \
    if(q->count == q->space) return false;                                                                                        \
    index = q->front + q->count;                                                                                                  \
    if(index > q->space) index -= q->space;                                                                                       \
    *index_ptr = index;                                                                                                           \
    q->count++;                                                                                                                   \
    return true;                                                                                                                  \
}                                                                                                                                 \
                                                                                                                                  \
static inline bool function_prefix##_enqueue_ptr(struct struct_name* q, type** entry_ptr, uint32_t* index_ptr)                    \
{                                                                                                                                 \
    uint32_t index;                                                                                                               \
    assert(entry_ptr);                                                                                                            \
    if(function_prefix##_enqueue_index(q, &index))                                                                                \
    {                                                                                                                             \
        if(index_ptr) *index_ptr = index;                                                                                         \
        *entry_ptr = q->data + index;                                                                                             \
        return true;                                                                                                              \
    }                                                                                                                             \
    *entry_ptr = NULL;                                                                                                            \
    return false;                                                                                                                 \
}                                                                                                                                 \
                                                                                                                                  \
static inline bool function_prefix##_enqueue(struct struct_name* q, type value, uint32_t* index_ptr)                              \
{                                                                                                                                 \
    uint32_t index;                                                                                                               \
    if(function_prefix##_enqueue_index(q, &index))                                                                                \
    {                                                                                                                             \
        if(index_ptr) *index_ptr = index;                                                                                         \
        q->data[index] = value;                                                                                                   \
        return true;                                                                                                              \
    }                                                                                                                             \
    return false;                                                                                                                 \
}                                                                                                                                 \
                                                                                                                                  \
static inline bool function_prefix##_dequeue_ptr(struct struct_name* q, type** entry_ptr)                                         \
{                                                                                                                                 \
    type* ptr;                                                                                                                    \
    assert(entry_ptr);                                                                                                            \
    if(q->count == 0)                                                                                                             \
    {                                                                                                                             \
        *entry_ptr = NULL;                                                                                                        \
        return false;                                                                                                             \
    }                                                                                                                             \
    *entry_ptr = q->data + q->front;                                                                                              \
    q->count--;                                                                                                                   \
    q->front++;                                                                                                                   \
    if(q->front > q->space) q->front -= q->space;                                                                                 \
    return true;                                                                                                                  \
}                                                                                                                                 \
                                                                                                                                  \
static inline bool function_prefix##_dequeue(struct struct_name* q, type* value)                                                  \
{                                                                                                                                 \
    assert(value);                                                                                                                \
    if(q->count == 0) return false;                                                                                               \
    *value = q->data[q->front];                                                                                                   \
    q->count--;                                                                                                                   \
    q->front++;                                                                                                                   \
    if(q->front > q->space) q->front -= q->space;                                                                                 \
    return true;                                                                                                                  \
}                                                                                                                                 \
                                                                                                                                  \
/** if there is space left just enqueue and return */                                                                             \
/** otherwise; take the front of the queue and move it to the back */                                                             \
/** pointer is both the old front and new back of the queue, the index is the new back */                                         \
/** returns wheter the pointer was requeued, false if just enqueued */                                                            \
static inline bool function_prefix##_requeue_ptr(struct struct_name* q, type** entry_ptr, uint32_t* index_ptr)                    \
{                                                                                                                                 \
    assert(entry_ptr);                                                                                                            \
    if(function_prefix##_enqueue_ptr(q, entry_ptr, index_ptr))                                                                    \
    {                                                                                                                             \
        *entry_ptr = NULL;                                                                                                        \
        return false;                                                                                                             \
    }                                                                                                                             \
    else                                                                                                                          \
    {                                                                                                                             \
        if(index_ptr) *index_ptr = q->front;                                                                                      \
        *entry_ptr = q->data + q->front;                                                                                          \
        q->front++;                                                                                                               \
        if(q->front > q->space) q->front -= q->space;                                                                             \
        return true;                                                                                                              \
    }                                                                                                                             \
}                                                                                                                                 \
                                                                                                                                  \
/** remove the front of the queue without accessing its contents */                                                               \
/** should only be used after access_front has been used to check the entry that will be pruned */                                \
static inline void function_prefix##_prune_front(struct struct_name* q)                                                           \
{                                                                                                                                 \
    assert(q->count > 0);                                                                                                         \
    q->count--;                                                                                                                   \
    q->front++;                                                                                                                   \
    if(q->front > q->space) q->front -= q->space;                                                                                 \
}                                                                                                                                 \
                                                                                                                                  \
static inline bool function_prefix##_access_front(struct struct_name* q, type** entry_ptr)                                        \
{                                                                                                                                 \
    assert(entry_ptr);                                                                                                            \
    if(q->count == 0)                                                                                                             \
    {                                                                                                                             \
        *entry_ptr = NULL;                                                                                                        \
        return false;                                                                                                             \
    }                                                                                                                             \
    *entry_ptr = q->data + q->front;                                                                                              \
    return true;                                                                                                                  \
}                                                                                                                                 \
                                                                                                                                  \
static inline bool function_prefix##_access_back(struct struct_name* q, type** entry_ptr)                                         \
{                                                                                                                                 \
    uint32_t index;                                                                                                               \
    assert(entry_ptr);                                                                                                            \
    if(q->count == 0)                                                                                                             \
    {                                                                                                                             \
        *entry_ptr = NULL;                                                                                                        \
        return false;                                                                                                             \
    }                                                                                                                             \
    index = q->front + q->count;                                                                                                  \
    if(index > q->space) index -= q->space;                                                                                       \
    *entry_ptr = q->data + index;                                                                                                 \
    return true;                                                                                                                  \
}                                                                                                                                 \




