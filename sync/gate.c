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

#include <stdatomic.h>
#include <threads.h>
#include <stdbool.h>
#include <assert.h>


#include "sync/gate.h"

#define SOL_GATE_WAITING_FLAG   ((uint_fast32_t)0x80000000)
#define SOL_GATE_CONDITION_MASK ((uint_fast32_t)0x7FFFFFFF)

struct sol_sync_gate
{
    struct sol_sync_primitive primitive;

    struct sol_sync_gate_pool* pool;

    atomic_uint_fast32_t status;

    mtx_t* mutex;
    cnd_t* condition;
};


static void sol_sync_gate_impose_conditions_polymorphic(struct sol_sync_primitive* primitive, uint32_t count)
{
    /** undo polymorphism */
    struct sol_sync_gate* gate = (struct sol_sync_gate*)primitive;
    uint_fast32_t old_status;

    assert(count > 0);

    old_status = atomic_fetch_add_explicit(&gate->status, count, memory_order_relaxed);

    assert(!(old_status & SOL_GATE_WAITING_FLAG) || (old_status & SOL_GATE_CONDITION_MASK) > 0);
    /** it is illegal to impose a condition on a gate that is being waited on and has no outsatnding conditions
     * note: imposing conditions shouldn't incurr memory ordering restrictions, only satisfying them */

    assert(((old_status + count) & SOL_GATE_WAITING_FLAG) == ((old_status) & SOL_GATE_WAITING_FLAG));
    /** added conditions should not have count sufficient to alter the waiting flag bit */
}
static void sol_sync_gate_signal_conditions_polymorphic(struct sol_sync_primitive* primitive, uint32_t count)
{
    /** undo polymorphism */
    struct sol_sync_gate* gate = (struct sol_sync_gate*)primitive;
    uint_fast32_t current_status, replacement_status;
    bool locked; /** locked is synnonomous with needing to signal the condition */

    assert(count > 0);

    current_status = atomic_load_explicit(&gate->status, memory_order_acquire);
    locked = false;

    do
    {
        assert((current_status&SOL_GATE_CONDITION_MASK) >= count);
        /** more conditions have been signalled than were imposed!
         * the above may not catch this case immediately; may actually result in signalling a *different* gate after the intended gate gets recycled */

        if(current_status == count + SOL_GATE_WAITING_FLAG)
        {
            /** it's only possible for 1 thread to see this as true. and if locked before deps hitting 0: exactly 1 thread MUST see this as true
             * if more than one thread sees this value that implies we have 2 threads running trying to signal the last dependencies, which is "guarded" against by the above assert 
             * if no threads see this value it implies nobody decremented the last "count" many */
            if(!locked)
            {
                assert(gate->mutex);
                assert(gate->condition);
                mtx_lock(gate->mutex);/** must lock before going from waiting to "complete" (status==0) */
                locked = true;/** don't relock if compare exchange fails Z*/
            }
            replacement_status = 0;/** this indicates the waiting thread may proceed when it is woken up */
        }
        else
        {
            replacement_status = current_status - count;
        }

    }
    while(!atomic_compare_exchange_weak_explicit(&gate->status, &current_status, replacement_status, memory_order_release, memory_order_acquire));

    assert(locked == (current_status == count + SOL_GATE_WAITING_FLAG));/** must have locked if we need to signal the condition, and must not have locked if we don't */

    if(locked)
    {
        cnd_signal(gate->condition);
        mtx_unlock(gate->mutex);
    }
}
static void sol_sync_gate_retain_references_polymorphic(struct sol_sync_primitive* primitive, uint32_t count)
{
    assert(0);/** gate cannot be retained */
}
static void sol_sync_gate_release_references_polymorphic(struct sol_sync_primitive* primitive, uint32_t count)
{
    assert(0);/** gate cannot be retained */
}
static void sol_sync_gate_attach_successor_polymorphic(struct sol_sync_primitive* primitive, struct sol_sync_primitive* successor)
{
    assert(0);/** gate cannot have sucessors */
}

const static struct sol_sync_primitive_functions gate_sync_functions =
{
    .impose_conditions  = &sol_sync_gate_impose_conditions_polymorphic,
    .signal_conditions  = &sol_sync_gate_signal_conditions_polymorphic,
    .retain_references  = &sol_sync_gate_retain_references_polymorphic,
    .release_references = &sol_sync_gate_release_references_polymorphic,
    .attach_successor   = &sol_sync_gate_attach_successor_polymorphic,
};

static void sol_sync_gate_initialise(void* entry, void* data)
{
    struct sol_sync_gate* gate = entry;
    struct sol_sync_gate_pool* pool = data;

    gate->primitive.sync_functions = &gate_sync_functions;
    gate->pool = pool;

    gate->condition = NULL;
    gate->mutex = NULL;
    atomic_init(&gate->status, 0);
}

