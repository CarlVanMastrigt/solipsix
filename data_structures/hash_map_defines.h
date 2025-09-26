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

/** all includes put here instead of implement file to take advantage of pragma once */
#include <limits.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>

#include "sol_utils.h"

#warning rename this to sol_map_result_type
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
    uint8_t entry_space_exponent_initial;/** 2 ^ exponent_imit = max hash map size */
    uint8_t entry_space_exponent_limit;/** 2 ^ exponent_imit = max hash map size */
    uint8_t resize_fill_factor;/** out of 256 */
    uint8_t limit_fill_factor;/** out of 256 */
};


#define SOL_HASH_MAP_IDENTIFIER_EXIST_BIT 0x0001
#define SOL_HASH_MAP_DELTA_TEST_BIT 0x8000




