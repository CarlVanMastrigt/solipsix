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

#include "data_structures/hash_map_defines.h"


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
#error must define SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(KEY_TYPE, const ENTRY_TYPE*) returning a bool
#define SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(K,E) ((K)==(*E))
#endif

#ifndef SOL_HASH_MAP_KEY_FROM_ENTRY
#error must define SOL_HASH_MAP_KEY_FROM_ENTRY(const ENTRY_TYPE*) returning a KEY_TYPE
#define SOL_HASH_MAP_KEY_FROM_ENTRY(E) (*E)
#endif

#ifndef SOL_HASH_MAP_KEY_HASH
#error must define SOL_HASH_MAP_KEY_HASH(KEY_TYPE)` returning a uint64_t
#define SOL_HASH_MAP_KEY_HASH(K) (K)
#endif


/** optional tweaks to hash map properties */
#ifndef SOL_HASH_MAP_IDENTIFIER_TYPE
#define SOL_HASH_MAP_IDENTIFIER_TYPE uint16_t
#endif
#ifndef SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS
#define SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS 6
#endif


#ifdef SOL_HASH_MAP_CONTEXT_TYPE
#define SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL_CONTEXT(K,E) SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(K, E, map->context)
#define SOL_HASH_MAP_KEY_FROM_ENTRY_CONTEXT(E) SOL_HASH_MAP_KEY_FROM_ENTRY(E, map->context)
#define SOL_HASH_MAP_KEY_HASH_CONTEXT(K) SOL_HASH_MAP_KEY_HASH(K, map->context)
#else
#define SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL_CONTEXT(K,E) SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(K, E)
#define SOL_HASH_MAP_KEY_FROM_ENTRY_CONTEXT(E) SOL_HASH_MAP_KEY_FROM_ENTRY(E)
#define SOL_HASH_MAP_KEY_HASH_CONTEXT(K) SOL_HASH_MAP_KEY_HASH(K)
#endif

/** the offset is the inverse of the displacement capacity, stored in the top bits */
#define SOL_HASH_MAP_IDENTIFIER_OFFSET_SHIFT ((SOL_HASH_MAP_IDENTIFIER_TYPE)((sizeof(SOL_HASH_MAP_IDENTIFIER_TYPE) * CHAR_BIT) - SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS))
#define SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_HASH_BIT_COUNT ((SOL_HASH_MAP_IDENTIFIER_TYPE)SOL_HASH_MAP_IDENTIFIER_OFFSET_SHIFT)
#define SOL_HASH_MAP_IDENTIFIER_OFFSET_UNIT ((SOL_HASH_MAP_IDENTIFIER_TYPE)(1 << SOL_HASH_MAP_IDENTIFIER_OFFSET_SHIFT))
#define SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_HASH_MASK ((SOL_HASH_MAP_IDENTIFIER_TYPE)(SOL_HASH_MAP_IDENTIFIER_OFFSET_UNIT - 1))

#define SOL_HASH_MAP_IDENTIFIER_MAXIMUM_DISPLACEMENT_CAPACITY ((SOL_HASH_MAP_IDENTIFIER_TYPE)(-SOL_HASH_MAP_IDENTIFIER_OFFSET_UNIT))
/** ^ identifier must be DISTINCTLY SMALLER than this to move backwards, is all bits set in offset part of identifier
 * this is also the starting value of the offset part of the identifier (cannot move backwards)
 * as it is moved forward the number of potential moves remaining decreases, meaning we can set */

#define SOL_HASH_MAP_IDENTIFIER_MINIMUM_DISPLACEMENT_CAPACITY SOL_HASH_MAP_IDENTIFIER_OFFSET_UNIT
/** ^ identifier must be this big or larget to be valid */


#ifndef SOL_HASH_MAP_DECLARATION_PRESENT
struct SOL_HASH_MAP_STRUCT_NAME
{
    struct sol_hash_map_descriptor descriptor;

    uint8_t entry_space_exponent;

    SOL_HASH_MAP_ENTRY_TYPE* entries;
    SOL_HASH_MAP_IDENTIFIER_TYPE* identifiers;

    #ifdef SOL_HASH_MAP_CONTEXT_TYPE
    SOL_HASH_MAP_CONTEXT_TYPE context;
    #endif

    uint64_t entry_count;
    uint64_t entry_limit;
};
#else
#undef SOL_HASH_MAP_DECLARATION_PRESENT
#endif


/** need to early define insert as resize uses it */
SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_insert)(struct SOL_HASH_MAP_STRUCT_NAME* map, const SOL_HASH_MAP_ENTRY_TYPE* entry);

