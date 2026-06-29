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

#warning consider renaming to floating panel, have panel be implicit to better allow behaviour surrounding edges -- that would restrict functionality, but perhaps that is okay?

#include "solipsix/math/s16_extent.h"

#include "solipsix/gui/object.h"
#include "solipsix/gui/utilities.h"
#include "solipsix/gui/objects/floating_region.h"



/** these are intended to align with the overarching property flags seen in solipsix/gui/constants.h, but needn't have the same values 
 * allows locking the contents to either be restrained to one side or both (effectively maximising the contents) 
 * only when these are set will the floating regions position flags be passed to the child */
#define SOL_GUI_FLOATING_REGION_STATUS_ATTACHED_START_X   0x00000001
#define SOL_GUI_FLOATING_REGION_STATUS_ATTACHED_START_Y   0x00000002
#define SOL_GUI_FLOATING_REGION_STATUS_ATTACHED_END_X     0x00000004
#define SOL_GUI_FLOATING_REGION_STATUS_ATTACHED_END_Y     0x00000008
#define SOL_GUI_FLOATING_REGION_STATUS_ATTACHED_ALL_SIDES 0x0000000F

/** these impose controls on whether the above properties can be set */
#define SOL_GUI_FLOATING_REGION_PROPERTY_ATTACHABLE_START_X   0x00000010
#define SOL_GUI_FLOATING_REGION_PROPERTY_ATTACHABLE_START_Y   0x00000020
#define SOL_GUI_FLOATING_REGION_PROPERTY_ATTACHABLE_END_X     0x00000040
#define SOL_GUI_FLOATING_REGION_PROPERTY_ATTACHABLE_END_Y     0x00000080
#define SOL_GUI_FLOATING_REGION_PROPERTY_ATTACHABLE_ALL_SIZES 0x000000F0

#define SOL_GUI_FLOATING_REGION_PROPERTY_RESIZABLE_X  0x00000100
#define SOL_GUI_FLOATING_REGION_PROPERTY_RESIZABLE_Y  0x00000200

/** if a valid position has been set, ignore weak position requests */
#define SOL_GUI_FLOATING_REGION_STATUS_HAS_VALID_POSITION 0x00000400

#warning implement the above

struct sol_gui_floating_region
{
	struct sol_gui_object base;
	struct sol_gui_object* child;

	/** because it isn't great to have size be kept around after maximising and then unmaximising a "window" 
	 * this member variable allows us to retain a size which can then be applied to the child widget 
	 * MAY be useful to retain this more generally as the last "set" position 
	 * this also allows the position to be inherited between changes to contents/child */
	s16_rect unmaximised_size;

	/** important for directional (keyboard, gamepad) navigation, need to know what was/is "above" this in the hierarchy */
	// struct sol_gui_object* provoking_parent;

	/** flags relevant only to the floating region itself */
	uint32_t flags;
};


