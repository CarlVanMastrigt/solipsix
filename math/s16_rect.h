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

#include "math/s16_vec2.h"


typedef struct s16_rect
{
    s16_vec2 start;
    s16_vec2 end;
}
s16_rect;


static inline s16_rect s16_rect_set(int16_t x_start, int16_t y_start, int16_t x_end, int16_t y_end)
{
    return (s16_rect){.start = s16_vec2_set(x_start, y_start), .end = s16_vec2_set(x_end, y_end)};
}
static inline bool s16_rect_valid(s16_rect r)
{
    return r.start.x<=r.end.x && r.start.y<=r.end.y;
}
static inline bool s16_rect_will_intersect(s16_rect lhs, s16_rect rhs)
{
    return lhs.start.x < rhs.end.x && rhs.start.x < lhs.end.x && lhs.start.y < rhs.end.y && rhs.start.y < lhs.end.y;
}
static inline s16_rect s16_rect_intersect(s16_rect lhs, s16_rect rhs)
{
    // note that this function doesn't return whether the 2 rectangles intersect
    // in the case that intersection isn't guaranteed `s16_rect_will_intersect` can be used
    // OR the result of this function can be validated with `s16_rect_valid` as an invalid rect will be returned should they not intersect

    lhs.start.x = (rhs.start.x < lhs.start.x) ? lhs.start.x : rhs.start.x;
    lhs.start.y = (rhs.start.y < lhs.start.y) ? lhs.start.y : rhs.start.y;

    lhs.end.x = (rhs.end.x > lhs.end.x) ? lhs.end.x : rhs.end.x;
    lhs.end.y = (rhs.end.y > lhs.end.y) ? lhs.end.y : rhs.end.y;

    return lhs;
}
static inline s16_rect s16_rect_add_offset(s16_rect r, s16_vec2 o)
{
    return (s16_rect){.start.x=r.start.x+o.x, .start.y=r.start.y+o.y, .end.x=r.end.x+o.x, .end.y=r.end.y+o.y};
}
static inline s16_rect s16_rect_sub_offset(s16_rect r, s16_vec2 o)
{
    return (s16_rect){.start.x=r.start.x-o.x, .start.y=r.start.y-o.y,.end.x=r.end.x-o.x, .end.y=r.end.y-o.y};
}
static inline s16_rect s16_rect_dilate(s16_rect r, int32_t d)
{
    return (s16_rect){.start.x=r.start.x-d, .start.y=r.start.y-d,.end.x=r.end.x+d, .end.y=r.end.y+d};
}
static inline s16_rect s16_rect_add_border(s16_rect r, s16_vec2 b)
{
    return (s16_rect){.start = s16_vec2_sub(r.start, b),.end = s16_vec2_add(r.end, b)};
}
static inline s16_rect s16_rect_sub_border(s16_rect r, s16_vec2 b)
{
    return (s16_rect){.start = s16_vec2_add(r.start, b),.end = s16_vec2_sub(r.end, b)};
}
static inline bool s16_rect_contains_point(s16_rect r, s16_vec2 p)
{
    return ((r.start.x <= p.x)&&(r.start.y <= p.y)&&(r.end.x > p.x)&&(r.end.y> p.y));
}
static inline bool s16_rect_contains_origin(s16_rect r)
{
    return ((r.start.x <= 0) && (r.start.y <= 0) && (r.end.x > 0)&&(r.end.y> 0));
}
static inline s16_vec2 s16_rect_size(s16_rect r)
{
    return s16_vec2_sub(r.end, r.start);
}
