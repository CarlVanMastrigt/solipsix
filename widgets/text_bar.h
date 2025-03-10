/**
Copyright 2020,2021,2022,2023 Carl van Mastrigt

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

#ifndef WIDGET_TEXT_BAR_H
#define WIDGET_TEXT_BAR_H


typedef struct widget_text_bar
{
    widget_base base;

    struct widget_function_data_pair update_text_op;

    size_t text_space;// for use with dynamic text bars
	char* text;

	char* selection_begin;
    char* selection_end;

	uint32_t free_text:1;
	uint32_t allow_selection:1;
	uint32_t recalculate_text_size:1;

	int32_t visible_offset;///usually based on text_alignment, alternatively can be based on selection
	int32_t max_visible_offset;///usually based on text_alignment, alternatively can be based on selection
    int32_t text_pixel_length;

    // used for sizing, uses longest advance glyph to determine minimum bar size
	int16_t min_visible_glyph_count;
	///uint32_t max_strlen;needed if handling text internally (compositing internally, not sure this is desirable functionality though)
	widget_text_alignment text_alignment;///when selection not active?
}
widget_text_bar;

/// min_glyph_render_count==0  ->  render all text (doesnt really work with dynamic text though...
widget * create_static_text_bar(struct widget_context* context, const char * text);
widget * create_dynamic_text_bar(struct widget_context* context, int16_t min_visible_glyph_count, widget_text_alignment text_alignment,bool allow_selection);

void text_bar_widget_set_text_pointer(widget * w,char * text_pointer);///also marks as having been updated
void text_bar_widget_set_text(widget * w,const char * text_to_copy);


#endif

