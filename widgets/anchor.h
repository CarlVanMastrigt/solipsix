/**
Copyright 2020,2021,2022 Carl van Mastrigt

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

#ifndef solipsix_H
#include "solipsix.h"
#endif

#ifndef WIDGET_ANCHOR_H
#define WIDGET_ANCHOR_H


typedef struct widget_anchor
{
    widget_base base;

    widget * constraint;

    char * text;

    int x_clicked;
    int y_clicked;
}
widget_anchor;

void widget_text_anchor_initialise(widget_anchor* anchor, struct widget_context* context, widget* constraint, char* title);

widget * create_anchor(struct widget_context* context, widget* constraint, char* title);

#endif


