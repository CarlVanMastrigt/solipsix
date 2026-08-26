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

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_events.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "math/s16_vec2.h"
#include "solipsix/sol_input.h"
#include "solipsix/sol_font.h"

#include "solipsix/gui/object.h"
#include "solipsix/gui/objects/enterbox.h"

/** internal flags start from the top bit */

struct sol_gui_enterbox
{
	struct sol_gui_object base;

	struct sol_gui_enterbox_packet packet;

	/** string space allocated after enterbox */
	uint16_t max_strlen;

	/** max glyps to display, different to space to handle UTF-8 encoding */
	uint16_t max_glyphs;

	/** SOL_GUI_ENTERBOX_TEXT_FLAG_<flag> 
	 * flags passed back when getting text from external systems */
	uint16_t text_flags;

	/** SOL_GUI_ENTERBOX_STATUS_FLAG_<flag> 
	 * local/internal flags, mostly used for error tracking */
	uint16_t status_flags;

	/** start is inclusive, end is exclusive */
	uint16_t selection_start;
	uint16_t selection_end;

	enum sol_gui_enterbox_defocus_action defocus_action;
};



/** being edited, should be same as having focus, determines whether to render caret, grab updates, &c. */
#define SOL_GUI_ENTERBOX_STATUS_FLAG_EDITING 0x0001
/** is a selection actively in progress, */
#define SOL_GUI_ENTERBOX_STATUS_FLAG_MOUSE_SELECTING 0x0002



static inline void sol_gui_enterbox_validate_text(const char* text, uint16_t max_strlen)
{
	bool enterbox_null_term_found = false;
	uint16_t i;

	/** `max_strlen` does not include space for extra null term character */
	for(i = 0; i <= max_strlen; i++)
	{
		if(text[i] == '\0')
		{
			return;
		}
	}

	assert(enterbox_null_term_found);
}

static inline char* sol_gui_enterbox_get_text_buffer(struct sol_gui_enterbox* enterbox)
{
	return (char*)(enterbox+1);
}

static inline void sol_gui_enterbox_handle_changed_text(struct sol_gui_enterbox* enterbox)
{
	/** nothing to do at present */
}


/** will attempt to update the text buffer if relevant */
static inline char* sol_gui_enterbox_get_updated_text_buffer(struct sol_gui_object* obj)
{
	struct sol_gui_enterbox* enterbox = (struct sol_gui_enterbox*)obj;
	struct sol_gui_context* context = obj->context;
	char* text_buffer = sol_gui_enterbox_get_text_buffer(enterbox);
	
	const bool updates_locked = enterbox->status_flags & SOL_GUI_ENTERBOX_STATUS_FLAG_EDITING;
	const bool is_focued = context->focused_object == &enterbox->base;
	
	/** if focused dont change the text buffers contents */
	assert(is_focued == updates_locked);
	
	if(!updates_locked)
	{
		enterbox->text_flags = enterbox->packet.get_string(enterbox->packet.data, text_buffer, enterbox->max_glyphs, enterbox->max_strlen, false);

		/** make sure there is a null terminating string */
		text_buffer[enterbox->max_strlen] = '\0';

		if(enterbox->text_flags & SOL_GUI_ENTERBOX_TEXT_FLAG_CHANGED)
		{
			sol_gui_enterbox_handle_changed_text(enterbox);
			enterbox->text_flags &= ~SOL_GUI_ENTERBOX_TEXT_FLAG_CHANGED;
		}
	}

#ifndef NDEBUG
	/** validate the buffer whenever its acquired */
	sol_gui_enterbox_validate_text(text_buffer, enterbox->max_strlen);
#endif

	return text_buffer; 
}


/** start is inclusive, end is exclusive */
#warning generify this? or copy for relevant objects?
void sol_gui_enterbox_update_selection(struct sol_gui_object* obj, uint16_t start, uint16_t end)
{
	struct sol_gui_enterbox* enterbox = (struct sol_gui_enterbox*)obj;
	char* string_buffer = sol_gui_enterbox_get_text_buffer(enterbox);
	char swapped_char;
	bool success;
	uint16_t min, max;/** ORDERED range of bytes (max-exclusive) for the range selected */ 

	/** range must be inside buffer */
	assert(start <= enterbox->max_strlen && end <= enterbox->max_strlen);

	if(start < end)
	{
		min = start;
		max = end;
	}
	else
	{
		min = end;
		max = start;
	}

	if(max <= enterbox->max_strlen /*&& start < end */)
	{
		swapped_char = string_buffer[max];
		string_buffer[max] = '\0';
		success = SDL_SetPrimarySelectionText(string_buffer + min);
		if(!success)
		{
			fprintf(stderr, "failed to update selection with text:%s\n",string_buffer);
		}
		string_buffer[max] = swapped_char;

		enterbox->selection_start = start;
		enterbox->selection_end   = end;
	}
}

