/**
Copyright 2025 Carl van Mastrigt

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

#include "sol_sync.h"
#include "sync/queue.h"

void sol_sync_queue_initialise(struct sol_sync_queue* queue)
{
    struct sol_sync_primitive* placeholder_start = sol_sync_primitive_placeholder_predecessor_prepare();
    atomic_init(&queue->end_primitive, placeholder_start);
}

#warning have option to wait on the last entry on the queue to complete?
void sol_sync_queue_terminate(struct sol_sync_queue* queue)
{
    struct sol_sync_primitive* end_primitive;

    end_primitive = atomic_exchange_explicit(&queue->end_primitive, NULL, memory_order_relaxed);
    assert(end_primitive); /** don't double release */

    sol_sync_primitive_release_references(end_primitive, 1);
}

static inline void sol_sync_queue_enqueue_primitives_internal(struct sol_sync_queue* queue, struct sol_sync_primitive* first_primitive, struct sol_sync_primitive* last_primitive)
{
    struct sol_sync_primitive* old_end_primitive;

    sol_sync_primitive_retain_references(last_primitive, 1);

    old_end_primitive = atomic_exchange_explicit(&queue->end_primitive, last_primitive, memory_order_relaxed);

    assert(old_end_primitive);/** dont use a terminated queue */

    sol_sync_primitive_attach_successor(old_end_primitive, first_primitive);
    sol_sync_primitive_release_references(old_end_primitive, 1);
}

void sol_sync_queue_enqueue_primitive_range(struct sol_sync_queue* queue, struct sol_sync_primitive_range primitive_range)
{
    sol_sync_queue_enqueue_primitives_internal(queue, primitive_range.first, primitive_range.last);
}

void sol_sync_queue_enqueue_primitive(struct sol_sync_queue* queue, struct sol_sync_primitive* primitive)
{
    sol_sync_queue_enqueue_primitives_internal(queue, primitive, primitive);
}