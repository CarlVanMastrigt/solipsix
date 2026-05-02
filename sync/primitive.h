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

#pragma once

#include <inttypes.h>

struct sol_sync_primitive;


struct sol_sync_primitive_functions
{
    void(*const impose_conditions )(struct sol_sync_primitive*, uint32_t count);
    void(*const signal_conditions )(struct sol_sync_primitive*, uint32_t count);

    void(*const retain_references )(struct sol_sync_primitive*, uint32_t count);
    void(*const release_references)(struct sol_sync_primitive*, uint32_t count);

    /** because sucessor may satisfy the dependency/condition immediately, successor must have a condition set first 
     * on signalled primitive, this function should be the equivalent of a no-op, though checking that state should still be done correctly
     * i.e. the read to check the signalled state should have memory order acquire or equivalent */
    void(*const attach_successor  )(struct sol_sync_primitive*, struct sol_sync_primitive* successor);
};

/** struct sol_sync_primitive MUST be the first element of any synchronization primitive for the polymorphic interface to work correctly */
struct sol_sync_primitive
{
    const struct sol_sync_primitive_functions* sync_functions;
};



/** provided primitives all come with typed variants of these functions where appropriate
 * use of the typed variants is recommended where possible for clarity 
 * otherwise there is a generic interface which can be used below */


/** requires an another unsignalled condition or the sync primitive not to have been activated in order to call this 
 * adds a condition to the sync primitive that must be signalled before the primitive can execute and signal its successors 
 * must pair 1:1 with `sol_sync_primitive_signal_condition` */ 
static inline void sol_sync_primitive_impose_conditions(struct sol_sync_primitive* primitive, uint32_t count)
{
    primitive->sync_functions->impose_conditions(primitive, count);
}

/** signal/satisfy an extablished condition of the sync primitive
 * must pair 1:1 with `sol_sync_primitive_impose_condition` */ 
static inline void sol_sync_primitive_signal_conditions(struct sol_sync_primitive* primitive, uint32_t count)
{
    primitive->sync_functions->signal_conditions(primitive, count);
}

/** prevent the sync primitive being cleaned up/destroyed, does not prevent the primitive executing or signalling its successors
 * is used to hold on to a primitive in order to set up successors to that primitive 
 * must pair 1:1 with `sol_sync_primitive_release_reference` */
static inline void sol_sync_primitive_retain_references(struct sol_sync_primitive* primitive, uint32_t count)
{
    primitive->sync_functions->retain_references(primitive, count);
}

/** indicate that the setup of successors, for which we retained this sync primitive, is complete
 * must pair 1:1 with `sol_sync_primitive_retain_reference` */
static inline void sol_sync_primitive_release_references(struct sol_sync_primitive* primitive, uint32_t count)
{
    primitive->sync_functions->release_references(primitive, count);
}

/** establish an ordering dependency such that: `primitive` "happens before" `successor` (make `successor` a successor to `primitive`)
 *
 * `primitive` must have a reference retained or an unsignalled condition
 * `successor` must have an unsignalled condition or not yet have been activated
 */
static inline void sol_sync_primitive_attach_successor(struct sol_sync_primitive* primitive, struct sol_sync_primitive* successor)
{
    primitive->sync_functions->attach_successor(primitive, successor);
}


/** stall the current thread until the primitive would signal its successors
 * this is just a gate without the setup or management */
void sol_sync_primitive_wait(struct sol_sync_primitive* primitive);




/** get a placeholder / "dummy" primitive, it acts as if always retained and always signalled no matter conditions imposed or references released 
 * used when it is rare to not actually have a valid primitive, but it can happen (e.g. a queue, specifically only on first use) 
 * and it would be slower/awkward to check whether there is a primitive rather than just using this when there isn't one 
 * it also allows functions to return a valid/correct "nil" primitive or primitive range (i.e. one that does nothing) 
 * NOTE: the returned primitive must NOT be added as a dependency AND a successor, only one or the other as it does not actually handle synchronization */
struct sol_sync_primitive* sol_sync_primitive_placeholder_predecessor_prepare(void);
struct sol_sync_primitive* sol_sync_primitive_placeholder_successor_prepare(void);


/** helper struct, would usually be returned by a function that sets up a task chain/task graph */
struct sol_sync_primitive_range
{
    struct sol_sync_primitive* first;
    struct sol_sync_primitive* last;
};