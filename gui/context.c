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

#include <stdio.h>
#warning remove above
#include <assert.h>

#include <SDL2/SDL_timer.h>

#include "sol_input.h"

#include "gui/context.h"
#include "gui/container.h"


struct sol_gui_object* sol_gui_context_initialise(struct sol_gui_context* context, const struct sol_gui_theme* theme, vec2_s16 window_offset, vec2_s16 window_size)
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
		.highlighted_object_navigated = false,
		.highlighted_object = NULL,
		.focused_object = NULL,
		.previously_clicked_object = NULL,
		.SOL_GUI_EVENT_OBJECT_HIGHLIGHT_BEGIN = SOL_GUI_EVENT_BASE + 0,
		.SOL_GUI_EVENT_OBJECT_HIGHLIGHT_END   = SOL_GUI_EVENT_BASE + 1,
		.SOL_GUI_EVENT_OBJECT_FOCUS_BEGIN     = SOL_GUI_EVENT_BASE + 2,
		.SOL_GUI_EVENT_OBJECT_FOCUS_END       = SOL_GUI_EVENT_BASE + 3,
	};

	root_container = sol_gui_container_create(context);

	context->root_container = root_container;
	root_container->flags |= SOL_GUI_OBJECT_STATUS_FLAG_IS_ROOT;

	return root_container;
}

void sol_gui_context_terminate(struct sol_gui_context* context)
{
	if(context->previously_clicked_object)
	{
		assert(context->previously_clicked_object->context == context);
		sol_gui_object_release(context->previously_clicked_object);
	}
	sol_gui_context_set_highlighted_object(context, NULL, false);
	sol_gui_context_set_focused_object(context, NULL);

	// this will effectively recursively release the objects in the heirarchy
	sol_gui_object_release(context->root_container);
	assert(context->registered_object_count == 0);
}


#warning may need to scan up to find highlightable widget?
// chosen? elected? highlighted? focused?
void sol_gui_context_set_highlighted_object(struct sol_gui_context* context, struct sol_gui_object* obj, bool navigated)
{
	struct sol_input highlight_event;
	struct sol_gui_object* old_highlighted = context->highlighted_object;

	assert(!navigated || obj!=NULL);//should not be able to navigate to null (navigation scan should just fail) -- this may change
	assert(obj || navigated || !context->highlighted_object_navigated);// should not be removing navigated highlight with non-navigated highlight

	context->highlighted_object_navigated = navigated;// use this to determine how navigation happened in object (e.g. when highlight begin/end is called)

	#warning alternative to above is to have different kinds of "highlighted" objects; those under mouse and those navigated to (hovered and navigated?)
	#warning probably do want to store previously navigated highlighted object to allow UI re-entry (otherwise navigated highlighted object needs to be re-set whenever UI wants to be navigated)

	if(old_highlighted != obj)
	{
		if(old_highlighted)
		{
			assert(old_highlighted->context == context);
			assert(old_highlighted->flags & SOL_GUI_OBJECT_STATUS_FLAG_HIGHLIGHTED);

			old_highlighted->flags &= ~SOL_GUI_OBJECT_STATUS_FLAG_HIGHLIGHTED;

			// signal to the object that it has become highlighted
			highlight_event.sdl_event.user = (SDL_UserEvent)
			{
				.type = context->SOL_GUI_EVENT_OBJECT_HIGHLIGHT_END,
				.timestamp = SDL_GetTicks(),
			};
			old_highlighted->input_action(old_highlighted, &highlight_event);

			sol_gui_object_release(old_highlighted);
		}
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
		context->highlighted_object = obj;
	}
}

