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
#include <stdbool.h>
#include <string.h>
#include <assert.h>


#include "data_structures/stack.h"








#define SOL_ARRAY(type, struct_name, function_prefix)                                             \
                                                                                                  \
struct struct_name                                                                                \
{                                                                                                 \
    struct sol_available_indices_stack available_indices;                                         \
    type* array;                                                                                  \
    uint32_t space;                                                                               \
    uint32_t count;                                                                               \
};                                                                                                \
                                                                                                  \
                                                                                                  \
static inline void function_prefix##_initialise(struct struct_name* a, uint32_t initial_size)     \
{                                                                                                 \
    assert((initial_size & (initial_size - 1)) == 0);                                             \
    sol_available_indices_stack_initialise(&a->available_indices, initial_size);                  \
    a->array = malloc(sizeof(type) * initial_size);                                               \
    a->space = initial_size;                                                                      \
    a->count = 0;                                                                                 \
}                                                                                                 \
                                                                                                  \
static inline void function_prefix##_terminate(struct struct_name* a)                             \
{                                                                                                 \
    sol_available_indices_stack_terminate(&a->available_indices);                                 \
    free(a->array);                                                                               \
}                                                                                                 \
                                                                                                  \
static inline type* function_prefix##_append_ptr(struct struct_name* a, uint32_t* index_ptr)      \
{                                                                                                 \
    uint32_t i;                                                                                   \
    if(!sol_available_indices_stack_remove(&a->available_indices, &i))                            \
    {                                                                                             \
        if(a->count == a->space)                                                                  \
        {                                                                                         \
            a->space *= 2;                                                                        \
            a->array = realloc(a->array, sizeof(type) * a->space);                                \
        }                                                                                         \
        i = a->count++;                                                                           \
    }                                                                                             \
    if(index_ptr)                                                                                 \
    {                                                                                             \
    	*index_ptr = i;                                                                           \
    }                                                                                             \
    return a->array + i;                                                                          \
}                                                                                                 \
                                                                                                  \
static inline uint32_t function_prefix##_append(struct struct_name* a, type value)                \
{                                                                                                 \
	uint32_t i;                                                                                   \
    *function_prefix##_append_ptr(a, &i) = value;                                                 \
    return i;                                                                                     \
}                                                                                                 \
                                                                                                  \
/** returned pointer cannot be used after any other operation has occurred*/                      \
static inline const type * function_prefix##_remove_ptr(struct struct_name* a, uint32_t index)    \
{                                                                                                 \
    sol_available_indices_stack_append(&a->available_indices, index);                             \
    return a->array + index;                                                                      \
}                                                                                                 \
                                                                                                  \
static inline type function_prefix##_remove(struct struct_name* a, uint32_t index)                \
{                                                                                                 \
    sol_available_indices_stack_append(&a->available_indices, index);                             \
    return a->array[index];                                                                       \
}                                                                                                 \
                                                                                                  \
static inline void function_prefix##_reset(struct struct_name* a)                                 \
{                                                                                                 \
    sol_available_indices_stack_reset(&a->available_indices);                                     \
    a->count=0;                                                                                   \
}                                                                                                 \
                                                                                                  \
static inline type function_prefix##_get_entry(const struct struct_name* a, uint32_t index)       \
{                                                                                                 \
    return a->array[index];                                                                       \
}                                                                                                 \
                                                                                                  \
static inline type* function_prefix##_access_entry(struct struct_name* a, uint32_t index)         \
{                                                                                                 \
    return a->array + index;                                                                      \
}

