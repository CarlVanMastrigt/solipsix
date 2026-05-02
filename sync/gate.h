/**
Copyright 2024,2025,2026 Carl van Mastrigt

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


#include "solipsix/lockfree/pool.h"
#include "solipsix/sync/primitive.h"

/** special thank you to my dad Peter, for suggesting the name for this */

struct sol_sync_gate_pool
{
    struct sol_lockfree_pool available_gates;
};

void sol_sync_gate_pool_initialise(struct sol_sync_gate_pool* pool, size_t capacity_exponent);
void sol_sync_gate_pool_terminate(struct sol_sync_gate_pool* pool);


struct sol_sync_gate_handle
{
    /** gate is a polymorphic sync primitive
     * this pointer should never be altered once vended, but can be used freely as argument to `sync_primitive` function calls */
    struct sol_sync_primitive* primitive;
};

struct sol_sync_gate_handle sol_sync_gate_prepare(struct sol_sync_gate_pool* pool);

/** MUST be called at some point as this will clean up the prepared gate 
 * is similar to activate that other primitives use */
void sol_sync_gate_wait(struct sol_sync_gate_handle gate);


/** used to set up dependencies (calls to sol_sync_gate_signal_conditions) to wait on (dont return from wait until all signals happen)
 * must know that an gate has at least one outstanding dependency or that the gate hasn't yet had `sol_sync_gate_wait` called */
void sol_sync_gate_impose_conditions(struct sol_sync_gate_handle gate, uint32_t count);

/** must be called once for every dependency added by a call to sol_sync_gate_impose_conditions */
void sol_sync_gate_signal_conditions(struct sol_sync_gate_handle gate, uint32_t count);




