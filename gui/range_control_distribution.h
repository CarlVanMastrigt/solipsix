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
#include <stdio.h>

#include "solipsix/math/s16_extent.h"

/** |---before----|====inner=====|---after_---| */
/** |--posterior--|===interior===|--anterior--| */

enum sol_range_control_distribution_type
{
	SOL_VARIABLE_BAR_DISTRIBUTION_UINT32,
	SOL_VARIABLE_BAR_DISTRIBUTION_FLOAT32,
};

struct sol_range_control_distribution
{
	enum sol_range_control_distribution_type type;
	union
	{
		struct
		{
			uint32_t before;/** posterior */
			uint32_t inner;/** interior */
			uint32_t after;/** anterior */
		}
		uint32;

		struct
		{
			float before;
			float inner;
			float after;
		}
		float32;
	};
};


static inline s16_extent sol_range_control_distribution_interior_extent(struct sol_range_control_distribution distribution, s16_extent exterior_extent)
{
	int16_t size;
	int64_t total_size_int64;
	float total_size_float32;
	s16_extent result_extent;

	size = s16_extent_size(exterior_extent);
	if(size < 0)
	{
		return exterior_extent;
	}

	switch(distribution.type)
	{
	case SOL_VARIABLE_BAR_DISTRIBUTION_UINT32:
		total_size_int64 = (int64_t)distribution.uint32.before + (int64_t)distribution.uint32.inner + (int64_t)distribution.uint32.after;
		result_extent.start = exterior_extent.start + (int16_t) (( ((int64_t)distribution.uint32.before) * (int64_t)size) / total_size_int64);
		result_extent.end   = exterior_extent.start + (int16_t) (( ((int64_t)distribution.uint32.before + (int64_t)distribution.uint32.inner) * (int64_t)size) / total_size_int64);
		break;
	case SOL_VARIABLE_BAR_DISTRIBUTION_FLOAT32:
		total_size_float32 = (distribution.float32.before + distribution.float32.inner + distribution.float32.after);
		result_extent.start = exterior_extent.start + (int16_t) ( (distribution.float32.before) * (float)size / total_size_float32);
		result_extent.end =   exterior_extent.start + (int16_t) ( (distribution.float32.before + distribution.float32.inner) * (float)size / total_size_float32);
		break;
	}

	return result_extent;
}

static inline s16_extent sol_range_control_distribution_interior_extent_with_minimum_size(struct sol_range_control_distribution distribution, s16_extent exterior_extent, int16_t minimum_interior)
{
	int16_t size, half_interior;
	s16_extent result_extent;

	size = s16_extent_size(exterior_extent);
	if(size < minimum_interior)
	{
		return exterior_extent;
	}

	result_extent = sol_range_control_distribution_interior_extent(distribution, exterior_extent);

	/** interior size */
	size = s16_extent_size(result_extent);

	if(size < minimum_interior)
	{
		switch(distribution.type)
		{
		case SOL_VARIABLE_BAR_DISTRIBUTION_UINT32:
			distribution.uint32.inner = 0;
			break;
		case SOL_VARIABLE_BAR_DISTRIBUTION_FLOAT32:
			distribution.float32.inner = 0.0f;
			break;
		}

		half_interior = minimum_interior >> 1;

		exterior_extent.start += half_interior;
		exterior_extent.end += half_interior - minimum_interior;

		result_extent = sol_range_control_distribution_interior_extent(distribution, exterior_extent);

		result_extent.start -= half_interior;
		result_extent.end -= half_interior - minimum_interior;
	}

	return result_extent;
}

