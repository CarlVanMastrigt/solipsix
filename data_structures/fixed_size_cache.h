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
#include <assert.h>

#include "sol_utils.h"



struct sol_cache_link
{
    uint16_t older;
    uint16_t newer;
};

/**
SOL_CACHE_CMP( entry , key ) must be declared, is used for equality checking when finding
entries in the cache; this comparison should take pointers of the type (a) and
the key_type (B)
note: key_type can (and often should) be a pointer
*/

#define SOL_FIXED_SIZE_CACHE(cached_type, key_type, struct_name, function_prefix)                         \
struct struct_name                                                                                        \
{                                                                                                         \
    cached_type* entries;                                                                                 \
    struct sol_cache_link* links;                                                                         \
    uint16_t oldest;                                                                                      \
    uint16_t newest;                                                                                      \
    uint16_t first_free;                                                                                  \
    uint16_t count;                                                                                       \
};                                                                                                        \
                                                                                                          \
static inline void function_prefix##_initialise(struct struct_name* cache, uint32_t size)                 \
{                                                                                                         \
    assert(size >= 2);                                                                                    \
    /* note: links buffer is end of entries buffer (they are a single allocation) */                      \
    cache->entries = malloc((sizeof(cached_type) + sizeof(struct sol_cache_link)) * size);                \
    cache->links = (void*)(cache->entries + size);                                                        \
    cache->oldest = SOL_U16_INVALID;                                                                      \
    cache->newest = SOL_U16_INVALID;                                                                      \
    cache->count  = 0;                                                                                    \
    cache->first_free = size - 1;                                                                         \
    while(size--)                                                                                         \
    {                                                                                                     \
        cache->links[size].newer = size - 1;                                                              \
        cache->links[size].older = SOL_U16_INVALID;/*not needed*/                                         \
    }                                                                                                     \
    assert(cache->links[0].newer == SOL_U16_INVALID);                                                     \
}                                                                                                         \
                                                                                                          \
static inline void function_prefix##_terminate(struct struct_name* cache)                                 \
{                                                                                                         \
    free(cache->entries);                                                                                 \
}                                                                                                         \
                                                                                                          \
static inline cached_type* function_prefix##_find(struct struct_name* cache, const key_type key)          \
{                                                                                                         \
    uint16_t i,p;                                                                                         \
    const cached_type* e;                                                                                 \
    for(i = cache->newest; i != SOL_U16_INVALID; i = cache->links[i].older)                               \
    {                                                                                                     \
        e = cache->entries + i;                                                                           \
        if(SOL_CACHE_CMP( e , key ))                                                                      \
        {/** move to newest slot in cache */                                                              \
            if(cache->newest != i)                                                                        \
            {                                                                                             \
                if(cache->oldest == i)                                                                    \
                {                                                                                         \
                    cache->oldest = cache->links[i].newer;                                                \
                }                                                                                         \
                else                                                                                      \
                {                                                                                         \
                    p = cache->links[i].older;                                                            \
                    cache->links[p].newer = cache->links[i].newer;                                        \
                }                                                                                         \
                cache->links[cache->newest].newer = i;                                                    \
                cache->links[i].older = cache->newest;                                                    \
                cache->links[i].newer = SOL_U16_INVALID;                                                  \
                cache->newest = i;                                                                        \
            }                                                                                             \
            return cache->entries + i;                                                                    \
        }                                                                                                 \
    }                                                                                                     \
                                                                                                          \
    return NULL;                                                                                          \
}                                                                                                         \
                                                                                                          \
