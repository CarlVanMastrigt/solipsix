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
along with barrier.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once


#include <inttypes.h>
#include <stdatomic.h>

#include "lockfree/pool.h"

#include "sync/primitive.h"

struct sol_sync_barrier_pool
{
    struct sol_lockfree_pool barrier_pool;
    struct sol_lockfree_pool successor_pool;
};

void sol_sync_barrier_pool_initialise(struct sol_sync_barrier_pool* pool, size_t total_barrier_exponent, size_t total_successor_exponent);
void sol_sync_barrier_pool_terminate(struct sol_sync_barrier_pool* pool);

struct sol_sync_barrier_handle
{
    /** barrier is a polymorphic sync primitive
     * this pointer should never be altered once vended, but can be used freely as argument to `sync_primitive` function calls */
    struct sol_sync_primitive* primitive;
};

/** aquire a barrier that is ready to be used from the pool
 * must match a later call to `sol_sync_barrier_activate` */
struct sol_sync_barrier_handle sol_sync_barrier_prepare(struct sol_sync_barrier_pool* pool);

/** all initial setup has been done, barrier is considered "live" from here
 * after calling this refrences and conditions may only be added using an already held refence or condition respectively
 * must match an earlier call to `sol_sync_barrier_prepare` */
void sol_sync_barrier_activate(struct sol_sync_barrier_handle barrier);


/** corresponds to the number of times a matching `sol_sync_barrier_signal_conditions` must be called for the barrier before it can be executed
 * at least one dependency must be held in order to set up a dependency to that barrier if it has already been activated */
void sol_sync_barrier_impose_conditions(struct sol_sync_barrier_handle barrier, uint_fast32_t count);

/** use this to signal that some set of data and/or dependencies required by the barrier have been set up, total must be matched to the count provided to sol_sync_barrier_impose_conditions */
void sol_sync_barrier_signal_conditions(struct sol_sync_barrier_handle barrier, uint_fast32_t count);

/** NOTE: execution dependencies (conditions) also act as retained references (because we cannot clean up a barrier until it has been executed) 
 * as such if a dependency is required anyway then a refernce need not be held as well */

/** corresponds to some number of matching `sol_sync_barrier_release_references` calls that must happen before the barrier can be destroyed/released (i.e. the pointer becomes an invalid way to refernce this barrier)
 * at least one reference must be held in order to set up a successor to that barrier if it has already been activated */
void sol_sync_barrier_retain_references(struct sol_sync_barrier_handle barrier, uint_fast32_t count);

/** signal that we are done setting up things that must happen before cleanup (i.e. setting up successors) 
 * count across all calls must align with count across all calls to `sol_sync_barrier_retain_references` for the same barrier */
void sol_sync_barrier_release_references(struct sol_sync_barrier_handle barrier, uint_fast32_t count);


/** convenient typed dependency setup */
void sol_sync_barrier_attach_successor(struct sol_sync_barrier_handle barrier, struct sol_sync_primitive* successor);
