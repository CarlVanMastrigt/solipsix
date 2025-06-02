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

#include <stdio.h>

#include <assert.h>

#include "sol_utils.h"
#include "data_structures/hash_map.h"




#define SOL_HASH_MAP_IDENTIFIER_EXIST_BIT 0x8000
#define SOL_HASH_MAP_IDENTIFIER_HASH_MASK 0x7FFF
// note: top bit being set indicates it's empty
#define SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS 8
#define SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS 7

// if this bit is zero when subtracing the identifier of the key we're searching for <k>
// from the existing (set) identifier we're comparing to <c>
// [i.e. ((c-k) & SOL_HASH_MAP_DELTA_TEST_BIT) == 0 ]
// then <k> is greater than or equal to <c>
// note: this only holds if the maximum offset (below) of the hash map is maintained/respected
#define SOL_HASH_MAP_DELTA_TEST_BIT 0x4000
#define SOL_HASH_MAP_INDEX_DELTA_TEST_BIT 0x0080
// ^ is (SOL_HASH_MAP_DELTA_TEST_BIT >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS)

// maximum offset must use 1 bit less than the largest index identifier
// if offset from actual location is equal to this (or greater than) then the necessary condition imposed on identifiers has been violated
#define SOL_HASH_MAP_INVALID_OFFSET 0x80

// in order to correctly compare, we must ensure this bit is set in the comparison index value to wrap comparisons correctly
// #define SOL_HASH_MAP_IDENTIFIER_EXCLUSION_BIT 0x4000

#define SOL_HASH_MAP_IDENTIFIER_INDEX_MASK 0xFF

// #define SOL_HASH_MAP_IDENTIFIER_EXTRACT_INDEX_BITS(v) (((v) >> SOL_HASH_MAP_IDENTIFIER_FRACTIONAL_BITS) & SOL_HASH_MAP_IDENTIFIER_INDEX_MASK)
// ^ remove fractional bits and top bit (this is for checking that the maximum offset is not excceded)

// #define SOL_HASH_MAP_IDENTIFIER_SEARCH_STOP(k, c) ( (c & SOL_HASH_MAP_IDENTIFIER_EXIST_BIT) == 0  ||  ((c-k) & SOL_HASH_MAP_DELTA_TEST_BIT) == 0 )

// function equivalent of macro provided for convenience
// static inline bool sol_hash_map_identifier_search_stop(uint16_t key_identifier, uint16_t compare_identifier)
// {
// 	// stop searching if the identifier to check against does not exist (empty slot)
// 	// or the key is greater than or equal to the identifier to check against (i.e. didnt wrap)
// 	// note: it's not necessary to excluse the top (set) bit, as it will be present key and must exist in compare_identifier if the first condition isn't satisfied
// 	return (compare_identifier & SOL_HASH_MAP_IDENTIFIER_EXIST_BIT) == 0  ||  ((compare_identifier-key_identifier) & SOL_HASH_MAP_DELTA_TEST_BIT) == 0;
// }

static inline bool sol_hash_map_identifier_exists_and_ordered_before_key(uint16_t key_identifier, uint16_t compare_identifier)
{
	return compare_identifier && ((compare_identifier-key_identifier) & SOL_HASH_MAP_DELTA_TEST_BIT);
	// return (compare_identifier & SOL_HASH_MAP_IDENTIFIER_EXIST_BIT) && ((compare_identifier-key_identifier) & SOL_HASH_MAP_DELTA_TEST_BIT);
}

static inline bool sol_hash_map_identifier_exists_and_ordered_after_key(uint16_t key_identifier, uint16_t compare_identifier)
{
	return compare_identifier && ((compare_identifier-key_identifier) & SOL_HASH_MAP_DELTA_TEST_BIT) == 0;
	// return (compare_identifier & SOL_HASH_MAP_IDENTIFIER_EXIST_BIT) && ((compare_identifier-key_identifier) & SOL_HASH_MAP_DELTA_TEST_BIT) == 0;
}

static inline bool sol_hash_map_identifier_not_ordered_after_key(uint16_t key_identifier, uint16_t compare_identifier)
{
	return ((key_identifier-compare_identifier) & SOL_HASH_MAP_DELTA_TEST_BIT) == 0;
}

