/**
Copyright 2026 Carl van Mastrigt

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


typedef struct s16_extent
{
	/** note: start is inclusive, end is exclusive **/
    int16_t start;
    int16_t end;
}
s16_extent;

static inline s16_extent s16_extent_set(int16_t start, int16_t end)
{
	return (s16_extent){.start = start, .end = end};
}
static inline s16_extent s16_extent_add(s16_extent lhs, s16_extent rhs)
{
	return (s16_extent){.start = lhs.start + rhs.start, .end = lhs.end + rhs.end};
}
static inline s16_extent s16_extent_sub(s16_extent lhs, s16_extent rhs)
{
	return (s16_extent){.start = lhs.start - rhs.start, .end = lhs.end - rhs.end};
}
static inline s16_extent s16_extent_dilate(s16_extent e, int16_t d)
{
	return (s16_extent){.start = e.start-d, .end = e.end+d};
}
static inline bool s16_extent_contains(s16_extent e, int16_t v)
{
	return v >= e.start && v < e.end;
}
static inline bool s16_extent_clamp(s16_extent e, int16_t v)
{
	return v < e.start ? e.start : (v >= e.end ? e.end - 1 : v);
}
static inline int16_t s16_extent_size(s16_extent e)
{
	return e.end - e.start;
}
static inline s16_extent s16_extent_add_offset(s16_extent e, int16_t o)
{
	return (s16_extent){.start = e.start + o, .end = e.end + o};
}
static inline s16_extent s16_extent_from_start(s16_extent e)
{
	return (s16_extent){.start = 0, .end = e.end - e.start};
}
