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
// #include <SDL2/SDL_timer.h>
#include <assert.h>
#include <stdio.h>


void widget_context_initialise(struct widget_context* context, overlay_theme* theme)
{
    context->theme = theme;
    context->registered_widget_count = 0;

    context->double_click_time =  400;//?

    context->currently_active_widget  = NULL;
    context->only_interactable_widget = NULL;

    context->previously_clicked_widget = NULL;

    context->root_widget = NULL;
}

void widget_context_terminate(struct widget_context* context)
{
    // should delete/unregister all widgets before terminating context 
    assert(context->registered_widget_count == 0);

    assert(context->currently_active_widget == NULL);
    assert(context->only_interactable_widget == NULL);


}

/// one level below root
static widget* get_widgets_toplevel_ancestor(widget * w)
{
    if((w)&&(w->base.parent))
    {
        while(w->base.parent->base.parent)
        {
            w=w->base.parent;
        }
    }

    assert(w && w->base.parent && w->base.parent->base.status&WIDGET_IS_ROOT);///COULD NOT FIND APPROPRIATE WIDGET TOPLEVEL ANCESTOR (ROOT)
    // above error usually indicates you're trying to do something that requires a widget to be part of a valid hierarchy

    return w;
}

widget* find_root_widget(widget * w)
{
    assert(w);
    while(w->base.parent)
    {
        w = w->base.parent;
    }

    assert(w);///COULD NOT FIND APPROPRIATE WIDGET TOPLEVEL ANCESTOR (ROOT)
    // assert(w->base.status&WIDGET_IS_ROOT);///COULD NOT FIND APPROPRIATE WIDGET TOPLEVEL ANCESTOR (ROOT)
    if(!(w->base.status&WIDGET_IS_ROOT))
    {
        return NULL;
    }

    return w;
}



void render_widget_overlay(struct cvm_overlay_render_batch * restrict render_batch, widget * root_widget)
{
    assert(root_widget->base.status&WIDGET_IS_ROOT);
    render_widget(root_widget, root_widget->base.context->theme, 0, 0, render_batch, root_widget->base.r);
}


static void base_widget_render(overlay_theme * theme,widget * w,int16_t x_off,int16_t y_off,struct cvm_overlay_render_batch * restrict render_batch,rectangle bounds)
{
    puts("error calling base: render");
}

static widget * base_widget_select(overlay_theme * theme,widget * w,int16_t x_in,int16_t y_in)
{
    puts("error calling base: select");
    return NULL;
}

static void base_widget_min_w(overlay_theme * theme,widget * w)
{
    puts("error calling base: min_w");
}

static void base_widget_min_h(overlay_theme * theme,widget * w)
{
    puts("error calling base: min_h");
}

static void base_widget_set_w(overlay_theme * theme,widget * w)
{
    puts("error calling base: size_h");
}

static void base_widget_set_h(overlay_theme * theme,widget * w)
{
    puts("error calling base: set_h");
}

static widget_appearence_function_set base_appearence_functions=
{
    .render =   base_widget_render,
    .select =   base_widget_select,
    .min_w  =   base_widget_min_w,
    .min_h  =   base_widget_min_h,
    .set_w  =   base_widget_set_w,
    .set_h  =   base_widget_set_h
};



static void base_widget_left_click(overlay_theme * theme, widget * w, int x, int y, bool double_clicked)
{
    puts("error calling base: l_click");
}

static bool base_widget_left_release(overlay_theme * theme,widget * clicked,widget * released,int x,int y)
{
    puts("error calling base: l_release");
    return false;
}

static void base_widget_right_click(overlay_theme * theme,widget * w,int x,int y)
{
    puts("error calling base: r_click");
}

static void base_widget_mouse_movement(overlay_theme * theme,widget * w,int x,int y)
{
    puts("error calling base: m_move");
}

static bool base_widget_scroll(overlay_theme * theme,widget * w,int delta)
{
    ///this can be called as widgets with no behaviour form part of the search tree

    return false;
}

static bool base_widget_key_down(overlay_theme * theme,widget * w,SDL_Keycode keycode,SDL_Keymod mod)
{
    puts("error calling base: key_down");
    return false;
}

static bool base_widget_text_input(overlay_theme * theme,widget * w,char * text)
{
    puts("error calling base: text_input");
    return false;
}

static bool base_widget_text_edit(overlay_theme * theme,widget * w,char * text,int start,int length)
{
    puts("error calling base: text_edit");
    return false;
}

