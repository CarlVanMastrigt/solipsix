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

/** these are the enumerators used to describe various things to do with the GUI
 * incompatible options are enumerators (list of choices) compatible options are defines (bit-flags)*/

// layout of anything GUI related, e.g. contents of a box, text orientation, scrollable bar, the dynamic part of a grid (hopefully these are self explanitory)
enum sol_gui_layout
{
    SOL_GUI_LAYOUT_HORIZONTAL,
    SOL_GUI_LAYOUT_VERTICAL,
};

// how space in a container will be shared/distributed
enum sol_gui_space_distribution
{
	// dont use the remaining space, leave it empty at the start of the container
    SOL_GUI_SPACE_DISTRIBUTION_START,
    // dont use the remaining space, leave it empty at the end of the container
    SOL_GUI_SPACE_DISTRIBUTION_END,
	// first object in container gets all remaining space
    SOL_GUI_SPACE_DISTRIBUTION_FIRST,
    // last object in container gets all remaining space
    SOL_GUI_SPACE_DISTRIBUTION_LAST,
    // distribute remaining space evenly amonst all objects
    SOL_GUI_SPACE_DISTRIBUTION_EVEN,
    // distribute space in an attempt to make all objects in the container the same size, starting with the smallest first
    SOL_GUI_SPACE_DISTRIBUTION_SAME,
};

// used for things like text, specifically avoiding left/right because alignment may be rlative to horizontal OR vertical directions
enum sol_gui_alignment
{
    SOL_GUI_ALIGNMENT_START,
    SOL_GUI_ALIGNMENT_END,
    SOL_GUI_ALIGNMENT_CENTRE,
};
// also difficuly because we could need horizontal AND vertical alignment

// SOL_GUI_REFERENCE_BIT_COUNT + SOL_GUI_OBJECT_STATUS_BIT_COUNT + SOL_GUI_OBJECT_PROPERTY_BIT_COUNT <= 32
#define SOL_GUI_REFERENCE_BIT_COUNT 8


#define SOL_GUI_OBJECT_FLAGS_BIT_COUNT 24
#define SOL_GUI_OBJECT_STATUS_FLAG_REGISTERED      0x00000001 /** used for validation, ensures the object base is only registered and constructed once */
#define SOL_GUI_OBJECT_STATUS_FLAG_ENABLED         0x00000002 /* inactive objects are not visible or selectable and take up no space, used to quickly "remove" objects without having substantively alter "tree" */
#define SOL_GUI_OBJECT_STATUS_FLAG_IS_ROOT         0x00000004 /** used for validation in various places*/
#define SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED         0x00000008
#define SOL_GUI_OBJECT_STATUS_FLAG_HIGHLIGHTED     0x00000010
/// these placement used to communicate which edge of the screen (if any) a gui object is touching
#define SOL_GUI_OBJECT_POSITION_FLAG_FIRST_X       0x00000100
#define SOL_GUI_OBJECT_POSITION_FLAG_LAST_X        0x00000200
#define SOL_GUI_OBJECT_POSITION_FLAG_FIRST_Y       0x00000400
#define SOL_GUI_OBJECT_POSITION_FLAG_LAST_Y        0x00000800
// properties should be immutable after being set at widget creation
#define SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE 0x00800000
#define SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE     0x00400000
#define SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED      0x00200000 /** the object should have a border (size defined by the theme) applied to it */



enum sol_gui_colour
{
    SOL_GUI_COLOUR_ERROR=0,
    SOL_GUI_COLOUR_DEFAULT=0,// use default for whatever is being rendered, as it MUST be replaced will render as error colour
    SOL_GUI_COLOUR_BACKGROUND,
    SOL_GUI_COLOUR_MAIN,
    SOL_GUI_COLOUR_HIGHLIGHTED,
    SOL_GUI_COLOUR_FOCUSED,
    SOL_GUI_COLOUR_STANDARD_TEXT,
    SOL_GUI_COLOUR_COMPOSITION_TEXT,
    SOL_GUI_COLOUR_COUNT,
};