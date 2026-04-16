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

/** |--before--|===range===|--after--| */

enum sol_variable_bar_distribution_type
{
	SOL_VARIABLE_BAR_DISTRIBUTION_UINT32,
	SOL_VARIABLE_BAR_DISTRIBUTION_FLOAT32,
};

struct sol_variable_bar_distribution
{
	enum sol_variable_bar_distribution_type type;
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
