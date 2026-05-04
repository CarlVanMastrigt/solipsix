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
#include "gui/objects/container_basis.h"
#include "gui/objects/sequence.h"
#include "sol_utils.h"


struct sol_gui_sequence
{
	/** sequence is a container with specialised organizational functions **/
	struct sol_gui_container base;
};
// not worth storing max enabled child size and enabled child count just for between the `min_size` and `place_content` functions

// would be good to remove vector direct component access (sum and max then splice)

static void sol_gui_sequence_distribute_position_flags_horizontal(struct sol_gui_object* obj, uint32_t position_flags)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	struct sol_gui_object* prev_enabled_child;// need to operate on prev_child to know which is the last enabled child
	
	/** expect sequences position flags to already be set */
	assert((container->base.flags & SOL_GUI_OBJECT_POSITION_FLAGS_ALL) == position_flags);

	prev_enabled_child = NULL;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			if(prev_enabled_child)
			{
				/** prev_enabled_child is not last if child (another active child) is present */
				sol_gui_object_set_position_flags(prev_enabled_child, position_flags & ~SOL_GUI_OBJECT_POSITION_FLAG_LAST_X);
				position_flags &= ~SOL_GUI_OBJECT_POSITION_FLAG_FIRST_X;/** prev child may have been first (inherited from parent), subsequent entries will not be first */
			}
			prev_enabled_child = child;
		}
	}
	if(prev_enabled_child)
	{
		/** may be last (inherited from parent) */
		sol_gui_object_set_position_flags(prev_enabled_child, position_flags);
	}
}

static void sol_gui_sequence_distribute_position_flags_vertical(struct sol_gui_object* obj, uint32_t position_flags)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	struct sol_gui_object* prev_enabled_child;// need to operate on prev_child to know which is the last enabled child
	
	/** expect sequences position flags to already be set */
	assert((container->base.flags & SOL_GUI_OBJECT_POSITION_FLAGS_ALL) == position_flags);

	prev_enabled_child = NULL;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			if(prev_enabled_child)
			{
				/** prev_enabled_child is not last if child (another active child) is present */
				sol_gui_object_set_position_flags(prev_enabled_child, position_flags & ~SOL_GUI_OBJECT_POSITION_FLAG_LAST_Y);
				position_flags &= ~SOL_GUI_OBJECT_POSITION_FLAG_FIRST_Y;/** prev child may have been first (inherited from parent), subsequent entries will not be first */
			}
			prev_enabled_child = child;
		}
	}
	if(prev_enabled_child)
	{
		/** may be last (inherited from parent) */
		sol_gui_object_set_position_flags(prev_enabled_child, position_flags);
	}
}

static int16_t sol_gui_sequence_min_size_x_horizontal(struct sol_gui_object* obj)
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
			min_size_x += child_min_size_x;
		}
	}

	return min_size_x;
}

static int16_t sol_gui_sequence_min_size_y_vertical(struct sol_gui_object* obj)
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
			min_size_y += child_min_size_y;
		}
	}

	return min_size_y;
}

static int16_t sol_gui_sequence_min_size_x_horizontal_uniform(struct sol_gui_object* obj)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	int16_t largest_min_size_x, child_min_size_x, active_child_count;

	largest_min_size_x = 0;
	active_child_count = 0;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_min_size_x = sol_gui_object_min_size_x(child);
			largest_min_size_x = SOL_MAX(largest_min_size_x, child_min_size_x);
			active_child_count++;
		}
	}

	return largest_min_size_x * active_child_count;
}

static int16_t sol_gui_sequence_min_size_y_vertical_uniform(struct sol_gui_object* obj)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	int16_t largest_min_size_y, child_min_size_y, active_child_count;

	largest_min_size_y = 0;
	active_child_count = 0;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_min_size_y = sol_gui_object_min_size_y(child);
			largest_min_size_y = SOL_MAX(largest_min_size_y, child_min_size_y);
			active_child_count++;
		}
	}

	return largest_min_size_y * active_child_count;
}



