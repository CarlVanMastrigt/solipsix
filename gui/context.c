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
#include <stdio.h>
#warning remove above
#include <assert.h>

#include <SDL3/SDL_timer.h>

#include "sol_input.h"

#include "gui/context.h"
#include "gui/objects/container.h"





static inline void sol_gui_context_set_highlight(struct sol_gui_context* context, struct sol_gui_object* obj)
{
	struct sol_input highlight_event;

	assert(context->highlighted_object == NULL);// old highlight should be removed before new one added

	context->highlighted_object = obj;

	if(obj)
	{
		assert(obj->context == context);
		assert(obj->flags & SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE);
		assert( !(obj->flags & SOL_GUI_OBJECT_STATUS_FLAG_HIGHLIGHTED));// object should not already be highlighted

		obj->flags |= SOL_GUI_OBJECT_STATUS_FLAG_HIGHLIGHTED;

		// signal to the object that it has become highlighted
		highlight_event.sdl_event.user = (SDL_UserEvent)
		{
			.type = context->SOL_GUI_EVENT_OBJECT_HIGHLIGHT_BEGIN,
			.timestamp = SDL_GetTicks(),
		};
		obj->input_action(obj, &highlight_event);

		sol_gui_object_retain(obj);
	}
}

static inline void sol_gui_context_clear_highlight(struct sol_gui_context* context, struct sol_gui_object* obj)
{
	struct sol_input highlight_event;

	assert(context->highlighted_object == obj);// must have been highlighted object

	context->highlighted_object = NULL;

	if(obj)
	{
		assert(obj->context == context);
		assert(obj->flags & SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE);
		assert(obj->flags & SOL_GUI_OBJECT_STATUS_FLAG_HIGHLIGHTED);

		obj->flags &= ~SOL_GUI_OBJECT_STATUS_FLAG_HIGHLIGHTED;

		// signal to the object that it is no longer highlighted
		highlight_event.sdl_event.user = (SDL_UserEvent)
		{
			.type = context->SOL_GUI_EVENT_OBJECT_HIGHLIGHT_END,
			.timestamp = SDL_GetTicks(),
		};
		obj->input_action(obj, &highlight_event);

		if (context->previous_highlighted_object)
		{
			sol_gui_object_release(context->previous_highlighted_object);
			// release previous
		}

		context->previous_highlighted_object = obj;
	}
}


static inline void sol_gui_context_set_focus(struct sol_gui_context* context, struct sol_gui_object* obj)
{
	struct sol_input focus_event;

	assert(context->focused_object == NULL);// focus should be removed before being added

	context->focused_object = obj;

	if(obj)
	{
		assert(obj->context == context);
		assert(obj->flags & SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE);
		assert( !(obj->flags & SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED));// object should not already be focused

		obj->flags |= SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED;

		// signal to the object that it has become focused
		focus_event.sdl_event.user = (SDL_UserEvent)
		{
			.type = context->SOL_GUI_EVENT_OBJECT_FOCUS_BEGIN,
			.timestamp = SDL_GetTicks(),
		};
		obj->input_action(obj, &focus_event);

		sol_gui_object_retain(obj);
	}
}

static inline void sol_gui_context_clear_focus(struct sol_gui_context* context, struct sol_gui_object* obj)
{
	struct sol_input focus_event;

	assert(context->focused_object == obj);// must have been focused object

	context->focused_object = NULL;

	if(obj)
	{
		assert(obj->context == context);
		assert(obj->flags & SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE);
		assert(obj->flags & SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED);

		obj->flags &= ~SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED;

		// signal to the object that it is no longer focused
		focus_event.sdl_event.user = (SDL_UserEvent)
		{
			.type = context->SOL_GUI_EVENT_OBJECT_FOCUS_END,
			.timestamp = SDL_GetTicks(),
		};
		obj->input_action(obj, &focus_event);

		sol_gui_object_release(obj);
	}
}





