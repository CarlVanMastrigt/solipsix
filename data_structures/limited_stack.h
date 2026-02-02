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


#ifndef SOL_LIMITED_STACK_ENTRY_TYPE
#error must define SOL_LIMITED_STACK_ENTRY_TYPE
#define SOL_LIMITED_STACK_ENTRY_TYPE int
#endif

#ifndef SOL_LIMITED_STACK_STRUCT_NAME
#error must define SOL_LIMITED_STACK_STRUCT_NAME
#define SOL_LIMITED_STACK_STRUCT_NAME placeholder_stack
#endif

#ifndef SOL_LIMITED_STACK_FUNCTION_PREFIX
#define SOL_LIMITED_STACK_FUNCTION_PREFIX SOL_LIMITED_STACK_STRUCT_NAME
#endif


struct SOL_LIMITED_STACK_STRUCT_NAME
{
    SOL_LIMITED_STACK_ENTRY_TYPE* data;
    uint32_t space;
    uint32_t count;
};

static inline void SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_initialise)(struct SOL_LIMITED_STACK_STRUCT_NAME* s, uint32_t size)
{
    assert(size);
    s->data = malloc(sizeof(SOL_LIMITED_STACK_ENTRY_TYPE) * size);
    s->space = size;
    s->count = 0;
}

static inline void SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_terminate)(struct SOL_LIMITED_STACK_STRUCT_NAME* s)
{
    free(s->data);
}

#warning implement failure instead ?? (return bool)
static inline void SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_append_many)(struct SOL_LIMITED_STACK_STRUCT_NAME* s, const SOL_LIMITED_STACK_ENTRY_TYPE* values, uint32_t count)
{
    assert(s->count + count <= s->space);
    memcpy(s->data + s->count, values, sizeof(SOL_LIMITED_STACK_ENTRY_TYPE) * count);
    s->count += count;
}

/** withdraws the top `count` of the stack, will copy their contents into `values` is the callers responsibility to ensure values is a valid pointer
    can be provided a count higher than present in the stack; in which case the remaining count will be copied, as such the returned count should be respected */
static inline uint32_t SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_withdraw_many)(struct SOL_LIMITED_STACK_STRUCT_NAME* s, SOL_LIMITED_STACK_ENTRY_TYPE* values, uint32_t count)
{
    if(s->count < count)
    {
        count = s->count;
    }
    if(values)
    {
        memcpy(values, s->data + s->count - count, sizeof(SOL_LIMITED_STACK_ENTRY_TYPE) * count);
    }
    return count;
}

static inline SOL_LIMITED_STACK_ENTRY_TYPE* SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_append_all)(struct SOL_LIMITED_STACK_STRUCT_NAME* s)
{
    assert(s->count == 0);
    s->count = s->space;
    return s->data;
}

static inline SOL_LIMITED_STACK_ENTRY_TYPE* SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_append_many_ptr)(struct SOL_LIMITED_STACK_STRUCT_NAME* s, uint32_t count)
{
    SOL_LIMITED_STACK_ENTRY_TYPE* result;

    if(s->count + count > s->space)
    {
        return NULL;
    }
    else
    {
        result = s->data + s->count;
        s->count += count;
        return result;
    }
}

static inline SOL_LIMITED_STACK_ENTRY_TYPE* SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_append_ptr)(struct SOL_LIMITED_STACK_STRUCT_NAME* s)
{
    return SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_append_many_ptr)(s, 1);
}

static inline bool SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_append)(struct SOL_LIMITED_STACK_STRUCT_NAME* s, SOL_LIMITED_STACK_ENTRY_TYPE value)
{
    if(s->count == s->space)
    {
        return false;
    }
    else
    {
        s->data[s->count++] = value;
        return true;
    }
}

/** withdraws the top of the stack, same as regular withdraw, but avoids a potential copy, the data pointed to remains valid until another operation is done to the stack */
static inline bool SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_withdraw_ptr)(struct SOL_LIMITED_STACK_STRUCT_NAME* s, SOL_LIMITED_STACK_ENTRY_TYPE** entry_ptr)
{
    if(s->count == 0)
    {
        *entry_ptr = NULL;
        return false;
    }
    *entry_ptr = s->data + --s->count;
    return true;
}

static inline bool SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_withdraw)(struct SOL_LIMITED_STACK_STRUCT_NAME* s, SOL_LIMITED_STACK_ENTRY_TYPE* value)
{
    if(s->count == 0)
    {
        return false;
    }
    *value = s->data[--s->count];
    return true;
}

static inline void SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_reset)(struct SOL_LIMITED_STACK_STRUCT_NAME* s)
{
    s->count = 0;
}

static inline size_t SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_size)(struct SOL_LIMITED_STACK_STRUCT_NAME* s)
{
    return sizeof(SOL_LIMITED_STACK_ENTRY_TYPE) * s->count;
}

static inline void SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_copy)(struct SOL_LIMITED_STACK_STRUCT_NAME* s, void* dst)
{
    memcpy(dst, s->data, sizeof(SOL_LIMITED_STACK_ENTRY_TYPE) * s->count);
}

static inline uint32_t SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_count)(struct SOL_LIMITED_STACK_STRUCT_NAME* s)
{
    return s->count;
}

static inline SOL_LIMITED_STACK_ENTRY_TYPE* SOL_CONCATENATE(SOL_LIMITED_STACK_FUNCTION_PREFIX,_data)(struct SOL_LIMITED_STACK_STRUCT_NAME* s)
{
    return s->data;
}


#undef SOL_LIMITED_STACK_ENTRY_TYPE
#undef SOL_LIMITED_STACK_FUNCTION_PREFIX
#undef SOL_LIMITED_STACK_STRUCT_NAME
#undef SOL_STACK_DEFAULT_STARTING_SIZE
