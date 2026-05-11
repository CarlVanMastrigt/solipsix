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

#include "gui/object.h"

#include <stdio.h>


void sol_gui_object_construct(struct sol_gui_object* obj, struct sol_gui_context* context)
{
	assert(obj);
	assert(context);

	*obj = (struct sol_gui_object)
	{
		.context = context,
		// .structure_functions = NULL,
		// .input_action = NULL,
		.reference_count = 0,
		.flags = SOL_GUI_OBJECT_STATUS_FLAG_UNREFERENCED | SOL_GUI_OBJECT_STATUS_FLAG_ENABLED,
		// .property_flags = 0,
	};

	context->unreferenced_object_count++;
	context->registered_object_count++;
}

void sol_gui_object_retain(struct sol_gui_object* obj)
{
	struct sol_gui_context* context = obj->context;

	if(obj->flags & SOL_GUI_OBJECT_STATUS_FLAG_UNREFERENCED)
	{
		obj->flags &= ~SOL_GUI_OBJECT_STATUS_FLAG_UNREFERENCED;
		context->unreferenced_object_count--;
		assert(obj->reference_count == 0);
	}
	else
	{
		assert(obj->reference_count > 0);
	}

	obj->reference_count++;
}

bool sol_gui_object_release(struct sol_gui_object* obj)
{
	struct sol_gui_context* context = obj->context;

	assert(obj->reference_count > 0);

	obj->reference_count--;

	// parent maintains a reference, so this can only happen after being removed from container/containing widget and having all other references removed
	if(obj->reference_count == 0)
	{
		assert(obj->parent == NULL);
		assert(obj->prev == NULL);
		assert(obj->next == NULL);

		if(obj->structure_functions && obj->structure_functions->destroy)
		{
			obj->structure_functions->destroy(obj);
		}
		free(obj);
		context->registered_object_count--;
		return true;
	}
	return false;
}

void sol_gui_object_remove_from_parent(struct sol_gui_object* obj)
{
	assert(obj->parent);// shouldn't be called on root object or detached object
	if(obj->parent)
	{
		sol_gui_object_remove_child(obj->parent, obj);
	}
}

void sol_gui_object_recursive_release_refernces(struct sol_gui_object* obj)
{
	if(obj->structure_functions && obj->structure_functions->release_refernces)
	{
		obj->structure_functions->release_refernces(obj);
	}
}



void sol_gui_object_render(struct sol_gui_object* obj, s16_vec2 cumulative_offset, struct sol_overlay_render_batch* batch)
{
	assert(obj);

	if(obj->structure_functions && obj->structure_functions->render)
	{
		obj->structure_functions->render(obj, s16_rect_add_offset(obj->rect, cumulative_offset), batch);
	}
}

struct sol_gui_object* sol_gui_object_hit_scan(struct sol_gui_object* obj, s16_vec2 cumulative_offset, const s16_vec2 location)
{
	assert(obj);

	if(obj->structure_functions && obj->structure_functions->hit_scan)
	{
		return obj->structure_functions->hit_scan(obj, s16_rect_add_offset(obj->rect, cumulative_offset), location);
	}

	return NULL;
}
#warning could/should assert object non-null and handle it not being enabled? (or handle things being null ??)
void sol_gui_object_set_position_flags(struct sol_gui_object* obj, uint32_t position_flags)
{
	assert(obj);
	assert((position_flags & ~SOL_GUI_OBJECT_POSITION_FLAGS_ALL) == 0);// don't pass in non position flags
	/** position_flags &= SOL_GUI_OBJECT_POSITION_FLAGS_ALL; // alternative to above, good debug tool */

	/** remove setant flags and set new ones */
	obj->flags = (obj->flags & ~SOL_GUI_OBJECT_POSITION_FLAGS_ALL) | position_flags;

	if(obj->structure_functions && obj->structure_functions->distribute_position_flags)
	{
		obj->structure_functions->distribute_position_flags(obj, position_flags);
	}
}
int16_t sol_gui_object_min_size_x(struct sol_gui_object* obj)
{
	assert(obj);

	if(obj->structure_functions && obj->structure_functions->min_size_x)
	{
		obj->min_size.x = obj->structure_functions->min_size_x(obj);
	}
	else
	{
		obj->min_size.x = 0;
	}

	return obj->min_size.x;
}

int16_t sol_gui_object_min_size_y(struct sol_gui_object* obj)
{
	assert(obj);

	if(obj->structure_functions && obj->structure_functions->min_size_y)
	{
		obj->min_size.y = obj->structure_functions->min_size_y(obj);
	}
	else
	{
		obj->min_size.y = 0;
	}

	return obj->min_size.y;
}

void sol_gui_object_set_extent_x(struct sol_gui_object* obj, s16_extent extent_x)
{
	assert(obj);

	obj->rect.x = extent_x;

	if(obj->structure_functions && obj->structure_functions->set_extent_x)
	{
		obj->structure_functions->set_extent_x(obj, extent_x);
	}
}

void sol_gui_object_set_extent_y(struct sol_gui_object* obj, s16_extent extent_y)
{
	assert(obj);

	obj->rect.y = extent_y;

	if(obj->structure_functions && obj->structure_functions->set_extent_y)
	{
		obj->structure_functions->set_extent_y(obj, extent_y);
	}
}

