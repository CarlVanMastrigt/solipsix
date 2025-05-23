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

#pragma once

#include <stddef.h>
#include <inttypes.h>

#include "math/s16_vec2.h"
#include "math/s16_rect.h"

#include "gui/enums.h"
#include "gui/context.h"
#include "gui/theme.h"

/** some rules:
 * interaction MAY cause sizes to change and require a single "layout" pass of an ancestor (possibly the root object if interaction causes theme to change)
 * rendering & selection CANNOT require a layout pass, but MAY re-assess the contents of objects in some fashion (e.g. offset of scrollable lists if the backing list changed)
 *
*/

/// not sure if headers for these should be included, probably should
struct cvm_overlay_render_batch;
struct sol_input;

#warning possibly want render batch to know screen/window dimension to prevent rendering offscreen items


struct sol_gui_object;

struct sol_gui_object_structure_functions
{
	#warning should obj be const for most of these?

	// put in the batch to be passed into overlay rendererer
    void (*const render) (struct sol_gui_object* obj, s16_rect position, struct cvm_overlay_render_batch* batch);
    // position is in target space, determined from the object
    #warning should rendering consider / use bounds? limited render chould be managed with constrained renders, but this doesnt allow genericized use of widgets within constraints, which is probably undesirable...
    // struct that passes down information regarding current bounds/culling/fade could also pass animation information (e.g. current time)
    // would almost need to compose clip functions (could just do this?) intermediary render very undesirable, but may be "the" solution...
    // may also want generic bounding struct to apply to rendering, can impose box and panel clipping, fade, hard bounds &c.
    // inherited struct *could* incorporate offset, but then the contents would basically be copied at every stage of the stack

    // used to recursively find the gui_object under the pixel location provided
    struct sol_gui_object* (*const hit_scan) (struct sol_gui_object* obj, s16_rect position, const s16_vec2 location);// hit_scan, scan
    // this structure is suboptimal but more easily understood, it operates in absolute screen space, with location never changing

    // sets the min_size of the gui_object to inform parents of their minimum size
    s16_vec2 (*const min_size) (struct sol_gui_object* obj);

    // fills out any data affected by the position(size and location) of this object (which must have been set by the gui_object's parent) and decides how to distribute that space amongst its children if it has them
    // note: as all contents are relative, this should only take the dimensions of its rect into account (though its position will have been updated by the time this is called)
    void (*const place_content) (struct sol_gui_object* obj, s16_vec2 dimensions);

    // adds a child to this widget, will error if widget cannot accept children
    void (*const add_child) (struct sol_gui_object* obj, struct sol_gui_object* child);
    void (*const remove_child) (struct sol_gui_object* obj, struct sol_gui_object* child);

    void (*const destroy) (struct sol_gui_object* obj);
};

struct sol_gui_object
{
	struct sol_gui_context* context;

	const struct sol_gui_object_structure_functions* structure_functions;

	// may need enumerator for return type?
	bool (*input_action)(struct sol_gui_object* obj, const struct sol_input* input);

	#warning combine status and position flags; pass into rendering and allow them to inform nature of rendering (e.g. if colour passed is DEFAULT then pick correct one from status)
	uint32_t reference_count : SOL_GUI_REFERENCE_BIT_COUNT;
	uint32_t flags           : SOL_GUI_OBJECT_FLAGS_BIT_COUNT;

	s16_vec2 min_size;
	s16_rect position;/// this is relative to parent

	// other widgets in structure
	struct sol_gui_object* parent;
	struct sol_gui_object* next;
	struct sol_gui_object* prev;

	// need something that controls inteaction level of different actions (mouse-down/up, controller-button-down/up) do these activate? do they do something else? can the object track that has happened?
};
// above should be used as a base fot other GUI objects (first element of struct)


/// construct should be used when more data is needed, will instantiate the base object data in an existing object
void sol_gui_object_construct(struct sol_gui_object* obj, struct sol_gui_context* context);

void sol_gui_object_retain(struct sol_gui_object* obj);
bool sol_gui_object_release(struct sol_gui_object* obj);

// basically gets parent and then calls `sol_gui_object_remove_child`
void sol_gui_object_remove_from_parent(struct sol_gui_object* obj);




// these perform any meta operations on objects and call their internal functions if present
void                   sol_gui_object_render       (struct sol_gui_object* obj, s16_vec2 offset, struct cvm_overlay_render_batch* batch);
struct sol_gui_object* sol_gui_object_hit_scan     (struct sol_gui_object* obj, s16_vec2 offset, const s16_vec2 location);
s16_vec2               sol_gui_object_min_size     (struct sol_gui_object* obj, uint32_t position_flags);
void                   sol_gui_object_place_content(struct sol_gui_object* obj, s16_rect content_rect);
void                   sol_gui_object_add_child    (struct sol_gui_object* obj, struct sol_gui_object* child);
void                   sol_gui_object_remove_child (struct sol_gui_object* obj, struct sol_gui_object* child);



// perhaps handle active widget if necessary? also searches for
bool sol_gui_object_handle_input(struct sol_gui_object* obj, const struct sol_input* input);

// must be called if placement of contents has changed (BUT NOT MIN SIZE, if min size has potentially changed toplevel root must be reorganised)
void sol_gui_object_reposition_contents(struct sol_gui_object* obj);



// this should be used with the utmost care (prefer calling context reorganization function)
// void sol_gui_object_reorganise(struct sol_gui_object* obj, s16_rect rect);

