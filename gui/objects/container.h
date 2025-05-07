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

// container definition included as is common basis for custom gui objects
struct sol_gui_container
{
	struct sol_gui_object base;
	struct sol_gui_object* first_child;
	struct sol_gui_object* last_child;
};

void sol_gui_container_construct(struct sol_gui_container* container, struct sol_gui_context* context);

struct sol_gui_container* sol_gui_container_create(struct sol_gui_context* context);
struct sol_gui_object* sol_gui_container_object_create(struct sol_gui_context* context);

struct sol_gui_object* sol_gui_container_as_object(struct sol_gui_container* container);

// min_size and place content should be varied by anything that would use this, the following however may be shared
void sol_gui_container_render(struct sol_gui_object* obj, s16_rect position, struct cvm_overlay_render_batch* batch);
struct sol_gui_object* sol_gui_container_hit_scan(struct sol_gui_object* obj, s16_rect position, const s16_vec2 location);
void sol_gui_container_add_child(struct sol_gui_object* obj, struct sol_gui_object* child);
void sol_gui_container_remove_child(struct sol_gui_object* obj, struct sol_gui_object* child);
void sol_gui_container_destroy(struct sol_gui_object* obj);


// moves a child (potentioaly) relative to a sibling
// if placement is start/end sibling will be ignored (may be NULL)
// if sibling is NULL before/after will move child relative to present sibling in the spot before/after itself (basically before becomes; move backwards and after; move forwards)
// NOTE: placement is with respect to ALL objects in the container; not just active ones (could pass in ignore inactive but this raises too may questions IMO)
void sol_gui_container_move_child(struct sol_gui_container* container, struct sol_gui_object* child, struct sol_gui_object* sibling, enum sol_gui_placement placement);

int16_t sol_gui_container_enabled_child_count(const struct sol_gui_container* container);