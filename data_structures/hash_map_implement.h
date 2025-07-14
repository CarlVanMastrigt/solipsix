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

#include "data_structures/hash_map.h"


#ifndef SOL_HASH_MAP_STRUCT_NAME
#error must define SOL_HASH_MAP_STRUCT_NAME
#define SOL_HASH_MAP_STRUCT_NAME placeholder_hash_map
#endif

#ifndef SOL_HASH_MAP_FUNCTION_PREFIX
#error must define SOL_HASH_MAP_FUNCTION_PREFIX
#define SOL_HASH_MAP_FUNCTION_PREFIX placeholder_hash_map
#endif

#ifndef SOL_HASH_MAP_KEY_TYPE
#error must define SOL_HASH_MAP_KEY_TYPE
#define SOL_HASH_MAP_KEY_TYPE uint64_t
#endif

#ifndef SOL_HASH_MAP_ENTRY_TYPE
#error must define SOL_HASH_MAP_ENTRY_TYPE
#define SOL_HASH_MAP_ENTRY_TYPE uint64_t
#endif

#ifndef SOL_HASH_MAP_FUNCTION_KEYWORDS
#define SOL_HASH_MAP_FUNCTION_KEYWORDS
#endif

#ifndef SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL
#error must define SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(const KEY_TYPE*, const ENTRY_TYPE*) returning a bool
#define SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(K,E) ((*K)==(*E))
#endif

#ifndef SOL_HASH_MAP_KEY_HASH
#error must define SOL_HASH_MAP_KEY_HASH(const KEY_TYPE*)` returning a uint64_t
#define SOL_HASH_MAP_KEY_HASH(K) (*K)
#endif

#ifndef SOL_HASH_MAP_ENTRY_HASH
#error must define SOL_HASH_MAP_ENTRY_HASH(const KEY_TYPE*)` returning a uint64_t
#define SOL_HASH_MAP_ENTRY_HASH(E) (*E)
#endif

/** optional tweak to hash map properties */
#ifndef SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS
#define SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS 8
#endif



#define SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_SHIFT (16 - SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS)
#define SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS (SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_SHIFT-1)
// ^ bottom fractional bit will be repurposed (SOL_HASH_MAP_IDENTIFIER_EXIST_BIT)

// if this bit is zero when subtracing the identifier of the key we're searching for <k>
// from the existing (set) identifier we're comparing to <c>
// [i.e. ((c-k) & SOL_HASH_MAP_DELTA_TEST_BIT) == 0 ]
// then <k> is greater than or equal to <c>
// note: this only holds if the maximum offset (below) of the hash map is maintained/respected


/** maximum offset must use 1 bit less than the largest index identifier
    if offset from actual location is equal to this (or greater than) then the necessary condition imposed on identifiers has been violated */
#define SOL_HASH_MAP_INVALID_OFFSET (1 << (SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS - 1))
#define SOL_HASH_MAP_IDENTIFIER_INDEX_MASK ((1 << SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS) - 1)

static inline bool SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_identifier_exists_and_ordered_before_key)(uint16_t key_identifier, uint16_t compare_identifier)
{
    return compare_identifier && ((compare_identifier-key_identifier) & SOL_HASH_MAP_DELTA_TEST_BIT);
}

static inline bool SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_identifier_not_ordered_after_key)(uint16_t key_identifier, uint16_t compare_identifier)
{
    return ((key_identifier-compare_identifier) & SOL_HASH_MAP_DELTA_TEST_BIT) == 0;
}

static inline bool SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_identifier_exists_and_can_move_backwards__i)(uint64_t current_index, uint_fast16_t identifier)
{
    return identifier && (current_index & SOL_HASH_MAP_IDENTIFIER_INDEX_MASK) != (identifier >> SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_SHIFT);
}

/** can the identifier be moved to this index without invalidating the maps offset condition?
    (an offset will become invalid due to map saturation)
    this check only works if entries are moved one index/place at a time
    so is basically used to check whether an entry can be moved forward one place */
static inline bool SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_identifier_invalid_at_new_index__i)(uint64_t new_index, uint_fast16_t identifier)
{
    new_index = (new_index - SOL_HASH_MAP_INVALID_OFFSET) & SOL_HASH_MAP_IDENTIFIER_INDEX_MASK;
    identifier = identifier >> SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_SHIFT;
    return new_index == identifier;
}











