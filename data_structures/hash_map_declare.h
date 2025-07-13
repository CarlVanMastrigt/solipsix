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
#define KEY_TYPE int
#endif

#ifndef ENTRY_TYPE
#error must define ENTRY_TYPE
#define ENTRY_TYPE int
#endif

#ifndef FUNCTION_KEYWORDS
#define FUNCTION_KEYWORDS
#endif

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

#ifdef CONTEXT_TYPE
FUNCTION_KEYWORDS void SOL_CONCATENATE(FUNCTION_PREFIX,_initialise)(struct STRUCT_NAME* map, uint8_t initial_entry_space_exponent, struct sol_hash_map_descriptor descriptor, CONTEXT_TYPE context);
#else
FUNCTION_KEYWORDS void SOL_CONCATENATE(FUNCTION_PREFIX,_initialise)(struct STRUCT_NAME* map, uint8_t initial_entry_space_exponent, struct sol_hash_map_descriptor descriptor);
#endif

FUNCTION_KEYWORDS void SOL_CONCATENATE(FUNCTION_PREFIX,_terminate)(struct STRUCT_NAME* map);

/** completely empties the hash map, effectively removing all entries */
FUNCTION_KEYWORDS void SOL_CONCATENATE(FUNCTION_PREFIX,_clear)(struct STRUCT_NAME* map);

/** searches for key; then if found sets entry to allow direct access to it (if entry non null) then returns true
 *  pointer returned with entry becomes invalid after any other map functions are executed */
FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(FUNCTION_PREFIX,_find)(struct STRUCT_NAME* map, KEY_TYPE* key, ENTRY_TYPE** entry_ptr);

/** find that upon failure assigns space based key, returns true if insertion was performed
 *  pointer returned with entry becomes invalid after any other map functions are executed
 *  effectively "find or insert" */
FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(FUNCTION_PREFIX,_obtain)(struct STRUCT_NAME* map, KEY_TYPE* key, ENTRY_TYPE** entry_ptr);

/** assumes entry does not exist, returns false if (equivalent) entry already present (in which case it will copy contents) */
FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(FUNCTION_PREFIX,_insert)(struct STRUCT_NAME* map, ENTRY_TYPE* entry);

/** if key found copies it into entry (if entry nonnull) then returns true
 *  effectively: find() and delete() with (conditional) memcpy() in between */
FUNCTION_KEYWORDS enum sol_map_result SOL_CONCATENATE(FUNCTION_PREFIX,_remove)(struct STRUCT_NAME* map, KEY_TYPE* key, ENTRY_TYPE* entry);

/** entry MUST be EXACTLY the result of a prior successful `find()` or `obtain()` with no other map functions called in between
 *  will directly remove an entry from the map if it exists */
FUNCTION_KEYWORDS void SOL_CONCATENATE(FUNCTION_PREFIX,_delete_entry)(struct STRUCT_NAME* map, ENTRY_TYPE* entry_ptr);


#undef STRUCT_NAME
#undef FUNCTION_PREFIX
#undef KEY_TYPE
#undef ENTRY_TYPE
#undef FUNCTION_KEYWORDS

#ifdef CONTEXT_TYPE
#undef CONTEXT_TYPE
#endif