static void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_resize__i)(struct SOL_HASH_MAP_STRUCT_NAME* map)
{
    const uint64_t old_entry_space = (uint64_t)1 << map->entry_space_exponent;
    uint64_t index;
    enum sol_map_result insert_result;

    map->entry_space_exponent++;

    SOL_HASH_MAP_ENTRY_TYPE* old_entries = map->entries;
    SOL_HASH_MAP_ENTRY_TYPE* new_entries = malloc(sizeof(SOL_HASH_MAP_ENTRY_TYPE) << map->entry_space_exponent);
    map->entries = new_entries;

    SOL_HASH_MAP_IDENTIFIER_TYPE* old_identifiers = map->identifiers;
    SOL_HASH_MAP_IDENTIFIER_TYPE* new_identifiers = malloc(sizeof(SOL_HASH_MAP_IDENTIFIER_TYPE) << map->entry_space_exponent);
    map->identifiers = new_identifiers;
    memset(new_identifiers, 0x00, sizeof(SOL_HASH_MAP_IDENTIFIER_TYPE) << map->entry_space_exponent);

    if(map->entry_space_exponent == map->descriptor.entry_space_exponent_limit)
    {
        map->entry_limit = ((uint64_t)map->descriptor.limit_fill_factor << (map->entry_space_exponent-8));
    }
    else
    {
        map->entry_limit = ((uint64_t)map->descriptor.resize_fill_factor << (map->entry_space_exponent-8));
    }

    map->entry_count = 0;

    for(index = 0; index < old_entry_space; index++)
    {
        /** every location in the old map has a valid location so its unnecessary to account for failure to add here (guaranteed to succeed)*/
        if(old_identifiers[index])
        {
            insert_result = SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_insert)(map, old_entries + index);

            assert(insert_result == SOL_MAP_SUCCESS_INSERTED);
        }
    }

    free(old_identifiers);
    free(old_entries);
}

static inline bool SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_locate__i)(struct SOL_HASH_MAP_STRUCT_NAME* restrict map, SOL_HASH_MAP_KEY_TYPE key, SOL_HASH_MAP_IDENTIFIER_TYPE* restrict identifier, uint64_t* restrict index)
{
    uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;
    uint64_t index_mask = entry_space - 1;
    SOL_HASH_MAP_IDENTIFIER_TYPE* identifiers = map->identifiers;
    SOL_HASH_MAP_ENTRY_TYPE* entries = map->entries;

    while( identifiers[*index] && identifiers[*index] < *identifier)
    {
        *identifier -= SOL_HASH_MAP_IDENTIFIER_OFFSET_UNIT;
        *index = (*index == index_mask) ? 0 : *index + 1;
    }

    while(*identifier == identifiers[*index])
    {
        if(SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL_CONTEXT(key, (entries + *index)))
        {
            return true;/** precise key/entry found */
        }
        *identifier -= SOL_HASH_MAP_IDENTIFIER_OFFSET_UNIT;
        *index = (*index == index_mask) ? 0 : *index + 1;
        assert(*identifier >= SOL_HASH_MAP_IDENTIFIER_MINIMUM_DISPLACEMENT_CAPACITY);
    }

    return false;/** insertion required at index */
}

static inline void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_evict_index__i)(struct SOL_HASH_MAP_STRUCT_NAME* map, uint64_t index)
{
    uint64_t entry_space, index_mask, next_index;
    SOL_HASH_MAP_IDENTIFIER_TYPE identifier;

    SOL_HASH_MAP_ENTRY_TYPE* entries = map->entries;
    SOL_HASH_MAP_IDENTIFIER_TYPE* identifiers = map->identifiers;

    entry_space = (uint64_t)1 << map->entry_space_exponent;
    index_mask = entry_space - 1;

    /** move all following entries backwards until an empty slot or an entry with the maximum displacement capacity is encountered*/
    while(true)
    {
        next_index = (index==index_mask) ? 0 : index + 1;
        identifier = identifiers[next_index];
        if(identifier && identifier < SOL_HASH_MAP_IDENTIFIER_MAXIMUM_DISPLACEMENT_CAPACITY)
        {
            assert((SOL_HASH_MAP_IDENTIFIER_TYPE)(identifier + SOL_HASH_MAP_IDENTIFIER_OFFSET_UNIT) > identifier);
            entries    [index] = entries[next_index];
            identifiers[index] = identifier + SOL_HASH_MAP_IDENTIFIER_OFFSET_UNIT;
            index = next_index;
        }
        else break;
    }
    identifiers[index] = 0;
    map->entry_count--;
}




