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

#pragma once

#include <inttypes.h>

#include "solipsix/overlay/enums.h"
#include "solipsix/gui/range_control_distribution.h"

struct sol_gui_context;
struct sol_gui_object;

/** interface with external data */
struct sol_gui_range_control_packet
{
	/** the user provided data used to acquire and update the range */
	void* data;

	/** fetch the current range (being controlled) on demand*/
	void (*get_distribution)(const void* data, struct sol_range_control_distribution* distribution);

	/** update the range, range control operates on some number of pixels out of a possible range, 
	 * this provides the distilled information to an update function in its raw form;
	 * the user selected value in the range = `numerator/denominator` 
	 * finalised will be true if the value is no longer expected to be updated after this */
	void (*apply_distribution_update)(void* data, int16_t numerator, int16_t denominator, bool final_update);

	/** data may need cleanup (can be null) */
	void (*on_destruction)(void* data);
};


struct sol_gui_range_control_handle
{
	struct sol_gui_object* object;
};

struct sol_gui_range_control_handle sol_gui_range_control_create(struct sol_gui_context* context, struct sol_gui_range_control_packet packet, enum sol_overlay_orientation orientation);