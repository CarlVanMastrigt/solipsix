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
#include <stdint.h>

struct sol_gui_context;
struct sol_gui_object;

struct sol_gui_enterbox_handle
{
	struct sol_gui_object* object;
};

#define SOL_GUI_ENTERBOX_TEXT_FLAG_CHANGED 0x0001

/** text provided by the `get_string` function should be displayed 
 * greyed out and should be erased on focus, usually provided back when text is empty 
 * update will not be called unless this text was changed */
#define SOL_GUI_ENTERBOX_TEXT_FLAG_PLACEHOLDER 0x0002

/** interface with external data */
struct sol_gui_enterbox_packet
{
	/** the user provided data used to acquire and update the string */
	void* data;
	
	/** will constantly be used to try to fetch the string
	 * space is the size of the string buffer provided, including terminating zero
	 * should return set of flags appropriate to the changed text
	 * string should be null terminated either way but will be enforced to not read OOB by injecting one after this is called */
	uint16_t (*get_string)(void* data, char* string_buffer, uint16_t max_glyphs, uint16_t max_strlen);

	/** apply the changes of the current string, will be null terminated
	 * if `get_string` uses the update count for determining if the value has changed then this should as well 
	 * `get_string` should be expected to be called after this before its next use (e.g. next render) */
	void (*apply_string_update)(void* data, const char* string);

	/** data may need cleanup (can be null) */
	void (*on_destruction)(void* data);
};

/** `max_strlen` is max text buffer size; max bytes; and does not include null termination 
 * `initial_text` may be null, to indicate an empty string */
struct sol_gui_enterbox_handle sol_gui_enterbox_create(struct sol_gui_context* context, uint16_t max_glyphs, uint16_t max_strlen, struct sol_gui_enterbox_packet packet);