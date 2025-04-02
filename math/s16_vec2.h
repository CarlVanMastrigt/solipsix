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

#include "math/m16_vec2.h"

typedef struct s16_vec2
{
    int16_t x;
    int16_t y;
}
s16_vec2;

///rectangle
static inline s16_vec2 s16_vec2_set(int16_t x,int16_t y)
{
    return (s16_vec2){.x=x, .y=y};
}
static inline s16_vec2 s16_vec2_add(s16_vec2 lhs, s16_vec2 rhs)
{
    return (s16_vec2){.x=lhs.x+rhs.x, .y=lhs.y+rhs.y};
}
static inline s16_vec2 s16_vec2_sub(s16_vec2 lhs, s16_vec2 rhs)
{
    return (s16_vec2){.x=lhs.x-rhs.x, .y=lhs.y-rhs.y};
}
static inline s16_vec2 s16_vec2_mul(s16_vec2 lhs, s16_vec2 rhs)
{
    return (s16_vec2){.x=lhs.x*rhs.x, .y=lhs.y*rhs.y};
}
static inline s16_vec2 s16_vec2_mul_scalar(s16_vec2 v, int16_t s)
{
    return (s16_vec2){.x=v.x*s, .y=v.y*s};
}
static inline s16_vec2 s16_vec2_mask(s16_vec2 v, m16_vec2 m)
{
    return (s16_vec2){.x=v.x&m.x, .y=v.y&m.y};
}
static inline s16_vec2 s16_vec2_min(s16_vec2 lhs, s16_vec2 rhs)
{
    return (s16_vec2)
    {
        .x = lhs.x<rhs.x ? lhs.x : rhs.x,
        .y = lhs.y<rhs.y ? lhs.y : rhs.y,
    };
}
static inline s16_vec2 s16_vec2_max(s16_vec2 lhs, s16_vec2 rhs)
{
    return (s16_vec2)
    {
        .x = lhs.x>rhs.x ? lhs.x : rhs.x,
        .y = lhs.y>rhs.y ? lhs.y : rhs.y,
    };
}
static inline m16_vec2 s16_vec2_cmp_eq(s16_vec2 lhs, s16_vec2 rhs)
{
    return (m16_vec2)
    {
        .x = (lhs.x == rhs.x) ? 0xFFFF : 0,
        .y = (lhs.y == rhs.y) ? 0xFFFF : 0,
    };
}
static inline m16_vec2 s16_vec2_cmp_lt(s16_vec2 lhs, s16_vec2 rhs)
{
    return (m16_vec2)
    {
        .x = (lhs.x < rhs.x) ? 0xFFFF : 0,
        .y = (lhs.y < rhs.y) ? 0xFFFF : 0,
    };
}
static inline m16_vec2 s16_vec2_cmp_lte(s16_vec2 lhs, s16_vec2 rhs)
{
    return (m16_vec2)
    {
        .x = (lhs.x <= rhs.x) ? 0xFFFF : 0,
        .y = (lhs.y <= rhs.y) ? 0xFFFF : 0,
    };
}
static inline m16_vec2 s16_vec2_cmp_gt(s16_vec2 lhs, s16_vec2 rhs)
{
    return (m16_vec2)
    {
        .x = (lhs.x > rhs.x) ? 0xFFFF : 0,
        .y = (lhs.y > rhs.y) ? 0xFFFF : 0,
    };
}
static inline m16_vec2 s16_vec2_cmp_gte(s16_vec2 lhs, s16_vec2 rhs)
{
    return (m16_vec2)
    {
        .x = (lhs.x >= rhs.x) ? 0xFFFF : 0,
        .y = (lhs.y >= rhs.y) ? 0xFFFF : 0,
    };
}

