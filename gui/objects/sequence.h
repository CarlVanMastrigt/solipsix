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

#include "overlay/enums.h"
#include "gui/object.h"

struct sol_gui_sequence;

struct sol_gui_sequence* sol_gui_sequence_create(struct sol_gui_context* context, enum sol_overlay_orientation orientation, enum sol_gui_distribution distribution);
struct sol_gui_object* sol_gui_sequence_object_create(struct sol_gui_context* context, enum sol_overlay_orientation orientation, enum sol_gui_distribution distribution);

struct sol_gui_object* sol_gui_sequence_as_object(struct sol_gui_sequence* sequence);

