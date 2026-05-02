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

#pragma once

#include "solipsix/sync/primitive.h"

struct sol_sync_queue
{
    struct sol_sync_primitive* _Atomic end_primitive;
};

void sol_sync_queue_initialise(struct sol_sync_queue* queue);
void sol_sync_queue_terminate(struct sol_sync_queue* queue);

void sol_sync_queue_enqueue_primitive(struct sol_sync_queue* queue, struct sol_sync_primitive* primitive);
void sol_sync_queue_enqueue_primitive_range(struct sol_sync_queue* queue, struct sol_sync_primitive_range primitive_range);
