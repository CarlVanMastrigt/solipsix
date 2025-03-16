/**
Copyright 2020,2021,2022,2023,2024 Carl van Mastrigt

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

#ifndef solipsix_H
#include "solipsix.h"
#endif

#ifndef CVM_DATA_STRUCTURES_H
#define CVM_DATA_STRUCTURES_H


#include "data_structures/stack.h"

#include "data_structures/queue.h"

//#include "data_structures/array.h"




/**
may be worth looking into adding support for random location deletion and heap
location tracking to support that, though probably not as adaptable as might be
desirable
*/


/**
CVM_BIN_HEAP_CMP(A,B) must be declared, with true resulting in the value A
moving UP the heap

TODO: make any inputs to heap_cmp pointers and const

TODO: use test sort heap performance improvements
*/

#ifndef CVM_BIN_HEAP
#define CVM_BIN_HEAP(type,name)                                                 \
                                                                                \
typedef struct name##_bin_heap                                                  \
{                                                                               \
    type * heap;                                                                \
    uint_fast32_t space;                                                        \
    uint_fast32_t count;                                                        \
}                                                                               \
name##_bin_heap;                                                                \
                                                                                \
static inline void name##_bin_heap_ini( name##_bin_heap * h )                   \
{                                                                               \
    h->heap=malloc( sizeof( type ) );                                           \
    h->space=1;                                                                 \
    h->count=0;                                                                 \
}                                                                               \
                                                                                \
static inline void name##_bin_heap_add( name##_bin_heap * h , const type data ) \
{                                                                               \
    if(h->count==h->space)h->heap=realloc(h->heap,sizeof(type)*(h->space*=2));  \
                                                                                \
    uint32_t u,d;                                                               \
    d=h->count++;                                                               \
                                                                                \
    while(d && (CVM_BIN_HEAP_CMP(data,h->heap[(u=(d-1)>>1)])))                  \
    {                                                                           \
        h->heap[d]=h->heap[u];                                                  \
        d=u;                                                                    \
    }                                                                           \
                                                                                \
    h->heap[d]=data;                                                            \
}                                                                               \
                                                                                \
static inline void name##_bin_heap_clr( name##_bin_heap * h )                   \
{                                                                               \
    h->count=0;                                                                 \
}                                                                               \
                                                                                \
static inline bool name##_bin_heap_get( name##_bin_heap * h , type * data )     \
{                                                                               \
    if(h->count==0)                                                             \
    {                                                                           \
        return false;                                                           \
    }                                                                           \
                                                                                \
    *data=h->heap[0];                                                           \
                                                                                \
    uint32_t u,d;                                                               \
    type r=h->heap[--(h->count)];                                               \
    u=0;                                                                        \
                                                                                \
    while((d=(u<<1)+1) < (h->count))                                            \
    {                                                                           \
        d+=((d+1 < h->count)&&(CVM_BIN_HEAP_CMP(h->heap[d+1],h->heap[d])));     \
        if(CVM_BIN_HEAP_CMP(r,h->heap[d]))                                      \
        {                                                                       \
            break;                                                              \
        }                                                                       \
        h->heap[u]=h->heap[d];                                                  \
        u=d;                                                                    \
    }                                                                           \
                                                                                \
    h->heap[u]=r;                                                               \
    return 1;                                                                   \
}                                                                               \
                                                                                \
static inline void name##_bin_heap_del( name##_bin_heap * h )                   \
{                                                                               \
    free(h->heap);                                                              \
}                                                                               \

#endif






struct cvm_cache_link
{
    uint16_t older;
    uint16_t newer;
};

/**
CVM_CACHE_CMP( entry , key ) must be declared, is used for equality checking when finding
entries in the cache; this comparison should take pointers of the type (a) and
the key_type (B)
*/

#ifndef CVM_CACHE
#define CVM_CACHE(type,key_type,name)                                           \
typedef struct name##_cache                                                     \
{                                                                               \
    type* entries;                                                              \
    struct cvm_cache_link* links;                                               \
    uint16_t oldest;                                                            \
    uint16_t newest;                                                            \
    uint16_t first_free;                                                        \
    uint16_t count;                                                             \
}                                                                               \
name##_cache;                                                                   \
                                                                                \
