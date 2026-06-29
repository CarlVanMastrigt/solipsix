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

#include <assert.h>
#include <stdlib.h>

#include "solipsix/sol_input.h"
#include "solipsix/sol_font.h"

#include "solipsix/gui/object.h"
#include "solipsix/gui/objects/enterbox.h"



struct sol_gui_enterbox
{
	struct sol_gui_object base;

	struct sol_gui_enterbox_packet packet;

	/** string space allocated after enterbox */
	uint32_t max_characters;
};


bool sol_gui_enterbox_default_input_action(struct sol_gui_object* obj, const struct sol_input* input, const struct sol_gui_input_metadata metadata)
{
	struct sol_gui_enterbox* enterbox = (struct sol_gui_enterbox*)obj;
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;

	s16_vec2 mouse_location;
	
	s16_rect current_rect;
	bool hit_interior;

	switch(input->sdl_event.type)
	{
	// case SDL_EVENT_MOUSE_BUTTON_DOWN:
	// 	return true;

	// case SDL_EVENT_MOUSE_MOTION:
	// 	return false;

	// case SDL_EVENT_MOUSE_BUTTON_UP:
	// 	return false;
	}

	return false;
}

void sol_gui_enterbox_destroy(struct sol_gui_object* obj)
{
}

static void sol_gui_enterbox_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch)
{
	struct sol_gui_enterbox* enterbox = (struct sol_gui_enterbox*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = "ENTERBOX TEST";
	#warning set above
	s16_rect text_rect;

	theme->box_render(theme, obj->flags, position, batch, SOL_OVERLAY_COLOUR_DEFAULT);

	#warning make helper for below?
	text_rect.x = theme->box_content_extent_x(theme, obj->flags, position.x);
	text_rect.y = theme->box_content_extent_y(theme, obj->flags, position.y);

	sol_font_render_text_simple(text, theme->text_font, SOL_OVERLAY_COLOUR_STANDARD_TEXT, text_rect, batch);
}
static struct sol_gui_object* sol_gui_enterbox_hit_scan(struct sol_gui_object* obj, s16_rect position, const s16_vec2 location)
{
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;

	if(theme->box_select(theme, obj->flags, position, location))
	{
		return obj;
	}
	return NULL;
}
static int16_t sol_gui_enterbox_min_size_x(struct sol_gui_object* obj)
{
	struct sol_gui_enterbox* enterbox = (struct sol_gui_enterbox*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	// const char* text = sol_gui_button_get_buffer_const(button);
	int16_t text_size_x;

	// text_size_x = sol_font_size_text_x_simple(text, theme->text_font);
	text_size_x = 0;
	#warning need function for nominal character size for font (em square based?)

	return theme->box_size_x(theme, obj->flags, text_size_x);
}
static int16_t sol_gui_enterbox_min_size_y(struct sol_gui_object* obj)
{
	struct sol_gui_enterbox* enterbox = (struct sol_gui_enterbox*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	// const char* text = sol_gui_button_get_buffer_const(button);
	int16_t text_size_y;

	// text_size_y = sol_font_size_text_y_simple(text, theme->text_font);
	text_size_y = 0;
	#warning need function for nominal character size for font (em square based?)

	return theme->box_size_y(theme, obj->flags, text_size_y);
}

static const struct sol_gui_object_structure_functions sol_gui_text_enterbox_structure_functions =
{
	.render     = &sol_gui_enterbox_render,
	.hit_scan   = &sol_gui_enterbox_hit_scan,
	.min_size_x = &sol_gui_enterbox_min_size_x,
	.min_size_y = &sol_gui_enterbox_min_size_y,
	.destroy    = &sol_gui_enterbox_destroy,
};

void sol_gui_enterbox_construct(struct sol_gui_enterbox* enterbox, struct sol_gui_context* context, uint32_t max_characters, struct sol_gui_enterbox_packet packet)
{
	sol_gui_object_construct(&enterbox->base, context);

	enterbox->base.input_action = &sol_gui_enterbox_default_input_action;
	enterbox->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE | SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE | SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED;
	enterbox->base.structure_functions = &sol_gui_text_enterbox_structure_functions;

	enterbox->max_characters = max_characters;
	enterbox->packet = packet;
}

struct sol_gui_enterbox_handle sol_gui_enterbox_create(struct sol_gui_context* context, uint32_t max_characters, struct sol_gui_enterbox_packet packet)
{
	struct sol_gui_enterbox* enterbox = malloc(sizeof(struct sol_gui_enterbox));

	sol_gui_enterbox_construct(enterbox, context, max_characters, packet);

	return (struct sol_gui_enterbox_handle)
	{
		.object = (struct sol_gui_object*) enterbox,
	};
}