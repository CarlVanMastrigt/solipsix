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

#include <inttypes.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>

#include "coherent_structures/lockfree_pool.h"
#include "coherent_structures/coherent_queue_with_counter.h"
#include "coherent_structures/lockfree_hopper.h"

// struct sol_sync_primitive_functions;

struct sol_task_system
{
    sol_lockfree_pool task_pool;
    sol_coherent_queue_with_counter pending_tasks;

    sol_lockfree_pool successor_pool;///pool for storing successors (linked list/hopper per task)

    thrd_t* worker_threads;/// make this system extensible (to a degree) should stalled threads want to be switched to a worker thread (can perhaps do this without relying on scheduler though)
    uint32_t worker_thread_count;

    cnd_t worker_thread_condition;/// multipurpose, used for setup, shutdown and for handling stalled threads
    mtx_t worker_thread_mutex;

    uint32_t signalled_unstalls;
    uint32_t stalled_thread_count;

    bool shutdown_completed;
    bool shutdown_initiated;

    /// pool size for types and expected pool count (to allow over use without pointer change in cases where a second pool would overrun memory capabilities and crash)
    /// is above too much to expose to user? maybe take expected pool size and quarter it?
};

void sol_task_system_initialise(struct sol_task_system* task_system, uint32_t worker_thread_count, size_t total_task_exponent, size_t total_successor_exponent);

/// invalid to add tasks *that don't depend on other tasks* after this has been called
/// this CAN be called inside a task!
void sol_task_system_begin_shutdown(struct sol_task_system* task_system);

/// waits for all tasks to complete and worker threads to join before returning, as such cannot be called inside a task
/// any outstanding tasks will be executed before this returns
/// as such any task still in flight must not have dependencies that would only be satisfied after this function has returned
void sol_task_system_end_shutdown(struct sol_task_system* task_system);

/// cleans up allocations
void sol_task_system_terminate(struct sol_task_system* task_system);



struct sol_task
{
    const struct sol_sync_primitive_functions* sync_functions;

    struct sol_task_system* task_system;

    void(*task_function)(void*);
    void* task_function_data;

    /// need to only init atomics once: "If obj was not default-constructed, or this function is called twice on the same obj, the behavior is undefined."

    atomic_uint_fast32_t condition_count;
    atomic_uint_fast32_t reference_count;

    sol_lockfree_hopper successor_hopper;
};


// task starts inert/unactivated and cannot run (so that order of execution relative to other primitives can be established) `sol_task_activate` must be called for it to run
struct sol_task* sol_task_prepare(struct sol_task_system* task_system, void(*task_function)(void*), void* data);

/// allows a task to be executed
/// must either add all associated dependencies/condition before calling this OR impose conditions and retain references the task as necessary to set up dependencies later
void sol_task_activate(struct sol_task* task);//commit?



// all conditions must be satisfied for a task to run

/// corresponds to the number of times a matching `sol_task_signal_conditions` must be called for the task before it can be executed
/// at least one condition must be unsignalled in order to set up another condition to that task if it has already been enqueued
void sol_task_impose_conditions(struct sol_task* task, uint_fast32_t count);

/// use this to signal that some set of data and/or dependencies required by the task have been set up, total must be matched to the count provided to sol_task_impose_conditions
void sol_task_signal_conditions(struct sol_task* task, uint_fast32_t count);


/// corresponds to the number of times a matching `sol_task_release_reference` must be called for the task before it can be cleaned up
/// at least one retainer must be held in order to set up a successor to that task if it has already been enqueued
void sol_task_retain_references(struct sol_task* task, uint_fast32_t count);

/// signal that we are done setting up things that must happen before cleanup (e.g. setting up successors)
void sol_task_release_references(struct sol_task* task, uint_fast32_t count);



static inline union sol_sync_primitive* sol_task_as_sync_primitive(struct sol_task* task)
{
    return (union sol_sync_primitive*)task;
}
