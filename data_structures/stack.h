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

#define SOL_STACK(type, struct_name, function_prefix)                                                         \
                                                                                                              \
struct struct_name                                                                                            \
{                                                                                                             \
    type* data;                                                                                               \
    uint32_t space;                                                                                           \
    uint32_t count;                                                                                           \
};                                                                                                            \
                                                                                                              \
static inline void function_prefix##_initialise(struct struct_name* s, uint32_t initial_size)                 \
{                                                                                                             \
    assert(initial_size>3 && !(initial_size & (initial_size-1)));                                             \
    s->data = malloc(sizeof(type) * initial_size);                                                            \
    s->space = initial_size;                                                                                  \
    s->count = 0;                                                                                             \
}                                                                                                             \
                                                                                                              \
static inline void function_prefix##_terminate(struct struct_name* s)                                         \
{                                                                                                             \
    free(s->data);                                                                                            \
}                                                                                                             \
                                                                                                              \
static inline void function_prefix##_append_many(struct struct_name* s, const type* values, uint32_t count)   \
{                                                                                                             \
    while(s->count + count > s->space)                                                                        \
    {                                                                                                         \
        s->space *= 2;                                                                                        \
        s->data = realloc(s->data, sizeof(type) * s->space);                                                  \
    }                                                                                                         \
    memcpy(s->data + s->count, values, sizeof(type) * count);                                                 \
    s->count += count;                                                                                        \
}                                                                                                             \
                                                                                                              \
                                                                                                              \
static inline uint32_t function_prefix##_remove_many(struct struct_name* s, type* values, uint32_t count)     \
{                                                                                                             \
    if(s->count < count) count = s->count;                                                                    \
    memcpy(values, s->data + s->count - count, sizeof(type) * count);                                         \
    return count;                                                                                             \
}                                                                                                             \
                                                                                                              \
static inline type* function_prefix##_append_ptr(struct struct_name* s)                                       \
{                                                                                                             \
    if(s->count == s->space)                                                                                  \
    {                                                                                                         \
        s->space *= 2;                                                                                        \
        s->data = realloc(s->data, sizeof(type) * s->space);                                                  \
    }                                                                                                         \
    return s->data + s->count++;                                                                              \
}                                                                                                             \
                                                                                                              \
static inline void function_prefix##_append(struct struct_name* s, type value)                                \
{                                                                                                             \
    *(function_prefix##_append_ptr(s)) = value;                                                               \
}                                                                                                             \
                                                                                                              \
static inline bool function_prefix##_remove_ptr(struct struct_name* s, type** entry_ptr)                      \
{                                                                                                             \
    if(s->count == 0)                                                                                         \
    {                                                                                                         \
        *entry_ptr = NULL;                                                                                    \
        return false;                                                                                         \
    }                                                                                                         \
    *entry_ptr = s->data + --s->count;                                                                        \
    return true;                                                                                              \
}                                                                                                             \
                                                                                                              \
static inline bool function_prefix##_remove(struct struct_name* s, type* value)                               \
{                                                                                                             \
    if(s->count == 0) return false;                                                                           \
    *value = s->data[--s->count];                                                                             \
    return true;                                                                                              \
}                                                                                                             \
                                                                                                              \
static inline void function_prefix##_reset(struct struct_name* s)                                             \
{                                                                                                             \
    s->count = 0;                                                                                             \
}                                                                                                             \
                                                                                                              \
static inline size_t function_prefix##_size(struct struct_name* s)                                            \
{                                                                                                             \
    return sizeof(type) * s->count;                                                                           \
}                                                                                                             \
                                                                                                              \
static inline void function_prefix##_copy(struct struct_name* s, void* dst)                                   \
{                                                                                                             \
    memcpy(dst, s->data, sizeof(type) * s->count);                                                            \
}

SOL_STACK(uint32_t, sol_available_indices_stack, sol_available_indices_stack)
/// used pretty frequently, including in SOL_ARRAY