static void sol_gui_sequence_set_extent_x_horizontal_start(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;
	int16_t extent_size;

	extent_size = s16_extent_size(extent);
	child_extent = s16_extent_set(extent_size, extent_size);

	for(child = container->last_child; child; child = child->prev)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_extent.start -= child->min_size.x;
			sol_gui_object_set_extent_x(child, child_extent);
			child_extent.end = child_extent.start;
		}
	}

	assert(child_extent.start >= 0);
}

static void sol_gui_sequence_set_extent_y_vertical_start(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;
	int16_t extent_size;

	extent_size = s16_extent_size(extent);
	child_extent = s16_extent_set(extent_size, extent_size);

	for(child = container->last_child; child; child = child->prev)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_extent.start -= child->min_size.y;
			sol_gui_object_set_extent_y(child, child_extent);
			child_extent.end = child_extent.start;
		}
	}

	assert(child_extent.start >= 0);
}

static void sol_gui_sequence_set_extent_x_horizontal_end(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;
	int16_t extent_size;

	extent_size = s16_extent_size(extent);
	child_extent = s16_extent_set(0, 0);

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_extent.end += child->min_size.x;
			sol_gui_object_set_extent_x(child, child_extent);
			child_extent.start = child_extent.end;
		}
	}

	assert(child_extent.end <= extent_size);
}

static void sol_gui_sequence_set_extent_y_vertical_end(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;
	int16_t extent_size;

	extent_size = s16_extent_size(extent);
	child_extent = s16_extent_set(0, 0);

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_extent.end += child->min_size.y;
			sol_gui_object_set_extent_y(child, child_extent);
			child_extent.start = child_extent.end;
		}
	}

	assert(child_extent.end <= extent_size);
}

static void sol_gui_sequence_set_extent_x_horizontal_first(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;
	int16_t extent_size;

	extent_size = s16_extent_size(extent);
	assert(extent_size >= obj->min_size.x);
	child_extent = s16_extent_set(0, extent_size - obj->min_size.x);

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_extent.end += child->min_size.x;
			sol_gui_object_set_extent_x(child, child_extent);
			child_extent.start = child_extent.end;
		}
	}

	assert(child_extent.end == extent_size);
}

static void sol_gui_sequence_set_extent_y_vertical_first(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;
	int16_t extent_size;

	extent_size = s16_extent_size(extent);
	assert(extent_size >= obj->min_size.y);
	child_extent = s16_extent_set(0, extent_size - obj->min_size.y);

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_extent.end += child->min_size.y;
			sol_gui_object_set_extent_y(child, child_extent);
			child_extent.start = child_extent.end;
		}
	}

	assert(child_extent.end == extent_size);
}

static void sol_gui_sequence_set_extent_x_horizontal_last(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;
	int16_t extent_size;

	extent_size = s16_extent_size(extent);
	assert(extent_size >= obj->min_size.x);
	child_extent = s16_extent_set(extent_size - obj->min_size.x, extent_size);

	for(child = container->last_child; child; child = child->prev)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_extent.start -= child->min_size.x;
			sol_gui_object_set_extent_x(child, child_extent);
			child_extent.end = child_extent.start;
		}
	}

	assert(child_extent.start == 0);
}

static void sol_gui_sequence_set_extent_y_vertical_last(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;
	int16_t extent_size;

	extent_size = s16_extent_size(extent);
	assert(extent_size >= obj->min_size.y);
	child_extent = s16_extent_set(extent_size - obj->min_size.y, extent_size);

	for(child = container->last_child; child; child = child->prev)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_extent.start -= child->min_size.y;
			sol_gui_object_set_extent_y(child, child_extent);
			child_extent.end = child_extent.start;
		}
	}

	assert(child_extent.start == 0);
}


static void sol_gui_sequence_set_extent_x_horizontal_uniform(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;
	int16_t child_count, child_index, child_size, remainder, extent_size;

	extent_size = s16_extent_size(extent);
	child_count = sol_gui_container_enabled_child_count(container);
	child_index = 0;

	child_size = extent_size / child_count;
	remainder = extent_size % child_count;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_extent.end += (child_index < remainder) ? child_size + 1 : child_size;
			sol_gui_object_set_extent_x(child, child_extent);
			child_extent.start = child_extent.end;
			child_index++;
		}
	}

	assert(child_extent.end == extent_size);
	assert(child_index == child_count);
}

