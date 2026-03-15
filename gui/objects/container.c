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
#include <assert.h>

#include "gui/objects/container.h"

#include "sol_utils.h"

#warning container should only perform actions on enabled children
#warning is it better to remove the concept of enabled/disabled entirely? would require altering structure to add/remove elements (add in random locations)

void sol_gui_container_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;

	// iterate back to front for when children can stack (first is on top)
	for(child = container->last_child; child; child = child->prev)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			sol_gui_object_render(child, s16_rect_start(position), batch);
		}
	}
}
struct sol_gui_object* sol_gui_container_hit_scan(struct sol_gui_object* obj, s16_rect position, const s16_vec2 location)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	struct sol_gui_object* result;

	// iterate(search) front to back for when children can stack (first is on top)
	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			result = sol_gui_object_hit_scan(child, s16_rect_start(position), location);
			if(result)
			{
				return result;
			}
		}
	}
	return NULL;
}

void sol_gui_container_distribute_position_flags(struct sol_gui_object* obj, uint32_t position_flags)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			sol_gui_object_set_position_flags(child, position_flags);
		}
	}
}

int16_t sol_gui_container_min_size_x(struct sol_gui_object* obj)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	int16_t min_size_x, child_min_size_x;

	min_size_x = 0;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_min_size_x = sol_gui_object_min_size_x(child);
			min_size_x = SOL_MAX(min_size_x, child_min_size_x);
		}
	}

	return min_size_x;
}

int16_t sol_gui_container_min_size_y(struct sol_gui_object* obj)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	int16_t min_size_y, child_min_size_y;

	min_size_y = 0;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_min_size_y = sol_gui_object_min_size_y(child);
			min_size_y = SOL_MAX(min_size_y, child_min_size_y);
		}
	}

	return min_size_y;
}

void sol_gui_container_set_extent_x(struct sol_gui_object* obj, s16_extent extent_x)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;

	child_extent = s16_extent_set(0, s16_extent_size(extent_x));

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			sol_gui_object_set_extent_x(child, child_extent);
		}
	}
}

void sol_gui_container_set_extent_y(struct sol_gui_object* obj, s16_extent extent_y)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;

	child_extent = s16_extent_set(0, s16_extent_size(extent_y));

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			sol_gui_object_set_extent_y(child, child_extent);
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
	.render                       = &sol_gui_container_render,
	.hit_scan                     = &sol_gui_container_hit_scan,
	.distribute_position_flags    = &sol_gui_container_distribute_position_flags,
	.min_size_x                   = &sol_gui_container_min_size_x,
	.min_size_y                   = &sol_gui_container_min_size_y,
	.set_extent_x                 = &sol_gui_container_set_extent_x,
	.set_extent_y                 = &sol_gui_container_set_extent_y,
	.add_child                    = &sol_gui_container_add_child,
	.remove_child                 = &sol_gui_container_remove_child,
	.destroy                      = &sol_gui_container_destroy,
};


void sol_gui_container_construct(struct sol_gui_container* container, struct sol_gui_context* context)
{
	sol_gui_object_construct(&container->base, context);

	container->base.structure_functions = &sol_gui_container_structure_functions;
	container->base.input_action = NULL;// container cannot accept input

	container->first_child = NULL;
	container->last_child  = NULL;
}

struct sol_gui_container* sol_gui_container_create(struct sol_gui_context* context)
{
	struct sol_gui_container* container = malloc(sizeof(struct sol_gui_container));

	sol_gui_container_construct(container, context);

	return container;
}

struct sol_gui_object* sol_gui_container_object_create(struct sol_gui_context* context)
{
	return sol_gui_container_as_object( sol_gui_container_create(context) );
}

struct sol_gui_object* sol_gui_container_as_object(struct sol_gui_container* container)
{
	return &container->base;
}




void sol_gui_container_move_child(struct sol_gui_container* container, struct sol_gui_object* child, struct sol_gui_object* sibling, enum sol_gui_placement placement)
{
	struct sol_gui_object* container_obj = sol_gui_container_as_object(container);

	assert(container);
	assert(child);
	assert(child->parent == container_obj);
	assert(sibling == NULL || sibling->parent == container_obj);

	switch (placement) {
		case SOL_GUI_PLACEMENT_START:
			sibling = container->first_child;
			placement = SOL_GUI_PLACEMENT_BEFORE;
			break;
		case SOL_GUI_PLACEMENT_END:
			sibling = container->last_child;
			placement = SOL_GUI_PLACEMENT_AFTER;
			break;
		case SOL_GUI_PLACEMENT_AFTER:
			if(sibling == NULL)
			{
				sibling = child->next;
			}
			break;
		case SOL_GUI_PLACEMENT_BEFORE:
			if(sibling == NULL)
			{
				sibling = child->prev;
			}
			break;
	}

	if(sibling && sibling != child)//these values can be brought about by above, and are no-ops
	{
		// remove child from present location
		if(child->prev)
		{
			child->prev->next = child->next;
		}
		else
		{
			assert(child == container->first_child);
			container->first_child = child->next;
		}

		if(child->next)
		{
			child->next->prev = child->prev;
		}
		else
		{
			assert(child == container->last_child);
			container->last_child = child->prev;
		}

		// put child in the desired location
		switch (placement) {
		case SOL_GUI_PLACEMENT_AFTER:
			if(sibling->next)
			{
				sibling->next->prev = child;
				child->next = sibling->next;
			}
			else
			{
				assert(container->last_child = sibling);
				container->last_child = child;
				child->next = NULL;
			}
			sibling->next = child;
			child->prev = sibling;
			break;
		case SOL_GUI_PLACEMENT_BEFORE:
			if(sibling->prev)
			{
				sibling->prev->next = child;
				child->prev = sibling->prev;
			}
			else
			{
				assert(container->first_child = sibling);
				container->first_child = child;
				child->prev = NULL;
			}
			sibling->prev = child;
			child->next = sibling;
			break;
		default:
			assert(false);// should be cut out by above switch
		}

		// contents have (potentially) changed position (though not size) so need to reposition the containers contents
		sol_gui_object_reposition_contents(container_obj);
	}
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