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

/** maybe move this to utils folder with hash_map.h ? */


#pragma once

#define SOL_STACK_ENTRY_TYPE uint32_t
#define SOL_STACK_FUNCTION_PREFIX sol_available_indices_stack
#define SOL_STACK_STRUCT_NAME sol_available_indices_stack
#include "data_structures/stack.h"

