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

#include <stdlib.h>
#include <assert.h>

#include "sync/task.h"


struct sol_sync_task
{
    struct sol_sync_primitive primitive;

    struct sol_sync_task_system* task_system;

    void(*task_function)(void*);
    void* task_function_data;

    /// need to only init atomics once: "If obj was not default-constructed, or this function is called twice on the same obj, the behavior is undefined."

    atomic_uint_fast32_t condition_count;
    atomic_uint_fast32_t reference_count;

    struct sol_lockfree_hopper successor_hopper;
};

#define SOL_TASK_INTERNAL_COUNTER_BIT ((uint_fast32_t)0x80000000)

static void sol_sync_task_release_references_polymorphic(struct sol_sync_primitive* primitive, uint32_t count);

static inline struct sol_sync_task* sol_sync_task_worker_thread_get_task(struct sol_sync_task_system* task_system)
{
    struct sol_sync_task* task;

    mtx_lock(&task_system->worker_thread_mutex);

    while(!sol_task_queue_dequeue(&task_system->pending_task_queue, &task))
    {
        assert(task_system->stalled_thread_count < task_system->worker_thread_count);
        task_system->stalled_thread_count++;

        /** if all threads are stalled (there is no work left to do) and we've asked the system to shut down (this requires there are no more tasks being created) then we can finalise shutdown */
        if(task_system->shutdown_initiated && (task_system->stalled_thread_count == task_system->worker_thread_count))
        {
            task_system->stalled_thread_count--;
            task_system->shutdown_completed = true;
            cnd_broadcast(&task_system->worker_thread_condition);/// wake up all stalled threads so that they can exit
            mtx_unlock(&task_system->worker_thread_mutex);
            return NULL;
        }

        /** wait untill more workers are needed or we're shutting down (with appropriate checks in case of spurrious wakeup) */
        cnd_wait(&task_system->worker_thread_condition, &task_system->worker_thread_mutex);

        assert(task_system->stalled_thread_count > 0);
        task_system->stalled_thread_count--;

        if(task_system->shutdown_completed)
        {
            mtx_unlock(&task_system->worker_thread_mutex);
            return NULL;
        }
    }

    mtx_unlock(&task_system->worker_thread_mutex);

    return task;
}

static int sol_sync_task_worker_thread_function(void* in)
{
    struct sol_sync_task_system* task_system = in;
    struct sol_sync_task* task;
    struct sol_sync_primitive** successor_ptr;
    uint32_t first_successor_index, successor_index;

    while((task = sol_sync_task_worker_thread_get_task(task_system)))
    {
        task->task_function(task->task_function_data);

        first_successor_index = sol_lockfree_hopper_close(&task->successor_hopper);
        successor_ptr = sol_lockfree_pool_get_entry_pointer(&task_system->successor_pool, first_successor_index);

        /** is important for deterministic task pool usage to release the task BEFORE actually signalling successors (if it is not still being retained) 
         * this retained reference ensures that the task is retained until all its conditions have been signalled (as opposed to just all external retains) 
         * this is so that task conforms to the primitive requirement that an unsatisfied condition also retains the prinmitive
         * note that the task itself is invalid after this point, but all necessary data for what follows has already been extracted by closing the hopper */
        sol_sync_task_release_references_polymorphic((struct sol_sync_primitive*)task, SOL_TASK_INTERNAL_COUNTER_BIT); /** we cast back to the base primitive type */

        successor_index = first_successor_index;

        while(successor_ptr)
        {
            sol_sync_primitive_signal_conditions(*successor_ptr, 1);
            successor_ptr = sol_lockfree_pool_iterate(&task_system->successor_pool, &successor_index);
        }

        sol_lockfree_pool_relinquish_entry_index_range(&task_system->successor_pool, first_successor_index, successor_index);
    }

    return 0;
}




