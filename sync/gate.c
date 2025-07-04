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

#include <stdatomic.h>
#include <threads.h>
#include <stdbool.h>
#include <assert.h>


#include "sync/gate.h"

#define SOL_GATE_WAITING_FLAG   ((uint_fast32_t)0x80000000)
#define SOL_GATE_SIGNAL_STATUS  ((uint_fast32_t)0x80000001) /**would be last condition to signal AND thread is waiting*/
#define SOL_GATE_CONDITION_MASK ((uint_fast32_t)0x7FFFFFFF)

struct sol_sync_gate
{
    struct sol_sync_primitive primitive;

    struct sol_sync_gate_pool* pool;

    atomic_uint_fast32_t status;

    mtx_t* mutex;
    cnd_t* condition;
};


static void sol_sync_gate_impose_condition_polymorphic(struct sol_sync_primitive* primitive)
{
    sol_sync_gate_impose_conditions((struct sol_sync_gate*)primitive, 1);
}
static void sol_sync_gate_signal_condition_polymorphic(struct sol_sync_primitive* primitive)
{
    sol_sync_gate_signal_conditions((struct sol_sync_gate*)primitive, 1);
}
static void sol_sync_gate_attach_successor_polymorphic(struct sol_sync_primitive* primitive, struct sol_sync_primitive* successor)
{
    assert(0);// gate cannot have sucessors
}
static void sol_sync_gate_retain_reference_polymorphic(struct sol_sync_primitive* primitive)
{
    assert(0);// gate cannot be retained
}
static void sol_sync_gate_release_reference_polymorphic(struct sol_sync_primitive* primitive)
{
    assert(0);// gate cannot be retained
}

const static struct sol_sync_primitive_functions gate_sync_functions =
{
    .impose_condition  = &sol_sync_gate_impose_condition_polymorphic,
    .signal_condition  = &sol_sync_gate_signal_condition_polymorphic,
    .attach_successor  = &sol_sync_gate_attach_successor_polymorphic,
    .retain_reference  = &sol_sync_gate_retain_reference_polymorphic,
    .release_reference = &sol_sync_gate_release_reference_polymorphic,
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
    sol_lockfree_pool_initialise(&pool->available_gates,capacity_exponent,sizeof(struct sol_sync_gate));
    sol_lockfree_pool_call_for_every_entry(&pool->available_gates, &sol_sync_gate_initialise, pool);
}

void sol_sync_gate_pool_terminate(struct sol_sync_gate_pool* pool)
{
    sol_lockfree_pool_call_for_every_entry(&pool->available_gates, &sol_sync_gate_terminate, NULL);
    sol_lockfree_pool_terminate(&pool->available_gates);
}





struct sol_sync_gate * sol_sync_gate_prepare(struct sol_sync_gate_pool* pool)
{
    struct sol_sync_gate* gate;

    gate = sol_lockfree_pool_acquire_entry(&pool->available_gates);
    assert(atomic_load(&gate->status) == 0);
    /// this is 0 dependencies and in the non-waiting state, this could be made some high count with a fetch sub in `sol_sync_gate_wait` if error checking is desirable

    return gate;
}

// could have a system to execute tasks while waiting, but waiting is really undesirable anyway so avoid it flat out
void sol_sync_gate_wait(struct sol_sync_gate * gate)
{
    mtx_t mutex;
    cnd_t condition;
    uint_fast32_t current_status, replacement_status;
    bool successfully_replaced;

    current_status = atomic_load_explicit(&gate->status, memory_order_acquire);

    if(current_status)/// has dependencies remaining
    {
        mtx_init(&mutex,mtx_plain);
        cnd_init(&condition);

        assert(gate->mutex == NULL);
        assert(gate->condition == NULL);

        gate->mutex=&mutex;
        gate->condition=&condition;

        mtx_lock(&mutex);///must lock before making assertions regarding status being 0

        do
        {
            assert( ! (current_status & SOL_GATE_WAITING_FLAG) );/// must NOT already have waiting flag set, did you try and wait on this gate twice?
            replacement_status = current_status | SOL_GATE_WAITING_FLAG;
            successfully_replaced = atomic_compare_exchange_weak_explicit(&gate->status, &current_status, replacement_status, memory_order_release, memory_order_acquire);
        }
        while( !successfully_replaced && current_status);
        ///must either have nothing waiting or have flagged the status as waiting by this point
        /// release to ensure ordering of mutex lock (not 100% on whether this is actually necessary)

        while(current_status)///if we did flag the status as waiting
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

    /// at this point anything else that would use (signal) this gate must have completed, so we're safe to recycle
    sol_lockfree_pool_relinquish_entry(&gate->pool->available_gates,gate);
}

void sol_sync_gate_impose_conditions(struct sol_sync_gate * gate, uint_fast32_t count)
{
    uint_fast32_t old_status = atomic_fetch_add_explicit(&gate->status, count, memory_order_relaxed);
    assert(!(old_status & SOL_GATE_WAITING_FLAG) || (old_status & SOL_GATE_CONDITION_MASK) > 0);
    // illegal to impose a condition on a gate that is being waited on and has no outsatnding conditions
    /// ADDING dependencies shouldn't incurr memory ordering restrictions
}

void sol_sync_gate_signal_conditions(struct sol_sync_gate* gate, uint_fast32_t count)
{
    uint_fast32_t current_status, replacement_status;
    bool locked;

    current_status = atomic_load_explicit(&gate->status, memory_order_acquire);
    locked = false;

    do
    {
        assert((current_status&SOL_GATE_CONDITION_MASK)>0);/// you have signalled more times than dependencies were added! this is incredibly bad and unsafe
        /// the above may not catch this case immediately; may actually result in signalling a *different* gate after the intended gate gets recycled

        if(current_status == SOL_GATE_SIGNAL_STATUS)
        {
            ///it's only possible for 1 thread to see this value. and if locked before deps hitting 0: exactly 1 thread MUST see this value
            /// if more than one thread sees this value that implies we have 2 threads running trying to signal the last dependency, which is "guarded" against by the above assert
            if(!locked)
            {
                assert(gate->mutex);
                assert(gate->condition);
                mtx_lock(gate->mutex);/// must lock before going from waiting to "complete" (status==0)
                locked = true;/// don't relock if compare exchange fails
            }
            replacement_status = 0;/// this indicates the waiting thread may proceed when it is woken up
        }
        else
        {
            replacement_status = current_status - 1;
        }

    }
    while(!atomic_compare_exchange_weak_explicit(&gate->status, &current_status, replacement_status, memory_order_release, memory_order_acquire));

    assert(locked == (current_status == SOL_GATE_SIGNAL_STATUS));///must have locked if we need to signal the condition, and must not have locked if we don't

    if(current_status == SOL_GATE_SIGNAL_STATUS)
    {
        cnd_signal(gate->condition);
        mtx_unlock(gate->mutex);
    }
}

struct sol_sync_primitive* sol_sync_gate_primitive(struct sol_sync_gate* gate)
{
    return &gate->primitive;
}