static void base_widget_click_away(overlay_theme * theme,widget * w)
{
    puts("error calling base: click_away");
}

static void base_widget_add_child(widget * parent,widget * child)
{
    puts("error calling base: add_child");
}

static void base_widget_remove_child(widget * parent,widget * child)
{
    puts("error calling base: remove_child");
}

static void base_widget_delete(widget * w)
{
    puts("error calling base: delete");
}


static widget_behaviour_function_set base_behaviour_functions =
{
    .l_click      = base_widget_left_click,
    .l_release    = base_widget_left_release,
    .r_click      = base_widget_right_click,
    .m_move       = base_widget_mouse_movement,
    .scroll       = base_widget_scroll,
    .key_down     = base_widget_key_down,
    .text_input   = base_widget_text_input,
    .text_edit    = base_widget_text_edit,
    .click_away   = base_widget_click_away,
    .add_child    = base_widget_add_child,
    .remove_child = base_widget_remove_child,
    .wid_delete   = base_widget_delete
};

void widget_base_initialise(widget_base* base, struct widget_context* context, const widget_appearence_function_set * appearence_functions, const widget_behaviour_function_set * behaviour_functions)
{
    *base = (widget_base)
    {
        .context = context,
        .status = WIDGET_ACTIVE,
        .references = 1,
        .min_w = 0,
        .min_h = 0,
        .r = rectangle_ini(0,0,0,0),
        .parent = NULL,
        .next = NULL,
        .prev = NULL,
        .appearence_functions = appearence_functions,
        .behaviour_functions = behaviour_functions,
    };

    context->registered_widget_count++;
}

widget * create_widget(struct widget_context* context, size_t size)
{
    widget * w = malloc(size);

    widget_base_initialise(&w->base, context, &base_appearence_functions, &base_behaviour_functions);

    return w;
}


bool widget_active(widget * w)
{
    if(w->base.status&WIDGET_ACTIVE)return true;
    return false;
}



void adjust_coordinates_to_widget_local(widget * w,int * x,int * y)
{
    while(w->base.parent)
    {
        *x-=w->base.r.x1;
        *y-=w->base.r.y1;

        w=w->base.parent;
    }
}

void get_widgets_global_coordinates(widget * w,int * x,int * y)
{
	*x=0;
	*y=0;

    while(w->base.parent)
    {
        w=w->base.parent;

        *x+=w->base.r.x1;
        *y+=w->base.r.y1;
    }
}



static void organise_widget(widget * w, overlay_theme* theme, int width, int height)
{
    if(widget_active(w))
    {
        set_widget_minimum_width(w, theme, WIDGET_POS_FLAGS_H);
        organise_widget_horizontally(w, theme, 0, width);

        set_widget_minimum_height(w, theme, WIDGET_POS_FLAGS_V);
        organise_widget_vertically(w, theme, 0, height);
    }
}

void organise_root_widget(widget * root,int screen_width,int screen_height)
{
    #warning remove theme from this, instead get theme from the widgets
    assert(root->base.status&WIDGET_IS_ROOT);
    organise_widget(root, root->base.context->theme, screen_width, screen_height);

    const rectangle r = root->base.r;
    if(r.x1 != 0 || r.x2 != screen_width || r.y1 != 0 || r.y2 != screen_height)
    {
        fprintf(stderr, "warning: widget could not fit on screen\n>> %d->%d %d->%d vs %d, %d\n",r.x1,r.x2,r.y1,r.y2, screen_width, screen_height);
    }
}



void organise_toplevel_widget(widget * w)
{
    widget* ancestor = get_widgets_toplevel_ancestor(w);

    if(ancestor)
    {
        organise_widget(ancestor, ancestor->base.context->theme,
            ancestor->base.parent->base.r.x2 - ancestor->base.parent->base.r.x1,
            ancestor->base.parent->base.r.y2 - ancestor->base.parent->base.r.y1);
    }
    else
    {
        puts("why is there no toplevel widget!?");
    }
}

void move_toplevel_widget_to_front(widget * w)
{
    w = get_widgets_toplevel_ancestor(w);

    if((!w)||(w->base.parent->container.last==w))/// ||(w->base.status & WIDGET_STAYS_AT_BOTTOM)
    {
        return;
    }

    if(w->base.prev)
    {
        w->base.prev->base.next=w->base.next;
    }

    if(w->base.next)
    {
        w->base.next->base.prev=w->base.prev;
    }

    if(w->base.parent->container.first==w)
    {
        w->base.parent->container.first=w->base.next;
    }

    w->base.parent->container.last->base.next=w;
    w->base.prev=w->base.parent->container.last;
    w->base.parent->container.last=w;
    w->base.next=NULL;
}






