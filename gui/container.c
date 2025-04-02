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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "gui/container.h"


void sol_gui_container_render(struct sol_gui_object* obj, s16_vec2 offset, struct cvm_overlay_render_batch* batch)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;

	// iterate back to front for when children can stack (first is on top)
	for(child = container->last_child; child; child = child->prev)
	{
		sol_gui_object_render(child, offset, batch);
	}
}
struct sol_gui_object* sol_gui_container_hit_scan(struct sol_gui_object* obj, s16_vec2 location)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	struct sol_gui_object* result;

	// iterate(search) front to back for when children can stack (first is on top)
	for(child = container->first_child; child; child = child->next)
	{
		result = sol_gui_object_hit_scan(child, location);
		if(result)
		{
			return result;
		}
	}
	return NULL;
}
static s16_vec2 sol_gui_container_min_size(struct sol_gui_object* obj)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_vec2 min_size = {0,0};
	s16_vec2 child_min_size;

	for(child = container->first_child; child; child = child->next)
	{
		child_min_size = sol_gui_object_min_size(child);
		min_size = s16_vec2_max(min_size, child_min_size);
	}

	return min_size;
}
static void sol_gui_container_place_content(struct sol_gui_object* obj, s16_rect content_rect)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;

	for(child = container->first_child; child; child = child->next)
	{
		sol_gui_object_place_content(child, content_rect);
	}
}
void sol_gui_container_add_child(struct sol_gui_object* obj, struct sol_gui_object* child)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;

	if(container->last_child == NULL)
	{
		assert(container->first_child == NULL);
		container->first_child = child;
		container->last_child  = child;
	}
	else
	{
		assert(container->last_child->next == NULL);

		child->prev = container->last_child;
		container->last_child->next = child;
		container->last_child = child;
	}
}
void sol_gui_container_remove_child(struct sol_gui_object* obj, struct sol_gui_object* child)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	// are the asserts really necessary?

	if(child->next == NULL)
	{
		assert(container->last_child == child);
		container->last_child = child->prev;
	}
	else
	{
		assert(container->last_child != child);
		assert(child->next->prev == child);
		child->next->prev = child->prev;
	}

	if(child->prev == NULL)
	{
		assert(container->first_child == child);
		container->first_child = child->next;
	}
	else
	{
		assert(container->first_child != child);
		assert(child->prev->next == child);
		child->prev->next = child->next;
	}

}
void sol_gui_container_destroy(struct sol_gui_object* obj)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	struct sol_gui_object* next;

	assert(container->base.reference_count == 0);

	for(child = container->first_child; child; child = next)
	{
		next = child->next;// after destroy child may be invalid, so must call this here
		sol_gui_object_release(child);
		assert(next == container->first_child);// child must remove itself from the parent as part of above function
	}
}
static const struct sol_gui_object_structure_functions sol_gui_container_structure_functions =
{
	.render        = &sol_gui_container_render,
	.hit_scan      = &sol_gui_container_hit_scan,
	.min_size      = &sol_gui_container_min_size,
	.place_content = &sol_gui_container_place_content,
	.add_child     = &sol_gui_container_add_child,
	.remove_child  = &sol_gui_container_remove_child,
	.destroy       = &sol_gui_container_destroy,
};


void sol_gui_container_construct(struct sol_gui_container* container, struct sol_gui_context* context)
{
	sol_gui_object_construct(&container->base, context);

	container->base.structure_functions = &sol_gui_container_structure_functions;
	container->base.input_action = NULL;// container cannot accept input

	container->first_child = NULL;
	container->last_child  = NULL;
}

struct sol_gui_object* sol_gui_container_create(struct sol_gui_context* context)
{
	struct sol_gui_container* container = malloc(sizeof(struct sol_gui_container));

	sol_gui_container_construct(container, context);

	return &container->base;
}