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
#include <stdio.h>

#include "solipsix/sol_utils.h"
#include "solipsix/sol_input.h"
#include "solipsix/gui/objects/range_control.h"


static inline void sol_gui_range_control_alter_value(struct sol_gui_range_control* range_control, struct sol_gui_theme* theme, s16_rect current_rect, s16_vec2 mouse_location, struct sol_range_control_distribution distribution)
{
	s16_extent selection_extent;
	int16_t n, d;

	selection_extent = theme->range_control_selection(theme, range_control->base.flags, range_control->orientation, current_rect, distribution);

	switch(range_control->orientation)
	{
	case SOL_OVERLAY_ORIENTATION_HORIZONTAL:
		n = mouse_location.x;
		break;
	case SOL_OVERLAY_ORIENTATION_VERTICAL:
		n = mouse_location.y;
		break;
	}

	d = s16_extent_size(selection_extent) - 1;
	d = SOL_MAX(d, 1);

	n = n - selection_extent.start - range_control->interior_selection_offset;
	n = SOL_CLAMP(n, 0, d);

	range_control->update_action(range_control->data, n, d);
}

bool sol_gui_range_control_default_input_action(struct sol_gui_object* obj, const struct sol_input* input)
{
	struct sol_gui_range_control* range_control = (struct sol_gui_range_control*)obj;
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;

	struct sol_range_control_distribution distribution;
	s16_vec2 mouse_location;
	
	s16_rect current_rect;
	bool hit_interior;

	// should activate only on release?
	// dynamic case handling?

	switch(input->sdl_event.type)
	{
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		sol_gui_context_change_focused_object(context, obj);
		mouse_location = s16_vec2_set(input->sdl_event.button.x, input->sdl_event.button.y);
		current_rect = sol_gui_object_absolute_rect(obj);
		range_control->get_distribution(range_control->data, &distribution);

		hit_interior = theme->range_control_interior(theme, obj->flags, range_control->orientation, current_rect, mouse_location, distribution, &range_control->interior_selection_offset);

		if(!hit_interior)
		{
			sol_gui_range_control_alter_value(range_control, theme, current_rect, mouse_location, distribution);
		}
		return true;

	case SDL_EVENT_MOUSE_MOTION:
		if(obj->flags & SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED)
		{
			mouse_location = s16_vec2_set(input->sdl_event.motion.x, input->sdl_event.motion.y);
			current_rect = sol_gui_object_absolute_rect(obj);
			range_control->get_distribution(range_control->data, &distribution);

			sol_gui_range_control_alter_value(range_control, theme, current_rect, mouse_location, distribution);

			return true;
		}
		return false;


		

	case SDL_EVENT_MOUSE_BUTTON_UP:
		sol_gui_context_change_focused_object(context, NULL);

		return false;

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

void sol_gui_range_control_destroy_action(struct sol_gui_object* obj)
{
	struct sol_gui_range_control* range_control = (struct sol_gui_range_control*)obj;
	if(range_control->destroy_action)
	{
		range_control->destroy_action(range_control->data);
	}
}








static void sol_gui_range_control_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch)
{
	struct sol_gui_range_control* range_control = (struct sol_gui_range_control*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	struct sol_range_control_distribution distribution;

	range_control->get_distribution(range_control->data, &distribution);

	theme->range_control_render(theme, obj->flags, range_control->orientation, position, batch, SOL_OVERLAY_COLOUR_DEFAULT, SOL_OVERLAY_COLOUR_STANDARD_TEXT, distribution);
}
static struct sol_gui_object* sol_gui_range_control_hit_scan(struct sol_gui_object* obj, s16_rect position, const s16_vec2 location)
{
	struct sol_gui_range_control* range_control = (struct sol_gui_range_control*)obj;
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;

	if(theme->range_control_select(theme, obj->flags, range_control->orientation, position, location))
	{
		return obj;
	}
	return NULL;
}
static int16_t sol_gui_range_control_min_size_x(struct sol_gui_object* obj)
{
	struct sol_gui_range_control* range_control = (struct sol_gui_range_control*)obj;
	struct sol_gui_theme* theme = obj->context->theme;

	return theme->range_control_size_x(theme, obj->flags, range_control->orientation, range_control->min_gradations);
}
static int16_t sol_gui_range_control_min_size_y(struct sol_gui_object* obj)
{
	struct sol_gui_range_control* range_control = (struct sol_gui_range_control*)obj;
	struct sol_gui_theme* theme = obj->context->theme;

	return theme->range_control_size_y(theme, obj->flags, range_control->orientation, range_control->min_gradations);
}

static const struct sol_gui_object_structure_functions sol_gui_text_range_control_structure_functions =
{
	.render     = &sol_gui_range_control_render,
	.hit_scan   = &sol_gui_range_control_hit_scan,
	.min_size_x = &sol_gui_range_control_min_size_x,
	.min_size_y = &sol_gui_range_control_min_size_y,
	.destroy    = &sol_gui_range_control_destroy_action,
};

void sol_gui_range_control_construct(struct sol_gui_range_control* range_control, struct sol_gui_context* context, enum sol_overlay_orientation orientation, void(*get_distribution)(const void*, struct sol_range_control_distribution*), void(*update_action)(void*, int16_t, int16_t), void(*destroy_action)(void*), void* data)
{
	sol_gui_object_construct(&range_control->base, context);

	range_control->base.input_action = &sol_gui_range_control_default_input_action;
	range_control->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE | SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE | SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED;
	range_control->base.structure_functions = &sol_gui_text_range_control_structure_functions;

	range_control->get_distribution = get_distribution;
	range_control->update_action = update_action;
	range_control->destroy_action = destroy_action;
	range_control->data = data;
	range_control->orientation = orientation;
	range_control->min_gradations = 64;
}

// cannot construct these as they have flexible buffers
struct sol_gui_range_control* sol_gui_range_control_create(struct sol_gui_context* context, enum sol_overlay_orientation orientation, void(*get_distribution)(const void*, struct sol_range_control_distribution*), void(*update_action)(void*, int16_t, int16_t), void(*destroy_action)(void*), void* data)
{
	struct sol_gui_range_control* range_control = malloc(sizeof(struct sol_gui_range_control));

	sol_gui_range_control_construct(range_control, context, orientation, get_distribution, update_action, destroy_action, data);

	return range_control;
}

struct sol_gui_object* sol_gui_range_control_object_create(struct sol_gui_context* context, enum sol_overlay_orientation orientation, void(*get_distribution)(const void*, struct sol_range_control_distribution*), void(*update_action)(void*, int16_t, int16_t), void(*destroy_action)(void*), void* data)
{
	return sol_gui_range_control_as_object(sol_gui_range_control_create(context, orientation, get_distribution, update_action, destroy_action, data));
}