static inline void sol_gui_enterbox_defocus(struct sol_gui_context* context, struct sol_gui_enterbox* enterbox, char* string_buffer)
{
	switch(enterbox->defocus_action) 
	{
	case SOL_GUI_ENTERBOX_DEFOCUS_ACTION_APPLY:
		/** check to avoid buffer overflow */
		assert(string_buffer[enterbox->max_strlen] == '\0');
		enterbox->packet.apply_string_update(enterbox->packet.data, string_buffer);
		break;
	case SOL_GUI_ENTERBOX_DEFOCUS_ACTION_CLEAR:
		string_buffer[0] = '\0';
		enterbox->packet.apply_string_update(enterbox->packet.data, string_buffer);
		break;
	case SOL_GUI_ENTERBOX_DEFOCUS_ACTION_INVALIDATE:
		string_buffer[0] = '\0';
		enterbox->packet.get_string(enterbox->packet.data, string_buffer, enterbox->max_glyphs, enterbox->max_strlen, true);
		break;
	}

	enterbox->status_flags = 0;
	sol_gui_context_set_focused_object(context, NULL);
}

#warning SDL_GetPrimarySelectionText
#warning SDL_HasPrimarySelectionText

bool sol_gui_enterbox_default_input_action(struct sol_gui_object* obj, const struct sol_input* input)
{
	struct sol_gui_enterbox* enterbox = (struct sol_gui_enterbox*)obj;
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;
	uint16_t selection_offset;
	s16_vec2 mouse_location;
	s16_rect current_rect;
	s16_rect text_rect;

	char* string_buffer = sol_gui_enterbox_get_updated_text_buffer(obj);

	/** should have no status flags unless focused */
	// printf("%u %u\n",metadata.is_focused, enterbox->status_flags);
	assert(sol_gui_object_is_focused(obj) || enterbox->status_flags == 0);

	

	current_rect = sol_gui_object_absolute_rect(obj);
	/** helper for this would be good */
	text_rect.x = theme->box_content_extent_x(theme, obj->flags, current_rect.x);
	text_rect.y = theme->box_content_extent_y(theme, obj->flags, current_rect.y);
	

	switch(input->sdl_event.type)
	{
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if(sol_gui_object_is_under_mouse_cursor(obj))
		{
			sol_gui_object_promote_first_ancestor(obj);

			mouse_location = s16_vec2_set(input->sdl_event.button.x, input->sdl_event.button.y);

			#warning make which button a switch statement
			if(input->sdl_event.button.button == SDL_BUTTON_LEFT)
			{
				enterbox->status_flags |= SOL_GUI_ENTERBOX_STATUS_FLAG_EDITING | SOL_GUI_ENTERBOX_STATUS_FLAG_MOUSE_SELECTING;
				sol_gui_context_set_focused_object(context, obj);
				if(enterbox->text_flags & SOL_GUI_ENTERBOX_TEXT_FLAG_PLACEHOLDER)
				{
					/** empty the string to remove the placeholder */
					string_buffer[0] = '\0';
				}

				selection_offset = sol_font_find_caret_x_simple(string_buffer, theme->text_font, text_rect, mouse_location);
				printf("selection offset click: %u\n",selection_offset);

				enterbox->selection_start = selection_offset;
				enterbox->selection_end = selection_offset;
			}
			if(input->sdl_event.button.button == SDL_BUTTON_RIGHT)
			{
				#warning copy paste context menu
			}
			if(input->sdl_event.button.button == SDL_BUTTON_MIDDLE)
			{
				#warning auto paste selection if present
				#warning need a way to provide selection from currently(previously) focused object? selected text interface if extant
			}
			return true;
		}
		else if(sol_gui_object_is_focused(obj))
		{
			sol_gui_enterbox_defocus(context, enterbox, string_buffer);
			return true;
		}
		break;

	case SDL_EVENT_MOUSE_MOTION:
		if(sol_gui_object_is_focused(obj))
		{
			if(enterbox->status_flags & SOL_GUI_ENTERBOX_STATUS_FLAG_MOUSE_SELECTING)
			{
				mouse_location = s16_vec2_set(input->sdl_event.motion.x, input->sdl_event.motion.y);

				/** should still be holding mouse down */
				assert(input->sdl_event.motion.state & SDL_BUTTON_LMASK);
				
				selection_offset = sol_font_find_caret_x_simple(string_buffer, theme->text_font, text_rect, mouse_location);

				enterbox->selection_end = selection_offset;
			}
			return true;
		}
		break;

	case SDL_EVENT_MOUSE_BUTTON_UP:
		if(sol_gui_object_is_focused(obj))
		{
			if(input->sdl_event.button.button == SDL_BUTTON_LEFT)
			{
				/** note: may have already ended selecting status (e.g. by keyboard input) */
				enterbox->status_flags &= ~SOL_GUI_ENTERBOX_STATUS_FLAG_MOUSE_SELECTING;
				return true;
			}
		}
		break;

	case SDL_EVENT_KEY_DOWN:
		if(sol_gui_object_is_focused(obj))
		{
			if(input->sdl_event.key.key == SDLK_ESCAPE)
			{
				sol_gui_enterbox_defocus(context, enterbox, string_buffer);
				return true;
			}
		}
		break;

	case SDL_EVENT_TEXT_EDITING:
		#warning implement
	case SDL_EVENT_TEXT_INPUT:
		#warning implement


	}

	return sol_gui_object_is_focused(obj);
}

