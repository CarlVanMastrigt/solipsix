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

#include "solipsix.h"


static void textbox_check_visible_offset(widget * w,overlay_theme * theme)
{
    uint32_t i;
    for(i=0;i<w->textbox.text_block.line_count && w->textbox.selection_end > w->textbox.text_block.lines[i].finish;i++);

    if(w->textbox.y_offset > ((int16_t)i)*theme->font.line_spacing) w->textbox.y_offset = i*theme->font.line_spacing;
    if(w->textbox.y_offset < ((int16_t)i)*theme->font.line_spacing+theme->font.glyph_size-w->textbox.visible_size) w->textbox.y_offset = i*theme->font.line_spacing+theme->font.glyph_size-w->textbox.visible_size;
}

static void textbox_copy_selection_to_clipboard(widget * w)
{
    char *s_begin,*s_end;
    char tmp;

    if(w->textbox.selection_end > w->textbox.selection_begin) s_begin=w->textbox.selection_begin, s_end=w->textbox.selection_end;
    else s_begin=w->textbox.selection_end, s_end=w->textbox.selection_begin;

    if(s_begin!=s_end)
    {
        tmp=*s_end;
        *s_end='\0';

        SDL_SetClipboardText(s_begin);

        *s_end=tmp;
    }
}






static bool textbox_widget_scroll(overlay_theme * theme,widget * w,int delta)
{
    w->textbox.y_offset+=delta*w->textbox.wheel_delta;

    if(w->textbox.y_offset > w->textbox.max_offset) w->textbox.y_offset=w->textbox.max_offset;
    if(w->textbox.y_offset < 0) w->textbox.y_offset=0;

    return true;
}

static void textbox_widget_left_click(overlay_theme * theme, widget * w, int x, int y, bool double_clicked)
{
    adjust_coordinates_to_widget_local(w,&x,&y);
    w->textbox.selection_end=w->textbox.selection_begin=overlay_text_multiline_find_offset(&theme->font,&w->textbox.text_block,
        x-theme->x_box_offset-w->textbox.x_offset,y-theme->y_box_offset+w->textbox.y_offset);
}

static bool textbox_widget_left_release(overlay_theme * theme,widget * clicked,widget * released,int x,int y)
{
    return true;
}

#warning possibly have popup copy/paste for here and enterbox
/*static void textbox_widget_right_click(overlay_theme * theme,widget * w,int x,int y)
{
}*/

static void textbox_widget_mouse_movement(overlay_theme * theme,widget * w,int x,int y)
{
    adjust_coordinates_to_widget_local(w,&x,&y);
    w->textbox.selection_end=overlay_text_multiline_find_offset(&theme->font,&w->textbox.text_block,
        x-theme->x_box_offset-w->textbox.x_offset,y-theme->y_box_offset+w->textbox.y_offset);

    textbox_check_visible_offset(w,theme);
}

static bool textbox_widget_key_down(overlay_theme * theme,widget * w,SDL_Keycode keycode,SDL_Keymod mod)
{
    widget* root_widget;
    switch(keycode)
    {
    case SDLK_C:
        if(mod&SDL_KMOD_CTRL)
        {
            textbox_copy_selection_to_clipboard(w);
        }
        break;

    case SDLK_A:
        if(mod&SDL_KMOD_CTRL)
        {
            w->textbox.selection_begin=w->textbox.text;
            w->textbox.selection_end=w->textbox.text+strlen(w->textbox.text);
        }
        break;

    case SDLK_ESCAPE:
        root_widget = find_root_widget(w);
        assert(root_widget);
        set_currently_active_widget(w->base.context, NULL);
        break;

        default:;
    }

    textbox_check_visible_offset(w,theme);

    return true;
}

static void textbox_widget_click_away(overlay_theme * theme,widget * w)
{
    w->textbox.selection_begin=w->textbox.selection_end;///cancel selection but dont affect visibility if possible
}

static void textbox_widget_delete(widget * w)
{
    free(w->textbox.text);
    free(w->textbox.text_block.lines);
}

static widget_behaviour_function_set textbox_behaviour_functions=
{
    .l_click        =   textbox_widget_left_click,
    .l_release      =   textbox_widget_left_release,
    .r_click        =   blank_widget_right_click,
    .m_move         =   textbox_widget_mouse_movement,
    .scroll         =   textbox_widget_scroll,
    .key_down       =   textbox_widget_key_down,
    .text_input     =   blank_widget_text_input,
    .text_edit      =   blank_widget_text_edit,
    .click_away     =   textbox_widget_click_away,
    .add_child      =   blank_widget_add_child,
    .remove_child   =   blank_widget_remove_child,
    .wid_delete     =   textbox_widget_delete
};









