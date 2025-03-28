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

#ifndef WIDGET_CONTAINER_H
#define WIDGET_CONTAINER_H


typedef struct widget_container
{
    widget_base base;

    widget * first;
    widget * last;
}
widget_container;


void widget_container_initialise(widget_container* container, struct widget_context* context);
widget * create_container(struct widget_context* context, size_t size);


void container_widget_render(overlay_theme * theme,widget * w,int16_t x_off,int16_t y_off,struct cvm_overlay_render_batch * restrict render_batch,rectangle bounds);
widget * container_widget_select(overlay_theme * theme,widget * w,int16_t x_in,int16_t y_in);


void container_widget_add_child(widget * w,widget * child);
void container_widget_remove_child(widget * w,widget * child);
void container_widget_delete(widget * w);


#endif



