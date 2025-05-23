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

#pragma once

#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>


// need hash map variants that can have their key as competely separate and those that cannot (or as a mix of the two)
// ^ actually no, default types will be a structured
// should probably resemble lockfree algorithms in terms of structure (generic typing, not done via macro implementation &c.)
// will be used for dynamic buffer locations as well as image atlas locations

// NOTE: intention is that key-entries are polymorphic for comparison(access/insert) purposes
// this way a single pointer can represent both keys and entries

struct sol_hash_map;

void sol_hash_map_initialise(struct sol_hash_map* map, size_t initial_size_exponent, size_t entry_size, uint64_t(*hash_function)(void*), int(*compare_function)(void*, void*));
void sol_hash_map_terminate(struct sol_hash_map* map);

void sol_hash_map_clear(struct sol_hash_map* map);

// assumes entry does not exist, returns false if (equivalent) entry already present (in which case it will copy contents)
bool sol_hash_map_entry_insert(struct sol_hash_map* map, void* key_entry);

// searches for key; then if found sets entry to allow direct access to it (if entry non null) then returns true
// pointer returned with entry becomes invalid after any other map functions are executed
bool sol_hash_map_entry_find(struct sol_hash_map* map, void* key, void** entry_access);

// find that upon failure assigns space based key, returns true if insertion was performed
// pointer returned with entry becomes invalid after any other map functions are executed
// "find or insert"
bool sol_hash_map_entry_obtain(struct sol_hash_map* map, void* key, void** entry_access);

// if key found copies it into entry (if entry nonnull) then returns true
// effectively: find() and delete() with (conditional) memcpy() in between
bool sol_hash_map_entry_remove(struct sol_hash_map* map, void* key, void* entry);

// entry MUST be EXACTLY the result of a prior find() with no other map functions called in between, will directly remove an entry from the map if it exists
void sol_hash_map_entry_delete(struct sol_hash_map* map, void* entry_ptr);

// need an indirect variant that allows entries of variant sizes or large sized with separate backing and hash map buffers
// if storing (unlimited) strings for text then this will be required