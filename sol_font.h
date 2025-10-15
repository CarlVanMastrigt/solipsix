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

#include "overlay/enums.h"

#include "math/s16_vec2.h"
#include "math/s16_rect.h"

// this is to FULLY contain freetype
// create and destroy avoids freetype being defined in any more than one c file (size needn't be known globally)

struct sol_font_library;
struct sol_font;

struct sol_font_library* sol_font_library_create(void);
void sol_font_library_destroy(struct sol_font_library* font_library);

struct sol_font* sol_font_create(struct sol_font_library* font_library, const char* ttf_filename, int pixel_size, bool subpixel_offset_render, const char* default_script_id, const char* default_language_id, const char* default_direction_id);
void sol_font_destroy(struct sol_font* font);




struct sol_overlay_render_batch;

/* will need a better version for "composed" text segments eventually */
/** this just renders the text in a line with the default properties of the font library */
void sol_font_render_text_simple(const char* text, struct sol_font* font, enum sol_overlay_colour colour, s16_rect position, struct sol_overlay_render_batch* render_batch);
s16_vec2 sol_font_size_text_simple(const char* text, struct sol_font* font);


/** variants that function for single glyphs, the first found in the string, it will be extremely wasteful to provide a string that converts to more than 1 glyph
 * NOTE: this will function in a standardised way; centring the glyph and providing a uniform (per font, rather than a per glyph) size */
void sol_font_render_glyph_simple(const char* utf8_glyph, struct sol_font* font, enum sol_overlay_colour colour, s16_rect position, struct sol_overlay_render_batch* render_batch);
s16_vec2 sol_font_size_glyph_simple(const char* utf8_glyph, struct sol_font* font);