struct sol_gui_object* sol_gui_context_initialise(struct sol_gui_context* context, struct sol_gui_theme* theme, s16_vec2 window_offset, s16_vec2 window_size)
{
	struct sol_gui_object* root_container;
	uint32_t SOL_GUI_EVENT_BASE = SDL_RegisterEvents(4);
	*context = (struct sol_gui_context)
	{
		.window_size = window_size,
		.window_offset = window_offset,
		.theme = theme,
		.registered_object_count = 0,
		.content_fit = true,
		.highlighted_object = NULL,
		.focused_object = NULL,
		.highlight_removable = true,
		.previous_highlighted_object = NULL,
		.previously_clicked_object = NULL,
		.scratch_buffer = malloc(65536),
		.scratch_space = 65536,
		.SOL_GUI_EVENT_OBJECT_HIGHLIGHT_BEGIN = SOL_GUI_EVENT_BASE + 0,
		.SOL_GUI_EVENT_OBJECT_HIGHLIGHT_END   = SOL_GUI_EVENT_BASE + 1,
		.SOL_GUI_EVENT_OBJECT_FOCUS_BEGIN     = SOL_GUI_EVENT_BASE + 2,
		.SOL_GUI_EVENT_OBJECT_FOCUS_END       = SOL_GUI_EVENT_BASE + 3,
	};

	root_container = sol_gui_container_object_create(context);

	context->root_container = root_container;
	root_container->flags |= SOL_GUI_OBJECT_STATUS_FLAG_IS_ROOT;

	return root_container;
}

void sol_gui_context_terminate(struct sol_gui_context* context)
{
	bool root_widget_destroyed;

	sol_gui_context_clear_highlight(context, context->highlighted_object);
	sol_gui_context_clear_focus(context, context->focused_object);

	if(context->previously_clicked_object)
	{
		sol_gui_object_release(context->previously_clicked_object);
	}

	if(context->previous_highlighted_object)
	{
		sol_gui_object_release(context->previous_highlighted_object);
	}

	// this will effectively recursively release the objects in the heirarchy
	root_widget_destroyed = sol_gui_object_release(context->root_container);
	assert(root_widget_destroyed);

	assert(context->registered_object_count == 0);

	free(context->scratch_buffer);
}






#warning may need to scan up to find highlightable widget?
// chosen? elected? highlighted? focused?
void sol_gui_context_change_highlighted_object(struct sol_gui_context* context, struct sol_gui_object* obj, bool removable)
{

	struct sol_gui_object* old_highlighted = context->highlighted_object;

	if(!context->highlight_removable && obj==NULL)
	{
		// existing highlight specified it shouldn't be removable
		return;
	}

	context->highlight_removable = removable;

	if(old_highlighted != obj)
	{
		sol_gui_context_clear_highlight(context, old_highlighted);
		sol_gui_context_set_highlight(context, obj);
	}
}

void sol_gui_context_change_focused_object(struct sol_gui_context* context, struct sol_gui_object* obj)
{
	struct sol_input focus_event;
	struct sol_gui_object* old_focused = context->focused_object;

	if(old_focused != obj)
	{
		assert(obj == NULL || old_focused == NULL);// need to de-focus before next object can be focused (?)

		sol_gui_context_clear_focus(context, old_focused);
		sol_gui_context_set_focus(context, obj);
	}
}



void sol_gui_context_update_screen_offset(struct sol_gui_context* context, s16_vec2 window_offset)
{
	// in future this may actually need to do something
	context->window_offset = window_offset;
}

