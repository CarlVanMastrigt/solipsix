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

	s16_vec2 normal_border;
	s16_vec2 box_content_border;
	s16_vec2 box_text_border;
	s16_vec2 panel_border;
	s16_vec2 panel_content_border;

	uint32_t test_checkerboard_id_set : 1;

	uint64_t test_checkerboard_id;
};

static int16_t sol_gui_theme_simple_normal_size_x(struct sol_gui_theme* theme, uint32_t flags, int16_t content_size)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;
	int16_t size;

	/** box must be at least base_unit_size */
	size = SOL_MAX(content_size, simple_theme_data->base_unit_size.x);

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		size += simple_theme_data->normal_border.x * 2;
	}

	return size;
}
static int16_t sol_gui_theme_simple_normal_size_y(struct sol_gui_theme* theme, uint32_t flags, int16_t content_size)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;
	int16_t size;

	/** box must be at least base_unit_size */
	size = SOL_MAX(content_size, simple_theme_data->base_unit_size.x);

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		size += simple_theme_data->normal_border.x * 2;
	}

	return size;
}
static s16_extent sol_gui_theme_simple_normal_content_extent_x(struct sol_gui_theme* theme, uint32_t flags, s16_extent extent)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		extent = s16_extent_dilate(extent, -simple_theme_data->normal_border.x);
	}

	return extent;
}
static s16_extent sol_gui_theme_simple_normal_content_extent_y(struct sol_gui_theme* theme, uint32_t flags, s16_extent extent)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		extent = s16_extent_dilate(extent, -simple_theme_data->normal_border.y);
	}

	return extent;
}

#warning put bounds and animatable properties on the batch, and have batch setup struct for this information,
// mark sub components as mutable/inherited and pass them in as a const pointer that can be substituted?
// ^ either sub components of the whole struct, allowing it to be substituted at appropriate points
//   ^also by allowing transient image use we can do sub-renders, *compositor style*, with sub regions of the instance array to render and re-order when needed

static void sol_gui_theme_simple_box_render(struct sol_gui_theme* theme, uint32_t flags, s16_rect rect, enum sol_overlay_colour colour, struct sol_overlay_render_batch * batch)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	struct sol_overlay_render_element* render_data = sol_overlay_render_element_list_append_ptr(&batch->elements);

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
		rect = s16_rect_sub_border(rect, simple_theme_data->normal_border);
	}

	rect = s16_rect_intersect(rect, batch->bounds);

	if(s16_rect_valid(rect))
	{
		// uint32_t d0 = (0 << 30) | (colour << 24) ;
		#warning should also check rect is positive at this point
		*render_data =(struct sol_overlay_render_element)
	    {
	        {rect.x.start, rect.y.start, rect.x.end, rect.y.end},
	        {0, 0, 0, 0},
	        {0, colour, 0, 0}
	    };
	}
}

static bool sol_gui_theme_simple_box_select(struct sol_gui_theme* theme, uint32_t flags, s16_rect rect, s16_vec2 location)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		rect = s16_rect_sub_border(rect, simple_theme_data->normal_border);
	}

	return s16_rect_contains_point(rect, location);
}

static s16_extent sol_gui_theme_simple_box_content_extent_x(struct sol_gui_theme* theme, uint32_t flags, s16_extent extent)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		extent = s16_extent_dilate(extent, -simple_theme_data->normal_border.x);
	}

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_TEXT_CONTENT)
	{
		extent = s16_extent_dilate(extent, -simple_theme_data->box_text_border.x);
	}
	else
	{
		extent = s16_extent_dilate(extent, -simple_theme_data->box_content_border.x);
	}

	return extent;
}

static s16_extent sol_gui_theme_simple_box_content_extent_y(struct sol_gui_theme* theme, uint32_t flags, s16_extent extent)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		extent = s16_extent_dilate(extent, -simple_theme_data->normal_border.y);
	}

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_TEXT_CONTENT)
	{
		extent = s16_extent_dilate(extent, -simple_theme_data->box_text_border.y);
	}
	else
	{
		extent = s16_extent_dilate(extent, -simple_theme_data->box_content_border.y);
	}

	return extent;
}

