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

#include <stddef.h>
#include <assert.h>

#include "lockfree/hopper.h"

#define SOL_LOCKFREE_HOPPER_CLOSED_BIT  ((uint_fast64_t)0x8000000000000000llu)

void sol_lockfree_hopper_initialise(struct sol_lockfree_hopper* hopper)
{
    atomic_init(&hopper->head, SOL_LOCKFREE_HOPPER_CLOSED_BIT);
}

void sol_lockfree_hopper_terminate(struct sol_lockfree_hopper* hopper)
{
    assert(atomic_load_explicit(&hopper->head, memory_order_relaxed) == SOL_LOCKFREE_HOPPER_CLOSED_BIT);/// hopper should be locked/invalid upon termination
}

void sol_lockfree_hopper_reset(struct sol_lockfree_hopper* hopper)
{
    uint_fast64_t prev_value;

    prev_value = atomic_exchange_explicit(&hopper->head, SOL_LOCKFREE_POOL_INVALID_ENTRY, memory_order_relaxed);

    assert(prev_value == SOL_LOCKFREE_HOPPER_CLOSED_BIT);/// hopper should be locked/invalid upon reset
}

bool sol_lockfree_hopper_push(struct sol_lockfree_hopper* hopper, struct sol_lockfree_pool* pool, void* entry)
{
    uint_fast64_t current_head, replacement_head;
    uint32_t entry_index;

    assert((char*)entry >= pool->entry_data);
    assert((char*)entry < pool->entry_data + (pool->entry_size << pool->capacity_exponent));

    entry_index = (uint32_t) (((char*)entry - pool->entry_data) / pool->entry_size);

    current_head = atomic_load_explicit(&hopper->head, memory_order_relaxed);
    do
    {
        if(current_head & SOL_LOCKFREE_HOPPER_CLOSED_BIT)
        {
            assert(current_head == SOL_LOCKFREE_HOPPER_CLOSED_BIT);// if locked should ONLY be locked
            return false;
        }

        assert(current_head < (1 << pool->capacity_exponent) || current_head == SOL_LOCKFREE_POOL_INVALID_ENTRY);

        pool->next_buffer[entry_index] = (uint32_t)current_head;

        replacement_head = entry_index;
    }
    while(!atomic_compare_exchange_weak_explicit(&hopper->head, &current_head, replacement_head, memory_order_release, memory_order_relaxed));

    return true;
}

bool sol_lockfree_hopper_is_closed(struct sol_lockfree_hopper * hopper)
{
    return atomic_load_explicit(&hopper->head, memory_order_relaxed) & SOL_LOCKFREE_HOPPER_CLOSED_BIT;
}

uint32_t sol_lockfree_hopper_close(struct sol_lockfree_hopper* hopper)
{
    uint_fast64_t entry_index;

    entry_index = atomic_load_explicit(&hopper->head, memory_order_acquire);
    do
    {
        assert( !(entry_index & SOL_LOCKFREE_HOPPER_CLOSED_BIT));/// cannot lock the hopper twice
    }
    while(!atomic_compare_exchange_weak_explicit(&hopper->head, &entry_index, SOL_LOCKFREE_HOPPER_CLOSED_BIT, memory_order_acquire, memory_order_relaxed));

    assert((entry_index & SOL_LOCKFREE_POOL_ENTRY_MASK) == entry_index);

    return (uint32_t)(entry_index & SOL_LOCKFREE_POOL_ENTRY_MASK);
}