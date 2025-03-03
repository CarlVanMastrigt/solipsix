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

enum sol_overlay_colour
{
    SOL_OVERLAY_COLOUR_ERROR=0,
    SOL_OVERLAY_COLOUR_DEFAULT=0,// use default for whatever is being rendered, as it MUST be replaced will render as error colour
    SOL_OVERLAY_COLOUR_BACKGROUND,
    SOL_OVERLAY_COLOUR_MAIN,
    SOL_OVERLAY_COLOUR_HIGHLIGHTED,
    SOL_OVERLAY_COLOUR_FOCUSED,
    SOL_OVERLAY_COLOUR_STANDARD_TEXT,
    SOL_OVERLAY_COLOUR_COMPOSITION_TEXT,
    SOL_OVERLAY_COLOUR_COUNT,
};