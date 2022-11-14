/**
Copyright 2020,2021,2022 Carl van Mastrigt

This file is part of cvm_shared.

cvm_shared is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

cvm_shared is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with cvm_shared.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "cvm_shared.h"

#include <dirent.h>
//#include <stdlib.h>



#define CVM_FL_DIRECTORY_TYPE_ID 0
#define CVM_FL_MISCELLANEOUS_TYPE_ID 1
#define CVM_FL_CUSTOM_TYPE_OFFSET 2

///implement basics of dirent for windows?

static uint32_t test_count=0;

static inline int file_list_string_compare_number_blocks(const char * s1,const char * s2)
{
    char c1,c2,d;
    const char *n1,*n2,*e1,*e2;

    do
    {
        c1=*s1;
        c2=*s2;

        if(c1>='0' && c1<='9' && c2>='0' && c2<='9')///enter number analysis scheme
        {
            ///already know first is digit
            n1=s1+1;
            n2=s2+1;
            ///find end of number block
            while(*n1>='0' && *n1<='9')n1++;
            while(*n2>='0' && *n2<='9')n2++;
            e1=n1;
            e2=n2;
            d=0;///initially treat numbers as the same, let most significat different digit determine their difference
            while(1)
            {
                ///start with step back, last char, by definition, isnt a number
                n1--;
                n2--;
                if(n1<s1)
                {
                    while(n2>=s2) if(*n2-- != '0') return -1;///b is larger
                    if(d) return d;
                    else break;
                }
                if(n2<s2)
                {
                    while(n1>=s1) if(*n1-- != '0') return 1;///a is larger
                    if(d) return d;
                    else break;
                }
                if(*n1!=*n2) d=*n1-*n2;
            }
            ///skip to the point past the number block
            s1=e1;
            s2=e2;
            c1=*s1;
            c2=*s2;
        }
        else
        {
            s1++;
            s2++;
        }

        if(c1>='A' && c1<='Z')c1-='A'-'a';
        if(c2>='A' && c2<='Z')c2-='A'-'a';
    }
    while((c1==c2)&&(c1)&&(c2));

    return (c1-c2);
}

static inline int file_list_string_compare(const char * s1,const char * s2)
{
    char c1,c2;

    do
    {
        c1=*s1++;
        c2=*s2++;

        if(c1>='A' && c1<='Z')c1-='A'-'a';
        if(c2>='A' && c2<='Z')c2-='A'-'a';
    }
    while((c1==c2)&&(c1)&&(c2));

    return (c1-c2);
}

static int file_list_entry_comparison_basic(const void * a,const void * b)
{
    test_count++;
    /// return a-b in terms of names
    const file_list_entry * fse1=a;
    const file_list_entry * fse2=b;

    if(fse1->type_id != fse2->type_id && (fse1->type_id==CVM_FL_DIRECTORY_TYPE_ID || fse2->type_id==CVM_FL_DIRECTORY_TYPE_ID))return (((int)fse1->type_id)-((int)fse2->type_id));

    return file_list_string_comparison_number_blocks(fse1->filename,fse2->filename);
}

static int file_list_entry_comparison_type(const void * a,const void * b)
{
    test_count++;
    /// return a-b in terms of names
    const file_list_entry * fse1=a;
    const file_list_entry * fse2=b;

    if(fse1->type_id != fse2->type_id )
    {
        if(fse1->type_id==CVM_FL_DIRECTORY_TYPE_ID || fse2->type_id==CVM_FL_DIRECTORY_TYPE_ID) return (((int)fse1->type_id)-((int)fse2->type_id));///misc entries go at the end
        if(fse1->type_id==CVM_FL_MISCELLANEOUS_TYPE_ID || fse2->type_id==CVM_FL_MISCELLANEOUS_TYPE_ID) return (((int)fse2->type_id)-((int)fse1->type_id));///misc entries go at the end
        return (((int)fse1->type_id)-((int)fse2->type_id));///sort lowest indexed to highest
    }

    return file_list_string_comparison_number_blocks(fse1->filename,fse2->filename);
}



static void file_list_widget_clean_directory(char * directory)
{
    char *r,*w;

    r=w=directory;

    while(*r)
    {
        while(*r=='/')
        {
            if((r[1]=='.')&&(r[2]=='.')&&(r[3]=='/'))///go up a layer when encountering ../
            {
                r+=3;
                if(w>directory)w--;///reverse over '/'
                while((w>directory)&&(*w!='/')) w--;
            }
            else if((r[1]=='.')&&(r[2]=='/'))r+=2;///remove ./
            else if(r[1]=='/')r++;/// remove multiple /
            else break;
        }

        if(w!=r)*w=*r;

        r++;
        w++;
    }

    *w='\0';
}

static inline bool is_hidden_file(const char * filename)
{
    return filename && *filename=='.' && strcmp(filename,".") && strcmp(filename,"..");///exists, first entry is '.' and isnt just "." and isnt just ".."
}

///relevant at time widget changes size or contents change
static inline void file_list_widget_recalculate_scroll_properties(widget * w)
{
    w->file_list.max_offset=w->file_list.valid_entry_count*w->file_list.entry_height - w->file_list.visible_height;
    if(w->file_list.max_offset<0)w->file_list.max_offset=0;
    if(w->file_list.offset>w->file_list.max_offset)w->file_list.offset=w->file_list.max_offset;
}

static inline void file_list_widget_set_composite_buffer(widget * w)
{
    uint32_t s;
    char * e,*base,*name;

    assert(w->file_list.selected_entry>=0 && w->file_list.selected_entry<(int32_t)w->file_list.valid_entry_count);///should have a selected entry if setting composite buffer

    base=w->file_list.directory_buffer;
    for(e=base;*e;e++);
    s=e-base;

    name=w->file_list.entries[w->file_list.selected_entry].filename;
    for(e=name;*e;e++);
    s+=e-name;

    s+=2;///terminating null char, and space to add and ending / should it become necessary

    while(w->file_list.composite_buffer_size < s)w->file_list.composite_buffer=realloc(w->file_list.composite_buffer,sizeof(char)*(w->file_list.composite_buffer_size*=2));

    strcpy(w->file_list.composite_buffer,base);
    strcat(w->file_list.composite_buffer,name);

    if(w->file_list.directory_text_bar)text_bar_widget_set_text_pointer(w->file_list.directory_text_bar,w->file_list.composite_buffer);
}

static inline void file_list_widget_set_selected_entry(widget * w,int32_t selected_entry_index)
{
    int32_t o;

    o=selected_entry_index*w->file_list.entry_height;
    if(w->file_list.offset>o)w->file_list.offset=o;

    o-=w->file_list.visible_height-w->file_list.entry_height;
    if(w->file_list.offset<o)w->file_list.offset=o;

    w->file_list.selected_entry=selected_entry_index;

    file_list_widget_set_composite_buffer(w);
    ///set enterbox
}

static void file_list_widget_deselect_entry(widget * w)
{
    if(w->file_list.directory_text_bar)text_bar_widget_set_text_pointer(w->file_list.directory_text_bar,w->file_list.directory_buffer);
    ///clear enterbox
    w->file_list.selected_entry=-1;
}

static void file_list_widget_set_directory(widget * w,const char * directory)
{
    if(directory==NULL)directory=getenv("HOME");
    if(directory==NULL)directory="";
    uint32_t length=strlen(directory);

    while((length+2) >= w->file_list.directory_buffer_size)w->file_list.directory_buffer=realloc(w->file_list.directory_buffer,sizeof(char)*(w->file_list.directory_buffer_size*=2));
    strcpy(w->file_list.directory_buffer,directory);

    w->file_list.directory_buffer[length]='/';
    w->file_list.directory_buffer[length+1]='\0';

    file_list_widget_clean_directory(w->file_list.directory_buffer);

    file_list_widget_deselect_entry(w);
}

static void file_list_widget_organise_entries(widget * w)
{
    uint32_t valid_count,i;
    file_list_entry *entries,tmp_entry;
    widget_file_list * fl;///alias type (union to specific type) for more readable code

    fl=&w->file_list;///alias type (union to specific type) for more readable code

    entries=fl->entries;
    i=valid_count=fl->entry_count;

    do
    {
        i--;

        if(( fl->hide_hidden_entries &&  entries[i].is_hidden_file) ||
           ( fl->hide_control_entries && entries[i].is_control_entry ) ||
           ( fl->hide_misc_files && entries[i].type_id==CVM_FL_MISCELLANEOUS_TYPE_ID ) ||
           ( fl->fixed_directory && entries[i].type_id==CVM_FL_DIRECTORY_TYPE_ID))
        {
            valid_count--;

            if(valid_count!=i)
            {
                tmp_entry=entries[valid_count];
                entries[valid_count]=entries[i];
                entries[i]=tmp_entry;
            }
        }
    }
    while(i);

    fl->valid_entry_count=valid_count;

    qsort(entries,valid_count,sizeof(file_list_entry),file_list_entry_comparison_type);// file_list_entry_comparison_type file_list_entry_comparison_basic
}


#warning make static ??
void load_file_search_directory_entries(widget * w)
{
    DIR * directory;
    struct dirent * entry;
    const file_list_type * file_types;
    uint32_t file_type_count;
    uint32_t i,filename_buffer_offset,filename_length;
    uint16_t type_id;
    const char *filename,*ext,*type_ext;
    widget_file_list * fl;


    fl=&w->file_list;///alias type (union to specific type) for more readable code


    fl->entry_count=0;
    fl->valid_entry_count=0;
    filename_buffer_offset=0;

    directory = opendir(fl->directory_buffer);

    if(!directory)
    {
        printf("SUPPLIED DIRECTORY INVALID :%s:\n",fl->directory_buffer);
        file_list_widget_set_directory(w,getenv("HOME"));///load home directory as backup

        directory = opendir(fl->directory_buffer);
        if(!directory)
        {
            puts("HOME DIRECTORY INVALID");
            return;
        }
    }

    if(fl->save_mode_active)
    {
        file_types=fl->save_types;
        file_type_count=fl->save_type_count;
    }
    else
    {
        file_types=fl->load_types;
        file_type_count=fl->load_type_count;
    }

    while((entry=readdir(directory)))
    {
        if(entry->d_type==DT_DIR)
        {
            type_id=CVM_FL_DIRECTORY_TYPE_ID;
        }
        else if(entry->d_type==DT_REG)
        {
            type_id=CVM_FL_MISCELLANEOUS_TYPE_ID;

            ext=NULL;
            filename=entry->d_name;
            while(*filename)
            {
                if(*filename=='.')ext=filename+1;
                filename++;
            }

            if(ext) for(i=0;i<file_type_count && type_id==CVM_FL_MISCELLANEOUS_TYPE_ID;i++)
            {
                type_ext=file_types[i].type_extensions;

                while(*type_ext)
                {
                    if(!str_lower_cmp(type_ext,ext))
                    {
                        type_id=i+CVM_FL_CUSTOM_TYPE_OFFSET;
                        break;
                    }
                    while(*type_ext++);///move to next in concatenated extension list
                }
            }
        }

        filename_length=strlen(entry->d_name)+1;

        while((filename_buffer_offset+filename_length)>fl->filename_buffer_space)
        {
            fl->filename_buffer=realloc(fl->filename_buffer,(fl->filename_buffer_space*=2));
        }

        if(fl->entry_count==fl->entry_space)
        {
            fl->entries=realloc(fl->entries,sizeof(file_list_entry)*(fl->entry_space*=2));
        }

        fl->entries[fl->entry_count++]=(file_list_entry)
        {
            .filename_offset_=filename_buffer_offset,
            .type_id=type_id,
            .is_hidden_file=is_hidden_file(entry->d_name),
            .is_control_entry=!(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")),
            .text_length_calculated=false,
            .text_length=0,
        };

        strcpy(fl->filename_buffer+filename_buffer_offset,entry->d_name);
        filename_buffer_offset+=filename_length;
    }

    for(i=0;i<fl->entry_count;i++)
    {
        fl->entries[i].filename=fl->filename_buffer+fl->entries[i].filename_offset_;
    }

    closedir(directory);

    file_list_widget_organise_entries(w);
}












static bool file_list_widget_scroll(overlay_theme * theme,widget * w,int delta)
{
    w->file_list.offset-=theme->base_contiguous_unit_h*delta;
    if(w->file_list.offset<0)w->file_list.offset=0;
    if(w->file_list.offset>w->file_list.max_offset)w->file_list.offset=w->file_list.max_offset;

    return true;
}

static void file_list_widget_left_click(overlay_theme * theme,widget * w,int x,int y)
{
    rectangle r;
	int32_t index;
    adjust_coordinates_to_widget_local(w,&x,&y);

	index=(w->file_list.offset+y-theme->contiguous_box_y_offset)/theme->base_contiguous_unit_h;

    assert(index>=0);
    if(index<0)index=0;
    if(index>=(int32_t)w->file_list.valid_entry_count)return;

    /// *could* check against bounding h_bar for each item, but i don't see the value

    file_list_widget_set_selected_entry(w,index);

    if(w->file_list.selected_entry==index)
    {
        if(check_widget_double_clicked(w))
        {
            if(w->file_list.entries[index].type_id == CVM_FL_DIRECTORY_TYPE_ID)
            {
                puts("double clicked directory");
            }
            else
            {
                puts("double clicked file");
            }
        }
    }
    else
    {
        if(w->file_list.entries[index].type_id == CVM_FL_DIRECTORY_TYPE_ID)
        {
            puts("single clicked directory");
        }
        else
        {
            puts("single clicked file");
        }
    }
}

static bool file_list_widget_left_release(overlay_theme * theme,widget * clicked,widget * released,int x,int y)
{
    return true;
}

static bool file_list_widget_key_down(overlay_theme * theme,widget * w,SDL_Keycode keycode,SDL_Keymod mod)
{
    assert(w->file_list.selected_entry>=0 && w->file_list.selected_entry<(int32_t)w->file_list.valid_entry_count);///should have a selected entry if performing keyboard operations

    switch(keycode)
    {
    case SDLK_KP_8:/// keypad/numpad up
        if(mod&KMOD_NUM)break;
    case SDLK_KP_9:/// keypad/numpad page up
        if(mod&KMOD_NUM)break;
    case SDLK_PAGEUP:
    case SDLK_UP:
        if(w->file_list.selected_entry>0)file_list_widget_set_selected_entry(w,w->file_list.selected_entry-1);
        break;

    case SDLK_KP_7:/// keypad/numpad home
        if(mod&KMOD_NUM)break;
    case SDLK_HOME:
        file_list_widget_set_selected_entry(w,0);
        break;


    case SDLK_KP_2:/// keypad/numpad down
        if(mod&KMOD_NUM)break;
    case SDLK_KP_3:/// keypad/numpad page down
        if(mod&KMOD_NUM)break;
    case SDLK_PAGEDOWN:
    case SDLK_DOWN:
        if(w->file_list.selected_entry < (int32_t)w->file_list.valid_entry_count-1) file_list_widget_set_selected_entry(w,w->file_list.selected_entry+1);
        break;

    case SDLK_KP_1:/// keypad/numpad end
        if(mod&KMOD_NUM)break;
    case SDLK_END:
        file_list_widget_set_selected_entry(w,w->file_list.valid_entry_count-1);
        break;

    case SDLK_ESCAPE:
        set_currently_active_widget(NULL);
        break;

    case SDLK_RETURN:
        puts("file list perform action");///return to perform op on selected widget, same as double clicking
        break;

        default:;
    }

    return true;
}

void file_list_widget_delete(widget * w)
{
    free(w->file_list.filename_buffer);
    free(w->file_list.directory_buffer);
    free(w->file_list.composite_buffer);
    free(w->file_list.entries);
}

static widget_behaviour_function_set enterbox_behaviour_functions=
{
    .l_click        =   file_list_widget_left_click,
    .l_release      =   file_list_widget_left_release,
    .r_click        =   blank_widget_right_click,
    .m_move         =   blank_widget_mouse_movement,
    .scroll         =   file_list_widget_scroll,
    .key_down       =   file_list_widget_key_down,
    .text_input     =   blank_widget_text_input,
    .text_edit      =   blank_widget_text_edit,
    .click_away     =   blank_widget_click_away,///should invalidate current selection
    .add_child      =   blank_widget_add_child,
    .remove_child   =   blank_widget_remove_child,
    .wid_delete     =   file_list_widget_delete
};


static void file_list_widget_render(overlay_theme * theme,widget * w,int16_t x_off,int16_t y_off,cvm_overlay_element_render_buffer * erb,rectangle bounds)
{
    file_list_entry * fle;
    rectangle icon_r,r;
	const file_list_type * file_types;
	int32_t y,y_end,index,y_text_off;
	const char * icon_glyph;

    if(w->file_list.save_mode_active)file_types=w->file_list.save_types;
    else file_types=w->file_list.load_types;

	r=rectangle_add_offset(w->base.r,x_off,y_off);

	theme->box_render(erb,theme,bounds,r,w->base.status,OVERLAY_MAIN_COLOUR);


	y_end=r.y2-theme->contiguous_box_y_offset;
	index=w->file_list.offset/theme->base_contiguous_unit_h;
	y=r.y1+index*theme->base_contiguous_unit_h-w->file_list.offset+theme->contiguous_box_y_offset;
	y_text_off=(theme->base_contiguous_unit_h-theme->font.glyph_size)>>1;

    ///make fade and constrained
	overlay_text_single_line_render_data text_render_data=
	{
	    .flags=OVERLAY_TEXT_RENDER_BOX_CONSTRAINED|OVERLAY_TEXT_RENDER_FADING,
	    .x=r.x1+theme->h_bar_text_offset+ w->file_list.render_type_icons*(theme->h_bar_icon_text_offset+theme->base_contiguous_unit_w),///base_contiguous_unit_w used for icon space/sizing
	    //.y=,
	    //.text=,
	    .bounds=bounds,
	    .colour=OVERLAY_TEXT_COLOUR_0,
	    .box_r=r,
	    .box_status=w->base.status,
	    //.text_length=,
	};

	text_render_data.text_area=(rectangle){.x1=text_render_data.x,.y1=r.y1,.x2=r.x2-theme->h_bar_text_offset,.y2=r.y2};

	icon_r.x1=r.x1+theme->h_bar_text_offset;
	icon_r.x2=r.x1+theme->h_bar_text_offset+theme->base_contiguous_unit_w;

	while(y<y_end && index<(int32_t)w->file_list.valid_entry_count)
    {
        if(index==w->file_list.selected_entry)/// && is_currently_active_widget(w) not sure the currently selected widget thing is desirable, probably not, other programs don't do it
        {
            theme->h_bar_box_constrained_render(erb,theme,bounds,((rectangle){.x1=r.x1,.y1=y,.x2=r.x2,.y2=y+theme->base_contiguous_unit_h}),w->base.status,OVERLAY_HIGHLIGHTING_COLOUR,r,w->base.status);
        }

        fle=w->file_list.entries+index;

        if(w->file_list.render_type_icons)
        {
            ///render icon
            icon_r.y1=y;
            icon_r.y2=y+theme->base_contiguous_unit_h;

            if(fle->type_id==CVM_FL_DIRECTORY_TYPE_ID) icon_glyph="🗀";
            else if(fle->type_id==CVM_FL_MISCELLANEOUS_TYPE_ID) icon_glyph="🗎";
            else icon_glyph=file_types[fle->type_id-CVM_FL_CUSTOM_TYPE_OFFSET].icon;

            overlay_text_centred_glyph_box_constrained_render(erb,theme,bounds,icon_r,icon_glyph,OVERLAY_TEXT_COLOUR_0,r,w->base.status);
        }


        text_render_data.y=y+y_text_off;

        text_render_data.text=fle->filename;

        if(!fle->text_length_calculated)
        {
            fle->text_length=overlay_text_single_line_get_pixel_length(&theme->font,text_render_data.text);
            fle->text_length_calculated=true;
        }

        text_render_data.text_length=fle->text_length;

        overlay_text_single_line_render(erb,theme,&text_render_data);

        y+=theme->base_contiguous_unit_h;
        index++;
    }
}

static widget * file_list_widget_select(overlay_theme * theme,widget * w,int16_t x_in,int16_t y_in)
{
    if(theme->box_select(theme,rectangle_subtract_offset(w->base.r,x_in,y_in),w->base.status))return w;
    return NULL;
}

static void file_list_widget_min_w(overlay_theme * theme,widget * w)
{
    w->base.min_w = 2*theme->h_bar_text_offset+theme->font.max_advance*w->file_list.min_visible_glyphs + w->file_list.render_type_icons*(theme->h_bar_icon_text_offset+theme->base_contiguous_unit_w);
}

static void file_list_widget_min_h(overlay_theme * theme,widget * w)
{
    w->base.min_h=w->file_list.min_visible_rows*theme->base_contiguous_unit_h+2*theme->contiguous_box_y_offset;
}



static void file_list_widget_set_h(overlay_theme * theme,widget * w)
{
    w->file_list.visible_height=w->base.r.y2-w->base.r.y1 - 2*theme->contiguous_box_y_offset;
    w->file_list.entry_height=theme->base_contiguous_unit_h;

    file_list_widget_recalculate_scroll_properties(w);
}

static widget_appearence_function_set file_list_appearence_functions=
{
    .render =   file_list_widget_render,
    .select =   file_list_widget_select,
    .min_w  =   file_list_widget_min_w,
    .min_h  =   file_list_widget_min_h,
    .set_w  =   blank_widget_set_w,
    .set_h  =   file_list_widget_set_h
};

widget * create_file_list(int16_t min_visible_rows,int16_t min_visible_glyphs,const char * initial_directory,const file_list_type * save_types,uint32_t save_type_count,const file_list_type * load_types,uint32_t load_type_count)
{
    widget * w=create_widget();

    w->base.appearence_functions=&file_list_appearence_functions;
    w->base.behaviour_functions=&enterbox_behaviour_functions;

    w->file_list.filename_buffer_space=16;
    w->file_list.filename_buffer=malloc(sizeof(char)*w->file_list.filename_buffer_space);

    w->file_list.directory_buffer_size=16;
    w->file_list.directory_buffer=malloc(sizeof(char)*w->file_list.directory_buffer_size);

    w->file_list.composite_buffer_size=16;
    w->file_list.composite_buffer=malloc(sizeof(char)*w->file_list.composite_buffer_size);

    w->file_list.entry_count=0;
    w->file_list.valid_entry_count=0;
    w->file_list.entry_space=8;
    w->file_list.entries=malloc(sizeof(file_list_entry)*w->file_list.entry_space);

    w->file_list.save_types=save_types;
    w->file_list.save_type_count=save_type_count;
    w->file_list.load_types=load_types;
    w->file_list.load_type_count=load_type_count;

    w->file_list.fixed_directory=false;
    w->file_list.hide_misc_files=false;
    w->file_list.hide_control_entries=true;
    w->file_list.hide_hidden_entries=false;
    w->file_list.render_type_icons=true;
    w->file_list.save_mode_active=true;

    w->file_list.min_visible_rows=min_visible_rows;
    w->file_list.min_visible_glyphs=min_visible_glyphs;

    w->file_list.directory_text_bar=NULL;
    w->file_list.enterbox=NULL;
    w->file_list.parent_widget=NULL;
    w->file_list.error_popup=NULL;
    w->file_list.type_select_popup=NULL;

    w->file_list.selected_entry=-1;
    w->file_list.offset=0;
    w->file_list.max_offset=0;
    w->file_list.visible_height=0;
    w->file_list.entry_height=0;

    w->file_list.selected_out_type=0;

    file_list_widget_set_directory(w,initial_directory);
    load_file_search_directory_entries(w);

    return w;
}

void file_list_widget_set_directory_text_bar(widget * file_list,widget * text_bar)
{
    file_list->file_list.directory_text_bar=text_bar;

    if(file_list->file_list.selected_entry<0)text_bar_widget_set_text_pointer(text_bar,file_list->file_list.directory_buffer);
    else file_list_widget_set_composite_buffer(file_list);
}

