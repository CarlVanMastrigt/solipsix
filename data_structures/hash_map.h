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

#pragma once

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>


enum sol_map_result
{
	SOL_MAP_FAIL_FULL = 0,//could not add element because map is full (at least in the hash space region for the key being inserted)
	SOL_MAP_FAIL_ABSENT = 0,// key not found in map
	SOL_MAP_SUCCESS_FOUND = 1,
	SOL_MAP_SUCCESS_REPLACED = 1,
	SOL_MAP_SUCCESS_INSERTED = 2,
	SOL_MAP_SUCCESS_REMOVED = 3,
};

struct sol_hash_map_descriptor
{
	uint8_t entry_space_exponent_limit;
	uint8_t resize_fill_factor;// out of 256
	uint8_t limit_fill_factor;// out of 256
};



#define SOL_HASH_MAP_IDENTIFIER_EXIST_BIT 0x0001
// note: top bit being set indicates it's empty
#define SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS 8
#define SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_SHIFT 8
#define SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS (SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_SHIFT-1)
// ^ bottom fractional bit will be repurposed (SOL_HASH_MAP_IDENTIFIER_EXIST_BIT)

// if this bit is zero when subtracing the identifier of the key we're searching for <k>
// from the existing (set) identifier we're comparing to <c>
// [i.e. ((c-k) & SOL_HASH_MAP_DELTA_TEST_BIT) == 0 ]
// then <k> is greater than or equal to <c>
// note: this only holds if the maximum offset (below) of the hash map is maintained/respected
#define SOL_HASH_MAP_DELTA_TEST_BIT 0x8000

// maximum offset must use 1 bit less than the largest index identifier
// if offset from actual location is equal to this (or greater than) then the necessary condition imposed on identifiers has been violated
// #define SOL_HASH_MAP_INVALID_OFFSET 128
#define SOL_HASH_MAP_INVALID_OFFSET (1 << (SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS - 1))

#define SOL_HASH_MAP_IDENTIFIER_INDEX_MASK ((1 << SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS) - 1)

static inline bool sol_hash_map_identifier_exists_and_ordered_before_key(uint16_t key_identifier, uint16_t compare_identifier)
{
	return compare_identifier && ((compare_identifier-key_identifier) & SOL_HASH_MAP_DELTA_TEST_BIT);
}

static inline bool sol_hash_map_identifier_not_ordered_after_key(uint16_t key_identifier, uint16_t compare_identifier)
{
	return ((key_identifier-compare_identifier) & SOL_HASH_MAP_DELTA_TEST_BIT) == 0;
}

static inline bool sol_hash_map_identifier_exists_and_can_move_backwards(uint64_t current_index, uint_fast16_t identifier)
{
	return identifier && (current_index & SOL_HASH_MAP_IDENTIFIER_INDEX_MASK) != (identifier >> SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_SHIFT);
}

// can we move an identifier to this index? an offset will only become invalid due to map saturation
// this check only works if entries are moved one index/place at a time
// is basically used to check whether an entry can be moved forward one place
static inline bool sol_hash_map_identifier_invalid_at_new_index(uint64_t new_index, uint_fast16_t identifier)
{
	new_index = (new_index - SOL_HASH_MAP_INVALID_OFFSET) & SOL_HASH_MAP_IDENTIFIER_INDEX_MASK;
	identifier = identifier >> SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_SHIFT;
	return new_index == identifier;
}



#define SOL_HASH_MAP_DECLARATION(entry_type, key_type, struct_name, function_prefix)                                                            \
                                                                                                                                                \
struct struct_name                                                                                                                              \
{                                                                                                                                               \
	struct sol_hash_map_descriptor descriptor;                                                                                                  \
                                                                                                                                                \
	uint8_t entry_space_exponent;                                                                                                               \
                                                                                                                                                \
	entry_type* entries;                                                                                                                        \
	uint16_t* identifiers;                                                                                                                      \
                                                                                                                                                \
	uint64_t entry_count;                                                                                                                       \
	uint64_t entry_limit;                                                                                                                       \
};                                                                                                                                              \
                                                                                                                                                \
