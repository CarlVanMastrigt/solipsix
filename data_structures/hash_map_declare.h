/**
Copyright 2025,2026 Carl van Mastrigt

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
#define SOL_HASH_MAP_FUNCTION_PREFIX SOL_HASH_MAP_STRUCT_NAME
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

#ifdef SOL_HASH_MAP_STRUCT_DEFINE
    struct SOL_HASH_MAP_STRUCT_NAME
    {
        struct sol_hash_map_descriptor descriptor;

        uint8_t entry_space_exponent;

        SOL_HASH_MAP_ENTRY_TYPE* entries;
        uint16_t* identifiers;

        #ifdef SOL_HASH_MAP_CONTEXT_TYPE
        SOL_HASH_MAP_CONTEXT_TYPE context;
        #endif

        uint64_t entry_count;
        uint64_t entry_limit;
    };

    /** only if providing a complete definition of the struct should initialise and terminate functions be provided */
    #ifdef SOL_HASH_MAP_CONTEXT_TYPE
    SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_initialise)(struct SOL_HASH_MAP_STRUCT_NAME* map, struct sol_hash_map_descriptor descriptor, SOL_HASH_MAP_CONTEXT_TYPE context);
    #else
    SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_initialise)(struct SOL_HASH_MAP_STRUCT_NAME* map, struct sol_hash_map_descriptor descriptor);
    #endif

    SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_terminate)(struct SOL_HASH_MAP_STRUCT_NAME* map);

    #undef SOL_HASH_MAP_STRUCT_DEFINE
#else
    struct SOL_HASH_MAP_STRUCT_NAME;
#endif

#ifdef SOL_HASH_MAP_CONTEXT_TYPE
SOL_HASH_MAP_FUNCTION_KEYWORDS struct SOL_HASH_MAP_STRUCT_NAME* SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_create)(struct sol_hash_map_descriptor descriptor, SOL_HASH_MAP_CONTEXT_TYPE context);
#else
SOL_HASH_MAP_FUNCTION_KEYWORDS struct SOL_HASH_MAP_STRUCT_NAME* SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_create)(struct sol_hash_map_descriptor descriptor);
#endif

SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_destroy)(struct SOL_HASH_MAP_STRUCT_NAME* map);

/** completely empties the hash map, effectively removing all entries */
SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(FUNCTION_PREFIX,_clear)(struct SOL_HASH_MAP_STRUCT_NAME* map);

/** searches for key; then if found sets entry to allow direct access to it (if entry non null) then returns true
 *  pointer returned with entry becomes invalid after any other map functions are executed */
SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_operation_result SOL_CONCATENATE(FUNCTION_PREFIX,_find)(const struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_KEY_TYPE key, SOL_HASH_MAP_ENTRY_TYPE** entry_ptr);

/** find that upon failure assigns space based key, returns true if insertion was performed
 *  pointer returned with entry becomes invalid after any other map functions are executed
 *  effectively "find or insert" */
SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_operation_result SOL_CONCATENATE(FUNCTION_PREFIX,_obtain)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_KEY_TYPE key, SOL_HASH_MAP_ENTRY_TYPE** entry_ptr);

/** assumes entry does not exist, returns false if (equivalent) entry already present (in which case it will copy contents) */
SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_operation_result SOL_CONCATENATE(FUNCTION_PREFIX,_insert)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_ENTRY_TYPE* entry);

/** if key found copies it into entry (if entry nonnull) then returns true
 *  effectively: find() and delete() with (conditional) memcpy() in between */
SOL_HASH_MAP_FUNCTION_KEYWORDS enum sol_map_operation_result SOL_CONCATENATE(FUNCTION_PREFIX,_withdraw)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_KEY_TYPE key, SOL_HASH_MAP_ENTRY_TYPE* entry);

/** entry MUST be EXACTLY the result of a prior successful `find()` or `obtain()` with no other map functions called in between
 *  will directly remove an entry from the map if it exists */
SOL_HASH_MAP_FUNCTION_KEYWORDS void SOL_CONCATENATE(SOL_HASH_MAP_FUNCTION_PREFIX,_delete_entry)(struct SOL_HASH_MAP_STRUCT_NAME* map, SOL_HASH_MAP_ENTRY_TYPE* entry_ptr);


#undef SOL_HASH_MAP_STRUCT_NAME
#undef SOL_HASH_MAP_FUNCTION_PREFIX
#undef SOL_HASH_MAP_KEY_TYPE
#undef SOL_HASH_MAP_ENTRY_TYPE
#undef SOL_HASH_MAP_FUNCTION_KEYWORDS

#ifdef SOL_HASH_MAP_CONTEXT_TYPE
#undef SOL_HASH_MAP_CONTEXT_TYPE
#endif
