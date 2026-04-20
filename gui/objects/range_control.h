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

#include "solipsix/gui/object.h"
#include "solipsix/gui/range_control_distribution.h"
#include "solipsix/overlay/enums.h"

/** range control structure exposed in interface as is a reasonable basis for custom gui objects */
struct sol_gui_range_control
{
	struct sol_gui_object base;

	/** the user provided data used to acquire and update the range (context?) */
	void* data;

	/** fetch the current range (being controlled) on demand*/
	void (*get_distribution)(const void* data, struct sol_range_control_distribution* distribution);

	/** update the range, range control operates on some number of pixels out of a possible range, 
	 * this provides the distilled information to an update function in its raw form;
	 * the user selected value in the range = `numerator/denominator` */
	void (*update_action)(void* data, int16_t numerator, int16_t denominator);
	/** data may need cleanup (can be null) */
	void (*destroy_action)(void* data);

	enum sol_overlay_orientation orientation;

	int16_t min_gradations;/** will default to 16 if not provided */

	int16_t interior_selection_offset;/** preserved internal state */
};

void sol_gui_range_control_construct(struct sol_gui_range_control* range_control, struct sol_gui_context* context, enum sol_overlay_orientation orientation, void(*get_distribution)(const void*, struct sol_range_control_distribution*), void(*update_action)(void*, int16_t, int16_t), void(*destroy_action)(void*), void* data);

void sol_gui_range_control_destroy_action(struct sol_gui_object* obj);

// cannot construct these as they have flexible buffers
struct sol_gui_range_control* sol_gui_range_control_create(struct sol_gui_context* context, enum sol_overlay_orientation orientation, void(*get_distribution)(const void*, struct sol_range_control_distribution*), void(*update_action)(void*, int16_t, int16_t), void(*destroy_action)(void*), void* data);
struct sol_gui_object* sol_gui_range_control_object_create(struct sol_gui_context* context, enum sol_overlay_orientation orientation, void(*get_distribution)(const void*, struct sol_range_control_distribution*), void(*update_action)(void*, int16_t, int16_t), void(*destroy_action)(void*), void* data);

static inline struct sol_gui_object* sol_gui_range_control_as_object(struct sol_gui_range_control* range_control)
{
	return &range_control->base;
}