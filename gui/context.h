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

#include <inttypes.h>
#include <stddef.h>

#include "math/s16_vec2.h"

struct sol_input;
struct sol_gui_object;
struct cvm_overlay_render_batch;

/** context
 * outside ofgui setup/creation usually want to pass around context for rendering, input management &c.
*/

struct sol_gui_context
{
    // should be position and size of containing system window
    s16_vec2 window_offset;
    s16_vec2 window_size;
    s16_vec2 window_min_size;

    struct sol_gui_theme* theme;

    /// put settings here?? make it a pointer!?



    uint32_t registered_object_count;//for debug
    bool content_fit;

    // uint32_t double_click_time;//move to settings

    // if true, prominent object was set via GUI navigation (arrow keys not mouse) and so cannot be set to null with mouse
    // bool highlighted_object_navigated;// this needs a better name

    // in principle highlight (probably not focus) could be PER mouse, but that an edge case and a PITA to implement

    struct sol_gui_object* highlighted_object;// highlighted when using directional (arrows/joystick) navigation, can/should probably change with mouse motion?
    struct sol_gui_object* focused_object;// limit interaction to only this object and its children, click away/cancel called when defocused?

    bool highlight_removable;// instead of just replacable (highlight always replacable) if new highlight is non-null
    struct sol_gui_object* previous_highlighted_object;// necessary/useful for re-entering highlight via keyboard actions

    /// used for double click detection
    struct sol_gui_object* previously_clicked_object;
    uint32_t previously_clicked_time;

    struct sol_gui_object* root_container;// this should not change

    // scratch used by any part of the GUI when space is needed (specifically possible because a GUI context is single threaded)
    void* scratch_buffer;
    size_t scratch_space;

    // event used for interoperability with SDL and usage in gui object input
    uint32_t SOL_GUI_EVENT_OBJECT_HIGHLIGHT_BEGIN;
    uint32_t SOL_GUI_EVENT_OBJECT_HIGHLIGHT_END;
    uint32_t SOL_GUI_EVENT_OBJECT_FOCUS_BEGIN;
    uint32_t SOL_GUI_EVENT_OBJECT_FOCUS_END;
};

// also creates and returns the root object
struct sol_gui_object* sol_gui_context_initialise(struct sol_gui_context* context, struct sol_gui_theme* theme, s16_vec2 window_offset, s16_vec2 window_size);
void                   sol_gui_context_terminate (struct sol_gui_context* context);

// actually not sure how to handle these, they WILL retain objects though
void sol_gui_context_change_highlighted_object(struct sol_gui_context* context, struct sol_gui_object* obj, bool removable);
void sol_gui_context_change_focused_object    (struct sol_gui_context* context, struct sol_gui_object* obj);


/**
 * on failure should use context's window_min_size to ensure window is big enough before calling next
 * otherwise if min_size is null will supress errors and FORCE it to work
 *  this will result in content escaping the screen range, and will set content_fits to false
 * */
void sol_gui_context_update_screen_offset(struct sol_gui_context* context, s16_vec2 window_offset);
bool sol_gui_context_update_screen_size(struct sol_gui_context* context, s16_vec2 window_size);
// call this when contents of all widgets may have changed, e.g. at crteation time, after theme change, if a single widget in root has changed, instead try to be more precise
bool sol_gui_context_reorganise_root(struct sol_gui_context* context);

void sol_gui_context_render(struct sol_gui_context* context, struct cvm_overlay_render_batch* batch);
struct sol_gui_object* sol_gui_context_hit_scan(struct sol_gui_context* context, const s16_vec2 location);
bool sol_gui_context_handle_input(struct sol_gui_context* context, const struct sol_input* input);

