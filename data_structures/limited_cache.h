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


#include "data_structures/cache_defines.h"


#ifndef SOL_LIMITED_CACHE_ENTRY_TYPE
#error must define SOL_LIMITED_CACHE_ENTRY_TYPE
#define SOL_LIMITED_CACHE_ENTRY_TYPE int
#endif

#ifndef SOL_LIMITED_CACHE_KEY_TYPE
#error must define SOL_LIMITED_CACHE_KEY_TYPE
#define SOL_LIMITED_CACHE_KEY_TYPE int
#endif

#ifndef SOL_LIMITED_CACHE_FUNCTION_PREFIX
#error must define SOL_LIMITED_CACHE_FUNCTION_PREFIX
#define SOL_LIMITED_CACHE_FUNCTION_PREFIX placeholder_limited_cacahe
#endif

#ifndef SOL_LIMITED_CACHE_STRUCT_NAME
#error must define SOL_LIMITED_CACHE_STRUCT_NAME
#define SOL_LIMITED_CACHE_STRUCT_NAME placeholder_limited_cacahe
#endif

#ifndef SOL_LIMITED_CACHE_CMP_EQ
#warning must define SOL_LIMITED_CACHE_KEY_TYPE(SOL_LIMITED_CACHE_ENTRY_TYPE* E, SOL_LIMITED_CACHE_KEY_TYPE K)
/** NOTE: SOL_LIMITED_CACHE_KEY_TYPE can (and often should) be a pointer */
#define SOL_LIMITED_CACHE_CMP_EQ(E, K) (*E == K)
#endif


#ifdef SOL_LIMITED_CACHE_CONTEXT_TYPE
#define SOL_LIMITED_CACHE_CMP_EQ_CONTEXT(E, K) SOL_LIMITED_CACHE_CMP_EQ(E, K, context)
#else
#define SOL_LIMITED_CACHE_CMP_EQ_CONTEXT(E, K) SOL_LIMITED_CACHE_CMP_EQ(E, K)
#endif


#warning interface should probably change to use obtain mechanics similar to hash map! : obtain -> evict, new, found, this warrants the header for putting link def in

struct SOL_LIMITED_CACHE_STRUCT_NAME
{
    SOL_LIMITED_CACHE_ENTRY_TYPE* entries;
    struct sol_cache_link_u16* links;
    uint16_t header_link_index;
    uint16_t first_free;
    uint16_t count;
};

static inline void SOL_CONCATENATE(SOL_LIMITED_CACHE_FUNCTION_PREFIX,_initialise)(struct SOL_LIMITED_CACHE_STRUCT_NAME* cache, uint16_t size)
{
    assert(size >= 2);
    assert(size <= 1024);/** it is strongly suggested that another approach is used instead for the cache size requested */
    /** NOTE: links buffer is end of entries buffer (they are a single allocation) */
    cache->entries = malloc((sizeof(SOL_LIMITED_CACHE_ENTRY_TYPE) + sizeof(struct sol_cache_link_u16)) * (size+1));
    cache->links = (struct sol_cache_link_u16*)(cache->entries + size);
    cache->count = 0;
    cache->first_free = size - 1;

    /** cache is a ring buffer */
    cache->header_link_index = size;
    cache->links[size].newer = size;
    cache->links[size].older = size;

    while(size--)
    {
        cache->links[size].newer = size - 1;
        cache->links[size].older = SOL_U16_INVALID;/*not needed*/
    }
    assert(cache->links[0].newer == SOL_U16_INVALID);
}

static inline void SOL_CONCATENATE(SOL_LIMITED_CACHE_FUNCTION_PREFIX,_terminate)(struct SOL_LIMITED_CACHE_STRUCT_NAME* cache)
{
    /** cache should be empty when terminated */
    assert(cache->links[cache->header_link_index].older == cache->header_link_index);
    assert(cache->links[cache->header_link_index].newer == cache->header_link_index);
    assert(cache->count == 0);

    /** NOTE: links buffer is end of entries buffer (they are a single allocation) */
    free(cache->entries);
}

static inline SOL_LIMITED_CACHE_ENTRY_TYPE* SOL_CONCATENATE(SOL_LIMITED_CACHE_FUNCTION_PREFIX,_evict_oldest)(struct SOL_LIMITED_CACHE_STRUCT_NAME* cache)
{
    uint16_t oldest_index;
    struct sol_cache_link_u16* oldest_link;
    struct sol_cache_link_u16* const header_link = cache->links + cache->header_link_index;

    oldest_index = header_link->newer;
    oldest_link = cache->links + oldest_index;
    if(oldest_link == header_link)
    {
        /** cache is empty */
        assert(cache->count == 0);
        return NULL;
    }
    assert(cache->count > 0);

    /** remove oldest from the cache */
    cache->links[oldest_link->newer].older = cache->header_link_index;
    header_link->newer = oldest_link->newer;

    oldest_link->newer = cache->first_free;
    oldest_link->older = SOL_U16_INVALID;/*not needed*/
    cache->first_free = oldest_index;
    cache->count--;
    return cache->entries + oldest_index;
}

static inline void SOL_CONCATENATE(SOL_LIMITED_CACHE_FUNCTION_PREFIX,_remove)(struct SOL_LIMITED_CACHE_STRUCT_NAME* cache, SOL_LIMITED_CACHE_ENTRY_TYPE* entry)
{
    assert(cache->count > 0);
	assert(entry > cache->entries);
	assert(entry < (SOL_LIMITED_CACHE_ENTRY_TYPE*)cache->links);
	/** ^ note: links is end of entries buffer **/
    uint16_t index;
    struct sol_cache_link_u16* link;

    index = entry - cache->entries;
    link = cache->links + index;

    cache->links[link->newer].older = link->older;
    cache->links[link->older].newer = link->newer;

    link->newer = cache->first_free;
    link->older = SOL_U16_INVALID;/*not needed*/
    cache->first_free = index;
    cache->count--;
}