static void sol_gui_sequence_set_extent_y_vertical_uniform(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_container* container = (struct sol_gui_container*)obj;
	struct sol_gui_object* child;
	s16_extent child_extent;
	int16_t child_count, child_index, child_size_y, remainder, extent_size;

	extent_size = s16_extent_size(extent);
	child_count = sol_gui_container_enabled_child_count(container);
	child_index = 0;

	child_size_y = extent_size / child_count;
	remainder = extent_size % child_count;

	for(child = container->first_child; child; child = child->next)
	{
		if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
		{
			child_extent.end += (child_index < remainder) ? child_size_y + 1 : child_size_y;
			sol_gui_object_set_extent_y(child, child_extent);
			child_extent.start = child_extent.end;
			child_index++;
		}
	}

	assert(child_extent.end == extent_size);
	assert(child_index == child_count);
}


/** horizontal **/
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_horizontal_start =
{
	.render                    = &sol_gui_container_render,
	.hit_scan                  = &sol_gui_container_hit_scan,
	.distribute_position_flags = &sol_gui_sequence_distribute_position_flags_horizontal,
	.min_size_x                = &sol_gui_sequence_min_size_x_horizontal,
	.min_size_y                = &sol_gui_container_min_size_y,
	.set_extent_x              = &sol_gui_sequence_set_extent_x_horizontal_start,
	.set_extent_y              = &sol_gui_container_set_extent_y,
	.add_child                 = &sol_gui_container_add_child,
	.remove_child              = &sol_gui_container_remove_child,
	.destroy                   = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_horizontal_end =
{
	.render                    = &sol_gui_container_render,
	.hit_scan                  = &sol_gui_container_hit_scan,
	.distribute_position_flags = &sol_gui_sequence_distribute_position_flags_horizontal,
	.min_size_x                = &sol_gui_sequence_min_size_x_horizontal,
	.min_size_y                = &sol_gui_container_min_size_y,
	.set_extent_x              = &sol_gui_sequence_set_extent_x_horizontal_end,
	.set_extent_y              = &sol_gui_container_set_extent_y,
	.add_child                 = &sol_gui_container_add_child,
	.remove_child              = &sol_gui_container_remove_child,
	.destroy                   = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_horizontal_first =
{
	.render                    = &sol_gui_container_render,
	.hit_scan                  = &sol_gui_container_hit_scan,
	.distribute_position_flags = &sol_gui_sequence_distribute_position_flags_horizontal,
	.min_size_x                = &sol_gui_sequence_min_size_x_horizontal,
	.min_size_y                = &sol_gui_container_min_size_y,
	.set_extent_x              = &sol_gui_sequence_set_extent_x_horizontal_first,
	.set_extent_y              = &sol_gui_container_set_extent_y,
	.add_child                 = &sol_gui_container_add_child,
	.remove_child              = &sol_gui_container_remove_child,
	.destroy                   = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_horizontal_last =
{
	.render                    = &sol_gui_container_render,
	.hit_scan                  = &sol_gui_container_hit_scan,
	.distribute_position_flags = &sol_gui_sequence_distribute_position_flags_horizontal,
	.min_size_x                = &sol_gui_sequence_min_size_x_horizontal,
	.min_size_y                = &sol_gui_container_min_size_y,
	.set_extent_x              = &sol_gui_sequence_set_extent_x_horizontal_last,
	.set_extent_y              = &sol_gui_container_set_extent_y,
	.add_child                 = &sol_gui_container_add_child,
	.remove_child              = &sol_gui_container_remove_child,
	.destroy                   = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_horizontal_uniform =
{
	.render                    = &sol_gui_container_render,
	.hit_scan                  = &sol_gui_container_hit_scan,
	.distribute_position_flags = &sol_gui_sequence_distribute_position_flags_horizontal,
	.min_size_x                = &sol_gui_sequence_min_size_x_horizontal_uniform,
	.min_size_y                = &sol_gui_container_min_size_y,
	.set_extent_x              = &sol_gui_sequence_set_extent_x_horizontal_uniform,
	.set_extent_y              = &sol_gui_container_set_extent_y,
	.add_child                 = &sol_gui_container_add_child,
	.remove_child              = &sol_gui_container_remove_child,
	.destroy                   = &sol_gui_container_destroy,
};

/** vertical **/
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_vertical_start =
{
	.render                    = &sol_gui_container_render,
	.hit_scan                  = &sol_gui_container_hit_scan,
	.distribute_position_flags = &sol_gui_sequence_distribute_position_flags_vertical,
	.min_size_x                = &sol_gui_container_min_size_x,
	.min_size_y                = &sol_gui_sequence_min_size_y_vertical,
	.set_extent_x              = &sol_gui_container_set_extent_x,
	.set_extent_y              = &sol_gui_sequence_set_extent_y_vertical_start,
	.add_child                 = &sol_gui_container_add_child,
	.remove_child              = &sol_gui_container_remove_child,
	.destroy                   = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_vertical_end =
{
	.render                    = &sol_gui_container_render,
	.hit_scan                  = &sol_gui_container_hit_scan,
	.distribute_position_flags = &sol_gui_sequence_distribute_position_flags_vertical,
	.min_size_x                = &sol_gui_container_min_size_x,
	.min_size_y                = &sol_gui_sequence_min_size_y_vertical,
	.set_extent_x              = &sol_gui_container_set_extent_x,
	.set_extent_y              = &sol_gui_sequence_set_extent_y_vertical_end,
	.add_child                 = &sol_gui_container_add_child,
	.remove_child              = &sol_gui_container_remove_child,
	.destroy                   = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_vertical_first =
{
	.render                    = &sol_gui_container_render,
	.hit_scan                  = &sol_gui_container_hit_scan,
	.distribute_position_flags = &sol_gui_sequence_distribute_position_flags_vertical,
	.min_size_x                = &sol_gui_container_min_size_x,
	.min_size_y                = &sol_gui_sequence_min_size_y_vertical,
	.set_extent_x              = &sol_gui_container_set_extent_x,
	.set_extent_y              = &sol_gui_sequence_set_extent_y_vertical_first,
	.add_child                 = &sol_gui_container_add_child,
	.remove_child              = &sol_gui_container_remove_child,
	.destroy                   = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_vertical_last =
{
	.render                    = &sol_gui_container_render,
	.hit_scan                  = &sol_gui_container_hit_scan,
	.distribute_position_flags = &sol_gui_sequence_distribute_position_flags_vertical,
	.min_size_x                = &sol_gui_container_min_size_x,
	.min_size_y                = &sol_gui_sequence_min_size_y_vertical,
	.set_extent_x              = &sol_gui_container_set_extent_x,
	.set_extent_y              = &sol_gui_sequence_set_extent_y_vertical_last,
	.add_child                 = &sol_gui_container_add_child,
	.remove_child              = &sol_gui_container_remove_child,
	.destroy                   = &sol_gui_container_destroy,
};
static const struct sol_gui_object_structure_functions sol_gui_sequence_functions_vertical_uniform =
{
	.render                    = &sol_gui_container_render,
	.hit_scan                  = &sol_gui_container_hit_scan,
	.distribute_position_flags = &sol_gui_sequence_distribute_position_flags_vertical,
	.min_size_x                = &sol_gui_container_min_size_x,
	.min_size_y                = &sol_gui_sequence_min_size_y_vertical_uniform,
	.set_extent_x              = &sol_gui_container_set_extent_x,
	.set_extent_y              = &sol_gui_sequence_set_extent_y_vertical_uniform,
	.add_child                 = &sol_gui_container_add_child,
	.remove_child              = &sol_gui_container_remove_child,
	.destroy                   = &sol_gui_container_destroy,
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

struct sol_gui_sequence_handle sol_gui_sequence_create(struct sol_gui_context* context, enum sol_overlay_orientation orientation, enum sol_gui_distribution distribution)
{
	struct sol_gui_sequence* sequence = malloc(sizeof(struct sol_gui_sequence));

	sol_gui_sequence_construct(sequence, context, orientation, distribution);

	return (struct sol_gui_sequence_handle)
	{
		.object = (struct sol_gui_object*) sequence,
	};
}
