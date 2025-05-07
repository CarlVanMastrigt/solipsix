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

#include "gui/object.h"

struct sol_gui_panel;

struct sol_gui_panel* sol_gui_panel_create(struct sol_gui_context* context, bool clear_bordered, bool inset_content);
struct sol_gui_object* sol_gui_panel_object_create(struct sol_gui_context* context, bool clear_bordered, bool inset_content);

struct sol_gui_object* sol_gui_panel_as_object(struct sol_gui_panel* panel);




