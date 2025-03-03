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

#include "gui/theme.h"
#include "gui/enums.h"
#include "overlay/enums.h"
// probably need/want more includes from overlay
#include "sol_font.h"

struct sol_gui_theme_simple_data
{
	vec2_s16 base_unit_size;

	vec2_s16 box_border;
	vec2_s16 box_content_border;
	vec2_s16 box_text_border;
	vec2_s16 panel_content_border;
};



static void sol_gui_theme_simple_box_render(struct sol_gui_theme* theme, uint32_t flags, rect_s16 rect, struct cvm_overlay_render_batch * restrict render_batch, enum sol_overlay_colour colour)
{
}

static bool sol_gui_theme_simple_box_select(struct sol_gui_theme* theme, uint32_t flags, rect_s16 rect)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		rect = rect_s16_sub_border(rect, simple_theme_data->box_border);
	}

	return rect_s16_contains_origin(rect);
}

static rect_s16 sol_gui_theme_simple_box_place_content(struct sol_gui_theme* theme, uint32_t flags, rect_s16 rect)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		rect = rect_s16_sub_border(rect, simple_theme_data->box_border);
	}

	return rect;
}

static vec2_s16 sol_gui_theme_simple_box_size(struct sol_gui_theme* theme, uint32_t flags, vec2_s16 contents_size)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	vec2_s16 size = contents_size;

	// box must be at least base_unit_size
	size = vec2_s16_max(size, simple_theme_data->base_unit_size);

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		size = vec2_s16_add(size, vec2_s16_mul_scalar(simple_theme_data->box_border, 2));// added to either side
	}

	return size;
}

static void sol_gui_theme_simple_panel_render(struct sol_gui_theme* theme, uint32_t flags, rect_s16 rect, struct cvm_overlay_render_batch * restrict render_batch, enum sol_overlay_colour colour)
{
}

static bool sol_gui_theme_simple_panel_select(struct sol_gui_theme* theme, uint32_t flags, rect_s16 rect)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	if(flags & SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED)
	{
		rect = rect_s16_sub_border(rect, simple_theme_data->box_border);
	}

	return rect_s16_contains_origin(rect);
}

static rect_s16 sol_gui_theme_simple_panel_place_content(struct sol_gui_theme* theme, uint32_t flags, rect_s16 rect)
{
	return rect;/// needs review tbh, some way to tell if text offset is necessary &c.?
}

static vec2_s16 sol_gui_theme_simple_panel_size(struct sol_gui_theme* theme, uint32_t flags, vec2_s16 contents_size)
{
	struct sol_gui_theme_simple_data* simple_theme_data = theme->other_data;

	return vec2_s16_add(contents_size, vec2_s16_mul_scalar(simple_theme_data->panel_content_border, 2));
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
				.base_unit_size       = vec2_s16_set(16, 16),// min size
				.box_border           = vec2_s16_set(1, 1),
				.box_content_border   = vec2_s16_set(2, 2),
				.box_text_border      = vec2_s16_set(6, 2),
				.panel_content_border = vec2_s16_set(3, 3),
			};
			break;
		default:
		case 1: // normal
			font_size = 16;
			*simple_theme_data = (struct sol_gui_theme_simple_data)
			{
				.base_unit_size       = vec2_s16_set(20, 20),// min size
				.box_border           = vec2_s16_set(1, 1),
				.box_content_border   = vec2_s16_set(2, 2),
				.box_text_border      = vec2_s16_set(8, 2),
				.panel_content_border = vec2_s16_set(4, 4),
			};
			break;
		case 2: // large
			font_size = 24;
			*simple_theme_data = (struct sol_gui_theme_simple_data)
			{
				.base_unit_size       = vec2_s16_set(30, 30),// min size
				.box_border           = vec2_s16_set(2, 2),
				.box_content_border   = vec2_s16_set(3, 3),
				.box_text_border      = vec2_s16_set(12, 3),
				.panel_content_border = vec2_s16_set(5, 5),
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