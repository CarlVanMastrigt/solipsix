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

void blank_enterbox_function(widget * w)
{
    printf("blank enterbox: %s\n",w->enterbox.text);
}

static inline void enterbox_delete_selection(overlay_theme * theme,widget * w,char * s_begin,char * s_end)
{
    assert(s_begin<=s_end);///ATTEMPTING TO DELETE INVERTED SELECTION FROM ENTERBOX

    if(s_begin!=s_end)
    {
        memmove(s_begin,s_end,strlen(s_end) + 1);/// +1 for null terminating character
    }

    w->enterbox.text_pixel_length=overlay_text_single_line_get_pixel_length(&theme->font,w->enterbox.text);
}

static void enterbox_copy_selection_to_clipboard(widget * w)
{
    char *s_begin,*s_end;
    char tmp;

    if(w->enterbox.selection_end > w->enterbox.selection_begin) s_begin=w->enterbox.selection_begin, s_end=w->enterbox.selection_end;
    else s_begin=w->enterbox.selection_end, s_end=w->enterbox.selection_begin;


    /// copy all contents if nothing selected? if so uncomment next section, i personally dont like it
    if(s_begin!=s_end)
    {
        tmp=*s_end;
        *s_end='\0';

        SDL_SetClipboardText(s_begin);

        *s_end=tmp;
    }
//    else if(*w->enterbox.text)///not empty
//    {
//        SDL_SetClipboardText(w->enterbox.text);
//    }
}

/// perhaps return pass/fail such that enterbox can flash/fade a colour upon failure
static void enterbox_enter_text(overlay_theme * theme,widget * w,char * text)
{
    char *s_begin,*s_end;
    uint32_t new_glyph_count,new_strlen;

    if(w->enterbox.selection_end > w->enterbox.selection_begin) s_begin=w->enterbox.selection_begin, s_end=w->enterbox.selection_end;
    else s_begin=w->enterbox.selection_end, s_end=w->enterbox.selection_begin;

    if( text && (new_strlen=strlen(text)) > 0 && cvm_overlay_utf8_validate_string_and_count_glyphs(text,&new_glyph_count) &&
        ///new text exists, isnt empty and is valid unicode (also get glyph count of input text)
        cvm_overlay_utf8_count_glyphs_outside_range(w->enterbox.text,s_begin,s_end) + new_glyph_count <= w->enterbox.max_glyphs &&
        /// adding the new glyphs while replacing the old ones wont exceed limit
        strlen(w->enterbox.text) + s_begin - s_end + new_strlen <= w->enterbox.max_strlen)
        /// adding the new chars while replacing the old ones wont exceed limit
    {
        memmove(s_begin + new_strlen,s_end,strlen(s_end) + 1);/// +1 for null terminating character
        memcpy(s_begin,text,new_strlen);
        w->enterbox.selection_begin = w->enterbox.selection_end = s_begin + new_strlen;
    }

    w->enterbox.text_pixel_length=overlay_text_single_line_get_pixel_length(&theme->font,w->enterbox.text);
}

static void enterbox_check_visible_offset(overlay_theme * theme,widget * w)
{
    char tmp;
    int32_t current_offset,text_space,max_offset;

    text_space=w->base.r.x2-w->base.r.x1-2*theme->h_bar_text_offset-1;

    tmp=*w->enterbox.selection_end;
    *w->enterbox.selection_end='\0';

    current_offset=overlay_text_single_line_get_pixel_length(&theme->font,w->enterbox.text);

    *w->enterbox.selection_end=tmp;

//    if(w->enterbox.visible_offset>current_offset) w->enterbox.visible_offset=current_offset;
//    if(w->enterbox.visible_offset+text_space<current_offset) w->enterbox.visible_offset=current_offset-text_space;

    if(w->enterbox.visible_offset>current_offset-theme->h_text_fade_range) w->enterbox.visible_offset=current_offset-theme->h_text_fade_range;
    if(w->enterbox.visible_offset<current_offset-text_space+theme->h_text_fade_range) w->enterbox.visible_offset=current_offset-text_space+theme->h_text_fade_range;

    ///special condition to put offset at middle???
    if(w->enterbox.visible_offset<0)w->enterbox.visible_offset=0;
    max_offset=w->enterbox.text_pixel_length-text_space;///max offset
    if(max_offset < 0) w->enterbox.visible_offset=0;
    else if(w->enterbox.visible_offset > max_offset) w->enterbox.visible_offset=max_offset;
}