void function_prefix##_initialise(struct struct_name* map, uint8_t initial_entry_space_exponent, struct sol_hash_map_descriptor descriptor)     \
{                                                                                                                                               \
	assert(descriptor.entry_space_exponent_limit <= 32-SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS);                                           \
	/** beyond this is probably unreasonable, used to catch this accidentally being treated as raw COUNT rather than power of 2 */              \
	assert(initial_entry_space_exponent <= descriptor.entry_space_exponent_limit);                                                              \
	assert(initial_entry_space_exponent >= SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS);                                                            \
	assert(initial_entry_space_exponent >= 8);/* must have exponent greater than uint8_t for all bits in fill limit to be significant */        \
                                                                                                                                                \
	map->descriptor = descriptor;                                                                                                               \
                                                                                                                                                \
	map->entries = malloc(sizeof(entry_type) << initial_entry_space_exponent);                                                                  \
	map->identifiers = malloc(sizeof(uint16_t) << initial_entry_space_exponent);                                                                \
                                                                                                                                                \
	map->entry_space_exponent = initial_entry_space_exponent;                                                                                   \
	map->entry_count = 0;                                                                                                                       \
                                                                                                                                                \
	if(initial_entry_space_exponent == descriptor.entry_space_exponent_limit)                                                                   \
	{                                                                                                                                           \
		map->entry_limit = ((uint64_t)descriptor.limit_fill_factor << (initial_entry_space_exponent - 8));                                      \
	}                                                                                                                                           \
	else                                                                                                                                        \
	{                                                                                                                                           \
		map->entry_limit = ((uint64_t)descriptor.resize_fill_factor << (initial_entry_space_exponent - 8));                                     \
	}                                                                                                                                           \
                                                                                                                                                \
	memset(map->identifiers, 0x00, sizeof(uint16_t) << initial_entry_space_exponent);                                                           \
}                                                                                                                                               \
                                                                                                                                                \
static inline void function_prefix##_terminate(struct struct_name* map)                                                                         \
{                                                                                                                                               \
	free(map->entries);                                                                                                                         \
	free(map->identifiers);                                                                                                                     \
}                                                                                                                                               \
                                                                                                                                                \
static inline void function_prefix##_clear(struct struct_name* map)                                                                             \
{                                                                                                                                               \
	map->entry_count = 0;                                                                                                                       \
	memset(map->identifiers, 0x00, sizeof(uint16_t) << map->entry_space_exponent);                                                              \
}                                                                                                                                               \
                                                                                                                                                \
/** searches for key; then if found sets entry to allow direct access to it (if entry non null) then returns true                               \
 *  pointer returned with entry becomes invalid after any other map functions are executed */                                                   \
enum sol_map_result function_prefix##_find(struct struct_name* map, key_type* key, entry_type** entry_ptr);                                     \
                                                                                                                                                \
/** find that upon failure assigns space based key, returns true if insertion was performed                                                     \
 *  pointer returned with entry becomes invalid after any other map functions are executed                                                      \
 *  effectively "find or insert" */                                                                                                             \
enum sol_map_result function_prefix##_obtain(struct struct_name* map, key_type* key, entry_type** entry_ptr);                                   \
                                                                                                                                                \
/** assumes entry does not exist, returns false if (equivalent) entry already present (in which case it will copy contents) */                  \
enum sol_map_result function_prefix##_insert(struct struct_name* map, entry_type* entry);                                                       \
                                                                                                                                                \
/** if key found copies it into entry (if entry nonnull) then returns true                                                                      \
 *  effectively: find() and delete() with (conditional) memcpy() in between */                                                                  \
enum sol_map_result function_prefix##_remove(struct struct_name* map, key_type* key, entry_type* entry);                                        \
                                                                                                                                                \
/** entry MUST be EXACTLY the result of a prior successful `find()` or `obtain()` with no other map functions called in between                 \
 *  will directly remove an entry from the map if it exists */                                                                                  \
void function_prefix##_delete_entry(struct struct_name* map, entry_type* entry_ptr);                                                            \


/** required to define:
 *  `SOL_HASH_MAP_DECLARATION` with the same parameters
 *  `SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(const key_type*, const entry_type*)` returning a bool
 *  `SOL_HASH_MAP_KEY_HASH(const key_type*)` returning a uint64_t
 *  `SOL_HASH_MAP_ENTRY_HASH(const entry_type*)` returning a uint64_t
 * 		^ the KEY_HASH and ENTRY_HASH must return the same hash for a given entry and key
 * 		^ they can be the same function taking `const void*` and acting on a polymorphic struct if desired
 * */
#define SOL_HASH_MAP_IMPLEMENTATION(entry_type, key_type, struct_name, function_prefix)                                                         \
                                                                                                                                                \
