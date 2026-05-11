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

#include <stdlib.h>
#include <assert.h>

#warning consider renaming to floating panel, have panel be implicit to better allow behaviour surrounding edges -- that would restrict functionality though...

#include "solipsix/math/s16_extent.h"
// #include "solipsix/overlay/enums.h"

#include "solipsix/gui/object.h"
#include "solipsix/gui/objects/floating_region.h"

struct sol_gui_floating_region
{
	struct sol_gui_object base;
	struct sol_gui_object* child;

	/** */
	s16_extent managed_child_extent;
	int16_t selection_range;
	uint32_t position_flags_to_preserve;
};

static void sol_gui_floating_region_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch)
{
	struct sol_gui_floating_region* subregion = (struct sol_gui_floating_region*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	struct sol_gui_object* child = subregion->child;

	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
	{
		sol_gui_object_render(child, s16_rect_start(position), batch);
	}
}

static struct sol_gui_object* sol_gui_floating_region_hit_scan(struct sol_gui_object* obj, s16_rect position, const s16_vec2 location)
{
	struct sol_gui_floating_region* subregion = (struct sol_gui_floating_region*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	struct sol_gui_object* child = subregion->child;
	struct sol_gui_object* result;

	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
	{
		// child location is relative to parent
		result = sol_gui_object_hit_scan(child, s16_rect_start(position), location);
		if(result)
		{
			return result;
		}
	}

	return NULL;
}
static void sol_gui_floating_region_distribute_position_flags(struct sol_gui_object* obj, uint32_t position_flags)
{
	struct sol_gui_floating_region* floating_region = (struct sol_gui_floating_region*)obj;
	struct sol_gui_object* child = floating_region->child;

	if(child && (child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED))
	{
		sol_gui_object_set_position_flags(child, position_flags & floating_region->position_flags_to_preserve);
	}
}
static int16_t sol_gui_floating_region_min_size_x(struct sol_gui_object* obj)
{
	struct sol_gui_floating_region* floating_region = (struct sol_gui_floating_region*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	struct sol_gui_object* child = floating_region->child;
	int16_t content_min_size;

	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
	{
		content_min_size = sol_gui_object_min_size_x(child);
	}
	else
	{
		content_min_size = 0;
	}

	return content_min_size;
}
static int16_t sol_gui_floating_region_min_size_y(struct sol_gui_object* obj)
{
	struct sol_gui_floating_region* floating_region = (struct sol_gui_floating_region*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	struct sol_gui_object* child = floating_region->child;
	int16_t content_min_size;

	#warning should just make this (and associated functions) handle null and disbled objects wherever they can!
	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
	{
		content_min_size = sol_gui_object_min_size_y(child);
	}
	else
	{
		content_min_size = 0;
	}

	return content_min_size;
}
static void sol_gui_floating_region_set_extent_x(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_floating_region* floating_region = (struct sol_gui_floating_region*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	struct sol_gui_object* child = floating_region->child;
	s16_extent child_extent;
	int16_t available_size, child_size;

	available_size = s16_extent_size(extent);/** limit to child extent is all the space in the floating window */

	child_extent = floating_region->managed_child_extent;
	child_size = s16_extent_size(child_extent);

	assert(child_extent.start >= 0);
	assert(child_size >= 0);

	if(child_size < child->min_size.x)
	{
		child_size = child->min_size.x;
		child_extent.end = child_extent.start + child_size;
	}

	if(child_extent.end > available_size)
	{
		if(child_size > available_size)
		{
			child_extent = s16_extent_set(0, child_size);
		}
		else
		{
			child_extent = s16_extent_add_offset(child_extent, available_size - child_extent.end);
		}
	}

	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
	{
		sol_gui_object_set_extent_x(child, child_extent);
	}
}
static void sol_gui_floating_region_set_extent_y(struct sol_gui_object* obj, s16_extent extent)
{
	struct sol_gui_floating_region* floating_region = (struct sol_gui_floating_region*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	struct sol_gui_object* child = floating_region->child;
	s16_extent child_extent;
	int16_t available_size, child_size;

	available_size = s16_extent_size(extent);/** limit to child extent is all the space in the floating window */

	child_extent = floating_region->managed_child_extent;
	child_size = s16_extent_size(child_extent);

	assert(child_extent.start >= 0);
	assert(child_size >= 0);

	if(child_size < child->min_size.y)
	{
		child_size = child->min_size.y;
		child_extent.end = child_extent.start + child_size;
	}

	if(child_extent.end > available_size)
	{
		if(child_size > available_size)
		{
			child_extent = s16_extent_set(0, child_size);
		}
		else
		{
			child_extent = s16_extent_add_offset(child_extent, available_size - child_extent.end);
		}
	}

	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED)
	{
		sol_gui_object_set_extent_y(child, child_extent);
	}
}
void sol_gui_floating_region_add_child(struct sol_gui_object* obj, struct sol_gui_object* child)
{
	struct sol_gui_floating_region* floating_region = (struct sol_gui_floating_region*)obj;

	assert(floating_region->child == NULL);

	floating_region->child = child;
}
void sol_gui_floating_region_remove_child(struct sol_gui_object* obj, struct sol_gui_object* child)
{
	struct sol_gui_floating_region* floating_region = (struct sol_gui_floating_region*)obj;

	assert(child->next == NULL);
	assert(child->prev == NULL);
	assert(floating_region->child == child);

	floating_region->child = NULL;
}
void sol_gui_floating_region_release_references(struct sol_gui_object* obj)
{
	struct sol_gui_floating_region* floating_region = (struct sol_gui_floating_region*)obj;
	struct sol_gui_object* child = floating_region->child;

	if(child)
	{
		/** note: the order of these is very important */
		sol_gui_object_recursive_release_refernces(child);
		sol_gui_object_remove_child(obj, child);
	}
}

static const struct sol_gui_object_structure_functions sol_gui_floating_region_functions =
{
	.render                    = &sol_gui_floating_region_render,
	.hit_scan                  = &sol_gui_floating_region_hit_scan,
	.distribute_position_flags = &sol_gui_floating_region_distribute_position_flags,
	.min_size_x                = &sol_gui_floating_region_min_size_x,
	.min_size_y                = &sol_gui_floating_region_min_size_y,
	.set_extent_x              = &sol_gui_floating_region_set_extent_x,
	.set_extent_y              = &sol_gui_floating_region_set_extent_y,
	.add_child                 = &sol_gui_floating_region_add_child,
	.remove_child              = &sol_gui_floating_region_remove_child,
	.release_refernces         = &sol_gui_floating_region_release_references,
};



static void sol_gui_floating_region_construct(struct sol_gui_floating_region* region, struct sol_gui_context* context, uint32_t position_flags_to_preserve)
{
	struct sol_gui_object* base = &region->base;
	sol_gui_object_construct(base, context);

	base->structure_functions = &sol_gui_floating_region_functions;
	
	region->managed_child_extent = (s16_extent){};/** zero init */
	region->position_flags_to_preserve = position_flags_to_preserve;
	region->selection_range = 16;/// ??

	region->child = NULL;
}

struct sol_gui_floating_region_handle sol_gui_floating_region_create(struct sol_gui_context* context, uint32_t position_flags_to_preserve)
{
	struct sol_gui_floating_region* region = malloc(sizeof(struct sol_gui_floating_region));

	sol_gui_floating_region_construct(region, context, position_flags_to_preserve);

	return (struct sol_gui_floating_region_handle)
	{
		.object = (struct sol_gui_object*) region,
	};
}





void sol_gui_floating_region_set_relative_placement(struct sol_gui_floating_region_handle region_handle, struct sol_gui_object* reference_external_object, struct sol_gui_object* reference_decendant, enum sol_gui_relative_placement placement_x, enum sol_gui_relative_placement placement_y)
{
	struct sol_gui_floating_region* region = (struct sol_gui_floating_region*)region_handle.object;
	s16_rect region_rect_absolute, reference_rect_absolute, descendant_rect_relative;
	s16_vec2 delta;

	/** is invalid to place something relative to its own child */
	assert(!sol_gui_object_is_ancestor(reference_external_object, region_handle.object));

	/** get region.child -> anchor, absolute(reference) - absolute(anchor) */

	/** will do nothing (but NOT break) if region->child is NULL */ 
	region_rect_absolute = sol_gui_object_absolute_rect(region_handle.object);
	reference_rect_absolute = sol_gui_object_absolute_rect(reference_external_object);
	descendant_rect_relative = sol_gui_object_relative_rect(reference_decendant, region->child);

	#warning actual offset should involve theme because it probably want to be ajjustable and take into account the presence of a background panel
	#warning generify this behaviour if possible inside floating panel specifically, anchor is not the pivot object at all (want button/panel contents to be)
	delta.x = reference_rect_absolute.x.end - region_rect_absolute.x.start - descendant_rect_relative.x.start;
	delta.y = reference_rect_absolute.y.end - region_rect_absolute.y.start - descendant_rect_relative.y.start;

	printf("FR delta: %d %d\n", delta.x, delta.y);

}




struct sol_gui_floating_region_toggle_button_packet
{
    enum sol_gui_relative_placement anchor_placement_x;
    enum sol_gui_relative_placement anchor_placement_y;
    struct sol_gui_floating_region_handle floating_region_handle;
    struct sol_gui_object* reference_decendant;
    struct sol_gui_button_handle button_handle;/** important that this is not retained, its the data in the button and so will (should) only be destroyed after the button is, if it were retained the button would in effect be retaining itself */
};

static void floating_region_toggle_button_action_function(void* data)
{
	struct sol_gui_floating_region_toggle_button_packet* packet = data;
	struct sol_gui_floating_region* floating_region = (struct sol_gui_floating_region*) packet->floating_region_handle.object;

	#warning should alter `sol_gui_object_toggle_enabled_status` floating regions REALLY shouldn't require reorganise of structure on toggle (could be a conditionally valid intercept point for reorganise)
	/// ^ this is technically only true if the first ancestor is the floating region... (because of min size effects)
	if(floating_region->child && sol_gui_object_toggle_enabled_status(floating_region->child))
	{
		puts("AAA");
		sol_gui_floating_region_set_relative_placement(packet->floating_region_handle, packet->button_handle.object, packet->reference_decendant, packet->anchor_placement_x, packet->anchor_placement_y);
	}
}

static void floating_region_toggle_button_destroy_function(void* data)
{
	struct sol_gui_floating_region_toggle_button_packet* packet = data;

	sol_gui_object_release(packet->floating_region_handle.object);
	sol_gui_object_release(packet->reference_decendant);

	free(packet);
}


#warning could vary anchor/floating window, to permit out of bounds behaviour but force an anchor to be (at least partially) "on-screen" when re-enabled

void sol_gui_button_set_floating_region_toggle_button_packet(struct sol_gui_button_handle button_handle, struct sol_gui_floating_region_handle floating_region_handle, struct sol_gui_object* reference_decendant, enum sol_gui_relative_placement anchor_placement_x, enum sol_gui_relative_placement anchor_placement_y)
{
	struct sol_gui_floating_region_toggle_button_packet* toggle_packet = malloc(sizeof(struct sol_gui_floating_region_toggle_button_packet));

	assert(reference_decendant);
	assert(!sol_gui_object_is_ancestor(button_handle.object, floating_region_handle.object));

	/** to be able to access the anchor, it must be retained */
	sol_gui_object_retain(floating_region_handle.object);
	sol_gui_object_retain(reference_decendant);

	*toggle_packet = (struct sol_gui_floating_region_toggle_button_packet)
	{
		.anchor_placement_x = anchor_placement_x,
		.anchor_placement_y = anchor_placement_y,
		.floating_region_handle = floating_region_handle,
		.reference_decendant = reference_decendant,
		.button_handle = button_handle,
	};

	struct sol_gui_button_packet button_packet =
	{
		.action  = &floating_region_toggle_button_action_function,
		.destroy = &floating_region_toggle_button_destroy_function,
		.data = toggle_packet,
	};

	sol_gui_button_set_packet(button_handle, button_packet);
}


