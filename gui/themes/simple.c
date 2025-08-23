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
// #include <stdio.h>

#include "gui/theme.h"
#include "gui/enums.h"
#include "overlay/enums.h"
#include "overlay/render.h"
#include "sol_font.h"

struct sol_gui_theme_simple_data
{
	s16_vec2 base_unit_size;

	s16_vec2 box_border;
	s16_vec2 box_content_border;
	s16_vec2 box_text_border;
	s16_vec2 panel_border;
	s16_vec2 panel_content_border;
};

#warning put bounds and animatable properties on the batch, and have batch setup struct for this information,
// mark sub components as mutable/inherited and pass them in as a const pointer that can be substituted?
// ^ either sub components of the whole struct, allowing it to be substituted at appropriate points
//   ^also by allowing transient image use we can do sub-renders, *compositor style*, with sub regions of the instance array to render and re-order when needed

static void sol_gui_theme_simple_box_render(struct sol_gui_theme* theme, uint32_t flags, s16_rect rect, enum sol_overlay_colour colour, struct sol_overlay_render_batch * batch)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	struct sol_overlay_render_element* render_data = sol_overlay_render_element_stack_append_ptr(&batch->elements);

	if(colour == SOL_OVERLAY_COLOUR_DEFAULT)
	{
		if(flags & SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED)
		{
			colour = SOL_OVERLAY_COLOUR_FOCUSED;
		}
		else if (flags & SOL_GUI_OBJECT_STATUS_FLAG_HIGHLIGHTED)
		{
			colour = SOL_OVERLAY_COLOUR_HIGHLIGHTED;
		}
		else
		{
			colour = SOL_OVERLAY_COLOUR_MAIN;
		}
	}

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		rect = s16_rect_sub_border(rect, simple_theme_data->box_border);
	}

	rect = s16_rect_intersect(rect, batch->bounds);

	if(s16_rect_valid(rect))
	{
		#warning should also check rect is positive at this point
		*render_data =(struct sol_overlay_render_element)
	    {
	        {rect.start.x, rect.start.y, rect.end.x, rect.end.y},
	        {CVM_OVERLAY_ELEMENT_FILL, colour<<8, 0, 0},
	        {0,0,0,0}
	    };
	}

	// printf("render box: %d %d  %d %d\n",rect.start.x, rect.start.y, rect.end.x,rect.end.y);
}

static bool sol_gui_theme_simple_box_select(struct sol_gui_theme* theme, uint32_t flags, s16_rect rect, s16_vec2 location)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		rect = s16_rect_sub_border(rect, simple_theme_data->box_border);
	}

	return s16_rect_contains_point(rect, location);
}

static s16_rect sol_gui_theme_simple_box_place_content(struct sol_gui_theme* theme, uint32_t flags, s16_rect rect)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		rect = s16_rect_sub_border(rect, simple_theme_data->box_border);
	}

	rect = s16_rect_sub_border(rect, simple_theme_data->box_content_border);

	return rect;
}

static s16_vec2 sol_gui_theme_simple_box_size(struct sol_gui_theme* theme, uint32_t flags, s16_vec2 contents_size)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	// min size is content with theme border
	s16_vec2 size = s16_vec2_add(contents_size, s16_vec2_mul_scalar(simple_theme_data->box_content_border, 2));

	// box must be at least base_unit_size
	size = s16_vec2_max(size, simple_theme_data->base_unit_size);

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		size = s16_vec2_add(size, s16_vec2_mul_scalar(simple_theme_data->box_border, 2));// added to either side
	}

	return size;
}



static void sol_gui_theme_simple_panel_render(struct sol_gui_theme* theme, uint32_t flags, s16_rect rect, enum sol_overlay_colour colour, struct sol_overlay_render_batch * batch)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	struct sol_overlay_render_element* render_data = sol_overlay_render_element_stack_append_ptr(&batch->elements);

	if(colour == SOL_OVERLAY_COLOUR_DEFAULT)
	{
		#warning have panel be focusable if resizable? (somehow)
		colour = SOL_OVERLAY_COLOUR_BACKGROUND;
	}

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		rect = s16_rect_sub_border(rect, simple_theme_data->panel_border);
	}

	rect = s16_rect_intersect(rect, batch->bounds);

	if(s16_rect_valid(rect))
	{
		#warning should also check rect is positive at this point
		*render_data =(struct sol_overlay_render_element)
	    {
	        {rect.start.x, rect.start.y, rect.end.x, rect.end.y},
	        {CVM_OVERLAY_ELEMENT_FILL, colour<<8, 0, 0},
	        {0,0,0,0}
	    };
	}
}

