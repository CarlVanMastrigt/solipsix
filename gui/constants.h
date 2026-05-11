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



// how space in a container will be shared/distributed
enum sol_gui_distribution
{
	// dont use the remaining space, leave it empty at the start of the container, without uniform sizing is the same as first with an empty widget added to the start, added for conveneince
    SOL_GUI_SPACE_DISTRIBUTION_START,
    // dont use the remaining space, leave it empty at the end of the container, without uniform sizing is the same as last with an empty widget added to the end, added for conveneince
    SOL_GUI_SPACE_DISTRIBUTION_END,
	// first object in container gets all remaining space
    SOL_GUI_SPACE_DISTRIBUTION_FIRST,
    // last object in container gets all remaining space
    SOL_GUI_SPACE_DISTRIBUTION_LAST,
    // distribute remaining space evenly amongst all objects and ensure all contained objects are (as close to as possible) the same size
    SOL_GUI_SPACE_DISTRIBUTION_UNIFORM,
};

#warning implement these (or similar) in add_child associated functions
enum sol_gui_placement
{
    SOL_GUI_PLACEMENT_BEFORE,
    SOL_GUI_PLACEMENT_AFTER,
    SOL_GUI_PLACEMENT_START,
    SOL_GUI_PLACEMENT_END
};

enum sol_gui_relative_placement
{
    SOL_GUI_RELATIVE_PLACEMENT_BEFORE,/** end of object aligned with start of reference */
    SOL_GUI_RELATIVE_PLACEMENT_START_SIDE,/** start of both line up */
    SOL_GUI_RELATIVE_PLACEMENT_CENTRED,/** centrelines match */
    SOL_GUI_RELATIVE_PLACEMENT_END_SIDE,/** end of both line up */
    SOL_GUI_RELATIVE_PLACEMENT_AFTER,/** start of object aligned with end of reference */
};

// SOL_GUI_REFERENCE_BIT_COUNT + SOL_GUI_OBJECT_STATUS_BIT_COUNT + SOL_GUI_OBJECT_PROPERTY_BIT_COUNT <= 32
#define SOL_GUI_REFERENCE_BIT_COUNT 8


#define SOL_GUI_OBJECT_FLAGS_BIT_COUNT 24
#define SOL_GUI_OBJECT_STATUS_FLAG_UNREFERENCED    0x000001 /** used for freshly constructed surfaces, this is the state objects are created in before being retained by anything else (e.g. their parent container) */
#define SOL_GUI_OBJECT_STATUS_FLAG_ENABLED         0x000002 /* inactive objects are not visible or selectable and take up no space, used to quickly "remove" objects without having substantively alter "tree" */
#define SOL_GUI_OBJECT_STATUS_FLAG_IS_ROOT         0x000004 /** mostly used for validation in various places*/
#define SOL_GUI_OBJECT_STATUS_FLAG_FOCUSED         0x000008
#define SOL_GUI_OBJECT_STATUS_FLAG_HIGHLIGHTED     0x000010
/** note: 0x20 - 0x80 inclusive available */

/* these placement flags used to communicate which edge of the screen (if any) a gui object is touching */
#define SOL_GUI_OBJECT_POSITION_FLAG_FIRST_X       0x000100
#define SOL_GUI_OBJECT_POSITION_FLAG_LAST_X        0x000200
#define SOL_GUI_OBJECT_POSITION_FLAG_FIRST_Y       0x000400
#define SOL_GUI_OBJECT_POSITION_FLAG_LAST_Y        0x000800
#define SOL_GUI_OBJECT_POSITION_FLAGS_ALL          0x000F00

/* property flags should be immutable after being set at widget creation */
#define SOL_GUI_OBJECT_PROPERTY_FLAG_TEXT_CONTENT  0x001000 /** the object contains text, which may affect theme sizing and styling */
#define SOL_GUI_OBJECT_PROPERTY_FLAG_BORDERED      0x002000 /** the object should have a border (size defined by the theme) applied to it */
#define SOL_GUI_OBJECT_PROPERTY_FLAG_FOCUSABLE     0x004000
#define SOL_GUI_OBJECT_PROPERTY_FLAG_HIGHLIGHTABLE 0x008000
#define SOL_GUI_OBJECT_PROPERTY_FLAG_CONTRACT_X    0x010000 /** the object should have the minimum size applicable in the x dimension */
#define SOL_GUI_OBJECT_PROPERTY_FLAG_CONTRACT_Y    0x020000 /** the object should have the minimum size applicable in the x dimension */