// identifier must be greater than this to be placeable at index
static inline bool sol_hash_map_identifier_exists_and_valid_at_index(uint64_t index, uint_fast16_t identifier)
{
	uint_fast16_t index_identifier_threshold = (index & SOL_HASH_MAP_IDENTIFIER_INDEX_MASK);
	identifier = identifier >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS;
	return identifier && ((index_identifier_threshold - identifier) & SOL_HASH_MAP_INDEX_DELTA_TEST_BIT) == 0;
}

// an offset will only become invalid due to map saturation
static inline bool sol_hash_map_identifier_offset_invalid(uint64_t index, uint_fast16_t identifier)
{
	index = index & SOL_HASH_MAP_IDENTIFIER_INDEX_MASK;
	identifier = ((identifier >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) + SOL_HASH_MAP_INVALID_OFFSET) & SOL_HASH_MAP_IDENTIFIER_INDEX_MASK;
	return index == identifier;
}

static inline bool sol_hash_map_identifier_valid_for(uint64_t index, uint_fast16_t identifier)
{
	index = index & SOL_HASH_MAP_IDENTIFIER_INDEX_MASK;
	identifier = ((identifier >> SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS) + SOL_HASH_MAP_INVALID_OFFSET) & SOL_HASH_MAP_IDENTIFIER_INDEX_MASK;
	return index == identifier;
}

void sol_hash_map_initialise(struct sol_hash_map* map, size_t initial_size_exponent, size_t maximum_size_exponent, size_t entry_size, uint64_t(*hash_function)(const void*), bool(*compare_equal_function)(const void*, const void*))
{
	uint64_t i;

	initial_size_exponent = SOL_MAX(SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS, initial_size_exponent);
	maximum_size_exponent = SOL_MAX(SOL_HASH_MAP_IDENTIFIER_HASH_INDEX_BITS, maximum_size_exponent);
	assert(maximum_size_exponent < 32);// beyond this is probably unreasonable, used to catch this accidentally being treated as raw COUNT rather than power of 2
	assert(initial_size_exponent < maximum_size_exponent);

	map->entries = malloc(entry_size << initial_size_exponent);
	map->identifiers = malloc(sizeof(uint16_t) << initial_size_exponent);
	memset(map->identifiers, 0x00, sizeof(uint16_t) << initial_size_exponent);

	map->entry_space_exponent = initial_size_exponent;
	map->entry_space_exponent_limit = maximum_size_exponent;
	map->entry_count = 0;
	map->entry_size = entry_size;
	map->hash_function = hash_function;
	map->compare_equal_function = compare_equal_function;
}

void sol_hash_map_terminate(struct sol_hash_map* map)
{
	free(map->entries);
	free(map->identifiers);
}

void sol_hash_map_clear(struct sol_hash_map* map)
{
	map->entry_count = 0;
	memset(map->identifiers, 0x00, sizeof(uint16_t)<<map->entry_space_exponent);
	// ^ will set all identifiers to SOL_HASH_MAP_EMPTY_IDENTIFIER
}

static inline void* sol_hash_map_access_entry(struct sol_hash_map* map, uint64_t entry_index)
{
	return (char*)map->entries + (map->entry_size * entry_index);
}

static inline void sol_hash_map_resize(struct sol_hash_map* map)
{
	char* old_entries = map->entries;
	uint16_t* old_identifiers = map->identifiers;

	const uint64_t old_entry_space = (uint64_t)1 << map->entry_space_exponent;

	const uint64_t entry_space = (uint64_t)1 << (map->entry_space_exponent + 1);
	const uint64_t index_mask = entry_space - 1;

	uint64_t key_hash, index, old_index, prev_index, key_index;
	uint16_t key_identifier;
	const void* key_entry;

	map->entry_space_exponent++;
	map->entry_count = 0;
	map->entries = malloc(map->entry_size << map->entry_space_exponent);
	map->identifiers = malloc(sizeof(uint16_t) << map->entry_space_exponent);
	memset(map->identifiers, 0x00, sizeof(uint16_t) << map->entry_space_exponent);


	for(old_index = 0; old_index < old_entry_space; old_index++)
	{
		if(old_identifiers[old_index])
		{
			key_entry = old_entries + map->entry_size * old_index;

			key_hash = map->hash_function(key_entry);

			key_index = key_hash >> (64 - map->entry_space_exponent);/// implicitly masked to index_mask
			key_identifier = (key_hash >> (64 - map->entry_space_exponent - SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS)) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;
			// ^ get the last SOL_HASH_MAP_INDEX_BITS of the hash's index with SOL_HASH_MAP_FRACTIONAL_BITS bits following it, also set "exist" bit

			index = key_index;
			while(map->identifiers[index])
			{
				index = (index + 1) & index_mask;
			}

			while(index != key_index)
			{
				prev_index = (index - 1) & index_mask;
				// at this point prev must exist, only need to check ordering
				if(sol_hash_map_identifier_not_ordered_after_key(key_identifier, map->identifiers[prev_index]))
				{
					// code past here is very rarely run as input is already ordered quite well
					break;
				}
				// make space in new array
				memcpy(sol_hash_map_access_entry(map, index), sol_hash_map_access_entry(map, prev_index), map->entry_size);
				map->identifiers[index] = map->identifiers[prev_index];
				index = prev_index;
			}

			// add entry
			memcpy(sol_hash_map_access_entry(map, index), key_entry, map->entry_size);
			map->identifiers[index] = key_identifier;
		}
	}

	free(old_identifiers);
	free(old_entries);
}

