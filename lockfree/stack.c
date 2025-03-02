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

#include <assert.h>

#include "lockfree/stack.h"

void sol_lockfree_stack_initialise(struct sol_lockfree_stack* stack, struct sol_lockfree_pool* pool)
{
    atomic_init(&stack->head,SOL_LOCKFREE_POOL_INVALID_ENTRY);
    stack->next_buffer = pool->next_buffer;
    stack->entry_data = pool->entry_data;
    stack->entry_size = pool->entry_size;
    stack->capacity_exponent = pool->capacity_exponent;
}

void sol_lockfree_stack_terminate(struct sol_lockfree_stack* stack)
{
    assert((atomic_load_explicit(&stack->head, memory_order_relaxed) & SOL_LOCKFREE_POOL_ENTRY_MASK) == SOL_LOCKFREE_POOL_INVALID_ENTRY);/// stack should be empty upon termination
    *stack = (struct sol_lockfree_stack){};
    #warning use this when destroying everywhere
}



void sol_lockfree_stack_push(struct sol_lockfree_stack* stack, void* entry)
{
    uint_fast64_t entry_index, current_head, replacement_head;

    assert((char*)entry >= stack->entry_data);
    assert((char*)entry < stack->entry_data + (stack->entry_size << stack->capacity_exponent));

    entry_index = (uint32_t) (((char*)entry - stack->entry_data) / (ptrdiff_t)stack->entry_size);

    current_head = atomic_load_explicit(&stack->head, memory_order_relaxed);

    do
    {
        stack->next_buffer[entry_index] = (uint32_t)(current_head & SOL_LOCKFREE_POOL_ENTRY_MASK);
        replacement_head = ((current_head & SOL_LOCKFREE_POOL_CHECK_MASK) + SOL_LOCKFREE_POOL_CHECK_UNIT) | entry_index;
    }
    while(!atomic_compare_exchange_weak_explicit(&stack->head, &current_head, replacement_head, memory_order_release, memory_order_relaxed));
}

void* sol_lockfree_stack_pull(struct sol_lockfree_stack* stack)
{
    uint_fast64_t entry_index, current_head, replacement_head;

    current_head = atomic_load_explicit(&stack->head, memory_order_acquire);

    do
    {
        entry_index = current_head & SOL_LOCKFREE_POOL_ENTRY_MASK;
        /// if there are no more entries in this list then return NULL
        if(entry_index == SOL_LOCKFREE_POOL_INVALID_ENTRY)
        {
            return NULL;
        }

        replacement_head = ((current_head & SOL_LOCKFREE_POOL_CHECK_MASK) + SOL_LOCKFREE_POOL_CHECK_UNIT) | (uint_fast64_t)(stack->next_buffer[entry_index]);
    }
    while(!atomic_compare_exchange_weak_explicit(&stack->head, &current_head, replacement_head, memory_order_acquire, memory_order_acquire));
    /// success memory order could (conceptually) be relaxed here as we don't need to release/acquire any changes when that happens as nothing outside the atomic has changed
    /// but fail must acquire as the "next" member of "first" element may have changed

    assert(entry_index < ((uint_fast64_t)1 << stack->capacity_exponent));

    return stack->entry_data + entry_index * stack->entry_size;
}