void sol_gui_context_set_focused_object(struct sol_gui_context* context, struct sol_gui_object* obj)
{
	struct sol_input focus_event;
	struct sol_gui_object* old_focused = context->focused_object;

	if(old_focused != obj)
	{
		if(old_focused)
		{
			assert(old_focused->context == context);
			assert(old_focused->flags & SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED);
			assert(obj == NULL);// need to de-focus before next object can be focused (?)

			old_focused->flags &= ~SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED;

			// signal to the object that it has become focused
			focus_event.sdl_event.user = (SDL_UserEvent)
			{
				.type = context->SOL_GUI_EVENT_OBJECT_FOCUS_END,
				.timestamp = SDL_GetTicks(),
			};
			old_focused->input_action(old_focused, &focus_event);

			sol_gui_object_release(old_focused);
		}
		if(obj)
		{
			assert(obj->flags & SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE);
			assert( !(obj->flags & SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED));// object should not already be focused
			assert(obj->context == context);

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
		context->focused_object = obj;
	}
}




/// this is different than reorganise root, this assumes min_sizes havent changed
bool sol_gui_context_update_window(struct sol_gui_context* context, vec2_s16 window_offset, vec2_s16 window_size)
{
	// assume min_size setting isn't required
	if(vec2_m16_all(vec2_s16_cmp_eq(window_size, context->window_size)))
	{
		// nothing internal has changed, no need to place content again
		context->window_offset = window_offset;
	}
	else
	{
		context->content_fit = vec2_m16_all(vec2_s16_cmp_lte(context->window_min_size, window_size));
		rect_s16 content_rect = {.start={0,0}, .end = vec2_s16_max(context->window_min_size, context->window_size)};
		sol_gui_object_place_content(context->root_container, content_rect);
	}

	return context->content_fit;
}

// call this when contents of all widgets may have changed, e.g. at crteation time, after theme change, if a single "toplevel" object in root has changed, instead try to be more precise
bool sol_gui_context_reorganise_root(struct sol_gui_context* context)
{
	context->window_min_size = sol_gui_object_min_size(context->root_container);

	context->content_fit = vec2_m16_all(vec2_s16_cmp_lte(context->window_min_size, context->window_size));

	rect_s16 content_rect = {.start={0,0}, .end = vec2_s16_max(context->window_min_size, context->window_size)};
	sol_gui_object_place_content(context->root_container, content_rect);

	return context->content_fit;
}



bool sol_gui_context_handle_input(struct sol_gui_context* context, const struct sol_input* input)
{
	struct sol_gui_object* object;
	bool result;
	vec2_s16 mouse_location;
	const SDL_Event* sdl_event = &input->sdl_event;

	#warning assert its not in the range supported
	// assert(input->sdl_event.type != context->SOL_GUI_EVENT_OBJECT_HIGHLIGHTED);// this is not a valid event to signal

	const SDL_EventType sdl_type = sdl_event->type;
	#warning make above the behaviour after input
	// can consume input (returning true) actually thats probably just it


	#warning need to query for highlighted object? (under mouse OR resulting from directional input)

	// handle focused object
	if(context->focused_object)
	{
		// if something is focused may will consume input
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

	if(sdl_type == SDL_MOUSEMOTION)
	{
		// could also be touch screen hover or similar
		mouse_location = vec2_s16_set(sdl_event->motion.x, sdl_event->motion.y);
		#warning also do this if widgets have been reorganised? (will need to record latest mouse pos for this)
		object = sol_gui_object_hit_scan(context->root_container, mouse_location);

		// if an object is present under the mouse cursor it must be highlightable, if one isn't then to unset we must not unset a navigated highlighted object
		if(!context->highlighted_object_navigated || (object && (object->flags & SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE)))
		{
			sol_gui_context_set_highlighted_object(context, object, false);
		}


		#warning could make mouse move scan more efficient by tracking currently hovered widget and only updating when its no longer highlighted or layout changed
		// ^ this would replicate sublime behaviour of only changing highlighted widget when a DIFFERENT one gets hovered even if highlighted widget changes via keyboard input
	}

	// handle highlighted object, this can be set via mouse motion
	if(context->highlighted_object)
	{
		#warning is this correct/desirable? probably; enter works on highlighted object...
		object = context->highlighted_object;
		result = object->input_action(object, input);
		if(result)
		{
			return true;
		}
	}
}
