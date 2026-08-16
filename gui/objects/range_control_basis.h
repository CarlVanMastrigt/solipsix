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

#include "math/s16_extent.h"
#include "solipsix/gui/object.h"
#include "solipsix/gui/objects/range_control.h"
#include "solipsix/overlay/enums.h"


/** range control structure exposed in interface because it's a reasonable basis for custom gui objects */

struct sol_gui_range_control
{
	struct sol_gui_object base;

	struct sol_gui_range_control_packet packet;

	enum sol_overlay_orientation orientation;

	/** used by the theme to determine the minimum size of the bar to render;
	 * usually corresponding to visually distinct render states, or pixel offsets of the sliding bar 
	 * will default to 64 if not provided or set explicitly */
	int16_t min_gradations;

	/** when using non-mouse inputs this is the maximum number of times to change a value to cover the whole range, 
	 * adjustment will round to one of these gradations and will be at least one in the case of an integer range,
	 * will default to 64 if not provided or set explicitly */
	int16_t max_discrete_gradations;

	/** based on the initial click point; what is the range on screen that 
	 * may be offset from actual selection range if inside a special selection bar 
	 * is in the axis (x|y) corresponding to the range controls orientation (horizontal|vertical) 
	 * this is stored in an attempt to handle UI being changed while a selection bar is actively being changed
	 * TODO: could store absolute *display* position to handle window reposition as well */
	s16_extent relative_selection_extent;
};

void sol_gui_range_control_construct(struct sol_gui_range_control* range_control, struct sol_gui_context* context, struct sol_gui_range_control_packet packet, enum sol_overlay_orientation orientation);

void sol_gui_range_control_destroy(struct sol_gui_object* obj);
