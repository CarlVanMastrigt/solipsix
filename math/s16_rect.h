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

#include "math/s16_extent.h"
#include "math/s16_vec2.h"


typedef struct s16_rect
{
    /** note: extent start is inclusive, extent end is exclusive **/
    s16_extent x;
    s16_extent y;
}
s16_rect;


static inline s16_rect s16_rect_set(int16_t x_start, int16_t y_start, int16_t x_end, int16_t y_end)
{
    return (s16_rect){.x = s16_extent_set(x_start, x_end), .y = s16_extent_set(y_start, y_end)};
}
static inline bool s16_rect_valid(s16_rect r)
{
    return r.x.start <= r.x.end && r.y.start <= r.y.end;
}
static inline bool s16_rect_will_intersect(s16_rect lhs, s16_rect rhs)
{
    #warning implement in terms of extent intersects
    return lhs.x.start < rhs.x.end && rhs.x.start < lhs.x.end && lhs.y.start < rhs.y.end && rhs.y.start < lhs.y.end;
}
static inline s16_rect s16_rect_intersect(s16_rect lhs, s16_rect rhs)
{
    // note that this function doesn't return whether the 2 rectangles intersect
    // in the case that intersection isn't guaranteed `s16_rect_will_intersect` can be used
    // OR the result of this function can be validated by checking its dimensions are both greater than zero

    #warning implement in terms of extent intersects

    lhs.x.start = (rhs.x.start < lhs.x.start) ? lhs.x.start : rhs.x.start;
    lhs.y.start = (rhs.y.start < lhs.y.start) ? lhs.y.start : rhs.y.start;

    lhs.x.end = (rhs.x.end > lhs.x.end) ? lhs.x.end : rhs.x.end;
    lhs.y.end = (rhs.y.end > lhs.y.end) ? lhs.y.end : rhs.y.end;

    return lhs;
}
static inline s16_rect s16_rect_add_offset(s16_rect r, s16_vec2 o)
{
    return (s16_rect){.x.start=r.x.start+o.x, .y.start=r.y.start+o.y, .x.end=r.x.end+o.x, .y.end=r.y.end+o.y};
}
static inline s16_rect s16_rect_sub_offset(s16_rect r, s16_vec2 o)
{
    return (s16_rect){.x.start=r.x.start-o.x, .y.start=r.y.start-o.y,.x.end=r.x.end-o.x, .y.end=r.y.end-o.y};
}
static inline s16_rect s16_rect_dilate(s16_rect r, int32_t d)
{
    return (s16_rect){.x.start=r.x.start-d, .y.start=r.y.start-d,.x.end=r.x.end+d, .y.end=r.y.end+d};
}
static inline s16_rect s16_rect_add_border(s16_rect r, s16_vec2 b)
{
    return (s16_rect){.x = s16_extent_dilate(r.x, b.x), .y = s16_extent_dilate(r.y, b.y)};
}
static inline s16_rect s16_rect_sub_border(s16_rect r, s16_vec2 b)
{
    return (s16_rect){.x = s16_extent_dilate(r.x, -b.x), .y = s16_extent_dilate(r.y, -b.y)};
}
static inline bool s16_rect_contains_point(s16_rect r, s16_vec2 p)
{
    // return ((r.x.start <= p.x)&&(r.y.start <= p.y)&&(r.x.end > p.x)&&(r.y.end> p.y));
    return s16_extent_contains(r.x, p.x) && s16_extent_contains(r.y, p.y);
}
static inline s16_vec2 s16_rect_size(s16_rect r)
{
    return (s16_vec2){.x = s16_extent_size(r.x), .y = s16_extent_size(r.y)};
}
static inline s16_rect s16_rect_at_origin_with_size(s16_vec2 size)
{
    return (s16_rect){.x = s16_extent_set(0, size.x),.y = s16_extent_set(0, size.y)};
}
static inline s16_rect s16_rect_at_location_with_size(s16_vec2 location, s16_vec2 size)
{
    return (s16_rect){.x = s16_extent_set(location.x, location.x + size.x),.y = s16_extent_set(location.y, location.y + size.y)};
}
static inline s16_vec2 s16_rect_start(s16_rect r)
{
    return (s16_vec2){.x = r.x.start,.y = r.y.start};
}
static inline s16_vec2 s16_rect_end(s16_rect r)
{
    return (s16_vec2){.x = r.x.end,.y = r.y.end};
}
static inline int16_t s16_rect_max_dim(s16_rect r)
{
    int16_t x = s16_extent_size(r.x);
    int16_t y = s16_extent_size(r.y);
    return x > y ? x : y;
}
static inline int16_t s16_rect_min_dim(s16_rect r)
{
    int16_t x = s16_extent_size(r.x);
    int16_t y = s16_extent_size(r.y);
    return x < y ? x : y;
}
