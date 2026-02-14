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
#include "gui/objects/sequence.h"
#include "sol_utils.h"


struct sol_gui_sequence
{
	struct sol_gui_container base;// based off a container
};
// not worth storing max enabled child size and enabled child count just for between the `min_size` and `place_content` functions

// would be good to remove vector direct component access (sum and max then splice)

static s16_vec2 sol_gui_sequence_min_size_horizontal(struct sol_gui_object* obj)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	struct sol_gui_object* prev_enabled_child;// need to operate on prev_child to know which is the last enabled child
	s16_vec2 min_size;
	s16_vec2 child_min_size;
	uint32_t position_flags;

	position_flags = container->base.flags & SOL_GUI_OBJECT_POSITION_FLAGS_ALL;
	prev_enabled_child = NULL;
	min_size = s16_vec2_set(0, 0);

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			if(prev_enabled_child)
			{
				// prev_enabled_child is not last if child (another active child) is present
				child_min_size = sol_gui_object_min_size(prev_enabled_child, position_flags & ~SOL_GUI_OBJECT_POSITION_FLAG_LAST_X);
				min_size.x += child_min_size.x;
				min_size.y = SOL_MAX(min_size.y, child_min_size.y);
				position_flags &= ~SOL_GUI_OBJECT_POSITION_FLAG_FIRST_X;/// prev child was first, subsequent entries will not be first
			}
			prev_enabled_child = child;
		}
	}
	if(prev_enabled_child)
	{
		// may be last (inherited from parent)
		child_min_size = sol_gui_object_min_size(prev_enabled_child, position_flags);
		min_size.x += child_min_size.x;
		min_size.y = SOL_MAX(min_size.y, child_min_size.y);
	}

	return min_size;
}

static s16_vec2 sol_gui_sequence_min_size_vertical(struct sol_gui_object* obj)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	struct sol_gui_object* prev_enabled_child;// need to operate on prev_child to know which is the last enabled child
	s16_vec2 min_size;
	s16_vec2 child_min_size;
	uint32_t position_flags;

	position_flags = container->base.flags & SOL_GUI_OBJECT_POSITION_FLAGS_ALL;
	prev_enabled_child = NULL;
	min_size = s16_vec2_set(0, 0);

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			if(prev_enabled_child)
			{
				// prev_enabled_child is not last if child (another active child) is present
				child_min_size = sol_gui_object_min_size(prev_enabled_child, position_flags & ~SOL_GUI_OBJECT_POSITION_FLAG_LAST_Y);
				min_size.x = SOL_MAX(min_size.x, child_min_size.x);
				min_size.y += child_min_size.y;
				position_flags &= ~SOL_GUI_OBJECT_POSITION_FLAG_FIRST_Y;/// prev child was first, subsequent entries will not be first
			}
			prev_enabled_child = child;
		}
	}
	if(prev_enabled_child)
	{
		// may be last (inherited from parent)
		child_min_size = sol_gui_object_min_size(prev_enabled_child, position_flags);
		min_size.x = SOL_MAX(min_size.x, child_min_size.x);
		min_size.y += child_min_size.y;
	}

	return min_size;
}


static s16_vec2 sol_gui_sequence_min_size_horizontal_uniform(struct sol_gui_object* obj)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	struct sol_gui_object* prev_enabled_child;// need to operate on prev_child to know which is the last enabled child
	s16_vec2 largest_child;
	s16_vec2 child_min_size;
	uint32_t position_flags;
	int16_t enabled_child_count;

	position_flags = container->base.flags & SOL_GUI_OBJECT_POSITION_FLAGS_ALL;
	prev_enabled_child = NULL;
	largest_child = s16_vec2_set(0, 0);
	enabled_child_count = 0;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			if(prev_enabled_child)
			{
				// prev_enabled_child is not last if child (another active child) is present
				child_min_size = sol_gui_object_min_size(prev_enabled_child, position_flags & ~SOL_GUI_OBJECT_POSITION_FLAG_LAST_X);
				largest_child = s16_vec2_max(largest_child, child_min_size);
				position_flags &= ~SOL_GUI_OBJECT_POSITION_FLAG_FIRST_X;/// prev child was first, subsequent entries will not be first
			}
			prev_enabled_child = child;
			enabled_child_count++;
		}
	}
	if(prev_enabled_child)
	{
		// may be last (inherited from parent)
		child_min_size = sol_gui_object_min_size(prev_enabled_child, position_flags);
		largest_child = s16_vec2_max(largest_child, child_min_size);
	}

	return s16_vec2_mul(largest_child, s16_vec2_set(enabled_child_count, 1));
}