static int16_t sol_gui_theme_simple_box_size_x(struct sol_gui_theme* theme, uint32_t flags, int16_t content_size)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;
	int16_t size;

	/** min size is content with theme border */
	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_TEXT_CONTENT)
	{
		size = simple_theme_data->box_text_border.x * 2 + content_size;
	}
	else
	{
		size = simple_theme_data->box_content_border.x * 2 + content_size;
	}

	/** box must be at least base_unit_size */
	size = SOL_MAX(size, simple_theme_data->base_unit_size.x);

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		size += simple_theme_data->normal_border.x * 2;
	}

	return size;
}

static int16_t sol_gui_theme_simple_box_size_y(struct sol_gui_theme* theme, uint32_t flags, int16_t content_size)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;
	int16_t size;

	/** min size is content with theme border */
	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_TEXT_CONTENT)
	{
		size = simple_theme_data->box_text_border.y * 2 + content_size;
	}
	else
	{
		size = simple_theme_data->box_content_border.y * 2 + content_size;
	}

	/** box must be at least base_unit_size */
	size = SOL_MAX(size, simple_theme_data->base_unit_size.y);

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		size += simple_theme_data->normal_border.y * 2;
	}

	return size;
}



static void sol_gui_theme_simple_panel_render(struct sol_gui_theme* theme, uint32_t flags, s16_rect rect, enum sol_overlay_colour colour, struct sol_overlay_render_batch * batch)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	struct sol_overlay_render_element* render_data = sol_overlay_render_element_list_append_ptr(&batch->elements);

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
		// uint32_t d0 = (0 << 30) | (colour << 24) ;
		#warning should also check rect is positive at this point
		*render_data =(struct sol_overlay_render_element)
	    {
	        {rect.x.start, rect.y.start, rect.x.end, rect.y.end},
	        {0, 0, 0, 0},
	        {0, colour, 0, 0}
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

	return s16_rect_contains_point(rect, location);
}

#warning following could be generic theme functions ??
static s16_extent sol_gui_theme_simple_panel_content_extent_x(struct sol_gui_theme* theme, uint32_t flags, s16_extent extent)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		extent = s16_extent_dilate(extent, -simple_theme_data->panel_border.x);
	}

	extent = s16_extent_dilate(extent, -simple_theme_data->panel_content_border.x);

	return extent;
}

static s16_extent sol_gui_theme_simple_panel_content_extent_y(struct sol_gui_theme* theme, uint32_t flags, s16_extent extent)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		extent = s16_extent_dilate(extent, -simple_theme_data->panel_border.y);
	}

	extent = s16_extent_dilate(extent, -simple_theme_data->panel_content_border.y);

	return extent;
}

static int16_t sol_gui_theme_simple_panel_size_x(struct sol_gui_theme* theme, uint32_t flags, int16_t content_size)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;
	int16_t size;

	// min size is content with theme border
	size = simple_theme_data->panel_content_border.x * 2 + content_size;

	#warning minimal panel size?
	//size = s16_vec2_max(size, simple_theme_data->base_unit_size);

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		size += simple_theme_data->panel_border.x * 2;
	}

	return size;
}

