/**
Copyright 2025, 2026 Carl van Mastrigt

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

#include "math/s16_vec2.h"
#include "math/s16_rect.h"

#include "solipsix/gui/range_control_distribution.h"

/** size classes:
 * size classes allow the creation of boxes that neatly fit inside one another and can fulfill different purposes
 * would replace box, panel &c.
*/

#warning for texturing may want overlay to know window position relative to screen

struct sol_overlay_render_batch;
struct sol_font;
enum sol_overlay_colour;
enum sol_overlay_orientation;

struct sol_gui_theme
{
    struct sol_font* text_font;
    struct sol_font* icon_font;
    #warning would like a way to handle multiple fonts, perhaps an array and index into them? enum? should probably do same as colours!

    void* other_data;/// theme specific data


    int16_t parallel_fade_range;
    int16_t perpendicular_fade_range;
    #warning ^ instead have functions for this?


    /**
     * render: appends the data necessary to render into the render batch
     * select: returns true if the box/panel covers the origin (0,0) of the rectangles coordinate system (rectangle's position should be adjusted to offset actual pixel to assess)
     * place_content: returns a rectangle that fits inside the box/panel that has correct offsets to render content to
     * size: given some content, how big does the box/panel need to be (paired with place_content)
    */

    /** these should allow lining up custom widgets with the lines on boxes */ 
    int16_t    (*generic_size_x)           (struct sol_gui_theme*, uint32_t flags, int16_t content_size);
    int16_t    (*generic_size_y)           (struct sol_gui_theme*, uint32_t flags, int16_t content_size);
    s16_extent (*generic_content_extent_x) (struct sol_gui_theme*, uint32_t flags, s16_extent outer_extent);
    s16_extent (*generic_content_extent_y) (struct sol_gui_theme*, uint32_t flags, s16_extent outer_extent);


    void       (*box_render)              (struct sol_gui_theme*, uint32_t flags, s16_rect current_rect, struct sol_overlay_render_batch * batch, enum sol_overlay_colour colour);
    bool       (*box_select)              (struct sol_gui_theme*, uint32_t flags, s16_rect current_rect, s16_vec2 location);/// box should be offset should be such that the origin is the selection point to be queried
    int16_t    (*box_size_x)              (struct sol_gui_theme*, uint32_t flags, int16_t content_size);
    int16_t    (*box_size_y)              (struct sol_gui_theme*, uint32_t flags, int16_t content_size);
    /** given the extent of the box itself, get the extent of its contents */
    s16_extent (*box_content_extent_x)    (struct sol_gui_theme*, uint32_t flags, s16_extent box_extent);
    s16_extent (*box_content_extent_y)    (struct sol_gui_theme*, uint32_t flags, s16_extent box_extent);
    /** above return extent in same space as the panel extent */
    // may need a function that subtracts borders for items that require clipping to be passed on, OR parent information can be passed down (bounds and flags that dictate borders)

    // box render with scroll information ?? (for localised scroll, can be extra)
    void       (*panel_render)            (struct sol_gui_theme*, uint32_t flags, s16_rect current_rect, struct sol_overlay_render_batch * batch, enum sol_overlay_colour colour);
    bool       (*panel_select)            (struct sol_gui_theme*, uint32_t flags, s16_rect current_rect, s16_vec2 location);
    int16_t    (*panel_size_x)            (struct sol_gui_theme*, uint32_t flags, int16_t content_size);
    int16_t    (*panel_size_y)            (struct sol_gui_theme*, uint32_t flags, int16_t content_size);
    /** given the extent of the panel itself, get the extent of its contents */
    s16_extent (*panel_contents_extent_x) (struct sol_gui_theme*, uint32_t flags, s16_extent panel_extent);
    s16_extent (*panel_contents_extent_y) (struct sol_gui_theme*, uint32_t flags, s16_extent panel_extent);
    /** above return extent in same space as the panel extent */


    /** these are used for generic scroll bars, the intention being a box will first be rendered then this will be rendered as content on top, much the same as text, 
     * TODO: consider that these could be used for BOTH an x and y variable at the same time*/
    void       (*range_control_render)    (struct sol_gui_theme*, uint32_t flags, enum sol_overlay_orientation, s16_rect current_rect, struct sol_overlay_render_batch * batch, enum sol_overlay_colour main_colour, enum sol_overlay_colour interior_colour, struct sol_range_control_distribution);
    /** note: this will only communicate selection of the interior (grababale part) of the bar, box select is required to know if another part of the bar was selected 
     * the selected_offset is an axis applicable offset to apply to the cursor when determining the current position of the cursor withing the selection_extent */
    bool       (*range_control_select)    (struct sol_gui_theme*, uint32_t flags, enum sol_overlay_orientation, s16_rect current_rect, s16_vec2 location);
    bool       (*range_control_interior)  (struct sol_gui_theme*, uint32_t flags, enum sol_overlay_orientation, s16_rect current_rect, s16_vec2 location, struct sol_range_control_distribution, int16_t* range_offset);
    /** this returns the extent (in the appropriate axis) over which  */ 
    s16_extent (*range_control_selection) (struct sol_gui_theme*, uint32_t flags, enum sol_overlay_orientation, s16_rect current_rect, struct sol_range_control_distribution);

    /** the size of a variable box widget; should be used INSTEAD of the box size functions (despite also rendering the box being suggested) 
     * the number of gradations may be inaccurate if a nonzero interior region is provided in the distribution used to render */
    int16_t (*range_control_size_x) (struct sol_gui_theme*, uint32_t flags, enum sol_overlay_orientation, int16_t gradations);
    int16_t (*range_control_size_y) (struct sol_gui_theme*, uint32_t flags, enum sol_overlay_orientation, int16_t gradations);



    #warning should rendering consider / use bounds? limited render chould be managed with constrained renders, but this doesnt allow genericized use of widgets within constraints, which is probably undesirable...
    // struct that passes down information regarding current bounds/culling/fade could also pass animation information (e.g. current time)
    // would almost need to compose clip functions (could just do this?) intermediary render very undesirable, but may be "the" solution...
    // may also want generic bounding struct to apply to rendering, can impose box and panel clipping, fade, hard bounds &c.

    #warning call function clip against box (subset of clip generic which acts on any shaded/mask element)

    #warning add option to compose noise

    #warning allow colours to be dynamic over time (highlichted element pulsates)

    // render element with fade ?? (may require re-write of shader)
    #warning fade can be simplified in shader if the fade section is broken up, need only fade indicator for each vertex (?)
    #warning how do we handle text box that has parts coming in from both sides? (custom job?)
    // rectangle   (*get_sliderbar_offsets)(overlay_theme * theme,uint32_t status);///returns offsets from each respective side
};

