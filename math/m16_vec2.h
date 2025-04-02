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

typedef struct m16_vec2
{
    uint16_t x;
    uint16_t y;
}
m16_vec2;

static inline bool m16_vec2_any(m16_vec2 v)
{
    return v.x || v.y;
}
static inline bool m16_vec2_all(m16_vec2 v)
{
    return v.x && v.y;
}
