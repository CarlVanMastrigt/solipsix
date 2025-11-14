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

#include "solipsix.h"

///must be called any time length or offset is relevant in a function
static inline void text_bar_recalculate_text_size_and_offset(overlay_theme * theme,widget * w)
{
    if(w->text_bar.recalculate_text_size)
    {
        w->text_bar.text_pixel_length=overlay_text_single_line_get_pixel_length(&theme->font,w->text_bar.text);
        w->text_bar.max_visible_offset=w->text_bar.text_pixel_length-(w->base.r.x2-w->base.r.x1 - theme->h_bar_text_offset*2 -1);
        ///above: text length - visible text length (width of space where text can be displayed); -1 for space for caret
        if(w->text_bar.max_visible_offset < 0)w->text_bar.max_visible_offset=0;

        if(w->text_bar.text_alignment==WIDGET_TEXT_LEFT_ALIGNED)w->text_bar.visible_offset=0;
        else
        {
            assert(w->text_bar.text_alignment==WIDGET_TEXT_RIGHT_ALIGNED);
            w->text_bar.visible_offset=w->text_bar.max_visible_offset;
        }

        w->text_bar.recalculate_text_size=false;
    }
}


///perhaps call this upon resize of still active widget?
static void text_bar_check_visible_offset(overlay_theme * theme,widget * w)
{
    char tmp;
    int32_t current_offset,text_space,max_offset;

    if(!w->text_bar.text) return;

    text_space=w->text_bar.text_pixel_length-w->text_bar.max_visible_offset;

    tmp=*w->text_bar.selection_end;
    *w->text_bar.selection_end='\0';

    current_offset=overlay_text_single_line_get_pixel_length(&theme->font,w->text_bar.text);

    *w->text_bar.selection_end=tmp;

    if(w->text_bar.visible_offset>current_offset-theme->h_text_fade_range) w->text_bar.visible_offset=current_offset-theme->h_text_fade_range;
    if(w->text_bar.visible_offset<current_offset-text_space+theme->h_text_fade_range) w->text_bar.visible_offset=current_offset-text_space+theme->h_text_fade_range;

    ///special condition to put offset at middle???
    if(w->text_bar.visible_offset<0)w->text_bar.visible_offset=0;
    else if(w->text_bar.visible_offset > w->text_bar.max_visible_offset) w->text_bar.visible_offset=w->text_bar.max_visible_offset;
}


static void text_bar_copy_selection_to_clipboard(widget * w)
{
    char *s_begin,*s_end;
    char tmp;

    if(!w->text_bar.text) return;

    if(w->text_bar.selection_end > w->text_bar.selection_begin) s_begin=w->text_bar.selection_begin, s_end=w->text_bar.selection_end;
    else s_begin=w->text_bar.selection_end, s_end=w->text_bar.selection_begin;

    /// copy all contents if nothing selected? if so uncomment next section
    if(s_begin!=s_end)
    {
        tmp=*s_end;
        *s_end='\0';

        #warning possibly want abstraction over this, function in properties of event?
        SDL_SetClipboardText(s_begin);

        *s_end=tmp;
    }
//    else if(*w->text_bar.text)///not empty
//    {
//        SDL_SetClipboardText(w->text_bar.text);
//    }
}





static void text_bar_widget_left_click(overlay_theme * theme, widget * w, int x, int y, bool double_clicked)
{
    #warning handle double click (select all if selectable)
    text_bar_recalculate_text_size_and_offset(theme,w);

    adjust_coordinates_to_widget_local(w,&x,&y);
    w->text_bar.selection_begin=w->text_bar.selection_end=overlay_text_single_line_find_offset(&theme->font,w->text_bar.text,x-theme->h_bar_text_offset+w->text_bar.visible_offset);

    text_bar_check_visible_offset(theme,w);
}

static bool text_bar_widget_left_release(overlay_theme * theme,widget * clicked,widget * released,int x,int y)
{
    return true;
}

static void text_bar_widget_mouse_movement(overlay_theme * theme,widget * w,int x,int y)
{
    text_bar_recalculate_text_size_and_offset(theme,w);

    adjust_coordinates_to_widget_local(w,&x,&y);
    w->text_bar.selection_end=overlay_text_single_line_find_offset(&theme->font,w->text_bar.text,x-theme->h_bar_text_offset+w->text_bar.visible_offset);

    text_bar_check_visible_offset(theme,w);
}

static bool text_bar_widget_scroll(overlay_theme * theme,widget * w,int delta)
{
    text_bar_recalculate_text_size_and_offset(theme,w);

    w->text_bar.visible_offset-=delta*theme->font.max_advance;

    if(w->text_bar.visible_offset < 0)w->text_bar.visible_offset=0;
    else if(w->text_bar.visible_offset > w->text_bar.max_visible_offset) w->text_bar.visible_offset=w->text_bar.max_visible_offset;

    return true;
}

