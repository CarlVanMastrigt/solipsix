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

#include "math/s16_vec2.h"
#include "math/s16_rect.h"

#include "solipsix/gui/constants.h"


struct sol_gui_theme;

s16_vec2 sol_gui_required_pacement_offset(s16_rect rect, const struct sol_gui_theme* theme, s16_rect reference_rect, enum sol_gui_relative_placement placement_x, enum sol_gui_relative_placement placement_y);