#ifdef SOL_HASH_MAP_CONTEXT_TYPE
SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_initialise)(struct SOL_HASH_MAP_STRUCT_NAME* map, struct sol_hash_map_descriptor descriptor, SOL_HASH_MAP_CONTEXT_TYPE context)
#else
SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_initialise)(struct SOL_HASH_MAP_STRUCT_NAME* map, struct sol_hash_map_descriptor descriptor)
#endif
{
    assert(sizeof(SOL_HASH_MAP_IDENTIFIER_TYPE) * CHAR_BIT <= SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS);
    /** need to make sure this gets configured correctly */

    assert(descriptor.entry_space_exponent_limit <= 32 - SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_HASH_BIT_COUNT);
    /** beyond this is probably unreasonable, used to catch this accidentally being treated as raw COUNT rather than power of 2 */

    assert(descriptor.entry_space_exponent_initial >= 8);
    /** must have exponent greater than uint8_t for all bits in fill limit to be significant */

    assert(descriptor.entry_space_exponent_initial <= descriptor.entry_space_exponent_limit);
    assert(descriptor.entry_space_exponent_initial >= SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS);

    map->descriptor = descriptor;

    map->entries = malloc(sizeof(SOL_HASH_MAP_ENTRY_TYPE) << descriptor.entry_space_exponent_initial);
    map->identifiers = malloc(sizeof(SOL_HASH_MAP_IDENTIFIER_TYPE) << descriptor.entry_space_exponent_initial);

    #ifdef SOL_HASH_MAP_CONTEXT_TYPE
    map->context = context;
    #endif

    map->entry_space_exponent = descriptor.entry_space_exponent_initial;
    map->entry_count = 0;

    if(descriptor.entry_space_exponent_initial == descriptor.entry_space_exponent_limit)
    {
        map->entry_limit = ((uint64_t)descriptor.limit_fill_factor << (descriptor.entry_space_exponent_initial - 8));
    }
    else
    {
        map->entry_limit = ((uint64_t)descriptor.resize_fill_factor << (descriptor.entry_space_exponent_initial - 8));
    }

    memset(map->identifiers, 0x00, sizeof(SOL_HASH_MAP_IDENTIFIER_TYPE) << descriptor.entry_space_exponent_initial);
}

SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_terminate)(struct SOL_HASH_MAP_STRUCT_NAME* map)
{
    free(map->entries);
    free(map->identifiers);
}

#ifdef SOL_HASH_MAP_CONTEXT_TYPE
SOL_HASH_MAP_FUNCTION_KEYWORDS struct SOL_HASH_MAP_STRUCT_NAME* SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_create)(struct sol_hash_map_descriptor descriptor, SOL_HASH_MAP_CONTEXT_TYPE context)
{
    struct SOL_HASH_MAP_STRUCT_NAME* map = malloc(sizeof(struct SOL_HASH_MAP_STRUCT_NAME));
    SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_initialise)(map, descriptor, context);
    return map;
}
#else
SOL_HASH_MAP_FUNCTION_KEYWORDS struct SOL_HASH_MAP_STRUCT_NAME* SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_create)(struct sol_hash_map_descriptor descriptor)
{
    struct SOL_HASH_MAP_STRUCT_NAME* map = malloc(sizeof(struct SOL_HASH_MAP_STRUCT_NAME));
    SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_initialise)(map, descriptor);
    return map;
}
#endif

SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_destroy)(struct SOL_HASH_MAP_STRUCT_NAME* map)
{
    SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_terminate)(map);
    free(map);
}

SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_clear)(struct SOL_HASH_MAP_STRUCT_NAME* map)
{
    map->entry_count = 0;
    memset(map->identifiers, 0x00, sizeof(SOL_HASH_MAP_IDENTIFIER_TYPE) << map->entry_space_exponent);
}



SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_find)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_KEY_TYPE key, SOL_HASH_MAP_ENTRY_TYPE** entry_ptr)
{
    const uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;
    const uint64_t index_mask = entry_space - 1;
    const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH_CONTEXT(key);
    SOL_HASH_MAP_IDENTIFIER_TYPE key_identifier = (key_hash & SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_HASH_MASK) | SOL_HASH_MAP_IDENTIFIER_MAXIMUM_DISPLACEMENT_CAPACITY;
    uint64_t index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_HASH_BIT_COUNT) & index_mask;

    if(SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_locate__i)(map, key, &key_identifier, &index ))
    {
        *entry_ptr = map->entries + index;
        return SOL_MAP_SUCCESS_FOUND;
    }

    *entry_ptr = NULL;
    return SOL_MAP_FAIL_ABSENT;
}

SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_obtain)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_KEY_TYPE key, SOL_HASH_MAP_ENTRY_TYPE** entry_ptr)
{
    const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH_CONTEXT(key);

    uint64_t entry_space, index_mask, key_index, move_index, next_move_index, prev_move_index;
    SOL_HASH_MAP_IDENTIFIER_TYPE move_identifier, key_identifier, next_identifier, prev_identifier;

    SOL_HASH_MAP_ENTRY_TYPE* entries;
    SOL_HASH_MAP_IDENTIFIER_TYPE* identifiers;

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

    obtain_for_map_size:
    {
        entries = map->entries;
        identifiers = map->identifiers;

        entry_space = (uint64_t)1 << map->entry_space_exponent;
        index_mask = entry_space - 1;

        key_identifier = (key_hash & SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_HASH_MASK) | SOL_HASH_MAP_IDENTIFIER_MAXIMUM_DISPLACEMENT_CAPACITY;
        key_index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_HASH_BIT_COUNT) & index_mask;

        if(SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_locate__i)(map, key, &key_identifier, &key_index))
        {
            *entry_ptr = map->entries + key_index;
            return SOL_MAP_SUCCESS_FOUND;
        }

        move_identifier = key_identifier;
        move_index      = key_index;
        /** ^ note: initially checking that the entry being added is being added at a valid location */
        while(true)
        {
            if(move_identifier < SOL_HASH_MAP_IDENTIFIER_MINIMUM_DISPLACEMENT_CAPACITY)
            {
                while(move_index != key_index)
                {
                    /** move backwards: pick up identifier and replace with move identifier */
                    assert(move_identifier < SOL_HASH_MAP_IDENTIFIER_MAXIMUM_DISPLACEMENT_CAPACITY);

                    move_index = (move_index == 0) ? index_mask : move_index-1;

                    prev_identifier = identifiers[move_index];
                    identifiers[move_index] = move_identifier + SOL_HASH_MAP_IDENTIFIER_OFFSET_UNIT;
                    move_identifier = prev_identifier;
                }

                if(map->entry_space_exponent == map->descriptor.entry_space_exponent_limit)
                {
                    *entry_ptr = NULL;
                    return SOL_MAP_FAIL_FULL;
                }
                else
                {
                    SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_resize__i)(map);
                    goto obtain_for_map_size;
                }
            }

            /** move forwards */
            next_identifier = identifiers[move_index];
            identifiers[move_index] = move_identifier;

            if(next_identifier == 0)
            {
                break;
            }

            move_identifier = next_identifier - SOL_HASH_MAP_IDENTIFIER_OFFSET_UNIT;
            move_index = (move_index == index_mask) ? 0 : move_index + 1;
        }
    }

    /** shift everything forward one into the discovered empty slot */
    while(move_index != key_index)
    {
        prev_move_index = (move_index==0) ? index_mask : move_index-1;
        entries[move_index] = entries[prev_move_index];
        move_index = prev_move_index;
    }

    *entry_ptr = entries + key_index;
    map->entry_count++;

    return SOL_MAP_SUCCESS_INSERTED;
}

SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_insert)(struct SOL_HASH_MAP_STRUCT_NAME* map, const SOL_HASH_MAP_ENTRY_TYPE* entry)
{
    SOL_HASH_MAP_ENTRY_TYPE* entry_ptr;
    enum sol_map_result result;

    result = SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_obtain)(map, SOL_HASH_MAP_KEY_FROM_ENTRY_CONTEXT(entry), &entry_ptr);

    switch (result)
    {
    case SOL_MAP_SUCCESS_FOUND:/** note: SOL_MAP_SUCCESS_FOUND == SOL_MAP_SUCCESS_REPLACED */
    case SOL_MAP_SUCCESS_INSERTED:
        *entry_ptr = *entry;
    default:;
    }
    return result;
}

SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_remove)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_KEY_TYPE key, SOL_HASH_MAP_ENTRY_TYPE* entry)
{
    const uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;
    const uint64_t index_mask = entry_space - 1;
    const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH_CONTEXT(key);

    SOL_HASH_MAP_IDENTIFIER_TYPE key_identifier = (key_hash & SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_HASH_MASK) | SOL_HASH_MAP_IDENTIFIER_MAXIMUM_DISPLACEMENT_CAPACITY;
    uint64_t index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_HASH_BIT_COUNT) & index_mask;

    if(SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_locate__i)(map, key, &key_identifier, &index ))
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
#undef SOL_HASH_MAP_KEY_FROM_ENTRY
#undef SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS

#undef SOL_HASH_MAP_IDENTIFIER_OFFSET_SHIFT
#undef SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_HASH_BIT_COUNT
#undef SOL_HASH_MAP_IDENTIFIER_OFFSET_UNIT
#undef SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_HASH_MASK
#undef SOL_HASH_MAP_IDENTIFIER_MAXIMUM_DISPLACEMENT_CAPACITY
#undef SOL_HASH_MAP_IDENTIFIER_MINIMUM_DISPLACEMENT_CAPACITY

#undef SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL_CONTEXT
#undef SOL_HASH_MAP_KEY_FROM_ENTRY_CONTEXT
#undef SOL_HASH_MAP_KEY_HASH_CONTEXT

#ifdef SOL_HASH_MAP_CONTEXT_TYPE
#undef SOL_HASH_MAP_CONTEXT_TYPE
#endif