void blank_widget_render(overlay_theme * theme,widget * w,int16_t x_off,int16_t y_off,struct cvm_overlay_render_batch * restrict render_batch,rectangle bounds)
{
}

widget * blank_widget_select(overlay_theme * theme,widget * w,int16_t x_in,int16_t y_in)
{
    return NULL;
}

void blank_widget_min_w(overlay_theme * theme,widget * w)
{
}

void blank_widget_min_h(overlay_theme * theme,widget * w)
{
}

void blank_widget_set_w(overlay_theme * theme,widget * w)
{
}

void blank_widget_set_h(overlay_theme * theme,widget * w)
{
}




void blank_widget_left_click(overlay_theme * theme, widget * w, int x, int y, bool double_clicked)
{
}

bool blank_widget_left_release(overlay_theme * theme,widget * clicked,widget * released,int x,int y)
{
    return false;
}

void blank_widget_right_click(overlay_theme * theme,widget * w,int x,int y)
{
}

void blank_widget_mouse_movement(overlay_theme * theme,widget * w,int x,int y)
{
}

bool blank_widget_scroll(overlay_theme * theme,widget * w,int delta)
{
    return false;
}

bool blank_widget_key_down(overlay_theme * theme,widget * w,SDL_Keycode keycode,SDL_Keymod mod)
{
    return false;
}

bool blank_widget_text_input(overlay_theme * theme,widget * w,char * text)
{
    return false;
}

bool blank_widget_text_edit(overlay_theme * theme,widget * w,char * text,int start,int length)
{
    return false;
}

void blank_widget_click_away(overlay_theme * theme,widget * w)
{
}

void blank_widget_add_child(widget * parent,widget * child)
{
    puts("error calling blank: add_child");
}

void blank_widget_remove_child(widget * parent,widget * child)
{
    puts("error calling blank: remove_child");
}

void blank_widget_delete(widget * w)
{
    puts("error calling blank: delete");
}


bool is_currently_active_widget(widget * w)
{
    assert(w);
    return (w == w->base.context->currently_active_widget);
}



void set_currently_active_widget(struct widget_context* context, widget * w)
{
    widget* old_active = context->currently_active_widget;
    if((old_active) && (old_active != w))
    {
        old_active->base.behaviour_functions->click_away(context->theme, old_active);
    }

    context->currently_active_widget = w;
}

void set_only_interactable_widget(struct widget_context* context, widget * w)
{
    context->only_interactable_widget = w;
}


void render_widget(widget * w, overlay_theme* theme, int x_off, int y_off, struct cvm_overlay_render_batch * restrict render_batch, rectangle bounds)
{
    #warning remove theme from this, instead get theme from the widgets
    rectangle r=rectangle_add_offset(w->base.r,x_off,y_off);

    if(w && w->base.status&WIDGET_ACTIVE && rectangles_overlap(r,bounds))
    {
        w->base.appearence_functions->render(theme, w, x_off, y_off, render_batch, bounds);
    }
}

#warning remove theme from this...
widget * select_widget(widget * w, overlay_theme* theme, int x_in, int y_in)
{
    if(w && w->base.status&WIDGET_ACTIVE && rectangle_surrounds_point(w->base.r,(vec2i){.x=x_in,.y=y_in}))
    {
        return w->base.appearence_functions->select(theme, w, x_in, y_in);
    }
    return NULL;
}

int16_t set_widget_minimum_width(widget * w, overlay_theme* theme, uint32_t pos_flags)
{
    if(w && w->base.status&WIDGET_ACTIVE)
    {
        w->base.status&=WIDGET_POS_FLAGS_CLEAR;
        w->base.status|=(WIDGET_POS_FLAGS_H&pos_flags);

        w->base.appearence_functions->min_w(theme, w);

        return w->base.min_w;
    }
    return 0;
}

int16_t set_widget_minimum_height(widget * w, overlay_theme* theme, uint32_t pos_flags)
{
    if(w && w->base.status&WIDGET_ACTIVE)
    {
        w->base.status|=(WIDGET_POS_FLAGS_V&pos_flags);

        w->base.appearence_functions->min_h(theme, w);

        return w->base.min_h;
    }
    return 0;
}

