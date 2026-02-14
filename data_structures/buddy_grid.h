/**
Copyright 2021,2022,2024,2025,2026 Carl van Mastrigt

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

/** note derived/extracted from image atlas algorithm */
#include <inttypes.h>
#include "math/u16_vec2.h"

struct sol_buddy_grid_location
{
	u16_vec2 xy_offset;
	uint8_t array_layer;
};

struct sol_buddy_grid_description
{
	uint8_t image_x_dimension_exponent;
	uint8_t image_y_dimension_exponent;
	uint8_t image_array_dimension;
};

/** underlying type sufficiently complex as to not expose its internals here (so reqire allocation of struct) */
struct sol_buddy_grid;

struct sol_buddy_grid* sol_buddy_grid_create(struct sol_buddy_grid_description description);
void sol_buddy_grid_destroy(struct sol_buddy_grid* grid);

/** the acquired index must be released before destroying the buddy grid 
 * the index can be used as an index into an externally managed array 
 * (indices returned from an acquire call are guaranteed to not exceed the number of allocations at that time) */
bool sol_buddy_grid_acquire(struct sol_buddy_grid* grid, u16_vec2 size, uint32_t* index);
void sol_buddy_grid_release(struct sol_buddy_grid* grid, uint32_t index);

struct sol_buddy_grid_location sol_buddy_grid_get_location(struct sol_buddy_grid* grid, uint32_t index);

bool sol_buddy_grid_has_space(struct sol_buddy_grid* grid, u16_vec2 size);


/** TODO: look into permitting allocating with some kind of bias towards the end, preferable for very short lived allocations to be separated from long lived ones */
