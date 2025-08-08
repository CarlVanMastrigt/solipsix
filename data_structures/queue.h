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


#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "sol_utils.h"


#ifndef SOL_QUEUE_ENTRY_TYPE
#error must define SOL_QUEUE_ENTRY_TYPE
#define SOL_QUEUE_ENTRY_TYPE int
#endif

#ifndef SOL_QUEUE_FUNCTION_PREFIX
#error must define SOL_QUEUE_FUNCTION_PREFIX
#define SOL_QUEUE_FUNCTION_PREFIX placeholder_queue
#endif

#ifndef SOL_QUEUE_STRUCT_NAME
#error must define SOL_QUEUE_STRUCT_NAME
#define SOL_QUEUE_STRUCT_NAME placeholder_queue
#endif

/**
brief on queue resizing:
need to move the correct part of the buffer to maintain (modulo) indices after resizing:
|+++o+++|
|---o+++++++----|
vs
        |+++o+++|
|+++--------o+++|
^ realloced segment array with alignment of relative (intended) indices/offsets


iterating a queue:
for(i=0;i<q->count;i++) SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_access_entry)(q, q->front + i)
*/

struct SOL_QUEUE_STRUCT_NAME
{
    SOL_QUEUE_ENTRY_TYPE* data;
    uint32_t space;
    uint32_t count;
    uint32_t front;
};


static inline void SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_initialise)(struct SOL_QUEUE_STRUCT_NAME* q, uint32_t initial_size)
{
    assert((initial_size & (initial_size - 1)) == 0);
    q->data = malloc(sizeof(SOL_QUEUE_ENTRY_TYPE) * initial_size);
    q->space = initial_size;
    q->count = 0;
    q->front = 0;
}

static inline void SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_terminate)(struct SOL_QUEUE_STRUCT_NAME* q)
{
    free(q->data);
}

static inline SOL_QUEUE_ENTRY_TYPE* SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_access_entry)(struct SOL_QUEUE_STRUCT_NAME* q, uint32_t index)
{
    assert(index < q->front+q->count && index >= q->front);
    return q->data + (index & (q->space - 1));
}

static inline void SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_enqueue_index)(struct SOL_QUEUE_STRUCT_NAME* q, uint32_t* index_ptr)
{
    uint32_t front_offset, move_count;
    SOL_QUEUE_ENTRY_TYPE * src;
    assert(index_ptr);
    if(q->count == q->space)
    {
        q->data = realloc(q->data, sizeof(SOL_QUEUE_ENTRY_TYPE) * q->count * 2);
        front_offset = q->front & (q->space - 1);
        if(q->front & q->space)
        {
            src = q->data + front_offset;
            move_count = q->space - front_offset;
        }
        else
        {
            src = q->data;
            move_count = front_offset;
        }
        memcpy(src + q->space, src, sizeof(SOL_QUEUE_ENTRY_TYPE) * move_count);
        q->space *= 2;
    }
    *index_ptr = q->front + q->count;
    q->count++;
}

static inline void SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_enqueue_ptr)(struct SOL_QUEUE_STRUCT_NAME* q, SOL_QUEUE_ENTRY_TYPE** entry_ptr, uint32_t* index_ptr)
{
    uint32_t index;
    assert(entry_ptr);
    SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_enqueue_index)(q, &index);
    if(index_ptr)
    {
        *index_ptr = index;
    }
    *entry_ptr = q->data + (index & (q->space - 1));
}

static inline void SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_enqueue)(struct SOL_QUEUE_STRUCT_NAME* q, SOL_QUEUE_ENTRY_TYPE value, uint32_t* index_ptr)
{
    uint32_t index;
    SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_enqueue_index)(q, &index);
    if(index_ptr)
    {
        *index_ptr = index;
    }
    q->data[index & (q->space - 1)] = value;
}

static inline bool SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_dequeue_ptr)(struct SOL_QUEUE_STRUCT_NAME* q, SOL_QUEUE_ENTRY_TYPE** entry_ptr)
{
    assert(entry_ptr);
    if(q->count == 0)
    {
        *entry_ptr = NULL;
        return false;
    }
    *entry_ptr = q->data + (q->front & (q->space - 1));
    q->front++;
    q->count--;
    return true;
}

static inline bool SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_dequeue)(struct SOL_QUEUE_STRUCT_NAME* q, SOL_QUEUE_ENTRY_TYPE* value)
{
    assert(value);
    if(q->count == 0)
    {
        return false;
    }
    *value = q->data[q->front & (q->space - 1)];
    q->front++;
    q->count--;
    return true;
}

/** remove the front of the queue without accessing its contents */
/** should only be used after access_front has been used to check the entry that will be pruned */
static inline void SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_prune_front)(struct SOL_QUEUE_STRUCT_NAME* q)
{
    assert(q->count > 0);
    q->front++;
    q->count--;
}

static inline bool SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_access_front)(struct SOL_QUEUE_STRUCT_NAME* q, SOL_QUEUE_ENTRY_TYPE** entry_ptr)
{
    assert(entry_ptr);
    if(q->count == 0)
    {
        *entry_ptr = NULL;
        return false;
    }
    *entry_ptr = q->data + (q->front & (q->space - 1));
    return true;
}

static inline bool SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_access_back)(struct SOL_QUEUE_STRUCT_NAME* q, SOL_QUEUE_ENTRY_TYPE** entry_ptr)
{
    assert(entry_ptr);
    if(q->count == 0)
    {
        *entry_ptr = NULL;
        return false;
    }
    *entry_ptr = q->data + ((q->front + q->count - 1) & (q->space - 1));
    return true;
}

/** note: index may become valid again if queue cycles the full u32 before being assessed */
static inline bool SOL_CONCATENATE(SOL_QUEUE_FUNCTION_PREFIX,_index_valid)(struct SOL_QUEUE_STRUCT_NAME* q, uint32_t index)
{
    /** get index as relative */
    index -= q->front;
    return index < q->count;
}


#undef SOL_QUEUE_ENTRY_TYPE
#undef SOL_QUEUE_FUNCTION_PREFIX
#undef SOL_QUEUE_STRUCT_NAME