static void sol_sync_gate_terminate(void* entry, void* data)
{
    struct sol_sync_gate* gate = entry;
    assert(gate->mutex == NULL);
    assert(gate->condition == NULL);
}

void sol_sync_gate_pool_initialise(struct sol_sync_gate_pool* pool, size_t capacity_exponent)
{
    sol_lockfree_pool_initialise(&pool->available_gates, capacity_exponent, sizeof(struct sol_sync_gate));
    sol_lockfree_pool_call_for_every_entry(&pool->available_gates, &sol_sync_gate_initialise, pool);
}

void sol_sync_gate_pool_terminate(struct sol_sync_gate_pool* pool)
{
    sol_lockfree_pool_call_for_every_entry(&pool->available_gates, &sol_sync_gate_terminate, NULL);
    sol_lockfree_pool_terminate(&pool->available_gates);
}



struct sol_sync_gate_handle sol_sync_gate_prepare(struct sol_sync_gate_pool* pool)
{
    struct sol_sync_gate* gate;

    gate = sol_lockfree_pool_acquire_entry(&pool->available_gates);
    assert(atomic_load(&gate->status) == 0);
    /** 0 indicates no dependencies and gate is in the non-waiting state, this could be made some high count with a fetch sub in `sol_sync_gate_wait` if error checking is desirable */

    return (struct sol_sync_gate_handle)
    {
        .primitive = (struct sol_sync_primitive*) gate,
    };
}


void sol_sync_gate_wait(struct sol_sync_gate_handle gate_handle)
{
    /** undo polymorphism */
    struct sol_sync_gate* gate = (struct sol_sync_gate*)gate_handle.primitive;

    mtx_t mutex;
    cnd_t condition;
    uint_fast32_t current_status, replacement_status;
    bool successfully_replaced;

    current_status = atomic_load_explicit(&gate->status, memory_order_acquire);

    if(current_status)/** only wait if there are remaining conditions */
    {
        mtx_init(&mutex,mtx_plain);
        cnd_init(&condition);

        assert(gate->mutex == NULL);
        assert(gate->condition == NULL);

        gate->mutex = &mutex;
        gate->condition = &condition;

        mtx_lock(&mutex);/** must lock before altering status */

        do
        {
            assert( ! (current_status & SOL_GATE_WAITING_FLAG) );/** must not already have waiting flag set, did you try and wait on this gate twice? */
            replacement_status = current_status | SOL_GATE_WAITING_FLAG;
            successfully_replaced = atomic_compare_exchange_weak_explicit(&gate->status, &current_status, replacement_status, memory_order_release, memory_order_acquire);
        }
        while( !successfully_replaced && current_status);
        /** must either have nothing waiting or have flagged the status as waiting by this point
         * memory_order_release to ensure visibility of mutex and consition pointers */

        while(current_status)/** if we did flag the status as waiting, actually wait */
        {
            cnd_wait(&condition, &mutex);
            current_status = atomic_load_explicit(&gate->status, memory_order_acquire);/// load to double check signal actually happened in case of spurrious wake up
        }

        gate->mutex = NULL;
        gate->condition = NULL;

        mtx_unlock(&mutex);

        mtx_destroy(&mutex);
        cnd_destroy(&condition);
    }

    if(gate->pool)
    {
        /** at this point anything else that would use (signal) this gate must have completed, so it's "safe" to recycle */
        sol_lockfree_pool_relinquish_entry(&gate->pool->available_gates, gate);
    }
}

void sol_sync_gate_impose_conditions(struct sol_sync_gate_handle gate, uint32_t count)
{
    assert(gate.primitive->sync_functions->impose_conditions == &sol_sync_gate_impose_conditions_polymorphic);
    sol_sync_gate_impose_conditions_polymorphic(gate.primitive, count);
}

void sol_sync_gate_signal_conditions(struct sol_sync_gate_handle gate, uint32_t count)
{
    assert(gate.primitive->sync_functions->signal_conditions == &sol_sync_gate_signal_conditions_polymorphic);
    sol_sync_gate_signal_conditions_polymorphic(gate.primitive, count);
}



/** because the implementation of this is so similar to a gate to begin with: just do it here 
 * also it relies on the specific gate implementation details to be correct so it goes in the same compilation unit */
void sol_sync_primitive_wait(struct sol_sync_primitive* primitive)
{
    struct sol_sync_gate gate;
    struct sol_sync_gate_handle handle;

    sol_sync_gate_initialise(&gate, NULL);

    handle = (struct sol_sync_gate_handle)
    {
        .primitive = (struct sol_sync_primitive*) &gate,
    };

    /** make the gate dependent on the primitive before waiting on it */
    sol_sync_primitive_attach_successor(primitive, handle.primitive);

    sol_sync_gate_wait(handle);
}