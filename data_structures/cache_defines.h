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
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>

#include "sol_utils.h"

enum sol_cache_result
{
    SOL_CACHE_SUCCESS_FOUND    = 0, /** existing entry with key found and returned */
    SOL_CACHE_SUCCESS_INSERTED = 1, /** no entry with existing key is in cache, but there is still space, so an uninitialised entry was returned */
    SOL_CACHE_SUCCESS_REPLACED = 2, /** no entry with existing key is in cache, and the entry pointed to must be replaced */
    SOL_CACHE_FAIL_ABSENT      = 3, /** key not found in map */
    SOL_CACHE_RESULT_END       = 4,
};

struct sol_cache_link_u16
{
    uint16_t older;
    uint16_t newer;
};