static int16_t sol_gui_theme_simple_panel_size_y(struct sol_gui_theme* theme, uint32_t flags, int16_t content_size)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;
	int16_t size;

	// min size is content with theme border
	size = simple_theme_data->panel_content_border.y * 2 + content_size;

	#warning minimal panel size?
	//size = s16_vec2_max(size, simple_theme_data->base_unit_size);

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		size += simple_theme_data->panel_border.y * 2;
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
			font_size = 13;
			*simple_theme_data = (struct sol_gui_theme_simple_data)
			{
				.base_unit_size        = s16_vec2_set(16, 16),// min size
				.normal_border         = s16_vec2_set(1, 1),
				.box_content_border = s16_vec2_set(2, 2),
				.box_text_border    = s16_vec2_set(6, 2),
				.panel_content_border  = s16_vec2_set(3, 3),
				.panel_border          = s16_vec2_set(1, 1),
			};
			break;
		default:
		case 1: // normal
			// font_size = 976;
			// font_size = 1024;
			// font_size = 13 * 64;
			// font_size = 864;
			font_size = 1024;
			// font_size = 13 * 64 + 32;
			*simple_theme_data = (struct sol_gui_theme_simple_data)
			{
				.base_unit_size        = s16_vec2_set(20, 20),// min size// test button A
				.normal_border         = s16_vec2_set(1, 1),
				.box_content_border = s16_vec2_set(2, 2),
				.box_text_border    = s16_vec2_set(8, 2),
				.panel_content_border  = s16_vec2_set(4, 4),
				.panel_border          = s16_vec2_set(1, 1),
			};
			break;
		case 2: // large
			font_size = 24;
			*simple_theme_data = (struct sol_gui_theme_simple_data)
			{
				.base_unit_size        = s16_vec2_set(30, 30),// min size
				.normal_border         = s16_vec2_set(2, 2),
				.box_content_border = s16_vec2_set(3, 3),
				.box_text_border    = s16_vec2_set(12, 3),
				.panel_content_border  = s16_vec2_set(6, 6),
				.panel_border          = s16_vec2_set(2, 2),
			};
			break;//
	}

	simple_theme_data->test_checkerboard_id_set = false;
	#warning instead making this fixed sounds much preferable
	#warning REALLY need a way to track which atlas a resource came from, perhaps should have a universal resource vendor that also tracks container in identifier ???

	*theme = (struct sol_gui_theme)
	{
		//HB_SCRIPT_MATH			= HB_TAG ('Z','m','t','h'),
		#warning font should take subpixel rendering as a parameter (usually seems like its bad...)
		// .text_font = sol_font_create(font_library, "resources/verdana.ttf", font_size, true, "Latn", "eng", "ltr"),
		// .text_font = sol_font_create(font_library, "/usr/share/fonts/noto/NotoSansMono-Regular.ttf", font_size, true, "latn", "eng", "ltr"),
		// .text_font = sol_font_create(font_library, "/usr/share/fonts/noto/NotoSansMono-Regular.ttf", font_size, false, "latn", "eng", "ltr"),//file:///usr/share/fonts/noto/NotoSansImperialAramaic-Regular.ttf
		// .text_font = sol_font_create(font_library, "/usr/share/fonts/noto/NotoSansArabic-Regular.ttf", font_size, false, "arab", "ARA", "rtl"),
		// .text_font = sol_font_create(font_library, "resources/Comfortaa-Regular.ttf", font_size, true, "Latn", "eng", "ltr"),
		// .text_font = sol_font_create(font_library, "resources/Comfortaa-Regular.ttf", font_size, false, "Latn", "eng", "ltr"),
		// .text_font = sol_font_create(font_library, "resources/NotoColorEmoji-Regular.ttf", font_size, "Latn", "eng", "ltr"),
		.text_font = sol_font_create(font_library, "solipsix/resources/cvm_font_1.ttf", font_size, false, "Latn", "eng", "ltr"),
		.icon_font = sol_font_create(font_library, "solipsix/resources/cvm_font_1.ttf", font_size, false, "Latn", "eng", "ltr"),
		.other_data = simple_theme_data,

		.normal_size_x           = &sol_gui_theme_simple_normal_size_x,
		.normal_size_y           = &sol_gui_theme_simple_normal_size_y,
		.normal_content_extent_x = &sol_gui_theme_simple_normal_content_extent_x,
		.normal_content_extent_y = &sol_gui_theme_simple_normal_content_extent_y,

		.box_render              = &sol_gui_theme_simple_box_render,
		.box_select              = &sol_gui_theme_simple_box_select,
		.box_size_x              = &sol_gui_theme_simple_box_size_x,
		.box_size_y              = &sol_gui_theme_simple_box_size_y,
		.box_content_extent_x    = &sol_gui_theme_simple_box_content_extent_x,
		.box_content_extent_y    = &sol_gui_theme_simple_box_content_extent_y,

		.panel_render            = &sol_gui_theme_simple_panel_render,
		.panel_select            = &sol_gui_theme_simple_panel_select,
		.panel_size_x            = &sol_gui_theme_simple_panel_size_x,
		.panel_size_y            = &sol_gui_theme_simple_panel_size_y,
		.panel_contents_extent_x = &sol_gui_theme_simple_panel_content_extent_x,
		.panel_contents_extent_y = &sol_gui_theme_simple_panel_content_extent_y,
	};

	assert(theme->text_font);
	assert(theme->icon_font);
}

void sol_gui_theme_simple_terminate(struct sol_gui_theme* theme)
{
	free(theme->other_data);
	sol_font_destroy(theme->text_font);
	sol_font_destroy(theme->icon_font);
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