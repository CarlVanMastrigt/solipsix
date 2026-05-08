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

#include "solipsix/gui/objects/floating_region.h"
#include "solipsix/gui/constants.h"

struct sol_gui_context;
struct sol_gui_object;

struct sol_gui_anchor_handle
{
	struct sol_gui_object* object;
};

struct sol_gui_anchor_handle sol_gui_text_anchor_create(struct sol_gui_context* context, char* text, struct sol_gui_floating_region_handle floating_region);

void sol_gui_anchor_set_relative_placement(struct sol_gui_anchor_handle anchor, struct sol_gui_object* reference_object, enum sol_gui_relative_placement anchor_placement_x, enum sol_gui_relative_placement anchor_placement_y);



#include "solipsix/gui/objects/button.h"

/** to use this button must have been created with a null packet
 * this setup pattern minimises the chances of doing something catastrophically wrong */
void sol_gui_button_set_anchor_toggle_button_packet(struct sol_gui_button_handle button_object, struct sol_gui_anchor_handle anchor_handle, enum sol_gui_relative_placement anchor_placement_x, enum sol_gui_relative_placement anchor_placement_y);