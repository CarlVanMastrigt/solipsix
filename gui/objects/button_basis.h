/**
Copyright 2025,2026 Carl van Mastrigt

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

#include "solipsix/gui/object.h"

struct sol_gui_button
{
	struct sol_gui_object base;

	/** the effect of the button being pressed, data will be passed in */
	void (*select_action)(void*);
	/** data may need cleanup (can be null) */
	void (*destroy_action)(void*);
	void* data;
};

/** constructs the base button, the specific implementations cannot be constructed as they generally contain dynamic sized buffers */
void sol_gui_button_construct_default(struct sol_gui_button* button, struct sol_gui_context* context, void(*select_action)(void*), void(*destroy_action)(void*), void* data, bool action_on_release);


/** for use in `sol_gui_object_structure_functions` for custom buttons, does necessary cleanup (i.e. calls the destroy action) */
void sol_gui_button_destroy_action(struct sol_gui_object* obj);


bool sol_gui_button_default_input_action_on_button_down(struct sol_gui_object* obj, const struct sol_input* input);
bool sol_gui_button_default_input_action_on_button_up(struct sol_gui_object* obj, const struct sol_input* input);