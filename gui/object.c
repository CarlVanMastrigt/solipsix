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

void sol_gui_object_release(struct sol_gui_object* obj)
{
	struct sol_gui_context* context = obj->context;

	assert(obj->reference_count > 0);

	obj->reference_count--;

	if(obj->reference_count == 0)
	{
		sol_gui_object_remove_from_parent(obj);

		assert(obj->parent == NULL);
		assert(obj->prev == NULL);
		assert(obj->next == NULL);

		obj->structure_functions->destroy(obj);
		free(obj);
		context->registered_object_count--;
	}
}

void sol_gui_object_remove_from_parent(struct sol_gui_object* obj)
{
	if(obj->parent)// may be root object or detached object
	{
		sol_gui_object_remove_child(obj->parent, obj);
	}
}




void sol_gui_object_render(struct sol_gui_object* obj, vec2_s16 offset, struct cvm_overlay_render_batch* batch)
{
	assert(obj);

	if(obj->structure_functions && obj->structure_functions->render)
	{
		offset = vec2_s16_add(offset, obj->position.start);
		obj->structure_functions->render(obj, offset, batch);
	}
}

struct sol_gui_object* sol_gui_object_hit_scan(struct sol_gui_object* obj, vec2_s16 location)
{
	assert(obj);

	if(obj->structure_functions && obj->structure_functions->hit_scan)
	{
		location = vec2_s16_sub(location, obj->position.start);
		return obj->structure_functions->hit_scan(obj, location);
	}

	return NULL;
}

vec2_s16 sol_gui_object_min_size(struct sol_gui_object* obj)
{
	assert(obj);

	vec2_s16 min_size = {0,0};

	if(obj->structure_functions && obj->structure_functions->min_size)
	{
		min_size = obj->structure_functions->min_size(obj);
	}

	obj->min_size = min_size;
	return min_size;
}

void sol_gui_object_place_content(struct sol_gui_object* obj, rect_s16 content_rect)
{
	assert(obj);

	if(obj->structure_functions && obj->structure_functions->place_content)
	{
		obj->structure_functions->place_content(obj, content_rect);
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

	#warning add reference_count ? (child and parent?)

	child->parent = obj;
	obj->structure_functions->add_child(obj, child);
}

void sol_gui_object_remove_child(struct sol_gui_object* obj, struct sol_gui_object* child)
{
	assert(obj);
	assert(obj->structure_functions && obj->structure_functions->remove_child);
	assert(child);
	assert(child->parent == obj);

	#warning remove reference_count ? (child and parent?)

	obj->structure_functions->remove_child(obj, child);
	child->parent = NULL;
	child->prev = NULL;
	child->next = NULL;
}



// this should be used with the utmost care (prefer calling context reorganization function)
// void sol_gui_object_reorganise(struct sol_gui_object* obj, rect_s16 rect)
// {
// 	vec2_s16 min_size = sol_gui_object_min_size(obj);
// 	assert( vec2_s16_cmp_all_lte(min_size, rect_s16_size(rect)) );// provided rect must have enough space for
// 	sol_gui_object_place_content(obj, rect);
// }


// perhaps handle active widget if necessary? also searches for
bool sol_gui_object_handle_input(struct sol_gui_object* obj, const struct sol_input* input)
{
	return obj->input_action(obj, input);
}