static s16_vec2 sol_gui_sequence_min_size_vertical_uniform(struct sol_gui_object* obj)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	struct sol_gui_object* prev_enabled_child;// need to operate on prev_child to know which is the last enabled child
	s16_vec2 largest_child;
	s16_vec2 child_min_size;
	uint32_t position_flags;
	int16_t enabled_child_count;

	position_flags = container->base.flags & SOL_GUI_OBJECT_POSITION_FLAGS_ALL;
	prev_enabled_child = NULL;
	largest_child = s16_vec2_set(0, 0);
	enabled_child_count = 0;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			if(prev_enabled_child)
			{
				// prev_enabled_child is not last if child (another active child) is present
				child_min_size = sol_gui_object_min_size(prev_enabled_child, position_flags & ~SOL_GUI_OBJECT_POSITION_FLAG_LAST_X);
				largest_child = s16_vec2_max(largest_child, child_min_size);
				position_flags &= ~SOL_GUI_OBJECT_POSITION_FLAG_FIRST_X;/// prev child was first, subsequent entries will not be first
			}
			prev_enabled_child = child;
			enabled_child_count++;
		}
	}
	if(prev_enabled_child)
	{
		// may be last (inherited from parent)
		child_min_size = sol_gui_object_min_size(prev_enabled_child, position_flags);
		largest_child = s16_vec2_max(largest_child, child_min_size);
	}

	return s16_vec2_mul(largest_child, s16_vec2_set(1, enabled_child_count));
}



#warning largest child must be calculated in order to accurately place content for uniformly sized sequences!

static void sol_gui_sequence_place_content_horizontal_start(struct sol_gui_object* obj, s16_vec2 dimensions)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_rect child_rect;

	child_rect = s16_rect_at_origin_with_size(dimensions);

	for(child = container->last_child; child; child = child->prev)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_rect.start.x = child_rect.end.x - child->min_size.x;
			sol_gui_object_place_content(child, child_rect);
			child_rect.end.x = child_rect.start.x;
		}
	}
}

static void sol_gui_sequence_place_content_vertical_start(struct sol_gui_object* obj, s16_vec2 dimensions)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_rect child_rect;

	child_rect = s16_rect_at_origin_with_size(dimensions);

	for(child = container->last_child; child; child = child->prev)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_rect.start.y = child_rect.end.y - child->min_size.y;
			sol_gui_object_place_content(child, child_rect);
			child_rect.end.y = child_rect.start.y;
		}
	}
}

static void sol_gui_sequence_place_content_horizontal_end(struct sol_gui_object* obj, s16_vec2 dimensions)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_rect child_rect;

	child_rect.start = s16_vec2_set(0, 0);
	child_rect.end = s16_vec2_set(0, dimensions.y);

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_rect.end.x += child->min_size.x;
			sol_gui_object_place_content(child, child_rect);
			child_rect.start.x = child_rect.end.x;// prep for next child
		}
	}
}

static void sol_gui_sequence_place_content_vertical_end(struct sol_gui_object* obj, s16_vec2 dimensions)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_rect child_rect;

	child_rect.start = s16_vec2_set(0, 0);
	child_rect.end = s16_vec2_set(dimensions.x, 0);

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_rect.end.y += child->min_size.y;
			sol_gui_object_place_content(child, child_rect);
			child_rect.start.y = child_rect.end.y;// prep for next child
		}
	}
}

