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

#include "gui/object.h"

// button definition included as is common basis for custom gui objects
struct sol_gui_button
{
	struct sol_gui_object base;
	void (*select_action)(void*);
	void* data;
};

void sol_gui_button_construct(struct sol_gui_button* button, struct sol_gui_context* context, void(*select_action)(void*), void* data);
// struct sol_gui_object* sol_gui_button_create(struct sol_gui_context* context);


// cannot construct these as they have flexible buffers

struct sol_gui_object* sol_gui_text_button_create(struct sol_gui_context* context, void(*select_action)(void*), void* data, char* text);

struct sol_gui_object* sol_gui_utf8_icon_button_create(struct sol_gui_context* context, void(*select_action)(void*), void* data, char* utf8_icon);


