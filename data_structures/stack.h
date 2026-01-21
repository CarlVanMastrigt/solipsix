/**
Copyright 2024,2025,2026 Carl van Mastrigt

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


#ifndef SOL_STACK_ENTRY_TYPE
#error must define SOL_STACK_ENTRY_TYPE
/** adding this inside erroring macro to get clang to recognise it */
#define SOL_STACK_ALLOW_UNORDERED_OPERATIONS
#define SOL_STACK_ENTRY_TYPE int
#endif

#ifndef SOL_STACK_STRUCT_NAME
#error must define SOL_STACK_STRUCT_NAME
#define SOL_STACK_STRUCT_NAME placeholder_stack
#endif

#ifndef SOL_STACK_FUNCTION_PREFIX
#define SOL_STACK_FUNCTION_PREFIX SOL_STACK_STRUCT_NAME
#endif

#ifndef SOL_STACK_DEFAULT_STARTING_SIZE
#define SOL_STACK_DEFAULT_STARTING_SIZE 64
#endif

struct SOL_STACK_STRUCT_NAME
{
    SOL_STACK_ENTRY_TYPE* data;
    uint32_t space;
    uint32_t count;
};

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_initialise)(struct SOL_STACK_STRUCT_NAME* s, uint32_t initial_size)
{
    s->data = initial_size ? malloc(sizeof(SOL_STACK_ENTRY_TYPE) * initial_size) : NULL;
    s->space = initial_size;
    s->count = 0;
}

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_terminate)(struct SOL_STACK_STRUCT_NAME* s)
{
    if(s->space)
    {
        assert(s->data);
        free(s->data);
    }
    else
    {
        assert(s->data == NULL);
    }
}

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_append_many)(struct SOL_STACK_STRUCT_NAME* s, const SOL_STACK_ENTRY_TYPE* values, uint32_t count)
{
    if(s->count + count > s->space)
    {
        do
        {
            if(s->space == 0)
            {
                s->space = SOL_STACK_DEFAULT_STARTING_SIZE;
            }
            else
            {
                s->space *= 2;
            }
        }
        while(s->count + count > s->space);
        s->data = realloc(s->data, sizeof(SOL_STACK_ENTRY_TYPE) * s->space);
    }
    memcpy(s->data + s->count, values, sizeof(SOL_STACK_ENTRY_TYPE) * count);
    s->count += count;
}

/** removes the top `count` of the stack, will copy their contents into `values` is the callers responsibility to ensure values is a valid pointer
    can be provided a count higher than present in the stack; in which case the remaining count will be copied, as such the returned count should be respected */
static inline uint32_t SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_remove_many)(struct SOL_STACK_STRUCT_NAME* s, SOL_STACK_ENTRY_TYPE* values, uint32_t count)
{
    if(s->count < count)
    {
        count = s->count;
    }
    if(values)
    {
        memcpy(values, s->data + s->count - count, sizeof(SOL_STACK_ENTRY_TYPE) * count);
    }
    return count;
}

static inline SOL_STACK_ENTRY_TYPE* SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_append_many_ptr)(struct SOL_STACK_STRUCT_NAME* s, uint32_t count)
{
    SOL_STACK_ENTRY_TYPE* result;
    
    if(s->count + count > s->space)
    {
        do
        {
            if(s->space == 0)
            {
                s->space = SOL_STACK_DEFAULT_STARTING_SIZE;
            }
            else
            {
                s->space *= 2;
            }
        }
        while(s->count + count >= s->space);
        s->data = realloc(s->data, sizeof(SOL_STACK_ENTRY_TYPE) * s->space);
    }
    result = s->data + s->count;
    s->count += count;
    return result;
}

static inline SOL_STACK_ENTRY_TYPE* SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_append_ptr)(struct SOL_STACK_STRUCT_NAME* s)
{
    return SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_append_many_ptr)(s, 1);
}

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_append)(struct SOL_STACK_STRUCT_NAME* s, SOL_STACK_ENTRY_TYPE value)
{
    *(SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_append_ptr)(s)) = value;
}

/** removes the top of the stack, same as regular remove, but avoids a potential copy, the data pointed to remains valid until another operation is done to the stack */
static inline bool SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_remove_ptr)(struct SOL_STACK_STRUCT_NAME* s, SOL_STACK_ENTRY_TYPE** entry_ptr)
{
    if(s->count == 0)
    {
        *entry_ptr = NULL;
        return false;
    }
    *entry_ptr = s->data + --s->count;
    return true;
}

static inline bool SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_remove)(struct SOL_STACK_STRUCT_NAME* s, SOL_STACK_ENTRY_TYPE* value)
{
    if(s->count == 0)
    {
        return false;
    }
    *value = s->data[--s->count];
    return true;
}

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_reset)(struct SOL_STACK_STRUCT_NAME* s)
{
    s->count = 0;
}

static inline size_t SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_size)(struct SOL_STACK_STRUCT_NAME* s)
{
    return sizeof(SOL_STACK_ENTRY_TYPE) * s->count;
}

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_copy)(struct SOL_STACK_STRUCT_NAME* s, void* dst)
{
    memcpy(dst, s->data, sizeof(SOL_STACK_ENTRY_TYPE) * s->count);
}

static inline uint32_t SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_count)(struct SOL_STACK_STRUCT_NAME* s)
{
    return s->count;
}

static inline SOL_STACK_ENTRY_TYPE* SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_data)(struct SOL_STACK_STRUCT_NAME* s)
{
    return s->data;
}

/** basically the functions that turn a stack into a list
 * it is recommended to define the type as a list to differentiate them */
#ifdef SOL_STACK_ALLOW_UNORDERED_OPERATIONS
static inline SOL_STACK_ENTRY_TYPE SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_get_entry)(struct SOL_STACK_STRUCT_NAME* s, uint32_t index)
{
    assert(index < s->count);
    return s->data[index];
}
static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_remove_entry)(struct SOL_STACK_STRUCT_NAME* s, uint32_t index)
{
    assert(index < s->count);
    s->data[index] = s->data[--s->count];
}
#endif

#undef SOL_STACK_ENTRY_TYPE
#undef SOL_STACK_FUNCTION_PREFIX
#undef SOL_STACK_STRUCT_NAME
#undef SOL_STACK_DEFAULT_STARTING_SIZE