static void sol_sync_task_impose_conditions_polymorphic(struct sol_sync_primitive* primitive, uint32_t count)
{
    /** undo polymorphism */
    struct sol_sync_task* task = (struct sol_sync_task*)primitive;

    uint_fast32_t old_count = atomic_fetch_add_explicit(&task->condition_count, count, memory_order_relaxed);
    assert(old_count>0);/** should not be adding dependencies when none still exist (need held dependencies to addsetup more dependencies) */
}
static void sol_sync_task_signal_conditions_polymorphic(struct sol_sync_primitive* primitive, uint32_t count)
{
    /** undo polymorphism */
    struct sol_sync_task* task = (struct sol_sync_task*)primitive;
    struct sol_sync_task_system* task_system = task->task_system;

    uint_fast32_t old_count;

    /** this is responsible for coalescing all modifications, but also for making them available to the next thread/atomic to recieve this memory (after the potential release in this function) 
     * at present the `task_system->worker_thread_mutex` releases memory but acquire-release here is sufficient if that changes (e.g. if the task is executed immediately) */
    old_count = atomic_fetch_sub_explicit(&task->condition_count, count, memory_order_acq_rel);
    assert(old_count >= count);/** must not make condition count go negative */

    if(old_count==count)/** this is the last dependency/condition this task was waiting on, put it on list of "ready to run" tasks and make sure there's a worker thread to satisfy it */
    {
        mtx_lock(&task_system->worker_thread_mutex);

        sol_task_queue_enqueue(&task_system->pending_task_queue, task, NULL);

        if(task_system->stalled_thread_count)
        {
            assert(!task_system->shutdown_completed);/** shouldnt have stopped running if tasks have yet to complete */
            cnd_signal(&task_system->worker_thread_condition);
        }

        mtx_unlock(&task_system->worker_thread_mutex);
    }
}
static void sol_sync_task_retain_references_polymorphic(struct sol_sync_primitive* primitive, uint32_t count)
{
    /** undo polymorphism */
    struct sol_sync_task* task = (struct sol_sync_task*)primitive;

    uint_fast32_t old_count = atomic_fetch_add_explicit(&task->reference_count, count, memory_order_relaxed);
    assert(old_count!=0);/** should not be adding references without already holding one (need held successors reservations to addsetup more successors reservations) */
}
static void sol_sync_task_release_references_polymorphic(struct sol_sync_primitive* primitive, uint32_t count)
{
    /** undo polymorphism */
    struct sol_sync_task* task = (struct sol_sync_task*)primitive;

    /** need to release to prevent reads/writes of successor/completion data being moved after this operation */
    uint_fast32_t old_count = atomic_fetch_sub_explicit(&task->reference_count, count, memory_order_release);

    assert(old_count >= count);/** cannot release more references than were retained */

    if(old_count == count)
    {
        /** this should only happen after task completion, BUT the only time this counter can (if used properly) hit zero is if the task HAS completed! */
        sol_lockfree_pool_relinquish_entry(&task->task_system->task_pool, task);
    }
}
static void sol_sync_task_attach_successor_polymorphic(struct sol_sync_primitive* primitive, struct sol_sync_primitive* successor)
{
    /** undo polymorphism */
    struct sol_sync_task* task = (struct sol_sync_task*)primitive;

    struct sol_lockfree_pool* successor_pool;
    struct sol_sync_primitive** successor_ptr;

    assert(atomic_load_explicit(&task->reference_count, memory_order_relaxed));
    /** task must be retained to set up successors (can technically be satisfied illegally, using queue for re-use will make detection better but not infallible) */

    /** if hopper already locked then task has been completed, as such dont need to impose this condition on the successor to begin with */
    if( ! sol_lockfree_hopper_is_closed(&task->successor_hopper))
    {
        sol_sync_primitive_impose_conditions(successor, 1);

        successor_pool = &task->task_system->successor_pool;

        successor_ptr = sol_lockfree_pool_acquire_entry(successor_pool);
        assert(successor_ptr);///ran out of successors

        *successor_ptr = successor;

        if( ! sol_lockfree_hopper_push(&task->successor_hopper, successor_pool, successor_ptr))
        {
            // potential to run out of successors if thread stalls here, shouldn't be a problem unless system is under stress
            //  ^ (max_worker_threads * max_successors) should be sufficient overhead
            /// if we failed to add the successor then the task has already been completed, relinquish the storage and signal the successor
            sol_lockfree_pool_relinquish_entry(successor_pool, successor_ptr);
            sol_sync_primitive_signal_conditions(successor, 1);
        }
    }
}

const static struct sol_sync_primitive_functions task_sync_functions =
{
    .impose_conditions  = &sol_sync_task_impose_conditions_polymorphic,
    .signal_conditions  = &sol_sync_task_signal_conditions_polymorphic,
    .retain_references  = &sol_sync_task_retain_references_polymorphic,
    .release_references = &sol_sync_task_release_references_polymorphic,
    .attach_successor   = &sol_sync_task_attach_successor_polymorphic,
};

static void sol_sync_task_initialise(void* entry, void* data)
{
    struct sol_sync_task* task = entry;
    struct sol_sync_task_system* task_system = data;

    task->primitive.sync_functions = &task_sync_functions;
    task->task_system = task_system;

    sol_lockfree_hopper_initialise(&task->successor_hopper);

    atomic_init(&task->condition_count, 0);
    atomic_init(&task->reference_count, 0);
}


void sol_sync_task_system_initialise(struct sol_sync_task_system* task_system, uint32_t worker_thread_count, size_t total_task_exponent, size_t total_successor_exponent)
{
    uint32_t i;

    sol_lockfree_pool_initialise(&task_system->task_pool, total_task_exponent, sizeof(struct sol_sync_task));
    sol_lockfree_pool_initialise(&task_system->successor_pool, total_successor_exponent, sizeof(struct sol_sync_primitive*));

    sol_lockfree_pool_call_for_every_entry(&task_system->task_pool, &sol_sync_task_initialise, task_system);

    sol_task_queue_initialise(&task_system->pending_task_queue, 16);

    task_system->worker_threads = malloc(sizeof(thrd_t) * worker_thread_count);
    task_system->worker_thread_count = worker_thread_count;

    cnd_init(&task_system->worker_thread_condition);
    mtx_init(&task_system->worker_thread_mutex, mtx_plain);

    task_system->shutdown_completed = false;
    task_system->shutdown_initiated = false;

    task_system->stalled_thread_count = 0;

    /// will need setup mutex locked here if we want to wait on all workers to start before progressing (maybe useful to have, but I can't think of a reason)
    for(i=0; i<worker_thread_count; i++)
    {
        thrd_create(task_system->worker_threads+i, sol_sync_task_worker_thread_function, task_system);
    }
}