void function_prefix##_resize__i(struct struct_name* map)                                                                                       \
{                                                                                                                                               \
	uint64_t entry_hash, index, old_index, prev_index, entry_index;                                                                             \
	uint16_t identifier;                                                                                                                        \
	const entry_type* entry_ptr;                                                                                                                \
                                                                                                                                                \
	const uint64_t old_entry_space = (uint64_t)1 << map->entry_space_exponent;                                                                  \
                                                                                                                                                \
	map->entry_space_exponent++;                                                                                                                \
                                                                                                                                                \
	entry_type* old_entries = map->entries;                                                                                                     \
	entry_type* new_entries = malloc(sizeof(entry_type) << map->entry_space_exponent);                                                          \
	map->entries = new_entries;                                                                                                                 \
                                                                                                                                                \
	uint16_t* old_identifiers = map->identifiers;                                                                                               \
	uint16_t* new_identifiers = malloc(sizeof(uint16_t) << map->entry_space_exponent);                                                          \
	map->identifiers = new_identifiers;                                                                                                         \
	memset(new_identifiers, 0x00, sizeof(uint16_t) << map->entry_space_exponent);                                                               \
                                                                                                                                                \
	if(map->entry_space_exponent == map->descriptor.entry_space_exponent_limit)                                                                 \
	{                                                                                                                                           \
		map->entry_limit = ((uint64_t)map->descriptor.limit_fill_factor << (map->entry_space_exponent-8));                                      \
	}                                                                                                                                           \
	else                                                                                                                                        \
	{                                                                                                                                           \
		map->entry_limit = ((uint64_t)map->descriptor.resize_fill_factor << (map->entry_space_exponent-8));                                     \
	}                                                                                                                                           \
                                                                                                                                                \
	const uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;                                                                      \
	const uint64_t index_mask = entry_space - 1;                                                                                                \
                                                                                                                                                \
	for(old_index = 0; old_index < old_entry_space; old_index++)                                                                                \
	{                                                                                                                                           \
		identifier = old_identifiers[old_index];                                                                                                \
		if(identifier)                                                                                 	                                        \
		{                                                                                                                                       \
			entry_ptr = old_entries + old_index;                                                                                                \
			entry_hash = SOL_HASH_MAP_ENTRY_HASH(entry_ptr);                                                                                    \
			entry_index = (entry_hash >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) & index_mask;                                            \
			index = entry_index;                                                                                                                \
                                                                                                                                                \
			while(new_identifiers[index])                                                                                                       \
			{                                                                                                                                   \
				index = (index==index_mask) ? 0 : index+1;                                                                                      \
			}                                                                                                                                   \
                                                                                                                                                \
			while(index != entry_index)                                                                                                         \
			{                                                                                                                                   \
				prev_index = (index==0) ? index_mask : index-1;                                                                                 \
				/** at this point prev must exist, only need to check ordering */                                                               \
				if(sol_hash_map_identifier_not_ordered_after_key(identifier, new_identifiers[prev_index]))                                      \
				{                                                                                                                               \
					break;                                                                                                                      \
				}                                                                                                                               \
				new_entries    [index] = new_entries    [prev_index];                                                                           \
				new_identifiers[index] = new_identifiers[prev_index];                                                                           \
				index = prev_index;                                                                                                             \
			}                                                                                                                                   \
                                                                                                                                                \
			new_entries    [index] = *entry_ptr;                                                                                                \
			new_identifiers[index] = identifier;                                                       	                                        \
		}                                                                                                                                       \
	}                                                                                                                                           \
                                                                                                                                                \
	free(old_identifiers);                                                                                                                      \
	free(old_entries);                                                                                                                          \
}                                                                                                                                               \
                                                                                                                                                \
bool function_prefix##_locate__i(struct struct_name* map, key_type* key_ptr, uint16_t identifier, uint64_t index, uint64_t* index_result)       \
{                                                                                                                                               \
	uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;                                                                            \
	uint64_t index_mask = entry_space - 1;                                                                                                      \
	uint16_t* identifiers = map->identifiers;                                                                                                   \
	entry_type* entries = map->entries;                                                                                                         \
                                                                                                                                                \
	while(sol_hash_map_identifier_exists_and_ordered_before_key(identifier, identifiers[index]))                                                \
	{                                                                                                                                           \
		index = (index==index_mask) ? 0 : index+1;                                                                                              \
	}                                                                                                                                           \
                                                                                                                                                \
	while(identifier == identifiers[index])                                                                                                     \
	{                                                                                                                                           \
		if(SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(key_ptr, entries + index))                                                                          \
		{                                                                                                                                       \
			*index_result = index;                                                                                                              \
			return true;/** precise key/entry found */                                                                                          \
		}                                                                                                                                       \
		index = (index==index_mask) ? 0 : index+1;                                                                                              \
	}                                                                                                                                           \
                                                                                                                                                \
	*index_result = index;                                                                                                                      \
	return false;/** insertion required at index */                                                                                             \
}                                                                                                                                               \
                                                                                                                                                \
