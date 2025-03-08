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

#define SOL_QUEUE(type,name,start_size)                                         \
                                                                                \
typedef struct name##_queue                                                     \
{                                                                               \
    type* data;                                                                 \
    uint_fast32_t space;                                                        \
    uint_fast32_t count;                                                        \
    uint_fast32_t front;                                                        \
}                                                                               \
name##_queue;                                                                   \
                                                                                \
                                                                                \
static inline void name##_queue_initialise( name##_queue* q )                   \
{                                                                               \
    assert(( (start_size) & ( (start_size) - 1 )) == 0);                        \
    q->data=malloc( sizeof( type ) * start_size );                              \
    q->space=start_size;                                                        \
    q->count=0;                                                                 \
    q->front=0;                                                                 \
}                                                                               \
                                                                                \
static inline void name##_queue_terminate( name##_queue* q )                    \
{                                                                               \
    free(q->data);                                                              \
}                                                                               \
                                                                                \
static inline uint32_t name##_queue_new_index( name##_queue* q )                \
{                                                                               \
    uint_fast32_t front_offset, move_count;                                     \
    type * src;                                                                 \
    if(q->count == q->space)                                                    \
    {                                                                           \
        q->data = realloc(q->data, sizeof(type) * q->count * 2);                \
        front_offset = q->front & (q->space - 1);                               \
        if(q->front & q->space)                                                 \
        {                                                                       \
            src = q->data + front_offset;                                       \
            move_count = q->space - front_offset;                               \
        }                                                                       \
        else                                                                    \
        {                                                                       \
            src = q->data;                                                      \
            move_count = front_offset;                                          \
        }                                                                       \
        memcpy(src + q->space, src, sizeof(type) * move_count);                 \
        q->space *= 2;                                                          \
    }                                                                           \
    return q->front + q->count++;                                               \
}                                                                               \
                                                                                \
static inline type * name##_queue_access( name##_queue* q , uint32_t index )    \
{                                                                               \
    return q->data + (index & (q->space - 1));                                  \
}                                                                               \
                                                                                \
static inline type * name##_queue_new( name##_queue* q )                        \
{                                                                               \
    return q->data + ( name##_queue_new_index(q) & (q->space - 1));             \
}                                                                               \
                                                                                \
static inline uint32_t name##_queue_enqueue( name##_queue* q , type value )     \
{                                                                               \
    uint_fast32_t i;                                                            \
    i = name##_queue_new_index(q);                                              \
    q->data[i & (q->space - 1)] = value;                                        \
    return i;                                                                   \
}                                                                               \
                                                                                \
static inline bool name##_queue_dequeue( name##_queue* q , type* value )        \
{                                                                               \
    if(q->count == 0)                                                           \
    {                                                                           \
        return false;                                                           \
    }                                                                           \
    if(value)                                                                   \
    {                                                                           \
        *value = q->data[ q->front & (q->space - 1) ];                          \
    }                                                                           \
    q->front++;                                                                 \
    q->count--;                                                                 \
    return true;                                                                \
}                                                                               \
                                                                                \
static inline type * name##_queue_dequeue_ptr( name##_queue* q )                \
{                                                                               \
    type * ptr;                                                                 \
    if(q->count == 0)                                                           \
    {                                                                           \
        return NULL;                                                            \
    }                                                                           \
    ptr = q->data + (q->front & (q->space - 1));                                \
    q->front++;                                                                 \
    q->count--;                                                                 \
    return ptr;                                                                 \
}                                                                               \
                                                                                \
static inline type * name##_queue_get_front_ptr( name##_queue* q )              \
{                                                                               \
    if(q->count == 0)                                                           \
    {                                                                           \
        return NULL;                                                            \
    }                                                                           \
    return q->data + (q->front & (q->space - 1));                               \
}                                                                               \
                                                                                \
static inline type * name##_queue_get_back_ptr( name##_queue* q )               \
{                                                                               \
    if(q->count == 0)                                                           \
    {                                                                           \
        return NULL;                                                            \
    }                                                                           \
    return q->data + ((q->front + q->count - 1) & (q->space - 1));              \
}                                                                               \
                                                                                \
static inline uint32_t name##_queue_front_index( name##_queue* q )              \
{                                                                               \
    return q->front;                                                            \
}                                                                               \
                                                                                \
static inline uint32_t name##_queue_back_index( name##_queue* q )               \
{                                                                               \
    return q->front + q->count - 1;                                             \
}                                                                               \




