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
#include <stdbool.h>

typedef struct vec2_m16
{
    uint16_t x;
    uint16_t y;
}
vec2_m16;

static inline bool vec2_m16_any(vec2_m16 v)
{
    return v.x || v.y;
}
static inline bool vec2_m16_all(vec2_m16 v)
{
    return v.x && v.y;
}
