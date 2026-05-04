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

struct sol_gui_range_control_handle
{
	struct sol_gui_object* object;
};

struct sol_gui_range_control_handle sol_gui_range_control_create(struct sol_gui_context* context, enum sol_overlay_orientation orientation, void(*get_distribution)(const void*, struct sol_range_control_distribution*), void(*update_action)(void*, int16_t, int16_t), void(*destroy_action)(void*), void* data);