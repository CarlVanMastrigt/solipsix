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
#define _XOPEN_SOURCE 700

#include "solipsix.h"

#define _XOPEN_SOURCE 700

#include <dirent.h>


#define CVM_FL_DIRECTORY_TYPE_ID 0
#define CVM_FL_MISCELLANEOUS_TYPE_ID 1
#define CVM_FL_CUSTOM_TYPE_OFFSET 2

///implement basics of dirent for windows?

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
                    if(!d && n2>=s2)return -1;///n2 is the same number as n1 but has more digits so leading 0's takes precedence
                    while(n2>=s2) if(*n2-- != '0') return -1;///n2 is larger
                    if(d) return d;
                    else break;
                }
                if(n2<s2)
                {
                    if(!d) return 1; ///same as other branch but n1>=s1 is definitely satisfied by other branch not having been taken
                    while(n1>=s1) if(*n1-- != '0') return 1;///n1 is larger
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
    const file_list_entry * fse1=a;
    const file_list_entry * fse2=b;

    if(fse1->type_id != fse2->type_id && (fse1->type_id==CVM_FL_DIRECTORY_TYPE_ID || fse2->type_id==CVM_FL_DIRECTORY_TYPE_ID))return (((int)fse1->type_id)-((int)fse2->type_id));

    return file_list_string_compare_number_blocks(fse1->filename,fse2->filename);
}

static int file_list_entry_comparison_type(const void * a,const void * b)
{
    const file_list_entry * fse1=a;
    const file_list_entry * fse2=b;

    if(fse1->type_id != fse2->type_id )
    {
        if(fse1->type_id==CVM_FL_DIRECTORY_TYPE_ID || fse2->type_id==CVM_FL_DIRECTORY_TYPE_ID) return (((int)fse1->type_id)-((int)fse2->type_id));///misc entries go at the end
        if(fse1->type_id==CVM_FL_MISCELLANEOUS_TYPE_ID || fse2->type_id==CVM_FL_MISCELLANEOUS_TYPE_ID) return (((int)fse2->type_id)-((int)fse1->type_id));///misc entries go at the end
        return (((int)fse1->type_id)-((int)fse2->type_id));///sort lowest indexed to highest
    }

    return file_list_string_compare_number_blocks(fse1->filename,fse2->filename);
}


static int cvm_strcmp_lower(const char * s1,const char * s2)
{
    char c1,c2;

    do
    {
        c1=*(s1++);
        c2=*(s2++);

        if((c1>='A')&&(c1<='Z'))c1-='A'-'a';
        if((c2>='A')&&(c2<='Z'))c2-='A'-'a';

        if(c1!=c2)return c1-c2;
    }
    while((c1)&&(c2));

    return 0;
}

static void file_list_widget_clean_directory(char * directory)
{
    ///will only ever shorten directory so no need to alter buffer max size
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
    const char * e,*base,*name;

    assert(w->file_list.selected_entry_index>=0 && w->file_list.selected_entry_index<(int32_t)w->file_list.valid_entry_count);///should have a selected entry if setting composite buffer

    base=w->file_list.directory_buffer;
    for(e=base;*e;e++);
    s=e-base;

    name=w->file_list.entries[w->file_list.selected_entry_index].filename;
    for(e=name;*e;e++);
    s+=e-name;

    s+=2;///terminating null char, and space to add and ending / should it become necessary

    if(w->file_list.composite_buffer_size < s)w->file_list.composite_buffer=realloc(w->file_list.composite_buffer,sizeof(char)*(w->file_list.composite_buffer_size=s));

    strcpy(w->file_list.composite_buffer,base);
    strcat(w->file_list.composite_buffer,name);


    if(w->file_list.directory_text_bar)
    {
        if(w->file_list.entries[w->file_list.selected_entry_index].type_id==CVM_FL_DIRECTORY_TYPE_ID)
        {
            text_bar_widget_set_text_pointer(w->file_list.directory_text_bar,w->file_list.directory_buffer);
            ///terminate directory with a / (then add required null terminator)
            w->file_list.composite_buffer[s-2]='/';
            w->file_list.composite_buffer[s-1]='\0';
        }
        else
        {
            text_bar_widget_set_text_pointer(w->file_list.directory_text_bar,w->file_list.composite_buffer);
        }
    }
}