int16_t organise_widget_horizontally(widget * w, overlay_theme* theme, int16_t x_pos, int16_t width)
{
    if(w && w->base.status&WIDGET_ACTIVE)
    {
        w->base.r.x1=x_pos;
        w->base.r.x2=x_pos+width;
        w->base.appearence_functions->set_w(theme, w);
        return width;
    }
    return 0;
}

int16_t organise_widget_vertically(widget * w, overlay_theme* theme, int16_t y_pos, int16_t height)
{
    if(w && w->base.status&WIDGET_ACTIVE)
    {
        w->base.r.y1=y_pos;
        w->base.r.y2=y_pos+height;
        w->base.appearence_functions->set_h(theme, w);
        return height;
    }
    return 0;
}



static widget * get_widget_under_mouse(widget * root, int x_in, int y_in)
{
    widget *current,*selected;

    assert(root->base.status&WIDGET_IS_ROOT);

    struct widget_context* context = root->base.context;

    if(context->only_interactable_widget)
    {
        adjust_coordinates_to_widget_local(context->only_interactable_widget, &x_in, &y_in);
        selected = select_widget(context->only_interactable_widget, context->theme, x_in, y_in);

        if(selected) 
        {
            return selected;
        }
        else
        {
            /// returns base of interactable to ensure no OTHER systems (non-UI) will attempt to capture the input
            #warning consider doing this for other things like key-presses
            return context->only_interactable_widget;
        }
    }

    for(current = root->container.last; current; current = current->base.prev)
    {
        selected = select_widget(current, context->theme, x_in, y_in);

        if(selected)
        {
            //if(move_to_top)move_toplevel_widget_to_front(current);
            return selected;
        }
    }

    return NULL;
}


bool handle_widget_overlay_left_click(widget* root_widget,int x_in,int y_in)
{
    assert(root_widget->base.status&WIDGET_IS_ROOT);

    struct widget_context* context = root_widget->base.context;

    #warning remove SDL call here!
    uint32_t t = SDL_GetTicks();
    widget * w = get_widget_under_mouse(root_widget, x_in, y_in);

    // consider if this is correct
    set_currently_active_widget(context, w);
    #warning need a step to validate aspects of root widget upon stuff like window change

    close_auto_close_popup_tree(w);
    #warning ^ ????

    #warning if a widget can delete its own hierarchy then setting the previously clicked widget like this references a deleted widget, AS CAN close_auto_close_popup_tree!!!

    if(w)
    {
        /// not the greatest solution

        bool double_clicked = (t - context->previously_clicked_time) < context->double_click_time && w == context->previously_clicked_widget;

        /// must go here as it needs to be invalidatable inside the l_click function
        context->previously_clicked_widget = w;
        context->previously_clicked_time = t;

        move_toplevel_widget_to_front(w);

        w->base.behaviour_functions->l_click(context->theme, w, x_in, y_in, double_clicked);

        return true;
    }
    else
    {
        context->previously_clicked_widget = NULL;
        context->previously_clicked_time = t;
        return false;
    }
}

///returns wether this function intercepted (used) the input
bool handle_widget_overlay_left_release(widget* root_widget, int x_in, int y_in)
{
    assert(root_widget->base.status&WIDGET_IS_ROOT);

    struct widget_context* context = root_widget->base.context;
    widget* active_widget = context->currently_active_widget;

    if(active_widget == NULL)
    {

        return false;
    }

    widget * released_upon = get_widget_under_mouse(root_widget, x_in, y_in);

    // release returns whether to ignore this action 
    // (action being releasing on top of the specified widget, usually this is just checking you click and release the same widget)
    if(!active_widget->base.behaviour_functions->l_release(context->theme, active_widget, released_upon, x_in, y_in))
    {
        set_currently_active_widget(context, NULL);
    }

    return true;
}

bool handle_widget_overlay_movement(widget * root_widget,int x_in,int y_in)
{
    assert(root_widget->base.status&WIDGET_IS_ROOT);

    struct widget_context* context = root_widget->base.context;
    widget* active_widget = context->currently_active_widget;

    #warning remove SDL calls from this
    #warning is "button left" really sensible for this? should check how enterboxes work..
    if((active_widget)&&(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)))
    {
        active_widget->base.behaviour_functions->m_move(context->theme, active_widget, x_in, y_in);

        return true;
    }

    return false;
}

