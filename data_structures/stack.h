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


#ifndef SOL_STACK_TYPE
#error must define SOL_STACK_TYPE
#define SOL_STACK_TYPE int
#endif

#ifndef SOL_STACK_FUNCTION_PREFIX
#error must define SOL_STACK_FUNCTION_PREFIX
#define SOL_STACK_FUNCTION_PREFIX placeholder_stack
#endif

#ifndef SOL_STACK_STRUCT_NAME
#error must define SOL_STACK_STRUCT_NAME
#define SOL_STACK_STRUCT_NAME placeholder_stack
#endif

struct SOL_STACK_STRUCT_NAME
{
    SOL_STACK_TYPE* data;
    uint32_t space;
    uint32_t count;
};

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_initialise)(struct SOL_STACK_STRUCT_NAME* s, uint32_t initial_size)
{
    assert(initial_size>3 && !(initial_size & (initial_size-1)));
    s->data = malloc(sizeof(SOL_STACK_TYPE) * initial_size);
    s->space = initial_size;
    s->count = 0;
}

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_terminate)(struct SOL_STACK_STRUCT_NAME* s)
{
    free(s->data);
}

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_append_many)(struct SOL_STACK_STRUCT_NAME* s, const SOL_STACK_TYPE* values, uint32_t count)
{
    while(s->count + count > s->space)
    {
        s->space *= 2;
        s->data = realloc(s->data, sizeof(SOL_STACK_TYPE) * s->space);
    }
    memcpy(s->data + s->count, values, sizeof(SOL_STACK_TYPE) * count);
    s->count += count;
}


static inline uint32_t SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_remove_many)(struct SOL_STACK_STRUCT_NAME* s, SOL_STACK_TYPE* values, uint32_t count)
{
    if(s->count < count) count = s->count;
    memcpy(values, s->data + s->count - count, sizeof(SOL_STACK_TYPE) * count);
    return count;
}

static inline SOL_STACK_TYPE* SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_append_ptr)(struct SOL_STACK_STRUCT_NAME* s)
{
    if(s->count == s->space)
    {
        s->space *= 2;
        s->data = realloc(s->data, sizeof(SOL_STACK_TYPE) * s->space);
    }
    return s->data + s->count++;
}

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_append)(struct SOL_STACK_STRUCT_NAME* s, SOL_STACK_TYPE value)
{
    *(SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_append_ptr)(s)) = value;
}

static inline bool SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_remove_ptr)(struct SOL_STACK_STRUCT_NAME* s, SOL_STACK_TYPE** entry_ptr)
{
    if(s->count == 0)
    {
        *entry_ptr = NULL;
        return false;
    }
    *entry_ptr = s->data + --s->count;
    return true;
}

static inline bool SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_remove)(struct SOL_STACK_STRUCT_NAME* s, SOL_STACK_TYPE* value)
{
    if(s->count == 0) return false;
    *value = s->data[--s->count];
    return true;
}

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_reset)(struct SOL_STACK_STRUCT_NAME* s)
{
    s->count = 0;
}

static inline size_t SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_size)(struct SOL_STACK_STRUCT_NAME* s)
{
    return sizeof(SOL_STACK_TYPE) * s->count;
}

static inline void SOL_CONCATENATE(SOL_STACK_FUNCTION_PREFIX,_copy)(struct SOL_STACK_STRUCT_NAME* s, void* dst)
{
    memcpy(dst, s->data, sizeof(SOL_STACK_TYPE) * s->count);
}


#undef SOL_STACK_TYPE
#undef SOL_STACK_FUNCTION_PREFIX
#undef SOL_STACK_STRUCT_NAME