// if key is found return false, otherwise return the location to insert the new entry
// index = key_hash >> (64 - map->entry_space_exponent);
// key_identifier = (key_hash >> (64 - map->entry_space_exponent - SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS)) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;
static inline bool sol_hash_map_entry_locate(struct sol_hash_map* map, void* key, uint16_t key_identifier, uint64_t index, uint64_t* index_result)
{
	uint64_t entry_space = (uint64_t)1 << map->entry_space_exponent;
	uint64_t index_mask = entry_space - 1;
	void* entry_ptr;

	while(sol_hash_map_identifier_exists_and_ordered_before_key(key_identifier, map->identifiers[index]))
	{
		index = (index + 1) & index_mask;
	}

	while(key_identifier == map->identifiers[index])
	{
		entry_ptr = sol_hash_map_access_entry(map, index);
		if(map->compare_equal_function(key, entry_ptr))
		{
			// the entries match! it has been found
			*index_result = index;
			return true;// precise entry found
		}
		// implicit else
		index = (index + 1) & index_mask;
	}

	*index_result = index;
	return false;// insertion required
}

enum sol_map_result sol_hash_map_entry_find(struct sol_hash_map* map, void* key, void** entry_access)
{
	const uint64_t key_hash = map->hash_function(key);

	const uint16_t key_identifier = (key_hash >> (64 - map->entry_space_exponent - SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS)) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;
	uint64_t index = key_hash >> (64 - map->entry_space_exponent);

	if(sol_hash_map_entry_locate(map, key, key_identifier, index, &index ))
	{
		*entry_access = sol_hash_map_access_entry(map, index);
		return SOL_MAP_SUCCESS_FOUND;
	}

	*entry_access = NULL;
	return SOL_MAP_FAIL_ABSENT;
}

enum sol_map_result sol_hash_map_entry_insert(struct sol_hash_map* map, void* key_entry)
{
	void* entry_ptr;
	enum sol_map_result result;

	result = sol_hash_map_entry_obtain(map, key_entry, &entry_ptr);

	switch (result)
	{
	case SOL_MAP_SUCCESS_FOUND:// we are replacing this value, ergo SOL_MAP_SUCCESS_FOUND == SOL_MAP_SUCCESS_REPLACED
	case SOL_MAP_SUCCESS_INSERTED:
		memcpy(entry_ptr, key_entry, map->entry_size);
	default:
		break;
	}
	return result;
}

enum sol_map_result sol_hash_map_entry_obtain(struct sol_hash_map* map, void* key, void** entry_access)
{
	const uint64_t key_hash = map->hash_function(key);

	uint64_t entry_space, index_mask, key_index, move_index, next_move_index, prev_move_index;
	uint16_t key_identifier, move_identifier;
	void* entry_ptr;

	entry_space = (uint64_t)1 << map->entry_space_exponent;

	// very unlikely, needed bacause map could be in a vlaid state while fully saturated, in which case the search for a vlaid slot would repeat indefinitely
	if(map->entry_count == entry_space)
	{
		if(map->entry_space_exponent == map->entry_space_exponent_limit)
		{
			*entry_access = NULL;
			return SOL_MAP_FAIL_FULL;
		}
		else
		{
			sol_hash_map_resize(map);
		}
	}

