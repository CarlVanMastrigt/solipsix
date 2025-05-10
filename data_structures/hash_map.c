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

#include <assert.h>

#include "data_structures/hash_map.h"


struct sol_hash_map
{
	void* entry_data;
	size_t entry_space;// number of entries
	size_t entry_count;// number of entries
	size_t entry_size;

	uint64_t (*hash_function)(void*);// hash a a key (key/entry)
	int (*compare_function)(void*, void*);// check whether 2 entries (with the same hash) are equal
	// above should either return int and entries with the same has
};