#ifndef SOL_HASH_MAP_PRIOR_DECLARATION
struct SOL_HASH_MAP_STRUCT_NAME
{
    struct sol_hash_map_descriptor descriptor;

    uint8_t entry_space_exponent;

    SOL_HASH_MAP_ENTRY_TYPE* entries;
    uint16_t* identifiers;

    #ifdef SOL_HASH_MAP_CONTEXT_TYPE
    SOL_HASH_MAP_CONTEXT_TYPE context;
    #endif

    uint64_t entry_count;
    uint64_t entry_limit;
};
#else
#undef SOL_HASH_MAP_PRIOR_DECLARATION
#endif



static inline void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_resize__i)(struct SOL_HASH_MAP_STRUCT_NAME* map)
{
    uint64_t entry_hash, index, old_index, prev_index, entry_index;
    uint16_t identifier;
    const SOL_HASH_MAP_ENTRY_TYPE* entry_ptr;

    const uint64_t old_entry_space = (uint64_t)1 << map->entry_space_exponent;

    map->entry_space_exponent++;

    SOL_HASH_MAP_ENTRY_TYPE* old_entries = map->entries;
    SOL_HASH_MAP_ENTRY_TYPE* new_entries = malloc(sizeof(SOL_HASH_MAP_ENTRY_TYPE) << map->entry_space_exponent);
    map->entries = new_entries;

    uint16_t* old_identifiers = map->identifiers;
    uint16_t* new_identifiers = malloc(sizeof(uint16_t) << map->entry_space_exponent);
    map->identifiers = new_identifiers;
    memset(new_identifiers, 0x00, sizeof(uint16_t) << map->entry_space_exponent);

    if(map->entry_space_exponent == map->descriptor.entry_space_exponent_limit)
    {
        map->entry_limit = ((uint64_t)map->descriptor.limit_fill_factor << (map->entry_space_exponent-8));
    }
    else
    {
        map->entry_limit = ((uint64_t)map->descriptor.resize_fill_factor << (map->entry_space_exponent-8));
    }

    const uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;
    const uint64_t index_mask = entry_space - 1;

    for(old_index = 0; old_index < old_entry_space; old_index++)
    {
        identifier = old_identifiers[old_index];
        if(identifier)
        {
            entry_ptr = old_entries + old_index;
            #ifdef SOL_HASH_MAP_CONTEXT_TYPE
            entry_hash = SOL_HASH_MAP_ENTRY_HASH(entry_ptr, map->context);
            #else
            entry_hash = SOL_HASH_MAP_ENTRY_HASH(entry_ptr);
            #endif
            entry_index = (entry_hash >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) & index_mask;
            index = entry_index;

            while(new_identifiers[index])
            {
                index = (index==index_mask) ? 0 : index+1;
            }

            while(index != entry_index)
            {
                prev_index = (index==0) ? index_mask : index-1;
                /** at this point prev must exist, only need to check ordering */
                if(SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_identifier_not_ordered_after_key)(identifier, new_identifiers[prev_index]))
                {
                    break;
                }
                new_entries    [index] = new_entries    [prev_index];
                new_identifiers[index] = new_identifiers[prev_index];
                index = prev_index;
            }

            new_entries    [index] = *entry_ptr;
            new_identifiers[index] = identifier;
        }
    }

    free(old_identifiers);
    free(old_entries);
}

static inline bool SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_locate__i)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_KEY_TYPE* key_ptr, uint16_t identifier, uint64_t index, uint64_t* index_result)
{
    uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;
    uint64_t index_mask = entry_space - 1;
    uint16_t* identifiers = map->identifiers;
    SOL_HASH_MAP_ENTRY_TYPE* entries = map->entries;

    while(SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_identifier_exists_and_ordered_before_key)(identifier, identifiers[index]))
    {
        index = (index==index_mask) ? 0 : index+1;
    }

    while(identifier == identifiers[index])
    {
        #ifdef SOL_HASH_MAP_CONTEXT_TYPE
        if(SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(key_ptr, entries + index, map->context))
        #else
        if(SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(key_ptr, entries + index))
        #endif
        {
            *index_result = index;
            return true;/** precise key/entry found */
        }
        index = (index==index_mask) ? 0 : index+1;
    }

    *index_result = index;
    return false;/** insertion required at index */
}

