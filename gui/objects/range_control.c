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

#include "solipsix/sol_utils.h"
#include "solipsix/sol_input.h"

#include "solipsix/gui/objects/range_control.h"
#include "solipsix/gui/objects/range_control_basis.h"


static inline void sol_gui_range_control_alter_value_mouse(struct sol_gui_range_control* range_control, struct sol_gui_theme* theme, s16_vec2 mouse_location, bool final_update)
{
	s16_extent selection_extent;
	s16_rect current_rect;
	struct sol_range_control_distribution distribution;
	int16_t n, d;

	current_rect = sol_gui_object_absolute_rect(&range_control->base);
	range_control->packet.get_distribution(range_control->packet.data, &distribution);

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

	range_control->packet.apply_distribution_update(range_control->packet.data, n, d, final_update);
}

bool sol_gui_range_control_default_input_action(struct sol_gui_object* obj, const struct sol_input* input, const struct sol_gui_input_metadata metadata)
{
	struct sol_gui_range_control* range_control = (struct sol_gui_range_control*)obj;
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;

	struct sol_range_control_distribution distribution;
	s16_vec2 mouse_location;
	bool is_under_mouse;
	
	s16_rect current_rect;
	bool hit_interior;

	// should activate only on release?
	// dynamic case handling?

	switch(input->sdl_event.type)
	{
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if(metadata.is_mouse_over)
		{
			sol_gui_object_promote_first_ancestor(obj);
			if(input->sdl_event.button.button == 1)
			{
				mouse_location = s16_vec2_set(input->sdl_event.button.x, input->sdl_event.button.y);
				sol_gui_context_set_focused_object(context, obj);
				current_rect = sol_gui_object_absolute_rect(obj);
				range_control->packet.get_distribution(range_control->packet.data, &distribution);

				hit_interior = theme->range_control_interior(theme, obj->flags, range_control->orientation, current_rect, mouse_location, distribution, &range_control->interior_selection_offset);

				if(!hit_interior)
				{
					sol_gui_range_control_alter_value_mouse(range_control, theme, mouse_location, false);
				}
			}
			return true;
		}
		break;

	case SDL_EVENT_MOUSE_MOTION:
		if(metadata.is_focused)
		{
			sol_gui_object_promote_first_ancestor(obj);
			mouse_location = s16_vec2_set(input->sdl_event.motion.x, input->sdl_event.motion.y);
			sol_gui_range_control_alter_value_mouse(range_control, theme, mouse_location, false);
			return true;
		}
		break;

	case SDL_EVENT_MOUSE_BUTTON_UP:
		if(metadata.is_focused && input->sdl_event.button.button == 1)
		{
			sol_gui_object_promote_first_ancestor(obj);
			mouse_location = s16_vec2_set(input->sdl_event.button.x, input->sdl_event.button.y);
			sol_gui_range_control_alter_value_mouse(range_control, theme, mouse_location, true);
			sol_gui_context_set_focused_object(context, NULL);
			return true;
		}
		break;
	}

	return false;
}

void sol_gui_range_control_destroy(struct sol_gui_object* obj)
{
	struct sol_gui_range_control* range_control = (struct sol_gui_range_control*)obj;
	if(range_control->packet.on_destruction)
	{
		range_control->packet.on_destruction(range_control->packet.data);
	}
}








static void sol_gui_range_control_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch)
{
	struct sol_gui_range_control* range_control = (struct sol_gui_range_control*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	struct sol_range_control_distribution distribution;

	range_control->packet.get_distribution(range_control->packet.data, &distribution);

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
	.destroy    = &sol_gui_range_control_destroy,
};

void sol_gui_range_control_construct(struct sol_gui_range_control* range_control, struct sol_gui_context* context, struct sol_gui_range_control_packet packet, enum sol_overlay_orientation orientation)
{
	sol_gui_object_construct(&range_control->base, context);

	range_control->base.input_action = &sol_gui_range_control_default_input_action;
	range_control->base.flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE | SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE | SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED | SOL_GUI_OBJECT_PROPERTY_FLAG_CLICKABLE;
	range_control->base.structure_functions = &sol_gui_text_range_control_structure_functions;

	range_control->packet = packet;
	range_control->orientation = orientation;
	range_control->min_gradations = 64;
}

// cannot construct these as they have flexible buffers
struct sol_gui_range_control_handle sol_gui_range_control_create(struct sol_gui_context* context, struct sol_gui_range_control_packet packet, enum sol_overlay_orientation orientation)
{
	struct sol_gui_range_control* range_control = malloc(sizeof(struct sol_gui_range_control));

	sol_gui_range_control_construct(range_control, context, packet, orientation);

	return (struct sol_gui_range_control_handle)
	{
		.object = (struct sol_gui_object*) range_control,
	};
}