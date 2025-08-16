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

#include "math/s16_rect.h"
#include "vk/shunt_buffer.h"
#include "vk/image.h"
#include "vk/image_atlas.h"

#warning important to outline how bytes are used
/** note, 32 bytes total */
struct sol_overlay_render_element
{
    int16_t pos_rect[4];/// start(x,y), end(x,y)
    uint16_t tex_coords[4];/// base_tex(x,y), mask_tex(x,y)
    uint32_t data1[4];// extra data - texture_id:2
};

#define SOL_STACK_ENTRY_TYPE struct sol_overlay_render_element
#define SOL_STACK_FUNCTION_PREFIX sol_overlay_render_element_stack
#define SOL_STACK_STRUCT_NAME sol_overlay_render_element_stack
#include "data_structures/stack.h"

struct sol_overlay_render_context
{
    /** atlases used for storing backing information for verious purposes
     * some of these may be null depending on implementation details
     * these will, in order, match bind points in shader that renders overlay elements */
    struct sol_image_atlas* bc4_atlas;
    struct sol_image_atlas* r8_atlas;
    struct sol_image_atlas* rgba8_atlas;
};

/** batch is a bad name, need context, sub context stack/ranges for (potential) compositing passes
 * at that point is it maybe better to just handle the backing manually? */
struct sol_overlay_render_batch
{
    struct sol_overlay_render_context* context;

    /** bounds/limit of the current point in the render, this is almost a stack allocated value as it can change at any point in the render and that must then be carried forward */
    s16_rect bounds;

    /** actual UI element instance data */
    struct sol_overlay_render_element_stack elements;

    /** miscellaneous inline upload buffer */
    struct sol_vk_shunt_buffer upload_shunt_buffer;
};