static void enterbox_widget_left_click(overlay_theme * theme, widget * w, int x, int y, bool double_clicked)
{
    #warning handle double click (select all)
    #warning find another way to communicate to start accepting text input

    // if(!SDL_IsTextInputActive()) SDL_StartTextInput();///need to only call if not already active...

    if(*w->enterbox.composition_text)return;

    adjust_coordinates_to_widget_local(w,&x,&y);
    w->enterbox.selection_begin=w->enterbox.selection_end=overlay_text_single_line_find_offset(&theme->font,w->enterbox.text,x-theme->h_bar_text_offset+w->enterbox.visible_offset);

    if(w->enterbox.upon_input_func)
    {
        w->enterbox.upon_input_func(w);
    }

    enterbox_check_visible_offset(theme,w);
}

static bool enterbox_widget_left_release(overlay_theme * theme,widget * clicked,widget * released,int x,int y)
{
    /// should also find test position to set start/end of selection with... (actually move does this, which makes more sense)
    return true;
}

static void enterbox_widget_mouse_movement(overlay_theme * theme,widget * w,int x,int y)
{
    if(*w->enterbox.composition_text)return;

    adjust_coordinates_to_widget_local(w,&x,&y);
    w->enterbox.selection_end=overlay_text_single_line_find_offset(&theme->font,w->enterbox.text,x-theme->h_bar_text_offset+w->enterbox.visible_offset);

    enterbox_check_visible_offset(theme,w);
}

static bool enterbox_widget_scroll(overlay_theme * theme,widget * w,int delta)
{
    int32_t max_offset;

    enterbox_check_visible_offset(theme,w);

    max_offset=w->enterbox.text_pixel_length - (w->base.r.x2-w->base.r.x1-2*theme->h_bar_text_offset-1);///text length - max offset
    if(max_offset < 0) max_offset=0;

    w->enterbox.visible_offset-=delta*theme->font.max_advance;

    if(w->enterbox.visible_offset < 0)w->enterbox.visible_offset=0;
    else if(w->enterbox.visible_offset > max_offset) w->enterbox.visible_offset=max_offset;

    return true;
}

