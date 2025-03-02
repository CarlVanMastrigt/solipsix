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

#include "math/vec2_s16.h"


typedef struct rect_s16
{
    vec2_s16 start;
    vec2_s16 end;
}
rect_s16;

static inline rect_s16 rect_s16_intersect(rect_s16 lhs, rect_s16 rhs)
{
    lhs.start.x += (rhs.start.x > lhs.start.x) ? (rhs.start.x - lhs.start.x) : 0;
    lhs.start.y += (rhs.start.y > lhs.start.y) ? (rhs.start.y - lhs.start.y) : 0;

    lhs.end.x += (rhs.end.x < lhs.end.x) ? (rhs.end.x - lhs.end.x) : 0;
    lhs.end.y += (rhs.end.y < lhs.end.y) ? (rhs.end.y - lhs.end.y) : 0;

    return lhs;
}
static inline rect_s16 rect_s16_add_offset(rect_s16 r, vec2_s16 o)
{
    return (rect_s16){.start.x=r.start.x+o.x, .start.y=r.start.y+o.y, .end.x=r.end.x+o.x, .end.y=r.end.y+o.y};
}
static inline rect_s16 rect_s16_sub_offset(rect_s16 r, vec2_s16 o)
{
    return (rect_s16){.start.x=r.start.x-o.x, .start.y=r.start.y-o.y,.end.x=r.end.x-o.x, .end.y=r.end.y-o.y};
}
static inline rect_s16 rect_s16_dilate(rect_s16 r, int32_t d)
{
    return (rect_s16){.start.x=r.start.x-d, .start.y=r.start.y-d,.end.x=r.end.x+d, .end.y=r.end.y+d};
}
static inline rect_s16 rect_s16_add_border(rect_s16 r, vec2_s16 b)
{
    return (rect_s16){.start = vec2_s16_sub(r.start, b),.end = vec2_s16_add(r.start, b)};
}
static inline rect_s16 rect_s16_sub_border(rect_s16 r, vec2_s16 b)
{
    return (rect_s16){.start = vec2_s16_add(r.start, b),.end = vec2_s16_sub(r.start, b)};
}
static inline bool rect_s16_contains_point(rect_s16 r, vec2_s16 p)
{
    return ((r.start.x <= p.x)&&(r.start.y <= p.y)&&(r.end.x > p.x)&&(r.end.y> p.y));
}
static inline bool rect_s16_contains_origin(rect_s16 r)
{
    return ((r.start.x <= 0)&&(r.start.y <= 0)&&(r.end.x > 0)&&(r.end.y> 0));
}
static inline bool rect_s16_have_overlap(rect_s16 lhs, rect_s16 rhs)
{
    return lhs.start.x<rhs.end.x && lhs.end.x>rhs.start.x && lhs.start.y<rhs.end.y && lhs.end.y>rhs.start.y;
}
static inline vec2_s16 rect_s16_size(rect_s16 r)
{
    return vec2_s16_sub(r.end, r.start);
}
static inline bool rect_s16_valid(rect_s16 r)
{
    return r.start.x<=r.end.x && r.start.y<=r.end.y;
}