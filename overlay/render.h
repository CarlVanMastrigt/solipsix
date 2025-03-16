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

#include "data_structures/stack.h"

struct sol_overlay_render_element
{
    int16_t pos_rect[4];/// start(x,y), end(x,y)
    uint16_t tex_coords[4];/// base_tex(x,y), mask_tex(x,y)
    uint32_t data1[4];// extra data: texture_id{2}
};

//SOL_STACK(cvm_overlay_element_render_data, cvm_overlay_element_render_data_stack, cvm_overlay_element_render_data_stack)