static bool sol_gui_theme_simple_panel_select(struct sol_gui_theme* theme, uint32_t flags, s16_rect rect, s16_vec2 location)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		rect = s16_rect_sub_border(rect, simple_theme_data->panel_border);
	}

	return s16_rect_contains_origin(rect);
}

static s16_rect sol_gui_theme_simple_panel_place_content(struct sol_gui_theme* theme, uint32_t flags, s16_rect rect)
{
	#warning needs review tbh, some way to tell if text offset is necessary &c.?

	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		rect = s16_rect_sub_border(rect, simple_theme_data->panel_border);
	}

	rect = s16_rect_sub_border(rect, simple_theme_data->panel_content_border);

	return rect;
}

static s16_vec2 sol_gui_theme_simple_panel_size(struct sol_gui_theme* theme, uint32_t flags, s16_vec2 contents_size)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	// min size is content with theme border
	s16_vec2 size = s16_vec2_add(contents_size, s16_vec2_mul_scalar(simple_theme_data->panel_content_border, 2));

	#warning minimul panel size?
	//size = s16_vec2_max(size, simple_theme_data->base_unit_size);

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		size = s16_vec2_add(size, s16_vec2_mul_scalar(simple_theme_data->panel_border, 2));// added to either side
	}

	return size;
}






void sol_gui_theme_simple_initialise(struct sol_gui_theme* theme, struct sol_font_library* font_library, int size)
{
	struct sol_gui_theme_simple_data* simple_theme_data = malloc(sizeof(struct sol_gui_theme_simple_data));
	int16_t font_size;

	switch(size)
	{
		case 0: // small
			font_size = 12;
			*simple_theme_data = (struct sol_gui_theme_simple_data)
			{
				.base_unit_size       = s16_vec2_set(16, 16),// min size
				.box_border           = s16_vec2_set(1, 1),
				.box_content_border   = s16_vec2_set(2, 2),
				.box_text_border      = s16_vec2_set(6, 2),
				.panel_content_border = s16_vec2_set(3, 3),
				.panel_border         = s16_vec2_set(1, 1),
			};
			break;
		default:
		case 1: // normal
			font_size = 16;
			*simple_theme_data = (struct sol_gui_theme_simple_data)
			{
				.base_unit_size       = s16_vec2_set(20, 20),// min size
				.box_border           = s16_vec2_set(1, 1),
				.box_content_border   = s16_vec2_set(2, 2),
				.box_text_border      = s16_vec2_set(8, 2),
				.panel_content_border = s16_vec2_set(4, 4),
				.panel_border         = s16_vec2_set(1, 1),
			};
			break;
		case 2: // large
			font_size = 24;
			*simple_theme_data = (struct sol_gui_theme_simple_data)
			{
				.base_unit_size       = s16_vec2_set(30, 30),// min size
				.box_border           = s16_vec2_set(2, 2),
				.box_content_border   = s16_vec2_set(3, 3),
				.box_text_border      = s16_vec2_set(12, 3),
				.panel_content_border = s16_vec2_set(6, 6),
				.panel_border         = s16_vec2_set(2, 2),
			};
			break;
	}

	*theme = (struct sol_gui_theme)
	{
		.font = sol_font_create(font_library, "resources/Comfortaa-Regular.ttf", font_size),
		.other_data = simple_theme_data,

		.box_render          = &sol_gui_theme_simple_box_render,
		.box_select          = &sol_gui_theme_simple_box_select,
		.box_place_content   = &sol_gui_theme_simple_box_place_content,
		.box_size            = &sol_gui_theme_simple_box_size,

		.panel_render        = &sol_gui_theme_simple_panel_render,
		.panel_select        = &sol_gui_theme_simple_panel_select,
		.panel_place_content = &sol_gui_theme_simple_panel_place_content,
		.panel_size          = &sol_gui_theme_simple_panel_size,
	};
}

void sol_gui_theme_simple_terminate(struct sol_gui_theme* theme)
{
	free(theme->other_data);
	sol_font_destroy(theme->font);
}

struct sol_gui_theme* sol_gui_theme_simple_create(struct sol_font_library* font_library, int size)
{
	struct sol_gui_theme* theme = malloc(sizeof(struct sol_gui_theme));
	sol_gui_theme_simple_initialise(theme, font_library, size);
	return theme;
}

void sol_gui_theme_simple_destroy(struct sol_gui_theme* theme)
{
	sol_gui_theme_simple_terminate(theme);
	free(theme);
}