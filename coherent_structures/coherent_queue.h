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
#include <stdatomic.h>

#include "coherent_structures/lockfree_pool.h"

struct sol_coherent_queue
{
    /// faster under contention, slower when not contended
    atomic_uint_fast64_t add_index; /// will contain the stall count information
    atomic_uint_fast64_t add_fence;
    atomic_uint_fast64_t get_index;
    uint32_t* entry_indices;
    uint_fast64_t entry_mask;/// (2 ^ capacity_exponent) -1
    char* entry_data;/// backing memory managed/owned by the pool used to init the queue
    size_t entry_size;
    size_t capacity_exponent;
};


void sol_coherent_queue_initialise(struct sol_coherent_queue* queue, struct sol_lockfree_pool* pool);
void sol_coherent_queue_terminate(struct sol_coherent_queue* queue);

void  sol_coherent_queue_push(struct sol_coherent_queue* queue, void* entry);
void* sol_coherent_queue_pull(struct sol_coherent_queue* queue);///returns NULL on failure (b/c no elements remain)
