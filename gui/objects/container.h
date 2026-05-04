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

#include "solipsix/gui/enums.h"

struct sol_gui_context;
struct sol_gui_object;

struct sol_gui_container_handle
{
	struct sol_gui_object* object;
};

struct sol_gui_container_handle sol_gui_container_create(struct sol_gui_context* context);

/** moves a child (potentioaly) relative to a sibling
 * if placement is start/end sibling will be ignored (may be NULL)
 * if sibling is NULL before/after will move child relative to present sibling in the spot before/after itself (basically before becomes; move backwards and after; move forwards)
 * NOTE: placement is with respect to ALL objects in the container; not just active ones (could pass in ignore inactive but this raises too may questions IMO) */
void sol_gui_container_move_child(struct sol_gui_container_handle container, struct sol_gui_object* child, struct sol_gui_object* sibling, enum sol_gui_placement placement);

