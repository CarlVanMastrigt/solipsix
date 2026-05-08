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

#include "solipsix/sol_input.h"
#include "solipsix/overlay/enums.h"
#include "solipsix/gui/objects/button.h"

#include "solipsix/sol_font.h"
#include "solipsix/gui/objects/button_basis.h"


void sol_gui_button_set_packet(struct sol_gui_button_handle button_handle, struct sol_gui_button_packet packet)
{
	struct sol_gui_button* button = (struct sol_gui_button*)button_handle.object;

	assert(button->packet.action  == NULL);
	assert(button->packet.destroy == NULL);
	assert(button->packet.data    == NULL);

	button->packet = packet;
}


bool sol_gui_button_default_input_action_on_button_down(struct sol_gui_object* obj, const struct sol_input* input)
{
	struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_context* context = obj->context;
	s16_vec2 mouse_location;

	switch(input->sdl_event.type)
	{
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if(button->packet.action)
		{
			button->packet.action(button->packet.data);
		}
		return true;

	default:
		return false;
	}
}
bool sol_gui_button_default_input_action_on_button_up(struct sol_gui_object* obj, const struct sol_input* input)
{
	struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_context* context = obj->context;
	s16_vec2 mouse_location;

	switch(input->sdl_event.type)
	{
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		//button->select_action(button->data);
		sol_gui_context_change_focused_object(context, obj);
		return true;

	case SDL_EVENT_MOUSE_BUTTON_UP:
		sol_gui_context_change_focused_object(context, NULL);
		mouse_location = s16_vec2_set(input->sdl_event.motion.x, input->sdl_event.motion.y);
		if(obj == sol_gui_context_hit_scan(context, mouse_location))
		{
			if(button->packet.action)
			{
				button->packet.action(button->packet.data);
			}
			return true;
		}
		else
		{
			return false;
		}

	default:
		if(obj->flags & SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED)
		{
			return true;// must consume input if focused? seems stupid
		}
		return false;
	}
}

void sol_gui_button_destroy(struct sol_gui_object* obj)
{
	struct sol_gui_button* button = (struct sol_gui_button*)obj;
	if(button->packet.destroy)
	{
		button->packet.destroy(button->packet.data);
	}
}


void sol_gui_button_construct_default(struct sol_gui_button* button, struct sol_gui_context* context, struct sol_gui_button_packet packet, bool action_on_release)
{
	sol_gui_object_construct(&button->base, context);

	if(action_on_release)
	{
		button->base.input_action = &sol_gui_button_default_input_action_on_button_up;
		button->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE | SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE;
	}
	else
	{
		button->base.input_action = &sol_gui_button_default_input_action_on_button_down;
		button->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE;
	}

	button->packet = packet;
}










/** get the buffer malloc'd after the button,
actually preferable here NOT to have pointer as it communicates that its not an allocated buffer and should NOT be freed */

static inline void* sol_gui_button_get_buffer(struct sol_gui_button* button)
{
	return (button+1);
}
static inline const void* sol_gui_button_get_buffer_const(const struct sol_gui_button* button)
{
	return (button+1);
}



static void sol_gui_text_button_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch)
{
	struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_button_get_buffer_const(button);
	s16_rect text_rect;

	theme->box_render(theme, obj->flags, position, batch, SOL_OVERLAY_COLOUR_DEFAULT);

	#warning make helper for below?
	text_rect.x = theme->box_content_extent_x(theme, obj->flags, position.x);
	text_rect.y = theme->box_content_extent_y(theme, obj->flags, position.y);

	sol_font_render_text_simple(text, theme->text_font, SOL_OVERLAY_COLOUR_STANDARD_TEXT, text_rect, batch);
}
static struct sol_gui_object* sol_gui_text_button_hit_scan(struct sol_gui_object* obj, s16_rect position, const s16_vec2 location)
{
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;