static bool enterbox_widget_key_down(overlay_theme * theme,widget * w,SDL_Keycode keycode,SDL_Keymod mod)
{
    widget* root_widget;
    char *s_begin,*s_end,*s;

    if(w->enterbox.selection_end > w->enterbox.selection_begin) s_begin=w->enterbox.selection_begin, s_end=w->enterbox.selection_end;
    else s_begin=w->enterbox.selection_end, s_end=w->enterbox.selection_begin;

    switch(keycode)
    {
    case SDLK_C:
        if(mod&SDL_KMOD_CTRL)
        {
            enterbox_copy_selection_to_clipboard(w);
        }
        break;

    case SDLK_X:
        if(mod&SDL_KMOD_CTRL)
        {
            enterbox_copy_selection_to_clipboard(w);
            enterbox_delete_selection(theme,w,s_begin,s_end);
            w->enterbox.selection_begin = w->enterbox.selection_end = s_begin;
        }
        break;

    case SDLK_V:
        if(mod&SDL_KMOD_CTRL)
        {
            if(SDL_HasClipboardText())
            {
                enterbox_enter_text(theme,w,SDL_GetClipboardText());
            }
        }
        break;

    case SDLK_A:
        if(mod&SDL_KMOD_CTRL)
        {
            w->enterbox.selection_begin=w->enterbox.text;
            w->enterbox.selection_end=w->enterbox.text+strlen(w->enterbox.text);
        }
        break;

    case SDLK_BACKSPACE:
        if(mod&SDL_KMOD_CTRL)
        {
            if(mod&SDL_KMOD_SHIFT)s=w->textbox.text;
            else s=cvm_overlay_utf8_get_previous_word(w->enterbox.text,w->enterbox.selection_end);

            if(w->enterbox.selection_begin > w->enterbox.selection_end) w->enterbox.selection_begin-=w->enterbox.selection_end-s;
            else if(w->enterbox.selection_begin > s) w->enterbox.selection_begin=s;

            enterbox_delete_selection(theme,w,s,w->enterbox.selection_end);

            w->enterbox.selection_end=s;
        }
        else
        {
            if(s_begin==s_end) s_begin=cvm_overlay_utf8_get_previous_glyph(w->enterbox.text,s_begin);
            enterbox_delete_selection(theme,w,s_begin,s_end);
            w->enterbox.selection_begin = w->enterbox.selection_end = s_begin;
        }
        break;

    case SDLK_DELETE:
        if(mod&SDL_KMOD_CTRL)
        {
            if(mod&SDL_KMOD_SHIFT)s=w->enterbox.text+strlen(w->enterbox.text);
            else s=cvm_overlay_utf8_get_next_word(w->enterbox.selection_end);

            if(w->enterbox.selection_begin > w->enterbox.selection_end)
            {
                if(w->enterbox.selection_begin > s) w->enterbox.selection_begin -= s-w->enterbox.selection_end;
                else w->enterbox.selection_begin = w->enterbox.selection_end;
            }

            enterbox_delete_selection(theme,w,w->enterbox.selection_end,s);
        }
        else
        {
            if(s_begin==s_end) s_end=cvm_overlay_utf8_get_next_glyph(s_end);
            enterbox_delete_selection(theme,w,s_begin,s_end);
            w->enterbox.selection_begin = w->enterbox.selection_end = s_begin;
        }
        break;

    case SDLK_KP_4:/// keypad/numpad left
        if(mod&SDL_KMOD_NUM)break;
    case SDLK_LEFT:
        if(mod&SDL_KMOD_CTRL)///can move based on ctrl (jump word),  then set to same based on shift
        {
            if(mod&SDL_KMOD_SHIFT) w->enterbox.selection_end = cvm_overlay_utf8_get_previous_word(w->enterbox.text,w->enterbox.selection_end);
            else w->enterbox.selection_begin = w->enterbox.selection_end = cvm_overlay_utf8_get_previous_word(w->enterbox.text,w->enterbox.selection_end);
        }
        else if(mod&SDL_KMOD_SHIFT)w->enterbox.selection_end = cvm_overlay_utf8_get_previous_glyph(w->enterbox.text,w->enterbox.selection_end);
        else if(s_begin==s_end) w->enterbox.selection_begin = w->enterbox.selection_end = cvm_overlay_utf8_get_previous_glyph(w->enterbox.text,s_begin);
        else w->enterbox.selection_begin = w->enterbox.selection_end = s_begin;
        break;

    case SDLK_KP_6:/// keypad/numpad right
        if(mod&SDL_KMOD_NUM)break;
    case SDLK_RIGHT:
        if(mod&SDL_KMOD_CTRL)
        {
            if(mod&SDL_KMOD_SHIFT) w->enterbox.selection_end = cvm_overlay_utf8_get_next_word(w->enterbox.selection_end);
            else w->enterbox.selection_begin = w->enterbox.selection_end = cvm_overlay_utf8_get_next_word(w->enterbox.selection_end);
        }
        else if(mod&SDL_KMOD_SHIFT) w->enterbox.selection_end = cvm_overlay_utf8_get_next_glyph(w->enterbox.selection_end);
        else if(s_begin==s_end) w->enterbox.selection_begin = w->enterbox.selection_end = cvm_overlay_utf8_get_next_glyph(s_end);
        else w->enterbox.selection_begin = w->enterbox.selection_end = s_end;
        break;

    case SDLK_KP_7:/// keypad/numpad home
    case SDLK_KP_8:/// keypad/numpad up
    case SDLK_KP_9:/// keypad/numpad page up
         if(mod&SDL_KMOD_NUM)break;
    case SDLK_UP:
    case SDLK_PAGEUP:
    case SDLK_HOME:
        if(mod&SDL_KMOD_SHIFT) w->enterbox.selection_end = w->enterbox.text;
        else w->enterbox.selection_begin = w->enterbox.selection_end = w->enterbox.text;
        break;

    case SDLK_KP_1:/// keypad/numpad end
    case SDLK_KP_2:/// keypad/numpad down
    case SDLK_KP_3:/// keypad/numpad page down
         if(mod&SDL_KMOD_NUM)break;
    case SDLK_DOWN:
    case SDLK_PAGEDOWN:
    case SDLK_END:
        if(mod&SDL_KMOD_SHIFT) w->enterbox.selection_end = w->enterbox.text+strlen(w->enterbox.text);
        else w->enterbox.selection_begin = w->enterbox.selection_end = w->enterbox.text+strlen(w->enterbox.text);
        break;

    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        if((w->enterbox.activation_func)&&(!w->enterbox.activate_upon_deselect))
        {
            w->enterbox.activation_func(w);
        }
        set_currently_active_widget(w->base.context, NULL);
        break;

    case SDLK_ESCAPE:
        #warning probably dont want escape to ever activate, or at least not most of the time, any way to accomplish this? should it be the same as clicking away?
        set_currently_active_widget(w->base.context, NULL);
        break;

        default:;
    }

    enterbox_check_visible_offset(theme,w);

    if(w->enterbox.upon_input_func)
    {
        w->enterbox.upon_input_func(w);
    }

    return true;
}