/// this is different than reorganise root, this assumes min_sizes havent changed
bool sol_gui_context_update_screen_size(struct sol_gui_context* context, s16_vec2 window_size)
{
	// puts("UPDATE SCREEN SIZE");
	// if the sizes arent both equal, need to reorganise
	if(!m16_vec2_all(s16_vec2_cmp_eq(window_size, context->window_size)))
	{
		// puts("DO UPDATE");
		context->content_fit = m16_vec2_all(s16_vec2_cmp_lte(context->window_min_size, window_size));
		context->window_size = window_size;
		s16_rect content_rect = {.start={0,0}, .end = s16_vec2_max(context->window_min_size, context->window_size)};
		printf("CR: %d %d -> %d %d\n", content_rect.start.x, content_rect.start.y, content_rect.end.x, content_rect.end.y);
		sol_gui_object_place_content(context->root_container, content_rect);
	}

	return context->content_fit;
}

// call this when contents of all widgets may have changed, e.g. at crteation time, after theme change, if a single "toplevel" object in root has changed, instead try to be more precise
bool sol_gui_context_reorganise_root(struct sol_gui_context* context)
{
	context->window_min_size = sol_gui_object_min_size(context->root_container, SOL_GUI_OBJECT_POSITION_FLAGS_ALL);

	context->content_fit = m16_vec2_all(s16_vec2_cmp_lte(context->window_min_size, context->window_size));

	s16_rect content_rect = {.start={0,0}, .end = s16_vec2_max(context->window_min_size, context->window_size)};
	sol_gui_object_place_content(context->root_container, content_rect);

	return context->content_fit;
}

void sol_gui_context_render(struct sol_gui_context* context, struct sol_overlay_render_batch* batch)
{
	struct sol_gui_object* root_container = context->root_container;

	if(!m16_vec2_all(s16_vec2_cmp_eq(root_container->position.start, s16_vec2_set(0, 0))))
    {
        fprintf(stderr, "GUI rendering expects the root widget to start at 0,0\n");
    }

	sol_gui_object_render(root_container, s16_vec2_set(0, 0), batch);
}

struct sol_gui_object* sol_gui_context_hit_scan(struct sol_gui_context* context, const s16_vec2 location)
{
	struct sol_gui_object* root_container = context->root_container;

	if(!m16_vec2_all(s16_vec2_cmp_eq(root_container->position.start, s16_vec2_set(0, 0))))
    {
        fprintf(stderr, "GUI hit scan expects the root widget to start at 0,0\n");
    }

	return sol_gui_object_hit_scan(root_container, s16_vec2_set(0, 0), location);
}

bool sol_gui_context_handle_input(struct sol_gui_context* context, const struct sol_input* input)
{
	struct sol_gui_object* object;
	bool result;
	s16_vec2 mouse_location;

	const SDL_Event* sdl_event = &input->sdl_event;
	const SDL_EventType sdl_type = sdl_event->type;

	// handle focused object
	if(context->focused_object)
	{
		// if something is focused may/will consume input
		object = context->focused_object;

		result = object->input_action(object, input);
		if(result)
		{
			// note: can consume input without still being the focused object (e.g. click away to de-focus)
			return true;
		}

		assert(object != context->focused_object);
		// pretty sure if the focused object didn't consume input, then it should no longer be focused
		// this may warrant review though
	}

	if(sdl_type == SDL_EVENT_MOUSE_MOTION)
	{
		// could also be touch screen hover or similar
		mouse_location = s16_vec2_set(sdl_event->motion.x, sdl_event->motion.y);
		#warning also do this if widgets have been reorganised? (would need to record latest mouse pos for this, or query it from SDL) -- how to spook SDL event though??
		object = sol_gui_context_hit_scan(context, mouse_location);

		// search up the heirarchy
		while(object)
		{
			// only alter highlighted object when moving cursor over an object that is highlightable
			if(object->flags & SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE)
			{
				break;
			}
			object = object->parent;
		}

		sol_gui_context_change_highlighted_object(context, object, true);
	}

	// handle highlighted object, this can be set via mouse motion
	if(context->highlighted_object)
	{
		object = context->highlighted_object;
		result = object->input_action(object, input);
		if(result)
		{
			return true;
		}
		#warning add default navigation for highlighted objects (arrow keys/joystick)
	}

	return false;
}