static inline SOL_LIMITED_CACHE_ENTRY_TYPE* SOL_CONCATENATE(SOL_LIMITED_CACHE_FUNCTION_PREFIX,_access_oldest)(struct SOL_LIMITED_CACHE_STRUCT_NAME* cache)
{
    uint16_t oldest_index = cache->links[cache->header_link_index].newer;
    if(oldest_index == cache->header_link_index)
    {
        assert(cache->count == 0);
        return NULL;
    }
    assert(cache->count > 0);
    return cache->entries + oldest_index;
}

#ifdef SOL_LIMITED_CACHE_CONTEXT_TYPE
static inline enum sol_cache_result SOL_CONCATENATE(SOL_LIMITED_CACHE_FUNCTION_PREFIX,_obtain)(struct SOL_LIMITED_CACHE_STRUCT_NAME* cache, const SOL_LIMITED_CACHE_KEY_TYPE key, SOL_LIMITED_CACHE_ENTRY_TYPE** entry_ptr, SOL_LIMITED_CACHE_CONTEXT_TYPE* context)
#else
static inline enum sol_cache_result SOL_CONCATENATE(SOL_LIMITED_CACHE_FUNCTION_PREFIX,_obtain)(struct SOL_LIMITED_CACHE_STRUCT_NAME* cache, const SOL_LIMITED_CACHE_KEY_TYPE key, SOL_LIMITED_CACHE_ENTRY_TYPE** entry_ptr)
#endif
{
    uint16_t index;
    struct sol_cache_link_u16* link;
    struct sol_cache_link_u16* const header_link = cache->links + cache->header_link_index;
    enum sol_cache_result result;

    /** search from the newest cache entry */
    index = header_link->older;
    link = cache->links + index;

    while(link != header_link)
    {
        if(SOL_LIMITED_CACHE_CMP_EQ_CONTEXT((cache->entries + index), key))
        {
            /** remove entry from cache */
            cache->links[link->newer].older = link->older;
            cache->links[link->older].newer = link->newer;

            result = SOL_CACHE_SUCCESS_FOUND;
            goto found;
        }

        index = link->older;
        link = cache->links + index;
    }

    /** entry not found in cache */
    if(cache->first_free != SOL_U16_INVALID)
    {
        /** there is an unused cache entry; use it */
        index = cache->first_free;
        link = cache->links + index;
        cache->first_free = link->newer;

        cache->count++;

        result = SOL_CACHE_SUCCESS_INSERTED;
    }
    else
    {
        /** must recycle the oldest entry, remove it from the cache*/
        index = header_link->newer;
        link = cache->links + index;

        header_link->newer = link->newer;
        cache->links[link->older].newer = cache->header_link_index;

        result = SOL_CACHE_SUCCESS_REPLACED;
    }

    found: /** move entry to the front */

    link->older = header_link->older;
    link->newer = cache->header_link_index;

    cache->links[header_link->older].newer = index;
    header_link->older = index;

    *entry_ptr = cache->entries + index;
    return result;
}

#ifdef SOL_LIMITED_CACHE_CONTEXT_TYPE
static inline enum sol_cache_result SOL_CONCATENATE(SOL_LIMITED_CACHE_FUNCTION_PREFIX,_find)(struct SOL_LIMITED_CACHE_STRUCT_NAME* cache, const SOL_LIMITED_CACHE_KEY_TYPE key, SOL_LIMITED_CACHE_ENTRY_TYPE** entry_ptr, SOL_LIMITED_CACHE_CONTEXT_TYPE* context)
#else
static inline enum sol_cache_result SOL_CONCATENATE(SOL_LIMITED_CACHE_FUNCTION_PREFIX,_find)(struct SOL_LIMITED_CACHE_STRUCT_NAME* cache, const SOL_LIMITED_CACHE_KEY_TYPE key, SOL_LIMITED_CACHE_ENTRY_TYPE** entry_ptr)
#endif
{
    uint16_t index;
    struct sol_cache_link_u16* link;
    struct sol_cache_link_u16* const header_link = cache->links + cache->header_link_index;
    enum sol_cache_result result;

    /** search from the newest cache entry */
    index = header_link->older;
    link = cache->links + index;

    while(link != header_link)
    {
        if(SOL_LIMITED_CACHE_CMP_EQ_CONTEXT((cache->entries + index), key))
        {
            /** remove entry from cache and move to the front */
            cache->links[link->newer].older = link->older;
            cache->links[link->older].newer = link->newer;

            link->older = header_link->older;
            link->newer = cache->header_link_index;

            cache->links[header_link->older].newer = index;
            header_link->older = index;

            *entry_ptr = cache->entries + index;
            return SOL_CACHE_SUCCESS_FOUND;
        }

        index = link->older;
        link = cache->links + index;
    }

    /** entry not found in cache */
    *entry_ptr = NULL;
    return SOL_CACHE_FAIL_ABSENT;
}


#undef SOL_LIMITED_CACHE_ENTRY_TYPE
#undef SOL_LIMITED_CACHE_KEY_TYPE
#undef SOL_LIMITED_CACHE_FUNCTION_PREFIX
#undef SOL_LIMITED_CACHE_STRUCT_NAME
#undef SOL_LIMITED_CACHE_CMP_EQ

#undef SOL_LIMITED_CACHE_CMP_EQ_CONTEXT

#ifdef SOL_LIMITED_CACHE_CONTEXT_TYPE
#undef SOL_LIMITED_CACHE_CONTEXT_TYPE
#endif

