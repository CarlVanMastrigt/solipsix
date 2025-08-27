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

#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>

struct sol_buffer
{
	char* allocation;
	uint32_t total_space;
	uint32_t used_space;
};

/** cannot be terminated */
struct sol_buffer_allocation
{
	char* allocation;
	uint32_t size;
	uint32_t offset;
};

static inline void sol_buffer_initialise(struct sol_buffer* b, uint32_t space)
{
	assert(space);
	b->allocation = malloc(space);
	b->total_space = space;
	b->used_space = 0;
}
static inline void sol_buffer_terminate(struct sol_buffer* b)
{
	free(b->allocation);
}

static inline void sol_buffer_reset(struct sol_buffer* b)
{
	b->used_space = 0;
}

/** will return empty allocation if insufficient space remains */
static inline struct sol_buffer_allocation sol_buffer_fetch_allocation(struct sol_buffer* b, uint32_t size, uint32_t alignment)
{
	const uint32_t offset = (b->used_space + alignment - 1) & (- alignment);

	if(offset + size > b->total_space)
	{
		return (struct sol_buffer_allocation)
		{
			.allocation = NULL,
			.size = 0,
			.offset = 0,
		};
	}
	else
	{
		b->used_space = offset + size;
		return (struct sol_buffer_allocation)
		{
			.allocation = b->allocation + offset,
			.size = size,
			.offset = offset,
		};
	}
}

