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

#include <assert.h>
#include <stdlib.h>

#include <ft2build.h>
#include <freetype/freetype.h>

#include "sol_font.h"

struct sol_font_library
{
	FT_Library freetype_library;
	// add pointer to image atlas here?
};

struct sol_font
{
	// could avoid overlap and have these be ref-counted...

    FT_Face face;

    int16_t glyph_size;

    int16_t space_advance;
    int16_t space_character_index;

    int16_t line_spacing;// vertical advance?

    int16_t max_advance;

    // ///not a great solution but bubble sort new glyphs from end?, is rare enough op and gives low enough cache use to justify?
    // cvm_overlay_glyph * glyphs;
    // uint32_t glyph_count;
    // uint32_t glyph_space;
};




struct sol_font_library* sol_font_library_create(void)
{
	struct sol_font_library* font_library = malloc(sizeof(struct sol_font_library));
	int freetype_error = FT_Init_FreeType(&font_library->freetype_library);

    assert(!freetype_error);

    return font_library;
}

void sol_font_library_destroy(struct sol_font_library* font_library)
{
	int freetype_error = FT_Done_FreeType(font_library->freetype_library);
    assert(!freetype_error);

	free(font_library);
}





struct sol_font* sol_font_create(const struct sol_font_library* font_library, char* ttf_filename, int pixel_size)
{
	struct sol_font* font = malloc(sizeof(struct sol_font));
	return font;
}

void sol_font_destroy(struct sol_font* font)
{
	free(font);
}