void sol_sync_task_system_begin_shutdown(struct sol_sync_task_system* task_system)
{
    mtx_lock(&task_system->worker_thread_mutex);
    task_system->shutdown_initiated=true;
    cnd_signal(&task_system->worker_thread_condition);/// ensure at least one worker is awake to finalise shutdown
    mtx_unlock(&task_system->worker_thread_mutex);
}

void sol_sync_task_system_end_shutdown(struct sol_sync_task_system* task_system)
{
    uint32_t i;
    mtx_lock(&task_system->worker_thread_mutex);
    assert(task_system->shutdown_initiated);/// this will finalise shutdown of the task system, shutdown must have been initiated earlier
    while(!task_system->shutdown_completed)
    {
        cnd_wait(&task_system->worker_thread_condition, &task_system->worker_thread_mutex);
    }
    mtx_unlock(&task_system->worker_thread_mutex);

    for(i=0;i<task_system->worker_thread_count;i++)
    {
        /// if thread gets stuck here its possible that not all tasks were able to complete, perhaps because things werent shut down correctly and some tasks have outstanding dependencies
        thrd_join(task_system->worker_threads[i], NULL);
    }
    assert(task_system->stalled_thread_count==0);/// make sure everyone woke up okay
}

void sol_sync_task_system_terminate(struct sol_sync_task_system* task_system)
{
    free(task_system->worker_threads);

    cnd_destroy(&task_system->worker_thread_condition);
    mtx_destroy(&task_system->worker_thread_mutex);

    sol_lockfree_pool_terminate(&task_system->successor_pool);

    sol_task_queue_terminate(&task_system->pending_task_queue);
    sol_lockfree_pool_terminate(&task_system->task_pool);
}



struct sol_sync_task_handle sol_sync_task_prepare(struct sol_sync_task_system* task_system, void(*task_function)(void*), void * data)
{
    struct sol_sync_task* task;

    task = sol_lockfree_pool_acquire_entry(&task_system->task_pool);
    assert(task);//not enough tasks allocated

    task->task_function=task_function;
    task->task_function_data=data;

    sol_lockfree_hopper_reset(&task->successor_hopper);

    /** need to wait on "enqueue" op before beginning, ergo need one extra dependency for that (enqueue is really just a signal) */
    atomic_store_explicit(&task->condition_count, SOL_TASK_INTERNAL_COUNTER_BIT, memory_order_relaxed);
    /** need to retain a reference until the successors are actually signalled, ergo one extra reference that will be released after completing the task */
    atomic_store_explicit(&task->reference_count, SOL_TASK_INTERNAL_COUNTER_BIT, memory_order_relaxed);

    return (struct sol_sync_task_handle)
    {
        .primitive = (struct sol_sync_primitive*) task,
    };
}

void sol_sync_task_activate(struct sol_sync_task_handle task)
{
    /** this is basically just called differently to account for the "hidden" wait counter added on task creation */
    /** sol_sync_task_signal_conditions(task, 1); */
    assert(task.primitive->sync_functions->signal_conditions == &sol_sync_task_signal_conditions_polymorphic);
    sol_sync_task_signal_conditions_polymorphic(task.primitive, SOL_TASK_INTERNAL_COUNTER_BIT);
}

void sol_sync_task_impose_conditions(struct sol_sync_task_handle task, uint_fast32_t count)
{
    assert(task.primitive->sync_functions->impose_conditions == &sol_sync_task_impose_conditions_polymorphic);
    sol_sync_task_impose_conditions_polymorphic(task.primitive, count);
}

void sol_sync_task_signal_conditions(struct sol_sync_task_handle task, uint_fast32_t count)
{
    assert(task.primitive->sync_functions->signal_conditions == &sol_sync_task_signal_conditions_polymorphic);
    sol_sync_task_signal_conditions_polymorphic(task.primitive, count);
}

void sol_sync_task_retain_references(struct sol_sync_task_handle task, uint_fast32_t count)
{
    assert(task.primitive->sync_functions->retain_references == &sol_sync_task_retain_references_polymorphic);
    sol_sync_task_retain_references_polymorphic(task.primitive, count);
}

void sol_sync_task_release_references(struct sol_sync_task_handle task, uint_fast32_t count)
{
    assert(task.primitive->sync_functions->release_references == &sol_sync_task_release_references_polymorphic);
    sol_sync_task_release_references_polymorphic(task.primitive, count);
}

void sol_sync_task_attach_successor(struct sol_sync_task_handle task, struct sol_sync_primitive* successor)
{
    assert(task.primitive->sync_functions->attach_successor == &sol_sync_task_attach_successor_polymorphic);
    sol_sync_task_attach_successor_polymorphic(task.primitive, successor);
}





