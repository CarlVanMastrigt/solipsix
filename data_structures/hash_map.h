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
#include <assert.h>


enum sol_map_result
{
    SOL_MAP_FAIL_FULL        = 0,/** could not add element because map is full (at least in the hash space region for the key being inserted) */
    SOL_MAP_FAIL_ABSENT      = 0,/** key not found in map */
    SOL_MAP_SUCCESS_FOUND    = 1,
    SOL_MAP_SUCCESS_REPLACED = 1,
    SOL_MAP_SUCCESS_INSERTED = 2,
    SOL_MAP_SUCCESS_REMOVED  = 3,
    SOL_MAP_RESULT_END       = 4,
};

struct sol_hash_map_descriptor
{
    uint8_t entry_space_exponent_limit;/** 2 ^ exponent_imit = max hash map size */
    uint8_t resize_fill_factor;/** out of 256 */
    uint8_t limit_fill_factor;/** out of 256 */
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




