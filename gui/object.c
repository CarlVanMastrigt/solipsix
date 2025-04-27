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
		.reference_count = 1,// existing is a reference, MUST call delete
		.flags = SOL_GUI_OBJECT_STATUS_FLAG_REGISTERED | SOL_GUI_OBJECT_STATUS_FLAG_ENABLED,
		// .property_flags = 0,
	};

	context->registered_object_count++;
}

void sol_gui_object_retain(struct sol_gui_object* obj)
{
	assert(obj->reference_count > 0);

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




void sol_gui_object_render(struct sol_gui_object* obj, s16_vec2 offset, struct cvm_overlay_render_batch* batch)
{
	assert(obj);

	if(obj->structure_functions && obj->structure_functions->render)
	{
		// offset = s16_vec2_add(offset, obj->position.start);
		obj->structure_functions->render(obj, offset, batch);
	}
}

struct sol_gui_object* sol_gui_object_hit_scan(struct sol_gui_object* obj, s16_vec2 location)
{
	assert(obj);

	if(obj->structure_functions && obj->structure_functions->hit_scan)
	{
		// location = s16_vec2_sub(location, obj->position.start);
		return obj->structure_functions->hit_scan(obj, location);
	}

	return NULL;
}

s16_vec2 sol_gui_object_min_size(struct sol_gui_object* obj, uint32_t position_flags)
{
	assert(obj);
	assert((position_flags & ~SOL_GUI_OBJECT_POSITION_FLAGS_ALL) == 0);// don't pass in non position flags

	obj->flags |= position_flags & SOL_GUI_OBJECT_POSITION_FLAGS_ALL;

	if(obj->structure_functions && obj->structure_functions->min_size)
	{
		obj->min_size = obj->structure_functions->min_size(obj);
	}
	else
	{
		obj->min_size = s16_vec2_set(0, 0);
	}

	return obj->min_size;
}

void sol_gui_object_place_content(struct sol_gui_object* obj, s16_rect content_rect)
{
	assert(obj);

	if(obj->structure_functions && obj->structure_functions->place_content)
	{
		obj->structure_functions->place_content(obj, content_rect);
	}
	else
	{
		obj->position = content_rect;
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
}

void sol_gui_object_remove_child(struct sol_gui_object* obj, struct sol_gui_object* child)
{
	assert(obj);
	assert(obj->structure_functions && obj->structure_functions->remove_child);
	assert(child);
	assert(child->parent == obj);

	sol_gui_object_release(child);

	obj->structure_functions->remove_child(obj, child);
	child->parent = NULL;
	child->prev = NULL;
	child->next = NULL;
}



// this should be used with the utmost care (prefer calling context reorganization function)
// void sol_gui_object_reorganise(struct sol_gui_object* obj, s16_rect rect)
// {
// 	s16_vec2 min_size = sol_gui_object_min_size(obj);
// 	assert( s16_vec2_cmp_all_lte(min_size, s16_rect_size(rect)) );// provided rect must have enough space for
// 	sol_gui_object_place_content(obj, rect);
// }


// perhaps handle active widget if necessary? also searches for
bool sol_gui_object_handle_input(struct sol_gui_object* obj, const struct sol_input* input)
{
	if(obj->input_action)
	{
		return obj->input_action(obj, input);
	}
	fprintf(stderr, "should not allow input to be called when widget does not support input");
	return false;
}


