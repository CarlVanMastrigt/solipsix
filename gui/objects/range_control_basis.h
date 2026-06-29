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
#include "solipsix/gui/objects/range_control.h"
#include "solipsix/overlay/enums.h"


/** range control structure exposed in interface as is a reasonable basis for custom gui objects */
struct sol_gui_range_control
{
	struct sol_gui_object base;

	struct sol_gui_range_control_packet packet;

	enum sol_overlay_orientation orientation;

	int16_t min_gradations;/** will default to 16 if not provided */

	int16_t interior_selection_offset;/** preserved internal state */
};

void sol_gui_range_control_construct(struct sol_gui_range_control* range_control, struct sol_gui_context* context, struct sol_gui_range_control_packet packet, enum sol_overlay_orientation orientation);

void sol_gui_range_control_destroy(struct sol_gui_object* obj);