static void sol_gui_floating_region_render(struct sol_gui_object* obj, s16_rect position, struct sol_overlay_render_batch* batch)
{
	struct sol_gui_floating_region* subregion = (struct sol_gui_floating_region*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	struct sol_gui_object* child = subregion->child;

	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_VISIBLE)
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

	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_VISIBLE)
	{
		/** child location is relative to parent, so add floating_region (parent) offset */
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
	uint32_t position_flags_to_preserve = SOL_GUI_OBJECT_POSITION_FLAGS_ALL;

	if(child && (child->flags & SOL_GUI_OBJECT_STATUS_FLAG_VISIBLE))
	{
		sol_gui_object_set_position_flags(child, position_flags & position_flags_to_preserve);
	}
}
static int16_t sol_gui_floating_region_min_size_x(struct sol_gui_object* obj)
{
	struct sol_gui_floating_region* floating_region = (struct sol_gui_floating_region*)obj;
	struct sol_gui_theme* theme = obj->context->theme;
	struct sol_gui_object* child = floating_region->child;
	int16_t content_min_size;

	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_VISIBLE)
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
	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_VISIBLE)
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
	bool attached_start, attached_end;

	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_VISIBLE)
	{
		available_size = s16_extent_size(extent);/** limit to child extent is all the space in the floating window */

		child_extent = child->rect.x;/** preserve current extent if possible (child is free-floating) */
		child_size = s16_extent_size(child_extent);

		assert(child_extent.start >= 0);
		assert(child_size >= 0);

		if(child_size < child->min_size.x)
		{
			child_size = child->min_size.x;
			child_extent.end = child_extent.start + child_size;
		}

		attached_start = floating_region->flags & SOL_GUI_FLOATING_REGION_STATUS_ATTACHED_START_X;
		attached_end   = floating_region->flags & SOL_GUI_FLOATING_REGION_STATUS_ATTACHED_END_X;

		if(attached_start && attached_end)
		{
			child_extent = s16_extent_set(0, available_size);
		}
		else if(attached_start)
		{
			child_extent = s16_extent_set(0, child_size);
		}
		else if(attached_end)
		{
			child_extent = s16_extent_set(available_size - child_size, available_size);
		}
		else if(child_extent.end > available_size)
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
	bool attached_start, attached_end;

	if(child && child->flags & SOL_GUI_OBJECT_STATUS_FLAG_VISIBLE)
	{
		available_size = s16_extent_size(extent);/** limit to child extent is all the space in the floating window */

		child_extent = child->rect.y;/** preserve current extent if possible (child is free-floating) */
		child_size = s16_extent_size(child_extent);

		assert(child_extent.start >= 0);
		assert(child_size >= 0);

		if(child_size < child->min_size.y)
		{
			child_size = child->min_size.y;
			child_extent.end = child_extent.start + child_size;
		}

		attached_start = floating_region->flags & SOL_GUI_FLOATING_REGION_STATUS_ATTACHED_START_Y;
		attached_end   = floating_region->flags & SOL_GUI_FLOATING_REGION_STATUS_ATTACHED_END_Y;

		if(attached_start && attached_end)
		{
			child_extent = s16_extent_set(0, available_size);
		}
		else if(attached_start)
		{
			child_extent = s16_extent_set(0, child_size);
		}
		else if(attached_end)
		{
			child_extent = s16_extent_set(available_size - child_size, available_size);
		}
		else if(child_extent.end > available_size)
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



static void sol_gui_floating_region_construct(struct sol_gui_floating_region* region, struct sol_gui_context* context, uint32_t floating_region_flags)
{
	struct sol_gui_object* base = &region->base;
	sol_gui_object_construct(base, context);

	base->structure_functions = &sol_gui_floating_region_functions;
	
	region->child = NULL;
	region->flags = floating_region_flags;
}

struct sol_gui_floating_region_handle sol_gui_floating_region_create(struct sol_gui_context* context)
{
	struct sol_gui_floating_region* region = malloc(sizeof(struct sol_gui_floating_region));

	sol_gui_floating_region_construct(region, context, SOL_GUI_FLOATING_REGION_PROPERTY_ATTACHABLE_ALL_SIZES);

	return (struct sol_gui_floating_region_handle)
	{
		.object = (struct sol_gui_object*) region,
	};
}


void sol_gui_floating_region_set_content_relative_offset(struct sol_gui_floating_region_handle region_handle, s16_vec2 offset, bool allow_snapping)
{
	struct sol_gui_floating_region* region = (struct sol_gui_floating_region*)region_handle.object;
	struct sol_gui_object* child = region->child;
	s16_vec2 max_offset, child_size;

	/** will do nothing (but NOT break) if region->child is NULL (i.e. not present) */ 
	if (child)
	{
		child_size = s16_rect_size(child->rect);
		/** can't/shouldn't push rect to be outside of bounds, preserving child size */ 
		max_offset = s16_vec2_sub(s16_rect_size(region->base.rect), child_size);
		/** the approach of attempting to resize here is irreconcilable; 
		 * because minimum height is dependent on assigned width 
		 * it has the potential to produce infinite reorganise loops 
		 * as such; maintain size (known to be correct) and just do a best fit operation 
		 * size can be assigned if desired */

		offset = s16_vec2_clamp(offset, s16_vec2_set(0, 0), max_offset);

		child->rect = s16_rect_at_location_with_size(offset, child_size);
	}
}

void sol_gui_floating_region_set_content_absolute_offset(struct sol_gui_floating_region_handle region_handle, s16_vec2 offset, bool allow_snapping)
{
	s16_vec2 region_absolute_offset, relative_offset;

	region_absolute_offset = sol_gui_object_absolute_offset(region_handle.object);
	relative_offset = s16_vec2_sub(offset, region_absolute_offset);

	sol_gui_floating_region_set_content_relative_offset(region_handle, relative_offset, allow_snapping);
}

struct sol_gui_object* sol_gui_floating_region_get_content(struct sol_gui_floating_region_handle region_handle)
{
	struct sol_gui_floating_region* region = (struct sol_gui_floating_region*)region_handle.object;
	return region->child;
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
	struct sol_gui_object* region_content = floating_region->child;
	s16_rect button_rect_absolute, descendant_rect_relative;
	s16_vec2 offset;

	if(region_content && sol_gui_object_toggle_visibility(region_content))
	{
		#warning promoting first ancestor here will be invalidated by the button call promoting the ancestor of the clicked button later
		sol_gui_object_promote_first_ancestor(&floating_region->base);
		
		/** slight pain here in that we want to prescribe the size and placement, 
		 * but its necessary to derive/finalise the layout of the child to know the placement of the reference widget 
		 * as such `sol_gui_object_toggle_visibility` MUST reorganise this subtree to place its contents in a relative fashion, 
		 * which could mean needing to run the layout code twice if the desired placement and size would exceed the floating region */


		/** is invalid to place something relative to its own child */
		assert(!sol_gui_object_has_ancestor(packet->button_handle.object, region_content));
		
		button_rect_absolute = sol_gui_object_absolute_rect(packet->button_handle.object);
		descendant_rect_relative = sol_gui_object_relative_rect(packet->reference_decendant, region_content);

		/** assuming context and theme are the same for all */
		offset = sol_gui_required_pacement_offset(descendant_rect_relative, region_content->context->theme, button_rect_absolute, packet->anchor_placement_x, packet->anchor_placement_y);

		if( ! (floating_region->flags & SOL_GUI_FLOATING_REGION_STATUS_HAS_VALID_POSITION))
		{
			floating_region->flags |= SOL_GUI_FLOATING_REGION_STATUS_HAS_VALID_POSITION;
			sol_gui_floating_region_set_content_absolute_offset(packet->floating_region_handle, offset, false);
		}
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
	assert(!sol_gui_object_has_ancestor(button_handle.object, floating_region_handle.object));

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
		.on_activation  = &floating_region_toggle_button_action_function,
		.on_destruction = &floating_region_toggle_button_destroy_function,
		.data = toggle_packet,
	};

	sol_gui_button_set_packet(button_handle, button_packet);
}