static inline void function_prefix##_evict_index__i(struct struct_name* map, uint64_t index)                                                    \
{                                                                                                                                               \
	uint64_t entry_space, index_mask, next_index;                                                                                               \
	uint16_t identifier;                                                                                                                        \
                                                                                                                                                \
	entry_type* entries = map->entries;                                                                                                         \
	uint16_t* identifiers = map->identifiers;                                                                                                   \
                                                                                                                                                \
	entry_space = (uint64_t)1 << map->entry_space_exponent;                                                                                     \
	index_mask = entry_space - 1;                                                                                                               \
                                                                                                                                                \
	while(true)                                                                                                                                 \
	{                                                                                                                                           \
		next_index = (index==index_mask) ? 0 : index+1;                                                                                         \
		identifier = identifiers[next_index];                                                                                                   \
		if(sol_hash_map_identifier_exists_and_can_move_backwards(next_index, identifier))                                                       \
		{                                                                                                                                       \
			entries    [index] = entries[next_index];                                                                                           \
			identifiers[index] = identifier;                                                                                                    \
			index = next_index;                                                                                                                 \
		}                                                                                                                                       \
		else break;                                                                                                                             \
	}                                                                                                                                           \
	identifiers[index] = 0;                                                                                                                     \
	map->entry_count--;                                                                                                                         \
}                                                                                                                                               \
                                                                                                                                                \
enum sol_map_result function_prefix##_find(struct struct_name* map, key_type* key, entry_type** entry_ptr)                                      \
{                                                                                                                                               \
	const uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;                                                                      \
	const uint64_t index_mask = entry_space - 1;                                                                                                \
	const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH(key);                                                                                       \
	const uint16_t key_identifier = (key_hash << 1) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;                                                        \
	uint64_t index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) & index_mask;                                                   \
                                                                                                                                                \
	if(function_prefix##_locate__i(map, key, key_identifier, index, &index ))                                                                   \
	{                                                                                                                                           \
		*entry_ptr = map->entries + index;                                                                                                      \
		return SOL_MAP_SUCCESS_FOUND;                                                                                                           \
	}                                                                                                                                           \
                                                                                                                                                \
	*entry_ptr = NULL;                                                                                                                          \
	return SOL_MAP_FAIL_ABSENT;                                                                                                                 \
}                                                                                                                                               \
                                                                                                                                                \