	sol_hash_map_entry_obtain_for_map_size:
	{
		entry_space = (uint64_t)1 << map->entry_space_exponent;
		index_mask = entry_space - 1;

		key_index = key_hash >> (64 - map->entry_space_exponent);/// implicitly masked to index_mask
		// max_index = (index + SOL_HASH_MAP_MAXIMUM_OFFSET) & index_mask;

		key_identifier = (key_hash >> (64 - map->entry_space_exponent - SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS)) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;
		// ^ get the last SOL_HASH_MAP_INDEX_BITS of the hash's index with SOL_HASH_MAP_FRACTIONAL_BITS bits following it, also set "exist" bit

		if(sol_hash_map_entry_locate(map, key, key_identifier, key_index, &key_index))
		{
			*entry_access = sol_hash_map_access_entry(map, key_index);
			return SOL_MAP_SUCCESS_FOUND;
		}

		// need to make space to add the new entry
		move_identifier = key_identifier;
		next_move_index = key_index;// where the identifier will end up

		do // initially checking that the entry being added is being added at a valid location
		{
			// break condition, moved/added entry would exceed maximum valid offset for it's identifier
			if(sol_hash_map_identifier_offset_invalid(next_move_index, move_identifier))
			{
				if(map->entry_space_exponent == map->entry_space_exponent_limit)
				{
					*entry_access = NULL;
					return SOL_MAP_FAIL_FULL;
				}
				else
				{
					sol_hash_map_resize(map);
					goto sol_hash_map_entry_obtain_for_map_size;
				}
			}
			// note: yes, getting the identifier BEFORE changing the move_index, as its necessary to check that the new index is valid for every identifier
			move_index = next_move_index;
			move_identifier = map->identifiers[move_index];
			next_move_index = (move_index + 1) & index_mask;
		}
		while(move_identifier);// searching for a valid location to add

		// this could be a (fairly complicated) memove instead... (would need to respect the potential for wrapping the buffer length)
		while(move_index != key_index)
		{
			prev_move_index = (move_index - 1) & index_mask;
			memcpy(sol_hash_map_access_entry(map, move_index), sol_hash_map_access_entry(map, prev_move_index), map->entry_size);
			map->identifiers[move_index] = map->identifiers[prev_move_index];
			move_index = prev_move_index;
		}
		*entry_access = sol_hash_map_access_entry(map, key_index);
		map->identifiers[key_index] = key_identifier;
		map->entry_count++;

		return SOL_MAP_SUCCESS_INSERTED;
	}
}

static inline void sol_hash_map_entry_evict_index(struct sol_hash_map* map, uint64_t index)
{
	uint64_t entry_space, index_mask, next_index;
	uint16_t identifier, threshold_identifier;

	threshold_identifier =
	entry_space = (uint64_t)1 << map->entry_space_exponent;
	index_mask = entry_space - 1;

	sol_hash_map_entry_evict_index_shift_next_entry_backwards:
	{
		//sol_hash_map_identifier_exists_and_ordered_after_key
		next_index = (index + 1) & index_mask;
		identifier = map->identifiers[next_index];
		if(sol_hash_map_identifier_exists_and_valid_at_index(index, identifier))
		{
			memcpy(sol_hash_map_access_entry(map, index), sol_hash_map_access_entry(map, next_index), map->entry_size);
			map->identifiers[index] = map->identifiers[next_index];
			index = next_index;
			goto sol_hash_map_entry_evict_index_shift_next_entry_backwards;
		}
	}

	map->identifiers[index] = 0;// mark the first identifier that cannot be shifted backwards as empty

	map->entry_count--;
}

enum sol_map_result sol_hash_map_entry_remove(struct sol_hash_map* map, void* key, void* entry)
{
	const uint64_t key_hash = map->hash_function(key);

	const uint16_t key_identifier = (key_hash >> (64 - map->entry_space_exponent - SOL_HASH_MAP_IDENTIFIER_HASH_FRACTIONAL_BITS)) | SOL_HASH_MAP_IDENTIFIER_EXIST_BIT;
	uint64_t index = key_hash >> (64 - map->entry_space_exponent);

	if(sol_hash_map_entry_locate(map, key, key_identifier, index, &index ))
	{
		if(entry)
		{
			memcpy(entry, sol_hash_map_access_entry(map, index), map->entry_size);
		}
		sol_hash_map_entry_evict_index(map, index);
		return SOL_MAP_SUCCESS_REMOVED;
	}

	return SOL_MAP_FAIL_ABSENT;
}

void sol_hash_map_entry_delete(struct sol_hash_map* map, void* entry_ptr)
{
	const uint64_t index = ((char*)entry_ptr - (char*)map->entries) / map->entry_size;
	sol_hash_map_entry_evict_index(map, index);
}