#warning SDL_SystemCursor

static bool enterbox_widget_text_input(overlay_theme * theme,widget * w,char * text)
{
    enterbox_enter_text(theme,w,text);
    enterbox_check_visible_offset(theme,w);

    if(w->enterbox.upon_input_func)
    {
        w->enterbox.upon_input_func(w);
    }

    return true;
}

static bool enterbox_widget_text_edit(overlay_theme * theme,widget * w,char * text,int start,int length)
{
    strncpy(w->enterbox.composition_text,text,CVM_OVERLAY_MAX_COMPOSITION_BYTES);
    w->enterbox.composition_text[CVM_OVERLAY_MAX_COMPOSITION_BYTES-1]='\0';

    ///same visiblity checking as enterbox_check_visible_offset, but only needed here, and applying to specific vars so just in place code it
    int current_offset,text_space;

    text_space=w->base.r.x2-w->base.r.x1-2*theme->h_bar_text_offset-1;

    current_offset=overlay_text_single_line_get_pixel_length(&theme->font,w->enterbox.composition_text);

    if(w->enterbox.composition_visible_offset>current_offset) w->enterbox.composition_visible_offset=current_offset;
    if(w->enterbox.composition_visible_offset+text_space<current_offset) w->enterbox.composition_visible_offset=current_offset-text_space;
    current_offset-=text_space;///max offset
    if(current_offset < 0) w->enterbox.composition_visible_offset=0;
    else if(w->enterbox.composition_visible_offset > current_offset) w->enterbox.composition_visible_offset=current_offset;

    return true;
}

static void enterbox_widget_click_away(overlay_theme * theme,widget * w)
{
    // if(SDL_IsTextInputActive())SDL_StopTextInput();

    if((w->enterbox.activation_func)&&(w->enterbox.activate_upon_deselect))
    {
        w->enterbox.activation_func(w);
    }
}

static void enterbox_widget_delete(widget * w)
{
    free(w->enterbox.text);
    if(w->enterbox.free_data)free(w->enterbox.data);
}

static widget_behaviour_function_set enterbox_behaviour_functions=
{
    .l_click        =   enterbox_widget_left_click,
    .l_release      =   enterbox_widget_left_release,
    .r_click        =   blank_widget_right_click,
    .m_move         =   enterbox_widget_mouse_movement,
    .scroll         =   enterbox_widget_scroll,
    .key_down       =   enterbox_widget_key_down,
    .text_input     =   enterbox_widget_text_input,
    .text_edit      =   enterbox_widget_text_edit,
    .click_away     =   enterbox_widget_click_away,
    .add_child      =   blank_widget_add_child,
    .remove_child   =   blank_widget_remove_child,
    .wid_delete     =   enterbox_widget_delete
};

static void enterbox_widget_render(overlay_theme * theme,widget * w,int16_t x_off,int16_t y_off,struct cvm_overlay_render_batch * restrict render_batch,rectangle bounds)
{
    rectangle r=rectangle_add_offset(w->base.r, x_off, y_off);

	theme->h_bar_render(render_batch, theme, bounds, r, w->base.status, OVERLAY_MAIN_COLOUR);

	if(w->enterbox.update_contents_func && !is_currently_active_widget(w))w->enterbox.update_contents_func(w);

    if(w->enterbox.recalculate_text_size)
    {
        w->enterbox.text_pixel_length=overlay_text_single_line_get_pixel_length(&theme->font,w->enterbox.text);
        w->enterbox.recalculate_text_size=false;
    }

    overlay_text_single_line_render_data text_render_data=
	{
	    .flags=0,
	    .bounds=bounds,
	    .text_area=overlay_text_single_line_get_text_area(r,theme->font.glyph_size,theme->h_bar_text_offset),
	    .text_length=w->enterbox.text_pixel_length
	};

	text_render_data.y=text_render_data.text_area.y1;

	if(*w->enterbox.composition_text)
    {
        text_render_data.colour=OVERLAY_TEXT_COMPOSITION_COLOUR_0;
        text_render_data.text=w->enterbox.composition_text;
        text_render_data.selection_begin=text_render_data.selection_end=text_render_data.text+strlen(text_render_data.text);
        text_render_data.x=text_render_data.text_area.x1-w->enterbox.composition_visible_offset;
    }
    else
    {
        text_render_data.colour=OVERLAY_TEXT_COLOUR_0;
        text_render_data.text=w->enterbox.text;
        if(w->enterbox.selection_end > w->enterbox.selection_begin)
        {
            text_render_data.selection_begin = w->enterbox.selection_begin;
            text_render_data.selection_end = w->enterbox.selection_end;
        }
        else
        {
            text_render_data.selection_begin = w->enterbox.selection_end;
            text_render_data.selection_end = w->enterbox.selection_begin;
        }
        text_render_data.x=text_render_data.text_area.x1-w->enterbox.visible_offset;
    }

    text_render_data.flags |= (w->enterbox.min_glyphs_visible < w->enterbox.max_glyphs) ? OVERLAY_TEXT_RENDER_FADING : 0;
    text_render_data.flags |= is_currently_active_widget(w) ? OVERLAY_TEXT_RENDER_SELECTION : 0;

    overlay_text_single_line_render(render_batch, theme, &text_render_data);
}