static void sol_gui_sequence_place_content_horizontal_first(struct sol_gui_object* obj, s16_vec2 dimensions)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_rect child_rect;

	child_rect = s16_rect_at_origin_with_size(dimensions);
	child_rect.end.x -= obj->min_size.x;
	// all unconsumed space goes to the first entry, (which will have it's min size added)
	// above leaves just enough space for all children to be placed with minimum x size

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_rect.end.x += child->min_size.x;// step end forward
			sol_gui_object_place_content(child, child_rect);
			child_rect.start.x = child_rect.end.x;// bring start forward to match (end of this child is next enabled childs start)
		}
	}
}

static void sol_gui_sequence_place_content_vertical_first(struct sol_gui_object* obj, s16_vec2 dimensions)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_rect child_rect;

	child_rect = s16_rect_at_origin_with_size(dimensions);
	child_rect.end.y -= obj->min_size.y;
	// all unconsumed space goes to the first entry, (which will have it's min size added)
	// above leaves just enough space for all children to be placed with minimum y size

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_rect.end.y += child->min_size.y;// step end forward
			sol_gui_object_place_content(child, child_rect);
			child_rect.start.y = child_rect.end.y;// bring start forward to match (end of this child is next enabled childs start)
		}
	}
}

static void sol_gui_sequence_place_content_horizontal_last(struct sol_gui_object* obj, s16_vec2 dimensions)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_rect child_rect;

	child_rect.end = dimensions;
	child_rect.start = s16_vec2_set(obj->min_size.x, 0);
	// current size of child rect is remaining space in obj, which will be "given" to "first" enabled child encountered
	// because this iterates backwards the first enabled child encountered will be the last enabled widget
	for(child = container->last_child; child; child = child->prev)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_rect.start.x -= child->min_size.x;
			sol_gui_object_place_content(child, child_rect);
			child_rect.end.x = child_rect.start.x;// set end for next child encountered
		}
	}
}

static void sol_gui_sequence_place_content_vertical_last(struct sol_gui_object* obj, s16_vec2 dimensions)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_rect child_rect;

	child_rect.end = dimensions;
	child_rect.start = s16_vec2_set(0, obj->min_size.y);
	// current size of child rect is remaining space in obj, which will be "given" to "first" enabled child encountered
	// because this iterates backwards the first enabled child encountered will be the last enabled widget
	for(child = container->last_child; child; child = child->prev)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_rect.start.y -= child->min_size.y;
			sol_gui_object_place_content(child, child_rect);
			child_rect.end.y = child_rect.start.y;// set end for next child encountered
		}
	}
}


static void sol_gui_sequence_place_content_horizontal_uniform(struct sol_gui_object* obj, s16_vec2 dimensions)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_rect child_rect;
	int16_t child_count, child_index, width, remainder;


	child_rect.end = s16_vec2_set(0, dimensions.y);
	child_rect.start = s16_vec2_set(0, 0);

	child_count = sol_gui_container_enabled_child_count(container);
	child_index = 0;

	width = dimensions.x / child_count;
	remainder = dimensions.x % child_count;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_rect.end.x += (child_index < remainder) ? width + 1 : width;
			sol_gui_object_place_content(child, child_rect);
			child_rect.start.x = child_rect.end.x;// set start for next child encountered to this childs start
			child_index++;
		}
	}

	assert(child_rect.start.x == dimensions.x);
	assert(child_index == child_count);
}

