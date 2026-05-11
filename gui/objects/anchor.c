/**
Copyright 2026 Carl van Mastrigt

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

#include <stdio.h>


#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "solipsix/sol_input.h"

#include "solipsix/gui/object.h"
#include "solipsix/gui/objects/anchor.h"

#include "solipsix/sol_font.h"


struct sol_gui_anchor
{
	struct sol_gui_object base;

	struct sol_gui_floating_region_handle floating_region;
};

/** can/should the anchor be declared/defined in the floating_region file? -- they are tightly coupled */


/** for a variety of anchor functions to work properly, the floating region it retains must be an ancestor of the anchor */
static inline bool sol_gui_anchor_has_floating_region_ancestor(const struct sol_gui_anchor* anchor)
{
	return sol_gui_object_is_ancestor(&anchor->base, anchor->floating_region.object);
}


bool sol_gui_anchor_input_action(struct sol_gui_object* obj, const struct sol_input* input)
{
	struct sol_gui_anchor* anchor = (struct sol_gui_anchor*)obj;
	struct sol_gui_context* context = obj->context;
	s16_vec2 mouse_location;

	assert(sol_gui_anchor_has_floating_region_ancestor(anchor));

	switch(input->sdl_event.type)
	{
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if(anchor->floating_region.object)
		{
			puts("ANCHOR");
		}
		return true;

	default:
		return false;
	}
}

void sol_gui_anchor_construct(struct sol_gui_anchor* anchor, struct sol_gui_context* context, struct sol_gui_floating_region_handle floating_region)
{
	sol_gui_object_construct(&anchor->base, context);

	assert(floating_region.object); /** require a valid floating region to affect */

	anchor->base.input_action = &sol_gui_anchor_input_action;
	anchor->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE | SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE;

	anchor->floating_region = floating_region;

	sol_gui_object_retain(floating_region.object);
}

void sol_gui_anchor_release_references(struct sol_gui_object* obj)
{
	struct sol_gui_anchor* anchor = (struct sol_gui_anchor*)obj;
	sol_gui_object_release(anchor->floating_region.object);
	anchor->floating_region.object = NULL;
}









/** get the buffer malloc'd after the anchor,
actually preferable here NOT to have pointer as it communicates that its not an allocated buffer and should NOT be freed */

static inline void* sol_gui_anchor_get_buffer(struct sol_gui_anchor* anchor)
{
	return (anchor+1);
}
static inline const void* sol_gui_anchor_get_buffer_const(const struct sol_gui_anchor* anchor)
{
	return (anchor+1);
}



static void sol_gui_text_anchor_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch)
{
	struct sol_gui_anchor* anchor = (struct sol_gui_anchor*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_anchor_get_buffer_const(anchor);
	s16_rect text_rect;

	theme->box_render(theme, obj->flags, position, batch, SOL_OVERLAY_COLOUR_DEFAULT);

	#warning make helper for below?
	text_rect.x = theme->box_content_extent_x(theme, obj->flags, position.x);
	text_rect.y = theme->box_content_extent_y(theme, obj->flags, position.y);

	sol_font_render_text_simple(text, theme->text_font, SOL_OVERLAY_COLOUR_STANDARD_TEXT, text_rect, batch);
}
static struct sol_gui_object* sol_gui_text_anchor_hit_scan(struct sol_gui_object* obj, s16_rect position, const s16_vec2 location)
{
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;

	if(theme->box_select(theme, obj->flags, position, location))
	{
		return obj;
	}
	return NULL;
}
static int16_t sol_gui_text_anchor_min_size_x(struct sol_gui_object* obj)
{
	struct sol_gui_anchor* anchor = (struct sol_gui_anchor*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_anchor_get_buffer_const(anchor);
	int16_t text_size_x;

	text_size_x = sol_font_size_text_x_simple(text, theme->text_font);

	return theme->box_size_x(theme, obj->flags, text_size_x);
}
static int16_t sol_gui_text_anchor_min_size_y(struct sol_gui_object* obj)
{
	struct sol_gui_anchor* anchor = (struct sol_gui_anchor*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_anchor_get_buffer_const(anchor);
	int16_t text_size_y;

	text_size_y = sol_font_size_text_y_simple(text, theme->text_font);

	return theme->box_size_y(theme, obj->flags, text_size_y);
}
static const struct sol_gui_object_structure_functions sol_gui_text_anchor_structure_functions =
{
	.render            = &sol_gui_text_anchor_render,
	.hit_scan          = &sol_gui_text_anchor_hit_scan,
	.min_size_x        = &sol_gui_text_anchor_min_size_x,
	.min_size_y        = &sol_gui_text_anchor_min_size_y,
	.release_refernces = &sol_gui_anchor_release_references,
};

struct sol_gui_anchor_handle sol_gui_text_anchor_create(struct sol_gui_context* context, char* text, struct sol_gui_floating_region_handle floating_region)
{
	size_t text_len = strlen(text) + 1;
	struct sol_gui_anchor* anchor = malloc(sizeof(struct sol_gui_anchor) + text_len);
	void* text_buf = sol_gui_anchor_get_buffer(anchor);

	sol_gui_anchor_construct(anchor, context, floating_region);
	anchor->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED | SOL_GUI_OBJECT_PROPERTY_FLAG_TEXT_CONTENT;

	anchor->base.structure_functions = &sol_gui_text_anchor_structure_functions;

	memcpy(text_buf, text, text_len);

	return (struct sol_gui_anchor_handle)
	{
		.object = (struct sol_gui_object*) anchor,
	};
}