bool handle_widget_overlay_wheel(widget * root_widget,int x_in,int y_in,int delta)
{
    assert(root_widget->base.status&WIDGET_IS_ROOT);
    overlay_theme* theme = root_widget->base.context->theme;

    widget * w = get_widget_under_mouse(root_widget, x_in, y_in);

    if(w==NULL)
    {
        return false;
    }

    bool finished=false;

    while((w)&&(!finished))
    {
        finished=w->base.behaviour_functions->scroll(theme, w, delta);

        w = w->base.parent;
    }

    #warning following is a hack, should be handled by scriollbar internally by checking id currently active widget 
    handle_widget_overlay_movement(root_widget, x_in, y_in);///to avoid scrollbar/slider_bar etc. changing when currently clicked

    return true;
}

bool handle_widget_overlay_keyboard(widget * root_widget, SDL_Keycode keycode, SDL_Keymod mod)
{
    assert(root_widget->base.status&WIDGET_IS_ROOT);
    struct widget_context* context = root_widget->base.context;
    widget* active_widget = context->currently_active_widget;

    #warning this function should be genericized the most, should handle ACTIONS/COMMANDS instead of specific keys, that way it can handle controller input as well

    if(active_widget)
    {
        return active_widget->base.behaviour_functions->key_down(context->theme, active_widget, keycode, mod);
    }

    return false;
}

bool handle_widget_overlay_text_input(widget * root_widget, char * text)
{
    assert(root_widget->base.status&WIDGET_IS_ROOT);
    struct widget_context* context = root_widget->base.context;
    widget* active_widget = context->currently_active_widget;

    if(active_widget)
    {
        return active_widget->base.behaviour_functions->text_input(context->theme, active_widget, text);
    }

    return false;
}

bool handle_widget_overlay_text_edit(widget * root_widget, char * text, int start, int length)
{
    assert(root_widget->base.status&WIDGET_IS_ROOT);
    struct widget_context* context = root_widget->base.context;
    widget* active_widget = context->currently_active_widget;

    if(active_widget)
    {
        return active_widget->base.behaviour_functions->text_edit(context->theme, active_widget, text, start, length);
    }

    return false;
}



widget * add_child_to_parent(widget * parent,widget * child)
{
    assert(parent->base.context == child->base.context);// there should only ever be one context for widgets in the same heirarchy
    parent->base.behaviour_functions->add_child(parent,child);
    return child;
}

void remove_child_from_parent(widget * child)
{
    widget * parent=child->base.parent;

    if(parent == NULL)
    {
        assert(child->base.status&WIDGET_IS_ROOT);
    }
    else
    {
        assert(parent->base.context == child->base.context);// there should only ever be one context for widgets in the same hierarchy
        parent->base.behaviour_functions->remove_child(parent,child);
    }
    

    child->base.parent=NULL;///there is a lot of duplication of this, remove from all add child instances? does it matter? assert it has been done
    #warning instead assert above, removing the child should set its parent to NULL
}

void delete_widget(widget * w)
{
    assert(w);

    if(!w)
    {
        fprintf(stderr, "ERROR: deleting NULL widget");
        return;
    }

    struct widget_context* context = w->base.context;

    if(w == context->currently_active_widget)
    {
        // note: this is specifically NOT calling click away (do we want that though?)
        context->currently_active_widget = NULL;
    }
    if(w == context->only_interactable_widget)
    {
        context->only_interactable_widget = NULL;
    }
    if(w == context->previously_clicked_widget)
    {
        context->previously_clicked_widget = NULL;
    }

    assert(!(w->base.status&WIDGET_DO_NOT_DELETE));///should not be trying to delete a widget that has been marked as undeleteable, but handle it anyway
    if(w->base.status&WIDGET_DO_NOT_DELETE)
    {
        return;
    }

    #warning links to other widgets in popup.trigger_widget and any button that toggles another widget prevents deletion from being a clean process! (deletion of any widget in this kind of structure may cause a segfault)

    remove_child_from_parent(w);

    release_widget(w);
}


void retain_widget(widget * w)
{
    w->base.references++;
}

void release_widget(widget * w)
{
    w->base.references--;

    if(w->base.references == 0)
    {
        w->base.context->registered_widget_count--;
        w->base.behaviour_functions->wid_delete(w);
        free(w);
    }
}





widget* create_root_widget(struct widget_context* context, overlay_theme* theme)
{
    assert(context->root_widget == NULL);//should only be one root widget

    // better to have underlying init function that all these functions call?
    widget* w = create_container(context, sizeof(widget_container));///actually is just a pure container
    w->base.status |= WIDGET_IS_ROOT;

    context->root_widget = w;

    return w;
}