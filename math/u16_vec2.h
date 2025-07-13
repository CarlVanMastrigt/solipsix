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
#include <limits.h>

#include "math/m16_vec2.h"

typedef struct u16_vec2
{
    uint16_t x;
    uint16_t y;
}
u16_vec2;

///rectangle
static inline u16_vec2 u16_vec2_set(uint16_t x,uint16_t y)
{
    return (u16_vec2){.x=x, .y=y};
}
static inline u16_vec2 u16_vec2_add(u16_vec2 lhs, u16_vec2 rhs)
{
    return (u16_vec2){.x=lhs.x+rhs.x, .y=lhs.y+rhs.y};
}
static inline u16_vec2 u16_vec2_sub(u16_vec2 lhs, u16_vec2 rhs)
{
    return (u16_vec2){.x=lhs.x-rhs.x, .y=lhs.y-rhs.y};
}
static inline u16_vec2 u16_vec2_mul(u16_vec2 lhs, u16_vec2 rhs)
{
    return (u16_vec2){.x=lhs.x*rhs.x, .y=lhs.y*rhs.y};
}
static inline u16_vec2 u16_vec2_mul_scalar(u16_vec2 v, uint16_t s)
{
    return (u16_vec2){.x=v.x*s, .y=v.y*s};
}
static inline u16_vec2 u16_vec2_mask(u16_vec2 v, m16_vec2 m)
{
    return (u16_vec2){.x=v.x&m.x, .y=v.y&m.y};
}
static inline u16_vec2 u16_vec2_min(u16_vec2 lhs, u16_vec2 rhs)
{
    return (u16_vec2)
    {
        .x = lhs.x<rhs.x ? lhs.x : rhs.x,
        .y = lhs.y<rhs.y ? lhs.y : rhs.y,
    };
}
static inline u16_vec2 u16_vec2_max(u16_vec2 lhs, u16_vec2 rhs)
{
    return (u16_vec2)
    {
        .x = lhs.x>rhs.x ? lhs.x : rhs.x,
        .y = lhs.y>rhs.y ? lhs.y : rhs.y,
    };
}
static inline m16_vec2 u16_vec2_cmp_eq(u16_vec2 lhs, u16_vec2 rhs)
{
    return (m16_vec2)
    {
        .x = (lhs.x == rhs.x) ? UINT16_MAX : 0,
        .y = (lhs.y == rhs.y) ? UINT16_MAX : 0,
    };
}
static inline m16_vec2 u16_vec2_cmp_lt(u16_vec2 lhs, u16_vec2 rhs)
{
    return (m16_vec2)
    {
        .x = (lhs.x < rhs.x) ? UINT16_MAX : 0,
        .y = (lhs.y < rhs.y) ? UINT16_MAX : 0,
    };
}
static inline m16_vec2 u16_vec2_cmp_lte(u16_vec2 lhs, u16_vec2 rhs)
{
    return (m16_vec2)
    {
        .x = (lhs.x <= rhs.x) ? UINT16_MAX : 0,
        .y = (lhs.y <= rhs.y) ? UINT16_MAX : 0,
    };
}
static inline m16_vec2 u16_vec2_cmp_gt(u16_vec2 lhs, u16_vec2 rhs)
{
    return (m16_vec2)
    {
        .x = (lhs.x > rhs.x) ? UINT16_MAX : 0,
        .y = (lhs.y > rhs.y) ? UINT16_MAX : 0,
    };
}
static inline m16_vec2 u16_vec2_cmp_gte(u16_vec2 lhs, u16_vec2 rhs)
{
    return (m16_vec2)
    {
        .x = (lhs.x >= rhs.x) ? UINT16_MAX : 0,
        .y = (lhs.y >= rhs.y) ? UINT16_MAX : 0,
    };
}

