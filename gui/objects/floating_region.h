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

struct sol_gui_context;
struct sol_gui_object;

#include "solipsix/math/s16_vec2.h"
#include "solipsix/gui/constants.h"
#include "solipsix/gui/objects/button.h"

struct sol_gui_floating_region_handle
{
	struct sol_gui_object* object;
};

struct sol_gui_floating_region_handle sol_gui_floating_region_create(struct sol_gui_context* context, uint32_t position_flags_to_preserve);

#warning should be possible to abstract (some of?) the following functions, calculate placement devoid of reference objects, then apply using a standardised offset function

/** start of contained content (will usually be a panel) with respect to the floating region */ 
void sol_gui_floating_region_set_content_relative_offset(struct sol_gui_floating_region_handle floating_region, s16_vec2 offset);
/** start of contained content (will usually be a panel) with respect to the base context space (which is usually the screen) */ 
void sol_gui_floating_region_set_content_absolute_offset(struct sol_gui_floating_region_handle floating_region, s16_vec2 offset);

/** get the (sole) child of the floating region, which will be NULL if none has been asigned */
struct sol_gui_object* sol_gui_floating_region_get_content(struct sol_gui_floating_region_handle floating_region);


#include "solipsix/gui/objects/button.h"

/** toggle a floating window such that the reference child is aligned (as described) with the button used to toggle the floating_region
 * basically just sets up the button to toggle the floating regions child and call `sol_gui_floating_region_set_relative_placement` with the button as `reference_external_object` on toggle
 * to use this button must have been created with a null packet
 * this setup pattern minimises the chances of doing something catastrophically wrong */
void sol_gui_button_set_floating_region_toggle_button_packet(
	struct sol_gui_button_handle button, 
	struct sol_gui_floating_region_handle floating_region, 
	struct sol_gui_object* reference_decendant, 
	enum sol_gui_relative_placement placement_x, 
	enum sol_gui_relative_placement placement_y);



