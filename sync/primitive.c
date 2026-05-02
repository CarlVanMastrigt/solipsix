/**
Copyright 2026 Carl van Mastrigt

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

#include "solipsix/sync/primitive.h"


static void sol_sync_primitive_placeholder_predecessor_impose_conditions(struct sol_sync_primitive* primitive, uint32_t count)
{
	/** invalid to impose conditions on dummy predecessor */
	assert(false);
}
static void sol_sync_primitive_placeholder_predecessor_signal_conditions(struct sol_sync_primitive* primitive, uint32_t count)
{
	/** invalid to impose conditions on dummy predecessor */
	assert(false);
}
static void sol_sync_primitive_placeholder_predecessor_retain_references(struct sol_sync_primitive* primitive, uint32_t count)
{
}
static void sol_sync_primitive_placeholder_predecessor_release_references(struct sol_sync_primitive* primitive, uint32_t count)
{
}
static void sol_sync_primitive_placeholder_predecessor_attach_successor(struct sol_sync_primitive* primitive, struct sol_sync_primitive* successor)
{
}

const static struct sol_sync_primitive_functions primitive_placeholder_predecessor_sync_functions =
{
    .impose_conditions  = &sol_sync_primitive_placeholder_predecessor_impose_conditions,
    .signal_conditions  = &sol_sync_primitive_placeholder_predecessor_signal_conditions,
    .retain_references  = &sol_sync_primitive_placeholder_predecessor_retain_references,
    .release_references = &sol_sync_primitive_placeholder_predecessor_release_references,
    .attach_successor  = &sol_sync_primitive_placeholder_predecessor_attach_successor,
};

static struct sol_sync_primitive placeholder_predecessor_primitive =
{
	.sync_functions = &primitive_placeholder_predecessor_sync_functions,
};

struct sol_sync_primitive* sol_sync_primitive_placeholder_predecessor_prepare(void)
{
	return &placeholder_predecessor_primitive;
}



static void sol_sync_primitive_placeholder_successor_impose_conditions(struct sol_sync_primitive* primitive, uint32_t count)
{
}
static void sol_sync_primitive_placeholder_successor_signal_conditions(struct sol_sync_primitive* primitive, uint32_t count)
{
}
static void sol_sync_primitive_placeholder_successor_retain_references(struct sol_sync_primitive* primitive, uint32_t count)
{
	/** invalid to retain dummy successor, so retention should not be necessary */
	assert(false);
}
static void sol_sync_primitive_placeholder_successor_release_references(struct sol_sync_primitive* primitive, uint32_t count)
{
	/** invalid to retain dummy successor, so retention should not be necessary */
	assert(false);
}
static void sol_sync_primitive_placeholder_successor_attach_successor(struct sol_sync_primitive* primitive, struct sol_sync_primitive* successor)
{
	/** invalid to depend on dummy successor */
	assert(false);
}

const static struct sol_sync_primitive_functions primitive_placeholder_successor_sync_functions =
{
    .impose_conditions  = &sol_sync_primitive_placeholder_successor_impose_conditions,
    .signal_conditions  = &sol_sync_primitive_placeholder_successor_signal_conditions,
    .retain_references  = &sol_sync_primitive_placeholder_successor_retain_references,
    .release_references = &sol_sync_primitive_placeholder_successor_release_references,
    .attach_successor  = &sol_sync_primitive_placeholder_successor_attach_successor,
};


static struct sol_sync_primitive placeholder_successor_primitive =
{
	.sync_functions = &primitive_placeholder_successor_sync_functions,
};

struct sol_sync_primitive* sol_sync_primitive_placeholder_successor_prepare(void)
{
	return &placeholder_successor_primitive;
}



