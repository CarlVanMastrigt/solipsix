/**
Copyright 2020,2021,2022 Carl van Mastrigt

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

#ifndef solipsix_H
#include "solipsix.h"
#endif

#ifndef CUBIC_THEME_H
#define CUBIC_THEME_H

typedef struct cubic_theme_data
{
    cvm_vk_image_atlas_tile * foreground_image_tile;
    uint16_t * foreground_selection_grid;
    int foreground_offset_x;
    int foreground_offset_y;
    int foreground_r;
    int foreground_d;

    cvm_vk_image_atlas_tile * background_image_tile;
    uint16_t * background_selection_grid;
    int background_r;
    int background_d;

    cvm_vk_image_atlas_tile * internal_image_tile;
    //uint16_t * internal_selection_grid;//probably not needed
    int internal_r;
    int internal_d;
}
cubic_theme_data;

overlay_theme * create_cubic_theme(const FT_Library * freetype_library);
void destroy_cubic_theme(overlay_theme * theme, cvm_vk_image_atlas * backing_image_atlas);/// atlas temporary variable, will be removed once atlas has better management

#endif