static bool text_bar_widget_key_down(overlay_theme * theme,widget * w,SDL_Keycode keycode,SDL_Keymod mod)
{
    char *s_begin,*s_end,*s;

    if(!w->text_bar.text) return true;

    text_bar_recalculate_text_size_and_offset(theme,w);

    if(w->text_bar.selection_end > w->text_bar.selection_begin) s_begin=w->text_bar.selection_begin, s_end=w->text_bar.selection_end;
    else s_begin=w->text_bar.selection_end, s_end=w->text_bar.selection_begin;

    switch(keycode)
    {
    case SDLK_C:
        if(mod&SDL_KMOD_CTRL)
        {
            text_bar_copy_selection_to_clipboard(w);
        }
        break;

    case SDLK_X:
        if(mod&SDL_KMOD_CTRL)
        {
            text_bar_copy_selection_to_clipboard(w);///considering text is immutable just copy
        }
        break;

    case SDLK_A:
        if(mod&SDL_KMOD_CTRL)
        {
            w->text_bar.selection_begin=w->text_bar.text;
            w->text_bar.selection_end=w->text_bar.text+strlen(w->text_bar.text);
        }
        break;

    case SDLK_KP_4:/// keypad/numpad left
        if(mod&SDL_KMOD_NUM)break;
    case SDLK_LEFT:
        if(mod&SDL_KMOD_CTRL)///can move based on ctrl (jump word),  then set to same based on shift
        {
            if(mod&SDL_KMOD_SHIFT) w->text_bar.selection_end = cvm_overlay_utf8_get_previous_word(w->text_bar.text,w->text_bar.selection_end);
            else w->text_bar.selection_begin = w->text_bar.selection_end = cvm_overlay_utf8_get_previous_word(w->text_bar.text,w->text_bar.selection_end);
        }
        else if(mod&SDL_KMOD_SHIFT)w->text_bar.selection_end = cvm_overlay_utf8_get_previous_glyph(w->text_bar.text,w->text_bar.selection_end);
        else if(s_begin==s_end) w->text_bar.selection_begin = w->text_bar.selection_end = cvm_overlay_utf8_get_previous_glyph(w->text_bar.text,s_begin);
        else w->text_bar.selection_begin = w->text_bar.selection_end = s_begin;
        break;

    case SDLK_KP_6:/// keypad/numpad right
        if(mod&SDL_KMOD_NUM)break;
    case SDLK_RIGHT:
        if(mod&SDL_KMOD_CTRL)
        {
            if(mod&SDL_KMOD_SHIFT) w->text_bar.selection_end = cvm_overlay_utf8_get_next_word(w->text_bar.selection_end);
            else w->text_bar.selection_begin = w->text_bar.selection_end = cvm_overlay_utf8_get_next_word(w->text_bar.selection_end);
        }
        else if(mod&SDL_KMOD_SHIFT) w->text_bar.selection_end = cvm_overlay_utf8_get_next_glyph(w->text_bar.selection_end);
        else if(s_begin==s_end) w->text_bar.selection_begin = w->text_bar.selection_end = cvm_overlay_utf8_get_next_glyph(s_end);
        else w->text_bar.selection_begin = w->text_bar.selection_end = s_end;
        break;

    case SDLK_KP_7:/// keypad/numpad home
    case SDLK_KP_8:/// keypad/numpad up
    case SDLK_KP_9:/// keypad/numpad page up
         if(mod&SDL_KMOD_NUM)break;
    case SDLK_UP:
    case SDLK_PAGEUP:
    case SDLK_HOME:
        if(mod&SDL_KMOD_SHIFT) w->text_bar.selection_end = w->text_bar.text;
        else w->text_bar.selection_begin = w->text_bar.selection_end = w->text_bar.text;
        break;

    case SDLK_KP_1:/// keypad/numpad end
    case SDLK_KP_2:/// keypad/numpad down
    case SDLK_KP_3:/// keypad/numpad page down
         if(mod&SDL_KMOD_NUM)break;
    case SDLK_DOWN:
    case SDLK_PAGEDOWN:
    case SDLK_END:
        if(mod&SDL_KMOD_SHIFT) w->text_bar.selection_end = w->text_bar.text+strlen(w->text_bar.text);
        else w->text_bar.selection_begin = w->text_bar.selection_end = w->text_bar.text+strlen(w->text_bar.text);
        break;

    case SDLK_ESCAPE:
        set_currently_active_widget(w->base.context, NULL);
        break;

        default:;
    }

    text_bar_check_visible_offset(theme,w);

    return true;
}