void sol_gui_enterbox_destroy(struct sol_gui_object* obj)
{
}

static void sol_gui_enterbox_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch)
{
	struct sol_gui_enterbox* enterbox = (struct sol_gui_enterbox*)obj;
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;
	s16_rect text_rect;
	enum sol_overlay_colour text_colour;

	const char* text = sol_gui_enterbox_get_updated_text_buffer(obj);

	theme->box_render(theme, obj->flags, position, batch, SOL_OVERLAY_COLOUR_DEFAULT);

	#warning make helper for below?
	text_rect.x = theme->box_content_extent_x(theme, obj->flags, position.x);
	text_rect.y = theme->box_content_extent_y(theme, obj->flags, position.y);

	if (enterbox->text_flags & SOL_GUI_ENTERBOX_TEXT_FLAG_PLACEHOLDER)
	{
		text_colour = SOL_OVERLAY_COLOUR_PLACEHOLDER_TEXT;
	}
	else
	{
		text_colour = SOL_OVERLAY_COLOUR_STANDARD_TEXT;
	}

	sol_font_render_selection_simple(text, theme->text_font, enterbox->selection_start, enterbox->selection_end, SOL_OVERLAY_COLOUR_TEXT_SELECTION, text_rect, batch);

	sol_font_render_text_simple(text, theme->text_font, text_colour, text_rect, batch);
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
	int16_t glyph_size_x;

	glyph_size_x = sol_font_glyph_size(theme->text_font, SOL_FONT_SIZING_BOUNDS).x;
	#warning need function for nominal character size for font (em square based?)

	return theme->box_size_x(theme, obj->flags, glyph_size_x * enterbox->max_glyphs);
}
static int16_t sol_gui_enterbox_min_size_y(struct sol_gui_object* obj)
{
	struct sol_gui_enterbox* enterbox = (struct sol_gui_enterbox*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	int16_t text_size_y;

	text_size_y = sol_font_glyph_size(theme->text_font, SOL_FONT_SIZING_BOUNDS).y;

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

void sol_gui_enterbox_construct(struct sol_gui_enterbox* enterbox, struct sol_gui_context* context, uint16_t max_glyphs, uint16_t max_strlen, struct sol_gui_enterbox_packet packet, enum sol_gui_enterbox_defocus_action defocus_action)
{
	sol_gui_object_construct(&enterbox->base, context);
	char* text = (char*)(enterbox+1);
	text[0] = '\0';
	text[max_strlen] = '\0';

	/** each glyph takes at least one byte */
	assert(max_strlen >= max_glyphs);

	enterbox->base.input_action = &sol_gui_enterbox_default_input_action;
	enterbox->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE | SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE | SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED | SOL_GUI_OBJECT_PROPERTY_FLAG_TEXT_CONTENT;
	enterbox->base.structure_functions = &sol_gui_text_enterbox_structure_functions;

	enterbox->packet = packet;

	enterbox->max_glyphs = max_glyphs;
	enterbox->max_strlen = max_strlen;
	enterbox->text_flags = 0;
	enterbox->status_flags = 0;
	enterbox->defocus_action = defocus_action;

	/** shouldn't strictly be necessary */
	enterbox->selection_start = 0;
	enterbox->selection_end = 0;
}

struct sol_gui_enterbox_handle sol_gui_enterbox_create(struct sol_gui_context* context, uint16_t max_glyphs, uint16_t max_strlen, struct sol_gui_enterbox_packet packet, enum sol_gui_enterbox_defocus_action defocus_action)
{
	struct sol_gui_enterbox* enterbox = malloc(sizeof(struct sol_gui_enterbox) + max_strlen + 1);

	sol_gui_enterbox_construct(enterbox, context, max_glyphs, max_strlen, packet, defocus_action);

	return (struct sol_gui_enterbox_handle)
	{
		.object = (struct sol_gui_object*) enterbox,
	};
}