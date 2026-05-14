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

#include <assert.h>

#include "solipsix/gui/theme.h"
#include "solipsix/gui/utilities.h"

s16_vec2 sol_gui_required_pacement_offset(s16_rect rect, const struct sol_gui_theme* theme, s16_rect reference_rect, enum sol_gui_relative_placement placement_x, enum sol_gui_relative_placement placement_y)
{
	s16_vec2 result = s16_vec2_set(0, 0);

	switch (placement_x) 
	{
		case SOL_GUI_RELATIVE_PLACEMENT_BEFORE:
			result.x = (reference_rect.x.start - theme->horizontal_placement_spacing) - rect.x.end;
			break;
		case SOL_GUI_RELATIVE_PLACEMENT_START_SIDE:
			result.x = reference_rect.x.start - rect.x.start;
			break;
		case SOL_GUI_RELATIVE_PLACEMENT_CENTRED:
			result.x = ((reference_rect.x.start + reference_rect.x.end) - (rect.x.start + rect.x.end)) / 2;
			break;
		case SOL_GUI_RELATIVE_PLACEMENT_END_SIDE:
			result.x = reference_rect.x.end - rect.x.end;
			break;
		case SOL_GUI_RELATIVE_PLACEMENT_AFTER:
			result.x = (reference_rect.x.end + theme->horizontal_placement_spacing) - rect.x.start;
			break;
		default:
			/** unhandled placement value */
			assert(false);
	}

	switch (placement_y) 
	{
		case SOL_GUI_RELATIVE_PLACEMENT_BEFORE:
			result.y = (reference_rect.y.start - theme->vertical_placement_spacing) - rect.y.end;
			break;
		case SOL_GUI_RELATIVE_PLACEMENT_START_SIDE:
			result.y = reference_rect.y.start - rect.y.start;
			break;
		case SOL_GUI_RELATIVE_PLACEMENT_CENTRED:
			result.y = ((reference_rect.y.start + reference_rect.y.end) - (rect.y.start + rect.y.end)) / 2;
			break;
		case SOL_GUI_RELATIVE_PLACEMENT_END_SIDE:
			result.y = reference_rect.y.end - rect.y.end;
			break;
		case SOL_GUI_RELATIVE_PLACEMENT_AFTER:
			result.y = (reference_rect.y.end + theme->vertical_placement_spacing) - rect.y.start;
			break;
		default:
			/** unhandled placement value */
			assert(false);
	}

	return result;
}