static void text_bar_widget_click_away(overlay_theme * theme,widget * w)
{
    ///reset offset
    if(w->text_bar.text_alignment==WIDGET_TEXT_LEFT_ALIGNED)w->text_bar.visible_offset=0;
    else
    {
        assert(w->text_bar.text_alignment==WIDGET_TEXT_RIGHT_ALIGNED);
        w->text_bar.visible_offset=w->text_bar.max_visible_offset;///text length - visible text length; -1 for space for caret
    }
}

static void text_bar_widget_delete(widget * w)
{
    if(w->text_bar.free_text) free(w->text_bar.text);
}



static widget_behaviour_function_set text_bar_behaviour_functions=
{
    .l_click        =   text_bar_widget_left_click,
    .l_release      =   text_bar_widget_left_release,
    .r_click        =   blank_widget_right_click,
    .m_move         =   text_bar_widget_mouse_movement,
    .scroll         =   text_bar_widget_scroll,
    .key_down       =   text_bar_widget_key_down,
    .text_input     =   blank_widget_text_input,
    .text_edit      =   blank_widget_text_edit,
    .click_away     =   text_bar_widget_click_away,
    .add_child      =   blank_widget_add_child,
    .remove_child   =   blank_widget_remove_child,
    .wid_delete     =   text_bar_widget_delete
};





static void text_bar_widget_render(overlay_theme * theme,widget * w,int16_t x_off,int16_t y_off,struct cvm_overlay_render_batch * restrict render_batch,rectangle bounds)
{
    rectangle r=rectangle_add_offset(w->base.r,x_off,y_off);

    text_bar_recalculate_text_size_and_offset(theme,w);

	theme->h_bar_render(render_batch,theme,bounds,r,w->base.status,OVERLAY_MAIN_COLOUR);

    overlay_text_single_line_render_data text_render_data=
	{
	    .text=w->text_bar.text,
	    .bounds=bounds,
	    .colour=OVERLAY_TEXT_COLOUR_0,
        .flags=0,
	    .x=r.x1+theme->h_bar_text_offset,
	    .y=(r.y1+r.y2-theme->font.glyph_size)>>1,
	    .text_length=w->text_bar.text_pixel_length,
	    .text_area=overlay_text_single_line_get_text_area(r,theme->font.glyph_size,theme->h_bar_text_offset),
	};


    text_render_data.x-=w->text_bar.visible_offset;

    if(w->text_bar.min_visible_glyph_count)
    {
        text_render_data.flags |= OVERLAY_TEXT_RENDER_FADING;
    }
    if(is_currently_active_widget(w))
    {
        text_render_data.flags |= OVERLAY_TEXT_RENDER_SELECTION;

        if(w->text_bar.selection_end > w->text_bar.selection_begin)
        {
            text_render_data.selection_begin = w->text_bar.selection_begin;
            text_render_data.selection_end   = w->text_bar.selection_end;
        }
        else
        {
            text_render_data.selection_begin = w->text_bar.selection_end;
            text_render_data.selection_end   = w->text_bar.selection_begin;
        }
    }

    overlay_text_single_line_render(render_batch,theme,&text_render_data);
}

static widget * text_bar_widget_select(overlay_theme * theme,widget * w,int16_t x_in,int16_t y_in)
{
    if(theme->h_bar_select(theme,rectangle_subtract_offset(w->base.r,x_in,y_in),w->base.status))return w;

    return NULL;
}

static void text_bar_widget_min_w(overlay_theme * theme,widget * w)
{
    int16_t max_w;
    /// should re-query text here

    if(w->text_bar.min_visible_glyph_count)
    {
        w->base.min_w = 2*theme->h_bar_text_offset + w->text_bar.min_visible_glyph_count * theme->font.max_advance + 1;///+1 for caret
        w->text_bar.recalculate_text_size=true;
    }
    else w->base.min_w = 2*theme->h_bar_text_offset + overlay_text_single_line_get_pixel_length(&theme->font,w->text_bar.text) + 1;///+1 for caret
}

static void text_bar_widget_min_h(overlay_theme * theme,widget * w)
{
    w->base.min_h = theme->base_unit_h;
}

static void text_bar_widget_set_w(overlay_theme * theme,widget * w)
{
    text_bar_check_visible_offset(theme,w);
}



static widget_appearence_function_set text_bar_appearence_functions=
{
    .render =   text_bar_widget_render,
    .select =   text_bar_widget_select,
    .min_w  =   text_bar_widget_min_w,
    .min_h  =   text_bar_widget_min_h,
    .set_w  =   text_bar_widget_set_w,
    .set_h  =   blank_widget_set_h
};