	if(theme->box_select(theme, obj->flags, position, location))
	{
		return obj;
	}
	return NULL;
}
static int16_t sol_gui_text_button_min_size_x(struct sol_gui_object* obj)
{
	struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_button_get_buffer_const(button);
	int16_t text_size_x;

	text_size_x = sol_font_size_text_x_simple(text, theme->text_font);

	return theme->box_size_x(theme, obj->flags, text_size_x);
}
static int16_t sol_gui_text_button_min_size_y(struct sol_gui_object* obj)
{
	struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_button_get_buffer_const(button);
	int16_t text_size_y;

	text_size_y = sol_font_size_text_y_simple(text, theme->text_font);

	return theme->box_size_y(theme, obj->flags, text_size_y);
}
static const struct sol_gui_object_structure_functions sol_gui_text_button_structure_functions =
{
	.render     = &sol_gui_text_button_render,
	.hit_scan   = &sol_gui_text_button_hit_scan,
	.min_size_x = &sol_gui_text_button_min_size_x,
	.min_size_y = &sol_gui_text_button_min_size_y,
	.destroy    = &sol_gui_button_destroy,
};
struct sol_gui_text_button_handle sol_gui_text_button_create(struct sol_gui_context* context, struct sol_gui_button_packet packet, char* text)
{
	size_t text_len = strlen(text) + 1;
	struct sol_gui_button* button = malloc(sizeof(struct sol_gui_button) + text_len);
	void* text_buf = sol_gui_button_get_buffer(button);

	sol_gui_button_construct_default(button, context, packet, true);
	button->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED | SOL_GUI_OBJECT_PROPERTY_FLAG_TEXT_CONTENT;

	button->base.structure_functions = &sol_gui_text_button_structure_functions;

	memcpy(text_buf, text, text_len);

	return (struct sol_gui_text_button_handle)
	{
		.button.object = (struct sol_gui_object*) button,
	};
}



static void sol_gui_utf8_icon_button_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch)
{
	const struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_button_get_buffer_const(button);
	s16_rect icon_rect;

	theme->box_render(theme, obj->flags, position, batch, SOL_OVERLAY_COLOUR_DEFAULT);

	#warning make helper for below?
	icon_rect.x = theme->box_content_extent_x(theme, obj->flags, position.x);
	icon_rect.y = theme->box_content_extent_y(theme, obj->flags, position.y);

	sol_font_render_glyph_simple(text, theme->text_font, SOL_OVERLAY_COLOUR_STANDARD_TEXT, icon_rect, batch);
}
static struct sol_gui_object* sol_gui_utf8_icon_button_hit_scan(struct sol_gui_object* obj, s16_rect position, const s16_vec2 location)
{
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;

	if(theme->box_select(theme, obj->flags, position, location))
	{
		return obj;
	}
	return NULL;
}
static int16_t sol_gui_utf8_icon_button_min_size_x(struct sol_gui_object* obj)
{
	const struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_button_get_buffer_const(button);
	int16_t glyph_size_x;

	#warning should have a good/better way to create square boxes for icons and similar... read font directly to get an "icon square" ??
	glyph_size_x = 0;

	return theme->box_size_x(theme, obj->flags, glyph_size_x);
}
static int16_t sol_gui_utf8_icon_button_min_size_y(struct sol_gui_object* obj)
{
	const struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_button_get_buffer_const(button);
	int16_t glyph_size_y;

	#warning should have a good/better way to create square boxes for icons and similar... read font directly to get an "icon square" ??
	glyph_size_y = 0;

	return theme->box_size_y(theme, obj->flags, glyph_size_y);
}

static const struct sol_gui_object_structure_functions sol_gui_utf8_icon_button_structure_functions =
{
	.render     = &sol_gui_utf8_icon_button_render,
	.hit_scan   = &sol_gui_utf8_icon_button_hit_scan,
	.min_size_x = &sol_gui_utf8_icon_button_min_size_x,
	.min_size_y = &sol_gui_utf8_icon_button_min_size_y,
	.destroy    = &sol_gui_button_destroy,
};
struct sol_gui_utf8_icon_button_handle sol_gui_utf8_icon_button_create(struct sol_gui_context* context, struct sol_gui_button_packet packet, char* utf8_icon)
{
	size_t utf8_icon_len = strlen(utf8_icon) + 1;
	struct sol_gui_button* button = malloc(sizeof(struct sol_gui_button) + utf8_icon_len);
	void* utf8_icon_buf = sol_gui_button_get_buffer(button);

	sol_gui_button_construct_default(button, context, packet, true);
	button->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED;

	button->base.structure_functions = &sol_gui_utf8_icon_button_structure_functions;

	memcpy(utf8_icon_buf, utf8_icon, utf8_icon_len);

	return (struct sol_gui_utf8_icon_button_handle)
	{
		.button.object = (struct sol_gui_object*) button,
	};
}



