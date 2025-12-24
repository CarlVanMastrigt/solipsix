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
#include <assert.h>

#include "sol_utils.h"

#include "data_structures/available_indices_stack.h"



#ifndef SOL_ARRAY_ENTRY_TYPE
#error must define SOL_ARRAY_ENTRY_TYPE
#define SOL_ARRAY_ENTRY_TYPE int
#endif

#ifndef SOL_ARRAY_STRUCT_NAME
#error must define SOL_ARRAY_STRUCT_NAME
#define SOL_ARRAY_STRUCT_NAME placeholder_array
#endif

#ifndef SOL_ARRAY_FUNCTION_PREFIX
#define SOL_ARRAY_FUNCTION_PREFIX SOL_ARRAY_STRUCT_NAME
#endif



struct SOL_ARRAY_STRUCT_NAME
{
    struct sol_available_indices_stack available_indices;
    SOL_ARRAY_ENTRY_TYPE* array;
    uint32_t space;
    uint32_t count;
};

static inline void SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_initialise)(struct SOL_ARRAY_STRUCT_NAME* a, uint32_t initial_size)
{
    assert((initial_size & (initial_size - 1)) == 0);
    sol_available_indices_stack_initialise(&a->available_indices, initial_size);
    a->array = malloc(sizeof(SOL_ARRAY_ENTRY_TYPE) * initial_size);
    a->space = initial_size;
    a->count = 0;
}

static inline void SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_terminate)(struct SOL_ARRAY_STRUCT_NAME* a)
{
    sol_available_indices_stack_terminate(&a->available_indices);
    free(a->array);
}

static inline uint32_t SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_append_index)(struct SOL_ARRAY_STRUCT_NAME* a)
{
    uint32_t i;
    if(!sol_available_indices_stack_remove(&a->available_indices, &i))
    {
        if(a->count == a->space)
        {
            a->space *= 2;
            a->array = realloc(a->array, sizeof(SOL_ARRAY_ENTRY_TYPE) * a->space);
        }
        i = a->count++;
    }
    return i;
}

static inline SOL_ARRAY_ENTRY_TYPE* SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_append_ptr)(struct SOL_ARRAY_STRUCT_NAME* a, uint32_t* index_ptr)
{
    uint32_t i;
    i = SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_append_index)(a);
    if(index_ptr)
    {
    	*index_ptr = i;
    }
    return a->array + i;
}

static inline uint32_t SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_append)(struct SOL_ARRAY_STRUCT_NAME* a, SOL_ARRAY_ENTRY_TYPE value)
{
	uint32_t i;
    i = SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_append_index)(a);
    a->array[i] = value;
    return i;
}

/** returned pointer cannot be used after any other operation has occurred*/
static inline SOL_ARRAY_ENTRY_TYPE* SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_remove_ptr)(struct SOL_ARRAY_STRUCT_NAME* a, uint32_t index)
{
    sol_available_indices_stack_append(&a->available_indices, index);
    return a->array + index;
}

static inline SOL_ARRAY_ENTRY_TYPE SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_remove)(struct SOL_ARRAY_STRUCT_NAME* a, uint32_t index)
{
    sol_available_indices_stack_append(&a->available_indices, index);
    return a->array[index];
}

static inline void SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_reset)(struct SOL_ARRAY_STRUCT_NAME* a)
{
    sol_available_indices_stack_reset(&a->available_indices);
    a->count = 0;
}

static inline uint32_t SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_is_empty)(struct SOL_ARRAY_STRUCT_NAME* a)
{
    return a->count == sol_available_indices_stack_count(&a->available_indices);
}

static inline SOL_ARRAY_ENTRY_TYPE SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_get_entry)(const struct SOL_ARRAY_STRUCT_NAME* a, uint32_t index)
{
    return a->array[index];
}

static inline SOL_ARRAY_ENTRY_TYPE* SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_access_entry)(struct SOL_ARRAY_STRUCT_NAME* a, uint32_t index)
{
    return a->array + index;
}

static inline uint32_t SOL_CONCATENATE(SOL_ARRAY_FUNCTION_PREFIX,_active_count)(struct SOL_ARRAY_STRUCT_NAME* a)
{
    assert(a->count >= sol_available_indices_stack_count(&a->available_indices));
    return a->count - sol_available_indices_stack_count(&a->available_indices);
}


#undef SOL_ARRAY_ENTRY_TYPE
#undef SOL_ARRAY_FUNCTION_PREFIX
#undef SOL_ARRAY_STRUCT_NAME

