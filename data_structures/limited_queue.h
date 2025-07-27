/**
Copyright 2025 Carl van Mastrigt

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


#ifndef SOL_LIMITED_QUEUE_ENTRY_TYPE
#error must define SOL_LIMITED_QUEUE_ENTRY_TYPE
#define SOL_LIMITED_QUEUE_ENTRY_TYPE int
#endif

#ifndef SOL_LIMITED_QUEUE_FUNCTION_PREFIX
#error must define SOL_LIMITED_QUEUE_FUNCTION_PREFIX
#define SOL_LIMITED_QUEUE_FUNCTION_PREFIX placeholder_stack
#endif

#ifndef SOL_LIMITED_QUEUE_STRUCT_NAME
#error must define SOL_LIMITED_QUEUE_STRUCT_NAME
#define SOL_LIMITED_QUEUE_STRUCT_NAME placeholder_stack
#endif

/** rigid queue has a fixed size set at initialization (it is possible to change this, but doing so would invalidate held indices so is not supported at this time)
    useful for simple queues */

struct SOL_LIMITED_QUEUE_STRUCT_NAME
{
    SOL_LIMITED_QUEUE_ENTRY_TYPE* data;
    uint32_t space;
    uint32_t count;
    uint32_t front;
};


static inline void SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_initialise)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q, uint32_t size)
{
    q->data = malloc(sizeof(SOL_LIMITED_QUEUE_ENTRY_TYPE) * size);
    q->space = size;
    q->count = 0;
    q->front = 0;
}

static inline void SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_terminate)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q)
{
    free(q->data);
}

static inline SOL_LIMITED_QUEUE_ENTRY_TYPE* SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_access_entry)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q, uint32_t index)
{
    assert(index < q->space);
    return q->data + index;
}

static inline bool SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_enqueue_index)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q, uint32_t* index_ptr)
{
    uint32_t index;
    assert(index_ptr);
    if(q->count == q->space)
    {
        return false;
    }
    index = q->front + q->count;
    if(index >= q->space)
    {
        index -= q->space;
    }
    *index_ptr = index;
    q->count++;
    return true;
}

static inline bool SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_enqueue_ptr)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q, SOL_LIMITED_QUEUE_ENTRY_TYPE** entry_ptr, uint32_t* index_ptr)
{
    uint32_t index;
    assert(entry_ptr);
    if(SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_enqueue_index)(q, &index))
    {
        if(index_ptr)
        {
            *index_ptr = index;
        }
        *entry_ptr = q->data + index;
        return true;
    }
    *entry_ptr = NULL;
    return false;
}

static inline bool SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_enqueue)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q, SOL_LIMITED_QUEUE_ENTRY_TYPE value, uint32_t* index_ptr)
{
    uint32_t index;
    if(SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_enqueue_index)(q, &index))
    {
        if(index_ptr)
        {
            *index_ptr = index;
        }
        q->data[index] = value;
        return true;
    }
    return false;
}

static inline bool SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_dequeue_ptr)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q, SOL_LIMITED_QUEUE_ENTRY_TYPE** entry_ptr)
{
    SOL_LIMITED_QUEUE_ENTRY_TYPE* ptr;
    assert(entry_ptr);
    if(q->count == 0)
    {
        *entry_ptr = NULL;
        return false;
    }
    *entry_ptr = q->data + q->front;
    q->count--;
    q->front++;
    if(q->front > q->space)
    {
        q->front -= q->space;
    }
    return true;
}

static inline bool SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_dequeue)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q, SOL_LIMITED_QUEUE_ENTRY_TYPE* value)
{
    assert(value);
    if(q->count == 0)
    {
        return false;
    }
    *value = q->data[q->front];
    q->count--;
    q->front++;
    if(q->front >= q->space)
    {
        q->front -= q->space;
    }
    return true;
}

/** if there is space left just enqueue and return
    otherwise; take the front of the queue and move it to the back
    pointer is both the old front and new back of the queue, the index is the new back
    returns wheter the pointer was requeued, false if just enqueued */
static inline bool SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_requeue_ptr)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q, SOL_LIMITED_QUEUE_ENTRY_TYPE** entry_ptr, uint32_t* index_ptr)
{
    assert(entry_ptr);
    if(SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_enqueue_ptr)(q, entry_ptr, index_ptr))
    {
        *entry_ptr = NULL;
        return false;
    }
    else
    {
        if(index_ptr)
        {
            *index_ptr = q->front;
        }
        *entry_ptr = q->data + q->front;
        q->front++;
        if(q->front >= q->space)
        {
            q->front -= q->space;
        }
        return true;
    }
}

/** remove the front of the queue without accessing its contents
    should only be used after access_front has been used to check the entry that will be pruned */
static inline void SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_prune_front)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q)
{
    assert(q->count > 0);
    q->count--;
    q->front++;
    if(q->front >= q->space)
    {
        q->front -= q->space;
    }
}

static inline bool SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_access_front)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q, SOL_LIMITED_QUEUE_ENTRY_TYPE** entry_ptr)
{
    assert(entry_ptr);
    if(q->count == 0)
    {
        *entry_ptr = NULL;
        return false;
    }
    *entry_ptr = q->data + q->front;
    return true;
}

static inline bool SOL_CONCATENATE(SOL_LIMITED_QUEUE_FUNCTION_PREFIX,_access_back)(struct SOL_LIMITED_QUEUE_STRUCT_NAME* q, SOL_LIMITED_QUEUE_ENTRY_TYPE** entry_ptr)
{
    uint32_t index;
    assert(entry_ptr);
    if(q->count == 0)
    {
        *entry_ptr = NULL;
        return false;
    }
    index = q->front + q->count;
    if(index >= q->space)
    {
        index -= q->space;
    }
    *entry_ptr = q->data + index;
    return true;
}




