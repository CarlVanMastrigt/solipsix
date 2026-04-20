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

#include "solipsix/gui/object.h"

/** button structure exposed in interface as is expected to commonly be the basis of custom gui objects (custom buttons) */
struct sol_gui_button
{
	struct sol_gui_object base;

	/** the effect of the button being pressed, data will be passed in */
	void (*select_action)(void*);
	/** data may need cleanup (can be null) */
	void (*destroy_action)(void*);
	void* data;

	/** could have set of conditions for when button is activated: on select, on release after selection, &c. **/
};

void sol_gui_button_construct(struct sol_gui_button* button, struct sol_gui_context* context, void(*select_action)(void*), void(*destroy_action)(void*), void* data);
// struct sol_gui_object* sol_gui_button_create(struct sol_gui_context* context);

/** for use in `sol_gui_object_structure_functions` for custom buttons */
void sol_gui_button_destroy_action(struct sol_gui_object* obj);

// cannot construct these as they have flexible buffers
struct sol_gui_button* sol_gui_text_button_create(struct sol_gui_context* context, void(*select_action)(void*), void(*destroy_action)(void*), void* data, char* text);
struct sol_gui_object* sol_gui_text_button_object_create(struct sol_gui_context* context, void(*select_action)(void*), void(*destroy_action)(void*), void* data, char* text);

struct sol_gui_button* sol_gui_utf8_icon_button_create(struct sol_gui_context* context, void(*select_action)(void*), void(*destroy_action)(void*), void* data, char* utf8_icon);
struct sol_gui_object* sol_gui_utf8_icon_button_object_create(struct sol_gui_context* context, void(*select_action)(void*), void(*destroy_action)(void*), void* data, char* utf8_icon);

static inline struct sol_gui_object* sol_gui_button_as_object(struct sol_gui_button* button)
{
	return &button->base;
}