///selection being valid requires the ability to index into a buffer (selection pointers), ergo NEED to have some stable buffer, likely a local buffer (cannot be changed externally)




///static provably isnt best name, or want to add another variant for when text references string that is modified externally
#warning probably want a better mechanism to expose all the capabilities of the text bar
widget * create_static_text_bar(struct widget_context* context, const char * text)
{
	widget * w=create_widget(context, sizeof(widget_text_bar));

	w->base.appearence_functions=&text_bar_appearence_functions;
	w->base.behaviour_functions=&text_bar_behaviour_functions;

	///w->text_bar.set_text=NULL;
	///w->text_bar.data=NULL;

	w->text_bar.min_visible_glyph_count=0;

	w->text_bar.free_text=true;
	w->text_bar.allow_selection=false;
	w->text_bar.recalculate_text_size=false;

	w->text_bar.text_alignment=WIDGET_TEXT_LEFT_ALIGNED;

	if(text) w->text_bar.text=cvm_strdup(text);
    else w->text_bar.text=NULL;

    w->text_bar.visible_offset=0;
    w->text_bar.max_visible_offset=0;
    w->text_bar.text_pixel_length=0;

#warning use of selection_begin/selection_end elsewhere without validity checks seems quite dangerous/stupid, correct this, null selection should work
    w->text_bar.selection_begin=w->text_bar.text;
    w->text_bar.selection_end=w->text_bar.text;

	return w;
}

widget * create_dynamic_text_bar(struct widget_context* context, int16_t min_visible_glyph_count, widget_text_alignment text_alignment,bool allow_selection)
{
	widget * w=create_widget(context, sizeof(widget_text_bar));

	w->base.appearence_functions=&text_bar_appearence_functions;
	w->base.behaviour_functions=&text_bar_behaviour_functions;

	assert(min_visible_glyph_count);///ZERO GLYPH RENDER COUNT INVALID FOR DYNAMIC TEXT BARS
	w->text_bar.min_visible_glyph_count=min_visible_glyph_count;

	w->text_bar.free_text=false;
	w->text_bar.allow_selection=allow_selection;
	w->text_bar.recalculate_text_size=true;

	w->text_bar.text_alignment=text_alignment;

	w->text_bar.text=NULL;

	w->text_bar.visible_offset=0;
	w->text_bar.max_visible_offset=0;
    w->text_bar.text_pixel_length=0;

    w->text_bar.selection_begin=NULL;
    w->text_bar.selection_end=NULL;

	return w;
}


void text_bar_widget_set_text_pointer(widget * w,char * text_pointer)
{
    if(w->text_bar.free_text)free(w->text_bar.text);

    w->text_bar.text=text_pointer;

    w->text_bar.free_text=false;
    w->text_bar.visible_offset=0;
    w->text_bar.max_visible_offset=0;
    ///perhaps null isn't best, instead use just .text
    w->text_bar.selection_begin=w->text_bar.text;
    w->text_bar.selection_end=w->text_bar.text;
    w->text_bar.recalculate_text_size=true;
}

void text_bar_widget_set_text(widget * w,const char * text_to_copy)
{
    if(text_to_copy)
    {
        if(w->text_bar.free_text)
        {
            w->text_bar.text=realloc(w->text_bar.text,sizeof(char)*(strlen(text_to_copy)+1));
            strcpy(w->text_bar.text,text_to_copy);
        }
        else w->text_bar.text=cvm_strdup(text_to_copy);
    }
    else
    {
        if(w->text_bar.free_text)free(w->text_bar.text);
        w->text_bar.text=NULL;
    }

    w->text_bar.free_text=true;
    w->text_bar.visible_offset=0;
    w->text_bar.max_visible_offset=0;
    ///perhaps null isn't best, instead use just .text
    w->text_bar.selection_begin=w->text_bar.text;
    w->text_bar.selection_end=w->text_bar.text;
    w->text_bar.recalculate_text_size=true;
}

/**
programmable text bar is almost certainly undesirable, requires constant update, which i would prefer to avoid, instead changes should be externally managed and updates called
    ^ maybe there's no other way though?
        ^ to include current selection in bar, would need to link one widget to another, which is allowed, but is quite unsafe (text bar needs to look at file list widget)

for patching file search elements together; the file search manager should be doing the work (in a separate buffer presumably)



both here and in enterbox allow automatic scrolling by holding cursor beyond edges of screen, ideally NOT JUST UPON MOVEMENT EITHER (maybe not possible though)

implement textbar/enterbox scrolling?

*/
