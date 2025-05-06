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
#include <assert.h>

#include "gui/objects/panel.h"
#include "overlay/enums.h"


struct sol_gui_panel
{
	struct sol_gui_object base;
	struct sol_gui_object* child;
	uint32_t separate_content_from_edges: 1;
};

void sol_gui_panel_render(struct sol_gui_object* obj, s16_vec2 offset, struct cvm_overlay_render_batch* batch)
{
	struct sol_gui_panel* panel = (struct sol_gui_panel*)obj;
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;
	struct sol_gui_object* child = panel->child;

	s16_rect offset_content = s16_rect_add_offset(obj->position, offset);
	theme->panel_render(theme, obj->flags, offset_content, SOL_OVERLAY_COLOUR_DEFAULT, batch);

	// child location is relative to parent
	offset = s16_vec2_add(offset, obj->position.start);
	#warning surely the above can be moved into parent function!

	if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
	{
		sol_gui_object_render(child, offset, batch);
	}
}
struct sol_gui_object* sol_gui_panel_hit_scan(struct sol_gui_object* obj, s16_vec2 location)
{
	struct sol_gui_panel* panel = (struct sol_gui_panel*)obj;
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;
	struct sol_gui_object* child = panel->child;
	struct sol_gui_object* result;

	if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
	{
		// child location is relative to parent
		result = sol_gui_object_hit_scan(child, s16_vec2_sub(location, obj->position.start));
		if(result)
		{
			return result;
		}
	}

	if(theme->panel_select(theme, obj->flags, obj->position, location))
	{
		//panel is selectable
		return obj;
	}

	return NULL;
}
static s16_vec2 sol_gui_panel_min_size(struct sol_gui_object* obj)
{
	struct sol_gui_panel* panel = (struct sol_gui_panel*)obj;
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;
	struct sol_gui_object* child = panel->child;
	s16_vec2 content_min_size;
	uint32_t position_flags;

	position_flags = panel->separate_content_from_edges ? 0 : obj->flags & SOL_GUI_OBJECT_POSITION_FLAGS_ALL;

	if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
	{
		content_min_size = sol_gui_object_min_size(child, position_flags);
	}
	else
	{
		content_min_size = s16_vec2_set(0, 0);
	}

	return theme->panel_size(theme, obj->flags, content_min_size);
}
static void sol_gui_panel_place_content(struct sol_gui_object* obj, s16_rect content_rect)
{
	struct sol_gui_panel* panel = (struct sol_gui_panel*)obj;
	struct sol_gui_context* context = obj->context;
	struct sol_gui_theme* theme = context->theme;
	struct sol_gui_object* child = panel->child;

	obj->position = content_rect;
	#warning across all gui objects: this should probably be moved up a level (do this in sol_gui_object_place_content)

	content_rect = s16_rect_move_start_to_origin(content_rect);

	// child location is relative to parent
	content_rect = theme->panel_place_content(theme, obj->flags, content_rect);

	if(child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
	{
		sol_gui_object_place_content(child, content_rect);
	}
}
void sol_gui_panel_add_child(struct sol_gui_object* obj, struct sol_gui_object* child)
{
	struct sol_gui_panel* panel = (struct sol_gui_panel*)obj;

	assert(panel->child);

	panel->child = child;
}
void sol_gui_panel_remove_child(struct sol_gui_object* obj, struct sol_gui_object* child)
{
	struct sol_gui_panel* panel = (struct sol_gui_panel*)obj;

	assert(child->next == NULL);
	assert(child->prev == NULL);
	assert(panel->child == child);

	panel->child = NULL;
	// rest is handled by wrapper function
}
void sol_gui_panel_destroy(struct sol_gui_object* obj)
{
	struct sol_gui_panel* panel = (struct sol_gui_panel*)obj;

	if(panel->child)
	{
		sol_gui_object_remove_child(obj, panel->child);
		sol_gui_object_release(panel->child);
		// ^ this line is responsible for recursive deletion of widgets, implicitly a panel "gains ownership" of it's child when they are added
		// specifically they take implicit ownership of the initial reference all widgets start with
	}

	assert(panel->base.reference_count == 0);
}

static const struct sol_gui_object_structure_functions sol_gui_panel_functions =
{
	.render        = &sol_gui_panel_render,
	.hit_scan      = &sol_gui_panel_hit_scan,
	.min_size      = &sol_gui_panel_min_size,
	.place_content = &sol_gui_panel_place_content,
	.add_child     = &sol_gui_panel_add_child,
	.remove_child  = &sol_gui_panel_remove_child,
	.destroy       = &sol_gui_panel_destroy,
};

void sol_gui_panel_construct(struct sol_gui_panel* panel, struct sol_gui_context* context, bool clear_bordered, bool separate_content_from_edges)
{
	struct sol_gui_object* base = &panel->base;
	sol_gui_object_construct(base, context);

	base->structure_functions = &sol_gui_panel_functions;

	if(clear_bordered)
	{
		base->flags |= SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED;
	}

	panel->separate_content_from_edges = separate_content_from_edges;
	panel->child = NULL;
}

struct sol_gui_object* sol_gui_panel_create(struct sol_gui_context* context, bool clear_bordered, bool separate_content_from_edges)
{
	struct sol_gui_panel* panel = malloc(sizeof(struct sol_gui_panel));

	sol_gui_panel_construct(panel, context, clear_bordered, separate_content_from_edges);



	#warning panel may separate its contents from edges (remove SOL_GUI_OBJECT_POSITION_FLAGS_ALL) based on an input variable!
	// also make panel borderable?

	return &panel->base;
}