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

#include "solipsix/overlay/enums.h"

struct sol_gui_context;
struct sol_gui_object;

struct sol_gui_enterbox_handle
{
	struct sol_gui_object* object;
};

/** interface with external data */
struct sol_gui_enterbox_packet
{
	/** the user provided data used to acquire and update the string */
	void* data;
	
	/** will constantly be trying to fetch the string, will return true if it has changed
	 * `update_count` MAY be used to tell if the string has been changed externally since it was last updated */
	bool (*get_string)(void* data, char* string, uint32_t space);

	/** apply the changes of the current string
	 * if `get_string` uses the update count for determining if the value has changed then this should as well */
	void (*apply_string_update)(void* data, const char* string);

	/** data may need cleanup (can be null) */
	void (*on_destruction)(void* data);
};

struct sol_gui_enterbox_handle sol_gui_enterbox_create(struct sol_gui_context* context, uint32_t max_characters, struct sol_gui_enterbox_packet packet);