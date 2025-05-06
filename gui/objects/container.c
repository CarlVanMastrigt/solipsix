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

#include "gui/objects/container.h"

#warning container should only perform actions on enabled children
#warning is it better to remove the concept of enabled/disabled entirely? would require altering structure to add/remove elements (add in random locations)

void sol_gui_container_render(struct sol_gui_object* obj, s16_vec2 offset, struct cvm_overlay_render_batch* batch)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;

	// child locations are relative to parent
	offset = s16_vec2_add(offset, obj->position.start);

	// iterate back to front for when children can stack (first is on top)
	for(child = container->last_child; child; child = child->prev)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			sol_gui_object_render(child, offset, batch);
		}
	}
}
struct sol_gui_object* sol_gui_container_hit_scan(struct sol_gui_object* obj, s16_vec2 location)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	struct sol_gui_object* result;

	// child locations are relative to parent
	location = s16_vec2_sub(location, obj->position.start);

	// iterate(search) front to back for when children can stack (first is on top)
	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			result = sol_gui_object_hit_scan(child, location);
			if(result)
			{
				return result;
			}
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
	uint32_t position_flags;

	position_flags = obj->flags & SOL_GUI_OBJECT_POSITION_FLAGS_ALL;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_min_size = sol_gui_object_min_size(child, position_flags);
			min_size = s16_vec2_max(min_size, child_min_size);
		}
	}

	return min_size;
}
static void sol_gui_container_place_content(struct sol_gui_object* obj, s16_rect content_rect)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;

	obj->position = content_rect;
#warning move above to sol_gui_object_place_content and make below unnecessary by having it be a relative value!
	// this would also make it `content_size` instead which would make everything MUCH SIMPLLER (though may be better to keep it a rect... not sure)
	content_rect = s16_rect_move_start_to_origin(content_rect);

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			sol_gui_object_place_content(child, content_rect);
		}
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

	while((child = container->first_child))
	{
		sol_gui_object_remove_child(obj, child);
		sol_gui_object_release(child);
		// ^ this line is responsible for recursive deletion of gui objects, implicitly a container "gains ownership" of it's children when they are added
		// specifically they take implicit ownership of the initial reference all gui objects start with
		// they also lose ownership when the child is removed through normal means
	}

	assert(container->base.reference_count == 0);
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



int16_t sol_gui_container_enabled_child_count(const struct sol_gui_container* container)
{
	struct sol_gui_object* child;
	int16_t enabled_child_count = 0;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			enabled_child_count++;
		}
	}

	return enabled_child_count;
}