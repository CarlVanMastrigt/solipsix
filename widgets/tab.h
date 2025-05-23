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

#ifndef WIDGET_TAB_H
#define WIDGET_TAB_H

typedef struct widget_tab_folder
{
    widget_base base;

    widget * tab_button_container;
    widget * current_tab_page;
    widget * last;
    ///last needed so that if no contained widget is active (ergo no current_tab_page) all search functions can still operate (only link to children is NOT NULL)
    ///use last to allow quick/simple adding of widgets
}
widget_tab_folder;



widget * create_tab_folder(struct widget_context* context, widget ** button_box,widget_layout button_box_layout);
widget * create_tab_page(struct widget_context* context, widget * folder,char * title,widget * page_widget);

widget * create_vertical_tab_pair_box(struct widget_context* context, widget ** tab_folder);


#endif