static inline void file_list_widget_set_selected_entry(widget * w,int32_t selected_entry_index,bool set_enterbox_contents)
{
    int32_t o;

    o=selected_entry_index*w->file_list.entry_height;
    if(w->file_list.offset>o)w->file_list.offset=o;

    o-=w->file_list.visible_height-w->file_list.entry_height;
    if(w->file_list.offset<o)w->file_list.offset=o;

    w->file_list.selected_entry_index=selected_entry_index;

    file_list_widget_set_composite_buffer(w);
    ///set enterbox
    if(set_enterbox_contents && w->file_list.entries[selected_entry_index].type_id!=CVM_FL_DIRECTORY_TYPE_ID && w->file_list.enterbox)
    {
        set_enterbox_text(w->file_list.enterbox,w->file_list.entries[selected_entry_index].filename);
    }
}

static void file_list_widget_deselect_entry(widget * w)
{
    if(w->file_list.directory_text_bar)text_bar_widget_set_text_pointer(w->file_list.directory_text_bar,w->file_list.directory_buffer);
    w->file_list.selected_entry_index=-1;
}

static void file_list_widget_set_directory(widget * w,const char * directory)
{
    if(directory==NULL)directory=getenv("HOME");
    if(directory==NULL)directory="";
    uint32_t length=strlen(directory)+2;///need space for / and null terminator

    if(length >= w->file_list.directory_buffer_size)w->file_list.directory_buffer=realloc(w->file_list.directory_buffer,sizeof(char)*(w->file_list.directory_buffer_size=length));
    strcpy(w->file_list.directory_buffer,directory);

    w->file_list.directory_buffer[length-2]='/';
    w->file_list.directory_buffer[length-1]='\0';

    file_list_widget_clean_directory(w->file_list.directory_buffer);
}

///call this when altering filtering
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

    qsort(entries,valid_count,sizeof(file_list_entry),file_list_entry_comparison_basic);

    file_list_widget_recalculate_scroll_properties(w);
}

