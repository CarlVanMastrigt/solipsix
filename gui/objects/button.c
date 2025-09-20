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

#include "sol_input.h"
#include "overlay/enums.h"
#include "gui/objects/button.h"

#include "sol_font.h"

#include <stdio.h>





bool sol_gui_button_default_input_action(struct sol_gui_object* obj, const struct sol_input* input)
{
	struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_context* context = obj->context;
	s16_vec2 mouse_location;

	// should activate only on release?
	// dynamic case handling?

	switch(input->sdl_event.type)
	{
	case SDL_MOUSEBUTTONDOWN:
		//button->select_action(button->data);
		sol_gui_context_change_focused_object(context, obj);
		return true;

	case SDL_MOUSEBUTTONUP:
		sol_gui_context_change_focused_object(context, NULL);
		mouse_location = s16_vec2_set(input->sdl_event.motion.x, input->sdl_event.motion.y);
		if(obj == sol_gui_context_hit_scan(context, mouse_location))
		{
			button->select_action(button->data);
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
		// handle non-standard (custom) events
		// if(event_type == context->SOL_GUI_EVENT_OBJECT_HIGHLIGHT_END)
		// {
		// 	//do nothing here...
		// }
		return false;
	}
}

void sol_gui_button_construct(struct sol_gui_button* button, struct sol_gui_context* context, void(*select_action)(void*), void* data)
{
	sol_gui_object_construct(&button->base, context);

	button->base.input_action = &sol_gui_button_default_input_action;
	button->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE | SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE;
	#warning focusable is questionable property, but useful when taking action on release &c.

	button->select_action = select_action;
	button->data = data;
}

struct sol_gui_object* sol_gui_button_as_object(struct sol_gui_button* button)
{
	return &button->base;
}








// specific buttons

// get the buffer malloc'd after the button,
//actually preferable here NOT to have pointer as it communicates that its not an allocated buffer and should NOT be freed

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

	theme->box_render(theme, obj->flags, position, SOL_OVERLAY_COLOUR_DEFAULT, batch);

	text_rect = theme->box_place_content(theme, obj->flags, position);
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
static s16_vec2 sol_gui_text_button_min_size(struct sol_gui_object* obj)
{
	struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_button_get_buffer_const(button);
	s16_vec2 content_min_size;

	content_min_size = sol_font_size_text_simple(text, theme->text_font);

	return theme->box_size(theme, obj->flags, content_min_size);
}
static const struct sol_gui_object_structure_functions sol_gui_text_button_structure_functions =
{
	.render   = &sol_gui_text_button_render,
	.hit_scan = &sol_gui_text_button_hit_scan,
	.min_size = &sol_gui_text_button_min_size,
};
struct sol_gui_button* sol_gui_text_button_create(struct sol_gui_context* context, void(*select_action)(void*), void* data, char* text)
{
	size_t text_len = strlen(text) + 1;
	struct sol_gui_button* button = malloc(sizeof(struct sol_gui_button) + text_len);
	void* text_buf = sol_gui_button_get_buffer(button);

	sol_gui_button_construct(button, context, select_action, data);
	button->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED | SOL_GUI_OBJECT_PROPERTY_FLAG_TEXT_CONTENT;

	button->base.structure_functions = &sol_gui_text_button_structure_functions;

	memcpy(text_buf, text, text_len);

	return button;
}
struct sol_gui_object* sol_gui_text_button_object_create(struct sol_gui_context* context, void(*select_action)(void*), void* data, char* text)
{
	return sol_gui_button_as_object( sol_gui_text_button_create(context, select_action, data, text) );
}



static void sol_gui_utf8_icon_button_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch)
{
	const struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_button_get_buffer_const(button);
	s16_rect icon_rect;

	theme->box_render(theme, obj->flags, position, SOL_OVERLAY_COLOUR_DEFAULT, batch);


	icon_rect = theme->box_place_content(theme, obj->flags, position);
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
static s16_vec2 sol_gui_utf8_icon_button_min_size(struct sol_gui_object* obj)
{
	const struct sol_gui_button* button = (struct sol_gui_button*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	const char* text = sol_gui_button_get_buffer_const(button);

	#warning should have a good/better way to create square boxes for icons and similar... read font directly to get an "icon square" ??
	s16_vec2 content_size = s16_vec2_set(0,0);

	return theme->box_size(theme, obj->flags, content_size);
}
static const struct sol_gui_object_structure_functions sol_gui_utf8_icon_button_structure_functions =
{
	.render   = &sol_gui_utf8_icon_button_render,
	.hit_scan = &sol_gui_utf8_icon_button_hit_scan,
	.min_size = &sol_gui_utf8_icon_button_min_size,
};
struct sol_gui_button* sol_gui_utf8_icon_button_create(struct sol_gui_context* context, void(*select_action)(void*), void* data, char* utf8_icon)
{
	size_t utf8_icon_len = strlen(utf8_icon) + 1;
	struct sol_gui_button* button = malloc(sizeof(struct sol_gui_button) + utf8_icon_len);
	void* utf8_icon_buf = sol_gui_button_get_buffer(button);

	sol_gui_button_construct(button, context, select_action, data);
	button->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED;

	button->base.structure_functions = &sol_gui_utf8_icon_button_structure_functions;

	memcpy(utf8_icon_buf, utf8_icon, utf8_icon_len);

	return button;
}
struct sol_gui_object* sol_gui_utf8_icon_button_object_create(struct sol_gui_context* context, void(*select_action)(void*), void* data, char* utf8_icon)
{
	return sol_gui_button_as_object( sol_gui_utf8_icon_button_create(context, select_action, data, utf8_icon) );
}