static inline cached_type* function_prefix##_new(struct struct_name* cache, bool* evicted)                \
{                                                                                                         \
    uint16_t i;                                                                                           \
    if(cache->first_free != SOL_U16_INVALID)                                                              \
    {                                                                                                     \
        if(evicted)                                                                                       \
        {                                                                                                 \
            *evicted = false;                                                                             \
        }                                                                                                 \
        i = cache->first_free;                                                                            \
        cache->first_free = cache->links[i].newer;                                                        \
        if(cache->newest == SOL_U16_INVALID)                                                              \
        {                                                                                                 \
            cache->oldest = i;                                                                            \
        }                                                                                                 \
        else                                                                                              \
        {                                                                                                 \
            cache->links[cache->newest].newer = i;                                                        \
        }                                                                                                 \
    }                                                                                                     \
    else                                                                                                  \
    {                                                                                                     \
        if(evicted)                                                                                       \
        {                                                                                                 \
            *evicted = true;                                                                              \
        }                                                                                                 \
        i = cache->oldest;                                                                                \
        cache->oldest = cache->links[i].newer;                                                            \
        cache->links[cache->oldest].older = SOL_U16_INVALID;                                              \
        cache->links[cache->newest].newer = i;                                                            \
    }                                                                                                     \
    cache->links[i].newer = SOL_U16_INVALID;                                                              \
    cache->links[i].older = cache->newest;                                                                \
    cache->newest = i;                                                                                    \
    cache->count++;                                                                                       \
    return cache->entries + i;                                                                            \
}                                                                                                         \
                                                                                                          \
static inline cached_type* function_prefix##_evict(struct struct_name* cache)                             \
{                                                                                                         \
    uint16_t i;                                                                                           \
    i = cache->oldest;                                                                                    \
    if(i == SOL_U16_INVALID)                                                                              \
    {                                                                                                     \
        return NULL;                                                                                      \
    }                                                                                                     \
    cache->oldest = cache->links[i].newer;                                                                \
    if(cache->oldest == SOL_U16_INVALID)                                                                  \
    {                                                                                                     \
        cache->newest = SOL_U16_INVALID;                                                                  \
    }                                                                                                     \
    else                                                                                                  \
    {                                                                                                     \
        cache->links[cache->oldest].older = SOL_U16_INVALID;                                              \
    }                                                                                                     \
    cache->links[i].newer = cache->first_free;                                                            \
    cache->links[i].older = SOL_U16_INVALID;/*not needed*/                                                \
    cache->first_free = i;                                                                                \
    cache->count--;                                                                                       \
    return cache->entries + i;                                                                            \
}                                                                                                         \
                                                                                                          \
static inline void function_prefix##_remove(struct struct_name* cache, cached_type* entry)                \
{                                                                                                         \
	assert(entry > cache->entries);                                                                       \
	assert(entry < (cached_type*)cache->links);                                                           \
	/* ^ note: links is end of entries buffer*/                                                           \
    uint16_t i = entry - cache->entries;                                                                  \
    if(cache->oldest == i)                                                                                \
    {                                                                                                     \
        cache->oldest = cache->links[i].newer;                                                            \
        cache->links[cache->oldest].older = SOL_U16_INVALID;                                              \
    }                                                                                                     \
    else                                                                                                  \
    {                                                                                                     \
        cache->links[cache->links[i].older].newer = cache->links[i].newer;                                \
    }                                                                                                     \
    if(cache->newest == i)                                                                                \
    {                                                                                                     \
        cache->newest = cache->links[i].older;                                                            \
        cache->links[cache->newest].newer = SOL_U16_INVALID;                                              \
    }                                                                                                     \
    else                                                                                                  \
    {                                                                                                     \
        cache->links[cache->links[i].newer].older = cache->links[i].older;                                \
    }                                                                                                     \
    cache->links[i].newer = cache->first_free;                                                            \
    cache->links[i].older = SOL_U16_INVALID;/*not needed*/                                                \
    cache->first_free = i;                                                                                \
    cache->count--;                                                                                       \
}                                                                                                         \
                                                                                                          \
static inline cached_type* function_prefix##_get_oldest_ptr(struct struct_name* cache)                    \
{                                                                                                         \
    if(cache->oldest == SOL_U16_INVALID)                                                                  \
    {                                                                                                     \
        return NULL;                                                                                      \
    }                                                                                                     \
    return cache->entries + cache->oldest;                                                                \
}


