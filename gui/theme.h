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

#include "math/s16_vec2.h"
#include "math/s16_rect.h"

/** size classes:
 * size classes allow the creation of boxes that neatly fit inside one another and can fulfill different purposes
*/

#warning for texturing may want overlay to know window position relative to screen


struct cvm_overlay_render_batch;
enum sol_overlay_colour;
struct sol_font;

struct sol_gui_theme
{
    struct sol_font* font;
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

    // should this even use bounds? limited render should be managed with constrained renders
    void     (*box_render)        (struct sol_gui_theme* theme, uint32_t flags, s16_rect rect, struct cvm_overlay_render_batch * restrict render_batch, enum sol_overlay_colour colour);
    bool     (*box_select)        (struct sol_gui_theme* theme, uint32_t flags, s16_rect rect);/// box should be offset should be such that the origin is the selection point to be queried
    s16_rect (*box_place_content) (struct sol_gui_theme* theme, uint32_t flags, s16_rect rect);
    s16_vec2 (*box_size)          (struct sol_gui_theme* theme, uint32_t flags, s16_vec2 contents_size);

    // box render with scroll information ?? (for localised scroll, can be extra)
    void     (*panel_render)        (struct sol_gui_theme* theme, uint32_t flags, s16_rect rect, struct cvm_overlay_render_batch * restrict render_batch, enum sol_overlay_colour colour);
    bool     (*panel_select)        (struct sol_gui_theme* theme, uint32_t flags, s16_rect rect);
    s16_rect (*panel_place_content) (struct sol_gui_theme* theme, uint32_t flags, s16_rect rect);
    s16_vec2 (*panel_size)          (struct sol_gui_theme* theme, uint32_t flags, s16_vec2 contents_size);

    #warning call function clip against box (subset of clip generic which acts on any shaded/mask element)

    #warning add option to compose noise

    #warning allow colours to be dynamic over time (highlichted element pulsates)

    // render element with fade ?? (may require re-write of shader)
    #warning fade can be simplified in shader if the fade section is broken up, need only fade indicator for each vertex (?)
    #warning how do we handle text box that has parts coming in from both sides? (custom job?)
    // rectangle   (*get_sliderbar_offsets)(overlay_theme * theme,uint32_t status);///returns offsets from each respective side
};

//
static inline struct cvm_overlay_font* sol_gui_theme_get_font(struct sol_gui_theme* theme)
{
    return theme->font;
}