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
#include "sol_utils.h"

#ifndef STRUCT_NAME
#error must define STRUCT_NAME
#define STRUCT_NAME placeholder_hash_map
#endif

#ifndef FUNCTION_PREFIX
#error must define FUNCTION_PREFIX
#define FUNCTION_PREFIX placeholder_hash_map
#endif

#ifndef KEY_TYPE
#error must define KEY_TYPE
#define KEY_TYPE uint64_t
#endif

#ifndef ENTRY_TYPE
#error must define ENTRY_TYPE
#define ENTRY_TYPE uint64_t
#endif

#ifndef FUNCTION_KEYWORDS
#define FUNCTION_KEYWORDS
#endif

#ifndef KEY_ENTRY_CMP_EQUAL
#error must define KEY_ENTRY_CMP_EQUAL(const KEY_TYPE*, const ENTRY_TYPE*) returning a bool
#define KEY_ENTRY_CMP_EQUAL(K,E) ((*K)==(*E))
#endif

#ifndef KEY_HASH
#error must define KEY_HASH(const KEY_TYPE*)` returning a uint64_t
#define KEY_HASH(K) (*K)
#endif

#ifndef ENTRY_HASH
#error must define ENTRY_HASH(const KEY_TYPE*)` returning a uint64_t
#define ENTRY_HASH(E) (*E)
#endif



#ifndef PRIOR_DECLARATION
struct STRUCT_NAME
{
    struct sol_hash_map_descriptor descriptor;

    uint8_t entry_space_exponent;

    ENTRY_TYPE* entries;
    uint16_t* identifiers;

    #ifdef CONTEXT_TYPE
    CONTEXT_TYPE context;
    #endif

    uint64_t entry_count;
    uint64_t entry_limit;
};
#else
#undef PRIOR_DECLARATION
#endif