static widget * enterbox_widget_select(overlay_theme * theme,widget * w,int16_t x_in,int16_t y_in)
{
    if(theme->h_bar_select(theme,rectangle_subtract_offset(w->base.r,x_in,y_in),w->base.status))return w;

    return NULL;
}

static void enterbox_widget_min_w(overlay_theme * theme,widget * w)
{
    w->enterbox.text_pixel_length=overlay_text_single_line_get_pixel_length(&theme->font,w->enterbox.text);
    w->base.min_w = 2*theme->h_bar_text_offset+theme->font.max_advance*w->enterbox.min_glyphs_visible+1;///+1 for caret, only necessary when bearingX is 0
}

static void enterbox_widget_min_h(overlay_theme * theme,widget * w)
{
    w->base.min_h = theme->base_unit_h;
}

static void enterbox_widget_set_w(overlay_theme * theme,widget * w)
{
    enterbox_check_visible_offset(theme,w);
}

static widget_appearence_function_set enterbox_appearence_functions=
{
    .render =   enterbox_widget_render,
    .select =   enterbox_widget_select,
    .min_w  =   enterbox_widget_min_w,
    .min_h  =   enterbox_widget_min_h,
    .set_w  =   enterbox_widget_set_w,
    .set_h  =   blank_widget_set_h
};

widget * create_enterbox(struct widget_context* context, uint32_t max_strlen,uint32_t max_glyphs,uint32_t min_glyphs_visible,char * initial_text,widget_function activation_func,widget_function update_contents_func,widget_function upon_input_func,void * data,bool free_data,bool activate_upon_deselect)
{
	widget * w=create_widget(context, sizeof(widget_enterbox));

	w->enterbox.data=data;
	w->enterbox.activation_func=activation_func;
	w->enterbox.update_contents_func=update_contents_func;
	w->enterbox.upon_input_func=upon_input_func;

	w->enterbox.max_strlen=max_strlen;
	w->enterbox.max_glyphs=max_glyphs;
	w->enterbox.min_glyphs_visible=min_glyphs_visible;

	w->enterbox.text=malloc(max_strlen+1);///multiply by max unicode character size

    w->enterbox.activate_upon_deselect=activate_upon_deselect;
	w->enterbox.free_data=free_data;

	set_enterbox_text(w,initial_text);
	w->enterbox.visible_offset=0;

	w->enterbox.composition_text[0]='\0';///use first character as key as to whether text is present/valid
	w->enterbox.composition_visible_offset=0;

	w->base.appearence_functions=&enterbox_appearence_functions;
	w->base.behaviour_functions=&enterbox_behaviour_functions;

	return w;
}


void set_enterbox_text(widget * w,const char * text)
{
    ///error checking, don't put in release?
    assert(!text || strlen(text)<=w->enterbox.max_strlen);///ATTEMPTING TO SET A STRING WITHOUT PROVIDING ENOUGH CAPACITY

    uint32_t gc;
    if(text && cvm_overlay_utf8_validate_string_and_count_glyphs(text,&gc) && gc<=w->enterbox.max_glyphs && strlen(text)<=w->enterbox.max_strlen)
    {
        strcpy(w->enterbox.text,text);
    }
    else w->enterbox.text[0]='\0';

    w->enterbox.visible_offset=0;
    w->enterbox.selection_begin=w->enterbox.text;
    w->enterbox.selection_end=w->enterbox.text;
    w->enterbox.recalculate_text_size=true;
}


