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

// this is to FULLY contain freetype

struct sol_font_library;
struct sol_font;

struct sol_font_library* sol_font_library_create(void);// requires image atlas?
void sol_font_library_destroy(struct sol_font_library* font_library);

struct sol_font* sol_font_create(const struct sol_font_library* font_library, char* ttf_filename, int pixel_size);
void sol_font_destroy(struct sol_font* font);