enum sol_map_result function_prefix##_obtain(struct struct_name* map, key_type* key, entry_type** entry_ptr)                                    \
{                                                                                                                                               \
	const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH(key);                                                                                       \
	const uint16_t key_identifier = (key_hash << 1) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;                                                        \
                                                                                                                                                \
	uint64_t entry_space, index_mask, key_index, move_index, next_move_index, prev_move_index;                                                  \
	uint16_t move_identifier;                                                                                                                   \
                                                                                                                                                \
	entry_type* entries;                                                                                                                        \
	uint16_t* identifiers;                                                                                                                      \
                                                                                                                                                \
	if(map->entry_count == map->entry_limit)                                                                                                    \
	{                                                                                                                                           \
		if(map->entry_space_exponent == map->descriptor.entry_space_exponent_limit)                                                             \
		{                                                                                                                                       \
			*entry_ptr = NULL;                                                                                                                  \
			return SOL_MAP_FAIL_FULL;                                                                                                           \
		}                                                                                                                                       \
		else                                                                                                                                    \
		{                                                                                                                                       \
			function_prefix##_resize__i(map);                                                                                                   \
		}                                                                                                                                       \
	}                                                                                                                                           \
                                                                                                                                                \
	function_prefix##_obtain_for_map_size__goto:                                                                                                \
	{                                                                                                                                           \
		entry_space = (uint64_t)1 << map->entry_space_exponent;                                                                                 \
		index_mask = entry_space - 1;                                                                                                           \
                                                                                                                                                \
		key_index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) & index_mask;                                                    \
                                                                                                                                                \
		if(function_prefix##_locate__i(map, key, key_identifier, key_index, &key_index))                                                        \
		{                                                                                                                                       \
			*entry_ptr = map->entries + key_index;                                                                                              \
			return SOL_MAP_SUCCESS_FOUND;                                                                                                       \
		}                                                                                                                                       \
                                                                                                                                                \
		move_identifier = key_identifier;                                                                                                       \
		next_move_index = key_index;                                                                                                            \
		/** ^ note: initially checking that the entry being added is being added at a valid location */                                         \
		do                                                                                                                                      \
		{                                                                                                                                       \
			if(sol_hash_map_identifier_invalid_at_new_index(next_move_index, move_identifier))                                                  \
			{                                                                                                                                   \
				if(map->entry_space_exponent == map->descriptor.entry_space_exponent_limit)                                                     \
				{                                                                                                                               \
					*entry_ptr = NULL;                                                                                                          \
					return SOL_MAP_FAIL_FULL;                                                                                                   \
				}                                                                                                                               \
				else                                                                                                                            \
				{                                                                                                                               \
					function_prefix##_resize__i(map);                                                                                           \
					goto function_prefix##_obtain_for_map_size__goto;                                                                           \
				}                                                                                                                               \
			}                                                                                                                                   \
			move_index = next_move_index;                                                                                                       \
			move_identifier = map->identifiers[move_index];                                                                                     \
			next_move_index = (move_index==index_mask) ? 0 : move_index+1;                                                                      \
		}                                                                                                                                       \
		while(move_identifier);                                                                                                                 \
	}                                                                                                                                           \
                                                                                                                                                \
	entries = map->entries;                                                                                                                     \
	identifiers = map->identifiers;                                                                                                             \
                                                                                                                                                \
	/** shift everything forward one into the discovered empty slot */                                                                          \
	while(move_index != key_index)                                                                                                              \
	{                                                                                                                                           \
		prev_move_index = (move_index==0) ? index_mask : move_index-1;                                                                          \
		entries    [move_index] = entries    [prev_move_index];                                                                                 \
		identifiers[move_index] = identifiers[prev_move_index];                                                                                 \
		move_index = prev_move_index;                                                                                                           \
	}                                                                                                                                           \
                                                                                                                                                \
	*entry_ptr = entries + key_index;                                                                                                           \
	identifiers[key_index] = key_identifier;                                                                                                    \
	map->entry_count++;                                                                                                                         \
                                                                                                                                                \
	return SOL_MAP_SUCCESS_INSERTED;                                                                                                            \
}                                                                                                                                               \
                                                                                                                                                \
enum sol_map_result function_prefix##_insert(struct struct_name* map, entry_type* entry)                                                        \
{                                                                                                                                               \
	entry_type* entry_ptr;                                                                                                                      \
	enum sol_map_result result;                                                                                                                 \
                                                                                                                                                \
	result = function_prefix##_obtain(map, entry, &entry_ptr);                                                                                  \
                                                                                                                                                \
	switch (result)                                                                                                                             \
	{                                                                                                                                           \
	case SOL_MAP_SUCCESS_FOUND:/** note: SOL_MAP_SUCCESS_FOUND == SOL_MAP_SUCCESS_REPLACED */                                                   \
	case SOL_MAP_SUCCESS_INSERTED:                                                                                                              \
		*entry_ptr = *entry;                                                                                                                    \
	default:;                                                                                                                                   \
	}                                                                                                                                           \
	return result;                                                                                                                              \
}                                                                                                                                               \
                                                                                                                                                \
enum sol_map_result function_prefix##_remove(struct struct_name* map, key_type* key, entry_type* entry)                                         \
{                                                                                                                                               \
	const uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;                                                                      \
	const uint64_t index_mask = entry_space - 1;                                                                                                \
	const uint64_t key_hash = SOL_HASH_MAP_KEY_HASH(key);                                                                                       \
                                                                                                                                                \
	const uint16_t key_identifier = (key_hash << 1) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;                                                        \
	uint64_t index = (key_hash >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) & index_mask;                                                   \
                                                                                                                                                \
	if(function_prefix##_locate__i(map, key, key_identifier, index, &index ))                                                                   \
	{                                                                                                                                           \
		if(entry)                                                                                                                               \
		{                                                                                                                                       \
			*entry = map->entries[index];                                                                                                       \
		}                                                                                                                                       \
		function_prefix##_evict_index__i(map, index);                                                                                           \
		return SOL_MAP_SUCCESS_REMOVED;                                                                                                         \
	}                                                                                                                                           \
                                                                                                                                                \
	return SOL_MAP_FAIL_ABSENT;                                                                                                                 \
}                                                                                                                                               \
                                                                                                                                                \
void function_prefix##_delete_entry(struct struct_name* map, entry_type* entry_ptr)                                                             \
{                                                                                                                                               \
	const uint64_t index = entry_ptr - map->entries;                                                                                            \
	function_prefix##_evict_index__i(map, index);                                                                                               \
}                                                                                                                                               \


