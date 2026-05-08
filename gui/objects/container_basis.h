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

#include "solipsix/math/s16_vec2.h"
#include "solipsix/math/s16_rect.h"
#include "solipsix/math/s16_extent.h"

#include "solipsix/gui/object.h"

struct sol_gui_context;

struct sol_gui_container
{
	struct sol_gui_object base;
	struct sol_gui_object* first_child;
	struct sol_gui_object* last_child;
};

void sol_gui_container_construct(struct sol_gui_container* container, struct sol_gui_context* context);

/** generic functions */
void sol_gui_container_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch);
struct sol_gui_object* sol_gui_container_hit_scan(struct sol_gui_object* obj, s16_rect position, const s16_vec2 location);
void sol_gui_container_add_child(struct sol_gui_object* obj, struct sol_gui_object* child);
void sol_gui_container_remove_child(struct sol_gui_object* obj, struct sol_gui_object* child);
// void sol_gui_container_destroy(struct sol_gui_object* obj);
void sol_gui_container_recursive_release_references(struct sol_gui_object* obj);

void sol_gui_container_distribute_position_flags(struct sol_gui_object* obj, uint32_t position_flags);
int16_t sol_gui_container_min_size_x(struct sol_gui_object* obj);
int16_t sol_gui_container_min_size_y(struct sol_gui_object* obj);
void sol_gui_container_set_extent_x(struct sol_gui_object* obj, s16_extent extent_x);
void sol_gui_container_set_extent_y(struct sol_gui_object* obj, s16_extent extent_y);


int16_t sol_gui_container_enabled_child_count(const struct sol_gui_container* container);