static inline void SOL_CONCATENATE(FUNCTION_PREFIX,_resize__i)(struct STRUCT_NAME* map)
{
    uint64_t entry_hash, index, old_index, prev_index, entry_index;
    uint16_t identifier;
    const ENTRY_TYPE* entry_ptr;

    const uint64_t old_entry_space = (uint64_t)1 << map->entry_space_exponent;

    map->entry_space_exponent++;

    ENTRY_TYPE* old_entries = map->entries;
    ENTRY_TYPE* new_entries = malloc(sizeof(ENTRY_TYPE) << map->entry_space_exponent);
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
            #ifdef CONTEXT_TYPE
            entry_hash = ENTRY_HASH(entry_ptr, map->context);
            #else
            entry_hash = ENTRY_HASH(entry_ptr);
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
                if(sol_hash_map_identifier_not_ordered_after_key(identifier, new_identifiers[prev_index]))
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

static inline bool SOL_CONCATENATE(FUNCTION_PREFIX,_locate__i)(struct STRUCT_NAME* map, KEY_TYPE* key_ptr, uint16_t identifier, uint64_t index, uint64_t* index_result)
{
    uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;
    uint64_t index_mask = entry_space - 1;
    uint16_t* identifiers = map->identifiers;
    ENTRY_TYPE* entries = map->entries;

    while(sol_hash_map_identifier_exists_and_ordered_before_key(identifier, identifiers[index]))
    {
        index = (index==index_mask) ? 0 : index+1;
    }

    while(identifier == identifiers[index])
    {
        #ifdef CONTEXT_TYPE
        if(KEY_ENTRY_CMP_EQUAL(key_ptr, entries + index, map->context))
        #else
        if(KEY_ENTRY_CMP_EQUAL(key_ptr, entries + index))
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

static inline void SOL_CONCATENATE(FUNCTION_PREFIX,_evict_index__i)(struct STRUCT_NAME* map, uint64_t index)
{
    uint64_t entry_space, index_mask, next_index;
    uint16_t identifier;

    ENTRY_TYPE* entries = map->entries;
    uint16_t* identifiers = map->identifiers;

    entry_space = (uint64_t)1 << map->entry_space_exponent;
    index_mask = entry_space - 1;

    while(true)
    {
        next_index = (index==index_mask) ? 0 : index+1;
        identifier = identifiers[next_index];
        if(sol_hash_map_identifier_exists_and_can_move_backwards(next_index, identifier))
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



#ifdef CONTEXT_TYPE
FUNCTION_KEYWORDS void SOL_CONCATENATE(FUNCTION_PREFIX,_initialise)(struct STRUCT_NAME* map, uint8_t initial_entry_space_exponent, struct sol_hash_map_descriptor descriptor, CONTEXT_TYPE context)
#else
FUNCTION_KEYWORDS void SOL_CONCATENATE(FUNCTION_PREFIX,_initialise)(struct STRUCT_NAME* map, uint8_t initial_entry_space_exponent, struct sol_hash_map_descriptor descriptor)
#endif
{
    assert(descriptor.entry_space_exponent_limit <= 32-SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS);
    /** beyond this is probably unreasonable, used to catch this accidentally being treated as raw COUNT rather than power of 2 */
    assert(initial_entry_space_exponent <= descriptor.entry_space_exponent_limit);
    assert(initial_entry_space_exponent >= SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS);
    assert(initial_entry_space_exponent >= 8);/* must have exponent greater than uint8_t for all bits in fill limit to be significant */

    map->descriptor = descriptor;

    map->entries = malloc(sizeof(ENTRY_TYPE) << initial_entry_space_exponent);
    map->identifiers = malloc(sizeof(uint16_t) << initial_entry_space_exponent);

    #ifdef CONTEXT_TYPE
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

FUNCTION_KEYWORDS void SOL_CONCATENATE(FUNCTION_PREFIX,_terminate)(struct STRUCT_NAME* map)
{
    free(map->entries);
    free(map->identifiers);
}

FUNCTION_KEYWORDS void SOL_CONCATENATE(FUNCTION_PREFIX,_clear)(struct STRUCT_NAME* map)
{
    map->entry_count = 0;
    memset(map->identifiers, 0x00, sizeof(uint16_t) << map->entry_space_exponent);
}

FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(FUNCTION_PREFIX,_find)(struct STRUCT_NAME* map, KEY_TYPE* key, ENTRY_TYPE** entry_ptr)
{
    const uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;
    const uint64_t index_mask = entry_space - 1;
    #ifdef CONTEXT_TYPE
    const uint64_t key_hash = KEY_HASH(key, map->context);
    #else
    const uint64_t key_hash = KEY_HASH(key);
    #endif
    const uint16_t key_identifier = (key_hash << 1) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;
    uint64_t index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) & index_mask;

    if(SOL_CONCATENATE(FUNCTION_PREFIX,_locate__i)(map, key, key_identifier, index, &index ))
    {
        *entry_ptr = map->entries + index;
        return SOL_MAP_SUCCESS_FOUND;
    }

    *entry_ptr = NULL;
    return SOL_MAP_FAIL_ABSENT;
}

FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(FUNCTION_PREFIX,_obtain)(struct STRUCT_NAME* map, KEY_TYPE* key, ENTRY_TYPE** entry_ptr)
{
    #ifdef CONTEXT_TYPE
    const uint64_t key_hash = KEY_HASH(key, map->context);
    #else
    const uint64_t key_hash = KEY_HASH(key);
    #endif
    const uint16_t key_identifier = (key_hash << 1) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;

    uint64_t entry_space, index_mask, key_index, move_index, next_move_index, prev_move_index;
    uint16_t move_identifier;

    ENTRY_TYPE* entries;
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
            SOL_CONCATENATE(FUNCTION_PREFIX,_resize__i)(map);
        }
    }

    SOL_CONCATENATE(FUNCTION_PREFIX,_obtain_for_map_size__goto):
    {
        entry_space = (uint64_t)1 << map->entry_space_exponent;
        index_mask = entry_space - 1;

        key_index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) & index_mask;

        if(SOL_CONCATENATE(FUNCTION_PREFIX,_locate__i)(map, key, key_identifier, key_index, &key_index))
        {
            *entry_ptr = map->entries + key_index;
            return SOL_MAP_SUCCESS_FOUND;
        }

        move_identifier = key_identifier;
        next_move_index = key_index;
        /** ^ note: initially checking that the entry being added is being added at a valid location */
        do
        {
            if(sol_hash_map_identifier_invalid_at_new_index(next_move_index, move_identifier))
            {
                if(map->entry_space_exponent == map->descriptor.entry_space_exponent_limit)
                {
                    *entry_ptr = NULL;
                    return SOL_MAP_FAIL_FULL;
                }
                else
                {
                    SOL_CONCATENATE(FUNCTION_PREFIX,_resize__i)(map);
                    goto SOL_CONCATENATE(FUNCTION_PREFIX,_obtain_for_map_size__goto);
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

FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(FUNCTION_PREFIX,_insert)(struct STRUCT_NAME* map, ENTRY_TYPE* entry)
{
    ENTRY_TYPE* entry_ptr;
    enum sol_map_result result;

    result = SOL_CONCATENATE(FUNCTION_PREFIX,_obtain)(map, entry, &entry_ptr);

    switch (result)
    {
    case SOL_MAP_SUCCESS_FOUND:/** note: SOL_MAP_SUCCESS_FOUND == SOL_MAP_SUCCESS_REPLACED */
    case SOL_MAP_SUCCESS_INSERTED:
        *entry_ptr = *entry;
    default:;
    }
    return result;
}

FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(FUNCTION_PREFIX,_remove)(struct STRUCT_NAME* map, KEY_TYPE* key, ENTRY_TYPE* entry)
{
    const uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;
    const uint64_t index_mask = entry_space - 1;
    #ifdef CONTEXT_TYPE
    const uint64_t key_hash = KEY_HASH(key, map->context);
    #else
    const uint64_t key_hash = KEY_HASH(key);
    #endif

    const uint16_t key_identifier = (key_hash << 1) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;
    uint64_t index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) & index_mask;

    if(SOL_CONCATENATE(FUNCTION_PREFIX,_locate__i)(map, key, key_identifier, index, &index ))
    {
        if(entry)
        {
            *entry = map->entries[index];
        }
        SOL_CONCATENATE(FUNCTION_PREFIX,_evict_index__i)(map, index);
        return SOL_MAP_SUCCESS_REMOVED;
    }

    return SOL_MAP_FAIL_ABSENT;
}

FUNCTION_KEYWORDS void SOL_CONCATENATE(FUNCTION_PREFIX,_delete_entry)(struct STRUCT_NAME* map, ENTRY_TYPE* entry_ptr)
{
    const uint64_t index = entry_ptr - map->entries;
    assert(entry_ptr >= map->entries);
    assert(entry_ptr < map->entries + (1 << map->entry_space_exponent));
    SOL_CONCATENATE(FUNCTION_PREFIX,_evict_index__i)(map, index);
}


#undef STRUCT_NAME
#undef FUNCTION_PREFIX
#undef KEY_TYPE
#undef ENTRY_TYPE
#undef FUNCTION_KEYWORDS
#undef KEY_ENTRY_CMP_EQUAL
#undef KEY_HASH
#undef ENTRY_HASH

#ifdef CONTEXT_TYPE
#undef CONTEXT_TYPE
#endif