static inline void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_evict_index__i)(struct SOL_HASH_MAP_STRUCT_NAME* map, uint64_t index)
{
    uint64_t entry_space, index_mask, next_index;
    uint16_t identifier;

    SOL_HASH_MAP_ENTRY_TYPE* entries = map->entries;
    uint16_t* identifiers = map->identifiers;

    entry_space = (uint64_t)1 << map->entry_space_exponent;
    index_mask = entry_space - 1;

    while(true)
    {
        next_index = (index==index_mask) ? 0 : index+1;
        identifier = identifiers[next_index];
        if(SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_identifier_exists_and_can_move_backwards__i)(next_index, identifier))
        {
            entries    [index] = entries[next_index];
            identifiers[index] = identifier;
            index = next_index;
        }
        else break;
    }
    identifiers[index] = 0;
    map->entry_count--;
}



#ifdef SOL_HASH_MAP_CONTEXT_TYPE
SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_initialise)(struct SOL_HASH_MAP_STRUCT_NAME* map, uint8_t initial_entry_space_exponent, struct sol_hash_map_descriptor descriptor, SOL_HASH_MAP_CONTEXT_TYPE context)
#else
SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_initialise)(struct SOL_HASH_MAP_STRUCT_NAME* map, uint8_t initial_entry_space_exponent, struct sol_hash_map_descriptor descriptor)
#endif
{
    assert(descriptor.entry_space_exponent_limit <= 32-SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS);
    /** beyond this is probably unreasonable, used to catch this accidentally being treated as raw COUNT rather than power of 2 */
    assert(initial_entry_space_exponent <= descriptor.entry_space_exponent_limit);
    assert(initial_entry_space_exponent >= SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS);
    assert(initial_entry_space_exponent >= 8);/* must have exponent greater than uint8_t for all bits in fill limit to be significant */

    map->descriptor = descriptor;

    map->entries = malloc(sizeof(SOL_HASH_MAP_ENTRY_TYPE) << initial_entry_space_exponent);
    map->identifiers = malloc(sizeof(uint16_t) << initial_entry_space_exponent);

    #ifdef SOL_HASH_MAP_CONTEXT_TYPE
    map->context = context;
    #endif

    map->entry_space_exponent = initial_entry_space_exponent;
    map->entry_count = 0;

    if(initial_entry_space_exponent == descriptor.entry_space_exponent_limit)
    {
        map->entry_limit = ((uint64_t)descriptor.limit_fill_factor << (initial_entry_space_exponent - 8));
    }
    else
    {
        map->entry_limit = ((uint64_t)descriptor.resize_fill_factor << (initial_entry_space_exponent - 8));
    }

    memset(map->identifiers, 0x00, sizeof(uint16_t) << initial_entry_space_exponent);
}

SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_terminate)(struct SOL_HASH_MAP_STRUCT_NAME* map)
{
    free(map->entries);
    free(map->identifiers);
}

SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_clear)(struct SOL_HASH_MAP_STRUCT_NAME* map)
{
    map->entry_count = 0;
    memset(map->identifiers, 0x00, sizeof(uint16_t) << map->entry_space_exponent);
}

SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_find)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_KEY_TYPE* key, SOL_HASH_MAP_ENTRY_TYPE** entry_ptr)
{
    const uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;
    const uint64_t index_mask = entry_space - 1;
    #ifdef SOL_HASH_MAP_CONTEXT_TYPE
    const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH(key, map->context);
    #else
    const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH(key);
    #endif
    const uint16_t key_identifier = (key_hash << 1) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;
    uint64_t index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) & index_mask;

    if(SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_locate__i)(map, key, key_identifier, index, &index ))
    {
        *entry_ptr = map->entries + index;
        return SOL_MAP_SUCCESS_FOUND;
    }

    *entry_ptr = NULL;
    return SOL_MAP_FAIL_ABSENT;
}

SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_obtain)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_KEY_TYPE* key, SOL_HASH_MAP_ENTRY_TYPE** entry_ptr)
{
    #ifdef SOL_HASH_MAP_CONTEXT_TYPE
    const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH(key, map->context);
    #else
    const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH(key);
    #endif
    const uint16_t key_identifier = (key_hash << 1) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;

    uint64_t entry_space, index_mask, key_index, move_index, next_move_index, prev_move_index;
    uint16_t move_identifier;

    SOL_HASH_MAP_ENTRY_TYPE* entries;
    uint16_t* identifiers;

    if(map->entry_count == map->entry_limit)
    {
        if(map->entry_space_exponent == map->descriptor.entry_space_exponent_limit)
        {
            *entry_ptr = NULL;
            return SOL_MAP_FAIL_FULL;
        }
        else
        {
            SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_resize__i)(map);
        }
    }

    SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_obtain_for_map_size__goto):
    {
        entry_space = (uint64_t)1 << map->entry_space_exponent;
        index_mask = entry_space - 1;

        key_index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) & index_mask;

        if(SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_locate__i)(map, key, key_identifier, key_index, &key_index))
        {
            *entry_ptr = map->entries + key_index;
            return SOL_MAP_SUCCESS_FOUND;
        }

        move_identifier = key_identifier;
        next_move_index = key_index;
        /** ^ note: initially checking that the entry being added is being added at a valid location */
        do
        {
            if(SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_identifier_invalid_at_new_index__i)(next_move_index, move_identifier))
            {
                if(map->entry_space_exponent == map->descriptor.entry_space_exponent_limit)
                {
                    *entry_ptr = NULL;
                    return SOL_MAP_FAIL_FULL;
                }
                else
                {
                    SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_resize__i)(map);
                    goto SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_obtain_for_map_size__goto);
                }
            }
            move_index = next_move_index;
            move_identifier = map->identifiers[move_index];
            next_move_index = (move_index==index_mask) ? 0 : move_index+1;
        }
        while(move_identifier);
    }

    entries = map->entries;
    identifiers = map->identifiers;

    /** shift everything forward one into the discovered empty slot */
    while(move_index != key_index)
    {
        prev_move_index = (move_index==0) ? index_mask : move_index-1;
        entries    [move_index] = entries    [prev_move_index];
        identifiers[move_index] = identifiers[prev_move_index];
        move_index = prev_move_index;
    }

    *entry_ptr = entries + key_index;
    identifiers[key_index] = key_identifier;
    map->entry_count++;

    return SOL_MAP_SUCCESS_INSERTED;
}

SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_insert)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_ENTRY_TYPE* entry)
{
    SOL_HASH_MAP_ENTRY_TYPE* entry_ptr;
    enum sol_map_result result;

    result = SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_obtain)(map, entry, &entry_ptr);

    switch (result)
    {
    case SOL_MAP_SUCCESS_FOUND:/** note: SOL_MAP_SUCCESS_FOUND == SOL_MAP_SUCCESS_REPLACED */
    case SOL_MAP_SUCCESS_INSERTED:
        *entry_ptr = *entry;
    default:;
    }
    return result;
}

SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_remove)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_KEY_TYPE* key, SOL_HASH_MAP_ENTRY_TYPE* entry)
{
    const uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;
    const uint64_t index_mask = entry_space - 1;
    #ifdef SOL_HASH_MAP_CONTEXT_TYPE
    const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH(key, map->context);
    #else
    const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH(key);
    #endif

    const uint16_t key_identifier = (key_hash << 1) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;
    uint64_t index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) & index_mask;

    if(SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_locate__i)(map, key, key_identifier, index, &index ))
    {
        if(entry)
        {
            *entry = map->entries[index];
        }
        SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_evict_index__i)(map, index);
        return SOL_MAP_SUCCESS_REMOVED;
    }

    return SOL_MAP_FAIL_ABSENT;
}

SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_delete_entry)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_ENTRY_TYPE* entry_ptr)
{
    const uint64_t index = entry_ptr - map->entries;
    assert(entry_ptr >= map->entries);
    assert(entry_ptr < map->entries + (1 << map->entry_space_exponent));
    SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_evict_index__i)(map, index);
}


#undef SOL_HASH_MAP_STRUCT_NAME
#undef SOL_HASH_MAP_FUNCTION_PREFIX
#undef SOL_HASH_MAP_KEY_TYPE
#undef SOL_HASH_MAP_ENTRY_TYPE
#undef SOL_HASH_MAP_FUNCTION_KEYWORDS
#undef SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL
#undef SOL_HASH_MAP_KEY_HASH
#undef SOL_HASH_MAP_ENTRY_HASH

#ifdef SOL_HASH_MAP_CONTEXT_TYPE
#undef SOL_HASH_MAP_CONTEXT_TYPE
#endif