static inline void name##_cache_initialise                                      \
( name##_cache * cache , uint32_t size)                                         \
{                                                                               \
    assert(size >= 2);                                                          \
    cache->links = malloc(sizeof(struct cvm_cache_link) * size);                \
    cache->entries = malloc(sizeof( type ) * size);                             \
    cache->oldest = CVM_INVALID_U16;                                            \
    cache->newest = CVM_INVALID_U16;                                            \
    cache->count  = 0;                                                          \
    cache->first_free = size - 1;                                               \
    while(size--)                                                               \
    {                                                                           \
        cache->links[size].newer = size - 1;                                    \
        cache->links[size].older = CVM_INVALID_U16;/*not needed*/               \
    }                                                                           \
    assert(cache->links[0].newer == CVM_INVALID_U16);                           \
}                                                                               \
                                                                                \
static inline void name##_cache_terminate( name##_cache * cache )               \
{                                                                               \
    free(cache->links);                                                         \
    free(cache->entries);                                                       \
}                                                                               \
                                                                                \
static inline type * name##_cache_find                                          \
( name##_cache * cache , const key_type key )                                   \
{                                                                               \
    uint16_t i,p;                                                               \
    const type * e;                                                             \
    for(i = cache->newest; i != CVM_INVALID_U16; i = cache->links[i].older)     \
    {                                                                           \
        e = cache->entries + i;                                                 \
        if(CVM_CACHE_CMP( e , key ))                                            \
        {/** move to newest slot in cache */                                    \
            if(cache->newest != i)                                              \
            {                                                                   \
                if(cache->oldest == i)                                          \
                {                                                               \
                    cache->oldest = cache->links[i].newer;                      \
                }                                                               \
                else                                                            \
                {                                                               \
                    p = cache->links[i].older;                                  \
                    cache->links[p].newer = cache->links[i].newer;              \
                }                                                               \
                cache->links[cache->newest].newer = i;                          \
                cache->links[i].older = cache->newest;                          \
                cache->links[i].newer = CVM_INVALID_U16;                        \
                cache->newest = i;                                              \
            }                                                                   \
            return cache->entries + i;                                          \
        }                                                                       \
    }                                                                           \
                                                                                \
    return NULL;                                                                \
}                                                                               \
                                                                                \
static inline type * name##_cache_new( name##_cache * cache , bool * evicted )  \
{                                                                               \
    uint16_t i;                                                                 \
    if(cache->first_free != CVM_INVALID_U16)                                    \
    {                                                                           \
        if(evicted)                                                             \
        {                                                                       \
            *evicted = false;                                                   \
        }                                                                       \
        i = cache->first_free;                                                  \
        cache->first_free = cache->links[i].newer;                              \
        if(cache->newest == CVM_INVALID_U16)                                    \
        {                                                                       \
            cache->oldest = i;                                                  \
        }                                                                       \
        else                                                                    \
        {                                                                       \
            cache->links[cache->newest].newer = i;                              \
        }                                                                       \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        if(evicted)                                                             \
        {                                                                       \
            *evicted = true;                                                    \
        }                                                                       \
        i = cache->oldest;                                                      \
        cache->oldest = cache->links[i].newer;                                  \
        cache->links[cache->oldest].older = CVM_INVALID_U16;                    \
        cache->links[cache->newest].newer = i;                                  \
    }                                                                           \
    cache->links[i].newer = CVM_INVALID_U16;                                    \
    cache->links[i].older = cache->newest;                                      \
    cache->newest = i;                                                          \
    cache->count++;                                                             \
    return cache->entries + i;                                                  \
}                                                                               \
                                                                                \
static inline type * name##_cache_evict( name##_cache * cache )                 \
{                                                                               \
    uint16_t i;                                                                 \
    i = cache->oldest;                                                          \
    if(i == CVM_INVALID_U16)                                                    \
    {                                                                           \
        return NULL;                                                            \
    }                                                                           \
    cache->oldest = cache->links[i].newer;                                      \
    if(cache->oldest == CVM_INVALID_U16)                                        \
    {                                                                           \
        cache->newest = CVM_INVALID_U16;                                        \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        cache->links[cache->oldest].older = CVM_INVALID_U16;                    \
    }                                                                           \
    cache->links[i].newer = cache->first_free;                                  \
    cache->links[i].older = CVM_INVALID_U16;/*not needed*/                      \
    cache->first_free = i;                                                      \
    cache->count--;                                                             \
    return cache->entries + i;                                                  \
}                                                                               \
                                                                                \
static inline void name##_cache_remove( name##_cache * cache , type * entry)    \
{                                                                               \
    uint16_t i = entry - cache->entries;                                        \
    if(cache->oldest == i)                                                      \
    {                                                                           \
        cache->oldest = cache->links[i].newer;                                  \
        cache->links[cache->oldest].older = CVM_INVALID_U16;                    \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        cache->links[cache->links[i].older].newer = cache->links[i].newer;      \
    }                                                                           \
    if(cache->newest == i)                                                      \
    {                                                                           \
        cache->newest = cache->links[i].older;                                  \
        cache->links[cache->newest].newer = CVM_INVALID_U16;                    \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        cache->links[cache->links[i].newer].older = cache->links[i].older;      \
    }                                                                           \
    cache->links[i].newer = cache->first_free;                                  \
    cache->links[i].older = CVM_INVALID_U16;/*not needed*/                      \
    cache->first_free = i;                                                      \
    cache->count--;                                                             \
}                                                                               \
                                                                                \
static inline type * name##_cache_get_oldest_ptr( name##_cache * cache )        \
{                                                                               \
    if(cache->oldest == CVM_INVALID_U16)                                        \
    {                                                                           \
        return NULL;                                                            \
    }                                                                           \
    return cache->entries + cache->oldest;                                      \
}                                                                               \

#endif


















#endif