static void textbox_widget_render(overlay_theme * theme,widget * w,int16_t x_off,int16_t y_off,struct cvm_overlay_render_batch * restrict render_batch,rectangle bounds)
{
    char *sb,*se;

    rectangle r=rectangle_add_offset(w->base.r,x_off,y_off);
	theme->box_render(render_batch,theme,bounds,r,w->base.status,OVERLAY_MAIN_COLOUR);

	r.x1+=theme->x_box_offset;
	r.x2-=theme->x_box_offset;
	r.y1+=theme->y_box_offset;
	r.y2-=theme->y_box_offset;

    rectangle b=get_rectangle_overlap(r,bounds);
    if(rectangle_has_positive_area(b))
    {
        if(w->textbox.selection_begin==w->textbox.selection_end)overlay_text_multiline_render(render_batch,&theme->font,b,&w->textbox.text_block,r.x1,r.y1-w->textbox.y_offset,OVERLAY_TEXT_COLOUR_0);
        else
        {
            if(w->textbox.selection_end > w->textbox.selection_begin) sb=w->textbox.selection_begin, se=w->textbox.selection_end;
            else sb=w->textbox.selection_end, se=w->textbox.selection_begin;

            overlay_text_multiline_selection_render(render_batch,&theme->font,b,&w->textbox.text_block,r.x1,r.y1-w->textbox.y_offset,OVERLAY_TEXT_COLOUR_0,sb,se);
        }
    }
}

static widget * textbox_widget_select(overlay_theme * theme,widget * w,int16_t x_in,int16_t y_in)
{
    if(theme->box_select(theme,rectangle_subtract_offset(w->base.r,x_in,y_in),w->base.status))return w;
    return NULL;
}

static void textbox_widget_min_w(overlay_theme * theme,widget * w)
{
    w->base.min_w=w->textbox.min_horizontal_glyphs * theme->font.max_advance + 2*theme->x_box_offset;
}

static void textbox_widget_min_h(overlay_theme * theme,widget * w)
{
    overlay_text_multiline_processing(&theme->font,&w->textbox.text_block,w->textbox.text,w->base.r.x2-w->base.r.x1-2*theme->x_box_offset);

    if(w->textbox.min_visible_lines==0)
    {
        w->base.min_h=(w->textbox.text_block.line_count-1)*theme->font.line_spacing+theme->font.glyph_size+theme->y_box_offset*2;
    }
    else
    {
        w->base.min_h=(w->textbox.min_visible_lines-1)*theme->font.line_spacing+theme->font.glyph_size+theme->y_box_offset*2;
    }
}



static void textbox_widget_set_h(overlay_theme * theme,widget * w)
{
    w->textbox.wheel_delta= -theme->font.line_spacing;///w->textbox.font_index
    w->textbox.visible_size= w->base.r.y2-w->base.r.y1-2*theme->y_box_offset;

    if(w->textbox.min_visible_lines)
    {
        w->textbox.max_offset=(w->textbox.text_block.line_count-1)*theme->font.line_spacing+theme->font.glyph_size - w->textbox.visible_size;
        if(w->textbox.max_offset < 0)w->textbox.max_offset=0;
        if(w->textbox.y_offset > w->textbox.max_offset)w->textbox.y_offset = w->textbox.max_offset;
    }
}


static widget_appearence_function_set textbox_appearence_functions=
{
    .render =   textbox_widget_render,
    .select =   textbox_widget_select,
    .min_w  =   textbox_widget_min_w,
    .min_h  =   textbox_widget_min_h,
    .set_w  =   blank_widget_set_w,
    .set_h  =   textbox_widget_set_h
};






void change_textbox_text(widget * w,char * new_text,bool owns_new_text)///use responsibly, potentially need to reorganise widgets function as part of (or after) calling this
{
    free(w->textbox.text);

    if(owns_new_text) w->textbox.text=new_text;
    else w->textbox.text=cvm_strdup(new_text);

    w->textbox.y_offset=0;
    w->textbox.text_block.line_count=0;
    w->textbox.selection_begin=w->textbox.text;
    w->textbox.selection_end=w->textbox.text;

    if(w->textbox.min_visible_lines)
    {
        textbox_widget_min_h(w->base.context->theme, w);
        textbox_widget_set_h(w->base.context->theme, w);
    }
    else
    {
        organise_toplevel_widget(w);
    }
}

widget * create_textbox(struct widget_context* context, char * text,bool owns_text,int min_horizontal_glyphs,int min_visible_lines)///enable wrapping as input param? forces sizing and allows horizontal scrolling otherwise
{
    widget * w = create_widget(context, sizeof(widget_textbox));

    w->base.appearence_functions = &textbox_appearence_functions;
    w->base.behaviour_functions = &textbox_behaviour_functions;

    if(owns_text) w->textbox.text=text;
    else w->textbox.text=cvm_strdup(text);

    w->textbox.text_block.lines=malloc(sizeof(cvm_overlay_text_line)*16);
    w->textbox.text_block.line_space=16;
    w->textbox.text_block.line_count=0;

    w->textbox.min_visible_lines = min_visible_lines;///0=show all

    w->textbox.selection_begin = w->textbox.selection_end = w->textbox.text;

    w->textbox.min_horizontal_glyphs=min_horizontal_glyphs;

    w->textbox.x_offset=0;
    w->textbox.y_offset=0;
    w->textbox.max_offset=0;
    w->textbox.visible_size=0;
    w->textbox.wheel_delta=0;

    return w;
}


static const int32_t scroll_bar_zero=0;

widget * create_textbox_scrollbar(struct widget_context* context, widget* textbox)
{
    widget * w = create_slider_bar_dynamic(context, &textbox->textbox.y_offset, &scroll_bar_zero, &textbox->textbox.max_offset, &textbox->textbox.visible_size, &textbox->textbox.wheel_delta, NULL, NULL, false);
    #warning replace above with adjacent slider

    return w;
}