static void sol_gui_sequence_place_content_vertical_uniform(struct sol_gui_object* obj, s16_vec2 dimensions)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_rect child_rect;
	int16_t child_count, child_index, height, remainder;



	child_rect.end = s16_vec2_set(dimensions.x, 0);
	child_rect.start = s16_vec2_set(0, 0);

	child_count = sol_gui_container_enabled_child_count(container);
	child_index = 0;

	height = dimensions.y / child_count;
	remainder = dimensions.y % child_count;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_rect.end.y += (child_index < remainder) ? height + 1 : height;
			sol_gui_object_place_content(child, child_rect);
			child_rect.start.y = child_rect.end.y;// set start for next child encountered to this childs start
			child_index++;
		}
	}

	assert(child_rect.start.y == dimensions.y);
	assert(child_index == child_count);
}


// horizontal
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_horizontal_start =
{
	.render        = &sol_gui_container_render,
	.hit_scan      = &sol_gui_container_hit_scan,
	.min_size      = &sol_gui_sequence_min_size_horizontal,
	.place_content = &sol_gui_sequence_place_content_horizontal_start,
	.add_child     = &sol_gui_container_add_child,
	.remove_child  = &sol_gui_container_remove_child,
	.destroy       = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_horizontal_end =
{
	.render        = &sol_gui_container_render,
	.hit_scan      = &sol_gui_container_hit_scan,
	.min_size      = &sol_gui_sequence_min_size_horizontal,
	.place_content = &sol_gui_sequence_place_content_horizontal_end,
	.add_child     = &sol_gui_container_add_child,
	.remove_child  = &sol_gui_container_remove_child,
	.destroy       = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_horizontal_first =
{
	.render        = &sol_gui_container_render,
	.hit_scan      = &sol_gui_container_hit_scan,
	.min_size      = &sol_gui_sequence_min_size_horizontal,
	.place_content = &sol_gui_sequence_place_content_horizontal_first,
	.add_child     = &sol_gui_container_add_child,
	.remove_child  = &sol_gui_container_remove_child,
	.destroy       = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_horizontal_last =
{
	.render        = &sol_gui_container_render,
	.hit_scan      = &sol_gui_container_hit_scan,
	.min_size      = &sol_gui_sequence_min_size_horizontal,
	.place_content = &sol_gui_sequence_place_content_horizontal_last,
	.add_child     = &sol_gui_container_add_child,
	.remove_child  = &sol_gui_container_remove_child,
	.destroy       = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_horizontal_uniform =
{
	.render        = &sol_gui_container_render,
	.hit_scan      = &sol_gui_container_hit_scan,
	.min_size      = &sol_gui_sequence_min_size_horizontal_uniform,
	.place_content = &sol_gui_sequence_place_content_horizontal_uniform,
	.add_child     = &sol_gui_container_add_child,
	.remove_child  = &sol_gui_container_remove_child,
	.destroy       = &sol_gui_container_destroy,
};

// vertical
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_vertical_start =
{
	.render        = &sol_gui_container_render,
	.hit_scan      = &sol_gui_container_hit_scan,
	.min_size      = &sol_gui_sequence_min_size_vertical,
	.place_content = &sol_gui_sequence_place_content_vertical_start,
	.add_child     = &sol_gui_container_add_child,
	.remove_child  = &sol_gui_container_remove_child,
	.destroy       = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_vertical_end =
{
	.render        = &sol_gui_container_render,
	.hit_scan      = &sol_gui_container_hit_scan,
	.min_size      = &sol_gui_sequence_min_size_vertical,
	.place_content = &sol_gui_sequence_place_content_vertical_end,
	.add_child     = &sol_gui_container_add_child,
	.remove_child  = &sol_gui_container_remove_child,
	.destroy       = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_vertical_first =
{
	.render        = &sol_gui_container_render,
	.hit_scan      = &sol_gui_container_hit_scan,
	.min_size      = &sol_gui_sequence_min_size_vertical,
	.place_content = &sol_gui_sequence_place_content_vertical_first,
	.add_child     = &sol_gui_container_add_child,
	.remove_child  = &sol_gui_container_remove_child,
	.destroy       = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_vertical_last =
{
	.render        = &sol_gui_container_render,
	.hit_scan      = &sol_gui_container_hit_scan,
	.min_size      = &sol_gui_sequence_min_size_vertical,
	.place_content = &sol_gui_sequence_place_content_vertical_last,
	.add_child     = &sol_gui_container_add_child,
	.remove_child  = &sol_gui_container_remove_child,
	.destroy       = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_vertical_uniform =
{
	.render        = &sol_gui_container_render,
	.hit_scan      = &sol_gui_container_hit_scan,
	.min_size      = &sol_gui_sequence_min_size_vertical_uniform,
	.place_content = &sol_gui_sequence_place_content_vertical_uniform,
	.add_child     = &sol_gui_container_add_child,
	.remove_child  = &sol_gui_container_remove_child,
	.destroy       = &sol_gui_container_destroy,
};

void sol_gui_sequence_construct(struct sol_gui_sequence* sequence, struct sol_gui_context* context, enum sol_overlay_orientation orientation, enum sol_gui_distribution distribution)
{
	struct sol_gui_container* container = &sequence->base;
	struct sol_gui_object* base = &container->base;
	sol_gui_container_construct(container, context);

	switch(orientation)
	{
	case SOL_OVERLAY_ORIENTATION_HORIZONTAL:
		switch(distribution)
		{
		case SOL_GUI_SPACE_DISTRIBUTION_START:
			base->structure_functions = &sol_gui_sequence_functions_horizontal_start;
			break;
		case SOL_GUI_SPACE_DISTRIBUTION_END:
			base->structure_functions = &sol_gui_sequence_functions_horizontal_end;
			break;
		case SOL_GUI_SPACE_DISTRIBUTION_FIRST:
			base->structure_functions = &sol_gui_sequence_functions_horizontal_first;
			break;
		case SOL_GUI_SPACE_DISTRIBUTION_LAST:
			base->structure_functions = &sol_gui_sequence_functions_horizontal_last;
			break;
		case SOL_GUI_SPACE_DISTRIBUTION_UNIFORM:
			base->structure_functions = &sol_gui_sequence_functions_horizontal_uniform;
			break;
		}
		break;
	case SOL_OVERLAY_ORIENTATION_VERTICAL:
		switch(distribution)
		{
		case SOL_GUI_SPACE_DISTRIBUTION_START:
			base->structure_functions = &sol_gui_sequence_functions_vertical_start;
			break;
		case SOL_GUI_SPACE_DISTRIBUTION_END:
			base->structure_functions = &sol_gui_sequence_functions_vertical_end;
			break;
		case SOL_GUI_SPACE_DISTRIBUTION_FIRST:
			base->structure_functions = &sol_gui_sequence_functions_vertical_first;
			break;
		case SOL_GUI_SPACE_DISTRIBUTION_LAST:
			base->structure_functions = &sol_gui_sequence_functions_vertical_last;
			break;
		case SOL_GUI_SPACE_DISTRIBUTION_UNIFORM:
			base->structure_functions = &sol_gui_sequence_functions_vertical_uniform;
			break;
		}
		break;
	}
}

struct sol_gui_sequence* sol_gui_sequence_create(struct sol_gui_context* context, enum sol_overlay_orientation orientation, enum sol_gui_distribution distribution)
{
	struct sol_gui_sequence* sequence = malloc(sizeof(struct sol_gui_sequence));

	sol_gui_sequence_construct(sequence, context, orientation, distribution);

	return sequence;
}

struct sol_gui_object* sol_gui_sequence_object_create(struct sol_gui_context* context, enum sol_overlay_orientation orientation, enum sol_gui_distribution distribution)
{
	return sol_gui_sequence_as_object( sol_gui_sequence_create(context, orientation, distribution) );
}

struct sol_gui_object* sol_gui_sequence_as_object(struct sol_gui_sequence* sequence)
{
	return &sequence->base.base;
}