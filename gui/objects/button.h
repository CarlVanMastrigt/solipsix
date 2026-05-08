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

struct sol_gui_context;
struct sol_gui_object;

struct sol_gui_button_packet
{
	void(*action )(void*);
	void(*destroy)(void*);
	void* data;
};

#define SOL_GUI_BUTTON_PACKET_NULL ((struct sol_gui_button_packet){.action = NULL, .destroy = NULL, .data = NULL})

struct sol_gui_button_handle
{
	struct sol_gui_object* object;
};

/** button should have been init with a null packet for this to be valid to call */
void sol_gui_button_set_packet(struct sol_gui_button_handle button_handle, struct sol_gui_button_packet packet);


#warning should this just be union? could have different (custom) keyword `handle` ? which would be union?
struct sol_gui_text_button_handle
{
	/** yes, this pattern is a little crude, but it does provide slightly better type safety */
	struct sol_gui_button_handle button;
};

struct sol_gui_text_button_handle sol_gui_text_button_create(struct sol_gui_context* context, struct sol_gui_button_packet packet, char* text);


struct sol_gui_utf8_icon_button_handle
{
	/** yes, this pattern is a little crude, but it does provide slightly better type safety */
	struct sol_gui_button_handle button;
};

struct sol_gui_utf8_icon_button_handle sol_gui_utf8_icon_button_create(struct sol_gui_context* context, struct sol_gui_button_packet packet, char* utf8_icon);