void sol_gui_object_add_child(struct sol_gui_object* obj, struct sol_gui_object* child)
{
	assert(obj);
	assert(obj->structure_functions && obj->structure_functions->add_child);
	assert(child);
	assert(child->parent == NULL);
	assert(child->prev == NULL);
	assert(child->next == NULL);

	// retaining the parent would mean the lifetime of parent cannot properly/easily be determined when recursively releasing,
	//  this is because the parents RC will equal its number of children rather than any easily knowable number
	// also it doesnt make much sense to retain a parent just because the child is retained,
	//  the child can be correctly removed implicity as part of the parents deletion, making the child "free floating" as it's retention implies
	sol_gui_object_retain(child);

	child->parent = obj;

	obj->structure_functions->add_child(obj, child);

	assert(child->parent != NULL);
}

void sol_gui_object_remove_child(struct sol_gui_object* obj, struct sol_gui_object* child)
{
	assert(obj);
	assert(obj->structure_functions && obj->structure_functions->remove_child);
	assert(child);
	assert(child->parent == obj);

	obj->structure_functions->remove_child(obj, child);

	child->parent = NULL;
	child->prev = NULL;
	child->next = NULL;

	sol_gui_object_release(child);
}


s16_rect sol_gui_object_absolute_rect(const struct sol_gui_object* obj)
{
	s16_rect rect = obj->rect;

	while((obj = obj->parent))
	{
		/** offset the rect by all of its parents starts */
		rect = s16_rect_add_offset(rect, s16_rect_start(obj->rect));
	}

	return rect;
}

s16_rect sol_gui_object_relative_rect(const struct sol_gui_object* obj, const struct sol_gui_object* ancestor)
{
	s16_rect rect = obj->rect;

	while((obj = obj->parent) && obj != ancestor)
	{
		rect = s16_rect_add_offset(rect, s16_rect_start(obj->rect));
	}

	/** `ancestor` should actually be an ancestor of `obj`
	 * `obj` being null here implies ancestor was not encountered */
	assert(obj);

	return rect;
}

struct sol_gui_object* sol_gui_object_find_first_ancestor(struct sol_gui_object* obj)
{
	while(obj)
	{
		if(obj->parent)
		{
			if(obj->parent->flags & SOL_GUI_OBJECT_STATUS_FLAG_IS_ROOT)
			{
				/** if something is the root of a context-connected subtree its parent should be the root container of that context */
				assert(obj->parent == obj->context->root_container);
				return obj;
			}
			obj = obj->parent;
		}
		else
		{
			/** TODO: there are cases where this may actually desired: 
			 * e.g. a disconnected subtree that its desirable to calculate the min sizes for, 
			 * for those cases a compiler flag should/will be provided to disable this stderr print 
			 * (even as is it will behave "correctly") */
			fprintf(stderr, "attempted to find the first ancestor of a gui object not in a contexts tree, this is unlikely to be what you wanted");
			return obj;
		}
	}
	return NULL;
}

bool sol_gui_object_is_ancestor(const struct sol_gui_object* obj, const struct sol_gui_object* ancestor_to_search_for)
{
	assert(obj);

	while((obj = obj->parent))
	{
		if(obj == ancestor_to_search_for)
		{
			return true;
		}
	}
	return false;
}


void sol_gui_object_disable(struct sol_gui_object* obj)
{
	obj->flags &= ~SOL_GUI_OBJECT_STATUS_FLAG_ENABLED;

	#warning instead of `obj->reference_count` could set a "dirty" flag and as objects are created dirty then this would be redundant (also multi-change actions)
	#warning could also set this "need to lay out" on the first ancestors and run it as a step in/before rendering
	/** only need to lay out first ancestor if this object is not yet referenced or is a first widget */
	if(obj->reference_count && obj->parent != obj->context->root_container)
	{
		sol_gui_object_reorganise_first_ancestor(obj);
	}
}

bool sol_gui_object_toggle_enabled_status(struct sol_gui_object* obj)
{
	obj->flags ^= SOL_GUI_OBJECT_STATUS_FLAG_ENABLED;
	
	/** dont need to lay out widgets at the top of the tree that been disabled
	 * (those widgets always have a fixed size and have nothing adjacent to affect) */ 
	if(obj->flags & SOL_GUI_OBJECT_STATUS_FLAG_ENABLED || obj->parent != obj->context->root_container)
	{
		puts("reorganise");
		sol_gui_object_reorganise_first_ancestor(obj);
	}

	return sol_gui_object_is_enabled(obj);
}

void sol_gui_object_reorganise_first_ancestor(struct sol_gui_object* obj)
{
	const struct sol_gui_context* context = obj->context;
	uint32_t position_flags;
	s16_rect rect;
	s16_vec2 min_size;

	obj = sol_gui_object_find_first_ancestor(obj);

	position_flags = obj->flags & SOL_GUI_OBJECT_POSITION_FLAGS_ALL;

	if(obj->parent)
	{
		assert(obj->parent == context->root_container);
		assert(obj->parent->flags & SOL_GUI_OBJECT_STATUS_FLAG_IS_ROOT);
		rect = s16_rect_at_origin_with_size(context->window_size);
	}
	else
	{
		rect = obj->rect;
	}
	

	/** do the reorganise */
	sol_gui_object_set_position_flags(obj, position_flags);

	min_size.x = sol_gui_object_min_size_x(obj);
	if(min_size.x > s16_extent_size(rect.x))
	{
		rect.x.end = rect.x.start + min_size.x;
	}
	sol_gui_object_set_extent_x(obj, rect.x);

	min_size.y = sol_gui_object_min_size_y(obj);
	if(min_size.y > s16_extent_size(rect.y))
	{
		rect.y.end = rect.y.start + min_size.y;
	}
	sol_gui_object_set_extent_y(obj, rect.y);

	#warning if in context tree, need to set context requiremnts/ mark as invalid if too big
}