///only call this when changing/refreshing directory, NOT when altering filtering
void file_list_widget_load_directory_entries(widget * w)
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

    file_types=fl->file_types;
    file_type_count=fl->file_type_count;

    while((entry=readdir(directory)))
    {
        if(entry->d_type==4)//DT_DIR
        {
            type_id=CVM_FL_DIRECTORY_TYPE_ID;
        }
        else /// entry->d_type==DT_REG
        {
            ///assert(entry->d_type==DT_REG);
            /// ^ this actually fails, will need special handling for these? or ignore?

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
                    if(!cvm_strcmp_lower(type_ext,ext))
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

    fl->selected_entry_index=-1;
    fl->offset=0;
    if(fl->directory_text_bar)text_bar_widget_set_text_pointer(fl->directory_text_bar,fl->directory_buffer);
}

static void file_list_enter_selected_directory(widget * w)
{
    const char *s;
    uint32_t dl,sl;

    dl=strlen(w->file_list.directory_buffer);

    s=w->file_list.entries[w->file_list.selected_entry_index].filename;
    sl=strlen(s);

    if(dl+sl+2 > w->file_list.directory_buffer_size) w->file_list.directory_buffer=realloc(w->file_list.directory_buffer,sizeof(char)*(w->file_list.directory_buffer_size=dl+sl+2));

    strcat(w->file_list.directory_buffer,s);
    w->file_list.directory_buffer[dl+sl]='/';
    w->file_list.directory_buffer[dl+sl+1]='\0';

    file_list_widget_clean_directory(w->file_list.directory_buffer);

    file_list_widget_load_directory_entries(w);
}


void test_accept_function(void* data)
{
    puts("accept");
}
void test_cancel_function(void* data)
{
    puts("cancel");
}

static void file_list_widget_perform_action(widget * w)
{
    if(w->file_list.selected_entry_index>=0)
    {
        if(w->file_list.entries[w->file_list.selected_entry_index].type_id == CVM_FL_DIRECTORY_TYPE_ID)
        {
            file_list_enter_selected_directory(w);///don't allow folder section (when that is required) via double clicking
        }
        else
        {
            puts("PERFORM ACTION!");
            assert(w->file_list.action);

            create_self_deleting_dialogue(w->base.context, w->base.context->root_widget, "do thing?", "yes", "no",NULL,test_accept_function, test_cancel_function);
        }
    }
}



///REMOVE THIS
static bool test_file_action(widget * w,bool force)
{
    #warning REMOVE
    printf("TEST %d %s\n",w->file_list.selected_entry_index,w->file_list.composite_buffer);
}






static bool file_list_widget_scroll(overlay_theme * theme,widget * w,int delta)
{
    w->file_list.offset-=theme->base_contiguous_unit_h*delta;
    if(w->file_list.offset<0)w->file_list.offset=0;
    if(w->file_list.offset>w->file_list.max_offset)w->file_list.offset=w->file_list.max_offset;

    return true;
}

static void file_list_widget_left_click(overlay_theme * theme, widget * w, int x, int y, bool double_clicked)
{
    rectangle r;
	int32_t index;
    adjust_coordinates_to_widget_local(w,&x,&y);

	index=(w->file_list.offset+y-theme->contiguous_box_y_offset)/theme->base_contiguous_unit_h;

    assert(index>=0);
    if(index<0)index=0;
    if(index>=(int32_t)w->file_list.valid_entry_count)return;

    /// *could* check against bounding h_bar for each item, but i don't see the value

    if(w->file_list.selected_entry_index==index)
    {
        if(double_clicked)
        {
            file_list_widget_perform_action(w);
            return;
        }
    }

    file_list_widget_set_selected_entry(w,index,true);
}

static bool file_list_widget_left_release(overlay_theme * theme,widget * clicked,widget * released,int x,int y)
{
    return true;
}

static bool file_list_widget_key_down(overlay_theme * theme,widget * w,SDL_Keycode keycode,SDL_Keymod mod)
{
    switch(keycode)
    {
    case SDLK_KP_8:/// keypad/numpad up
        if(mod&SDL_KMOD_NUM)break;
    case SDLK_UP:
        if(w->file_list.selected_entry_index>0)file_list_widget_set_selected_entry(w,w->file_list.selected_entry_index-1,true);
        break;

    case SDLK_KP_9:/// keypad/numpad page up
        if(mod&SDL_KMOD_NUM)break;
    case SDLK_PAGEUP:
        w->file_list.offset-=((w->file_list.visible_height-1)/(w->file_list.entry_height*2) + 1)*w->file_list.entry_height;///shift half screen rounded up in visible entry units
        if(w->file_list.offset<0)w->file_list.offset=0;
        if(w->file_list.offset>w->file_list.max_offset)w->file_list.offset=w->file_list.max_offset;
        break;

    case SDLK_KP_7:/// keypad/numpad home
        if(mod&SDL_KMOD_NUM)break;
    case SDLK_HOME:
        file_list_widget_set_selected_entry(w,0,true);
        break;

    case SDLK_KP_2:/// keypad/numpad down
        if(mod&SDL_KMOD_NUM)break;
    case SDLK_DOWN:
        if(w->file_list.selected_entry_index < (int32_t)w->file_list.valid_entry_count-1) file_list_widget_set_selected_entry(w,w->file_list.selected_entry_index+1,true);
        break;

    case SDLK_KP_3:/// keypad/numpad page down
        if(mod&SDL_KMOD_NUM)break;
    case SDLK_PAGEDOWN:
        w->file_list.offset+=((w->file_list.visible_height-1)/(w->file_list.entry_height*2) + 1)*w->file_list.entry_height;///shift half screen rounded up in visible entry units
        if(w->file_list.offset<0)w->file_list.offset=0;
        if(w->file_list.offset>w->file_list.max_offset)w->file_list.offset=w->file_list.max_offset;
        break;

    case SDLK_KP_1:/// keypad/numpad end
        if(mod&SDL_KMOD_NUM)break;
    case SDLK_END:
        file_list_widget_set_selected_entry(w,w->file_list.valid_entry_count-1,true);
        break;

    case SDLK_ESCAPE:
        set_currently_active_widget(w->base.context, NULL);
        file_list_widget_deselect_entry(w);///not sure is desirable but w/e
        break;

    case SDLK_RETURN:
        file_list_widget_perform_action(w);///return/enter to perform op on selected widget, same as double clicking
        break;

        default:;
    }

    return true;
}

static void file_list_widget_click_away(overlay_theme * theme,widget * w)
{
    //file_list_widget_deselect_entry(w);
}

void file_list_widget_delete(widget * w)
{
    free(w->file_list.filename_buffer);
    free(w->file_list.directory_buffer);
    free(w->file_list.composite_buffer);
    free(w->file_list.entries);
    if(w->file_list.free_action_data)free(w->file_list.action_data);
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
    .click_away     =   file_list_widget_click_away,
    .add_child      =   blank_widget_add_child,
    .remove_child   =   blank_widget_remove_child,
    .wid_delete     =   file_list_widget_delete
};


static void file_list_widget_render(overlay_theme * theme,widget * w,int16_t x_off,int16_t y_off,struct cvm_overlay_render_batch * restrict render_batch,rectangle bounds)
{
    file_list_entry * fle;
    rectangle icon_r,r;
	const file_list_type * file_types;
	int32_t y,y_end,index,y_text_off;
	const char * icon_glyph;

    file_types=w->file_list.file_types;

	r=rectangle_add_offset(w->base.r,x_off,y_off);

	theme->box_render(render_batch, theme, bounds, r, w->base.status, OVERLAY_MAIN_COLOUR);


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
        if(index==w->file_list.selected_entry_index)/// && is_currently_active_widget(w) not sure the currently selected widget thing is desirable, probably not, other programs don't do it
        {
            theme->h_bar_box_constrained_render(render_batch,theme,bounds,((rectangle){.x1=r.x1,.y1=y,.x2=r.x2,.y2=y+theme->base_contiguous_unit_h}),w->base.status,OVERLAY_HIGHLIGHTING_COLOUR,r,w->base.status);
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

            overlay_text_centred_glyph_box_constrained_render(render_batch,theme,bounds,icon_r,icon_glyph,OVERLAY_TEXT_COLOUR_0,r,w->base.status);
        }


        text_render_data.y=y+y_text_off;

        text_render_data.text=fle->filename;

        if(!fle->text_length_calculated)
        {
            fle->text_length=overlay_text_single_line_get_pixel_length(&theme->font,text_render_data.text);
            fle->text_length_calculated=true;
        }

        text_render_data.text_length=fle->text_length;

        overlay_text_single_line_render(render_batch,theme,&text_render_data);

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

widget * create_file_list(struct widget_context* context, int16_t min_visible_rows,int16_t min_visible_glyphs,const char * initial_directory,const char *const * error_messages,uint16_t error_count)
{
    widget * w=create_widget(context, sizeof(widget_file_list));

    // extant_file_list_count++;

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

    w->file_list.file_types=NULL;
    w->file_list.file_type_count=0;
    w->file_list.action=test_file_action;///remove test_file_action
    w->file_list.action_data=NULL;
    w->file_list.free_action_data=false;

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
    w->file_list.type_select_popup=NULL;

    w->file_list.selected_entry_index=-1;
    w->file_list.offset=0;
    w->file_list.max_offset=0;
    w->file_list.visible_height=0;
    w->file_list.entry_height=0;

    w->file_list.selected_out_type=0;

    file_list_widget_set_directory(w,initial_directory);
    file_list_widget_load_directory_entries(w);

    return w;
}


widget * create_file_list_widget_directory_text_bar(struct widget_context* context, widget * file_list,uint32_t min_glyphs_visible)
{
    widget * text_bar=create_dynamic_text_bar(context, min_glyphs_visible,WIDGET_TEXT_RIGHT_ALIGNED,true);

    file_list->file_list.directory_text_bar=text_bar;

    if(file_list->file_list.selected_entry_index<0)text_bar_widget_set_text_pointer(text_bar,file_list->file_list.directory_buffer);
    else file_list_widget_set_composite_buffer(file_list);

    return text_bar;
}


static void file_list_widget_enterbox_input_function(widget * enterbox)
{
    uint32_t i;
    widget * file_list=enterbox->enterbox.data;

    ///iterate over list and jump to (exact) match
    ///only select upon exact match (POSIX file requirements :P) or could sub comparison function on windows (cvm_strcmp_lower)
    for(i=0;i<file_list->file_list.valid_entry_count;i++)
    {
        ///also try appending extension??
        if(!strcmp(enterbox->enterbox.text,file_list->file_list.entries[i].filename))
        {
            file_list_widget_set_selected_entry(file_list,i,false);
            return;
        }
    }

    ///no match found, deselect
    file_list_widget_deselect_entry(file_list);
}

static void file_list_widget_enterbox_action_function(widget * enterbox)
{
    widget * file_list=enterbox->enterbox.data;
    file_list_widget_perform_action(file_list);
}

widget * create_file_list_widget_enterbox(struct widget_context* context, widget * file_list,uint32_t min_glyphs_visible)
{
    widget * enterbox=create_enterbox(context, 256, 256, min_glyphs_visible, NULL, file_list_widget_enterbox_action_function, NULL, file_list_widget_enterbox_input_function, file_list, false, false);
    file_list->file_list.enterbox=enterbox;
    return enterbox;
}


static void file_list_widget_refresh_button_function(widget * w)
{
    widget * fl=w->button.data;///button must be set up with file list as data
    file_list_widget_load_directory_entries(fl);///button must be set up with file list as data
}

widget * create_file_list_widget_refresh_button(struct widget_context* context, widget * file_list)
{
    return create_icon_button(context, "R", file_list, false, file_list_widget_refresh_button_function);
}

static void file_list_widget_up_button_function(widget * w)
{
    char *d,*b;
    widget * fl=w->button.data;///button must be set up with file list as data

    ///as this will only reduce the length of the directory buffer nothing complex will be necessary and it can be done in place
    b=d=fl->file_list.directory_buffer;

    if(!d || !*d)return;///don't process null or empty director buffers

    while(*d++);
    d-=2;///find last char of directory buffer

    assert(*d=='/');

    do d--;///find next previous directory separator
    while(d>b && *d!='/');

    if(d>=b)///don't do this if we're at root directory, d would also be an invalid pointer here
    {
        d[1]='\0';///after finding last directory separator trim the rest of the buffer that follows
        file_list_widget_load_directory_entries(fl);
    }
}

widget * create_file_list_widget_up_button(struct widget_context* context, widget * file_list)
{
    return create_icon_button(context, "↑",file_list,false,file_list_widget_up_button_function);
}

static void file_list_widget_home_button_function(widget * w)
{
    widget * fl=w->button.data;///button must be set up with file list as data
    file_list_widget_set_directory(fl,getenv("HOME"));
    file_list_widget_load_directory_entries(fl);
}

widget * create_file_list_widget_home_button(struct widget_context* context, widget * file_list)
{
    return create_icon_button(context, "H", file_list, false, file_list_widget_home_button_function);
}


const char * file_list_widget_get_selected_filepath(widget * w)
{
    if(w->file_list.selected_entry_index>0) return w->file_list.composite_buffer;
    else return w->file_list.directory_buffer;
}




