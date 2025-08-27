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


/// "ring" (internal and external limit) variant
//static void cubic_create_shape(cvm_overlay_interactable_element * element,uint32_t ri,uint32_t ro)///change ri & r0 as 16ths of a pixel??
//{
//    uint32_t i,r,threshold,r4,x,y,xc,yc,xm,ym,t,r_min,r_max,rom,rim;
//    uint8_t *s;
//    uint8_t *data;
//
//    assert(ro<=640);///CUBIC OVERLAY ELEMENTS OF THIS SIZE NOT SUPPORTED
//
//    r4=(((ro>>4)-1)&~3)+4;///convert outer radius to pixels (not 1/16ths pixels) and round up to multiple of 4
//
//    element->tile=overlay_create_transparent_image_tile_with_staging(&s,r4*2,r4*2);
//
//    if(element->tile)
//    {
//        threshold=64;
//
//
//        data=s;
//        //data=malloc(sizeof(uint8_t)*r4*r4*4);
//
//        ro=ro*ro*ro;
//        ri=ri*ri*ri;
//
//        rom=ro*8;
//        rim=ri*8;
//
//        ro>>=12;
//        ri>>=12;
//
//
//
//        element->selection_grid=calloc((r4*r4)/4,sizeof(uint16_t));///r4*2*r4*2/16 == r4*r4/4
//
//        element->w=r4*2;
//        element->h=r4*2;
//
////        static struct timespec tso,tsn;
////        uint64_t dt;
////
////        clock_gettime(CLOCK_REALTIME,&tso);
//
//
//        for(x=0;x<r4;x++)
//        {
//            for(y=0;y<r4;y++)
//            {
//                t=0;
//                r_min=x*x*x + y*y*y;
//                r_max=(x+1)*(x+1)*(x+1) + (y+1)*(y+1)*(y+1);
//                if(r_min>=ro || r_max<=ri)t=0;
//                else if(r_max<=ro && r_min>=ri)t=255;
//                else for(xm=1;xm<32;xm+=2)for(ym=1;ym<32;ym+=2)///16 increments at pixel centres
//                {
//                    xc=(x<<5)+xm;
//                    yc=(y<<5)+ym;
//                    r=xc*xc*xc+yc*yc*yc;
//                    t+= r>rim && r<rom;
//                }
//                t-= t==256;
//
//                i=(r4+y)*r4*2+(r4+x);
//                data[i]=t;
//                element->selection_grid[i>>4]|=(t>threshold)<<(i&0x0F);
//
//                i=(r4+y)*r4*2+(r4-x-1);
//                data[i]=t;
//                element->selection_grid[i>>4]|=(t>threshold)<<(i&0x0F);
//
//                i=(r4-y-1)*r4*2+(r4+x);
//                data[i]=t;
//                element->selection_grid[i>>4]|=(t>threshold)<<(i&0x0F);
//
//                i=(r4-y-1)*r4*2+(r4-x-1);
//                data[i]=t;
//                element->selection_grid[i>>4]|=(t>threshold)<<(i&0x0F);
//            }
//        }
//
//        //memcpy(s,data,sizeof(uint8_t)*r4*r4*4);
//
////        clock_gettime(CLOCK_REALTIME,&tsn);
////
////        dt=(tsn.tv_sec-tso.tv_sec)*1000000000 + tsn.tv_nsec-tso.tv_nsec;
////
////        printf("CUBIC TIME = %lu      %u\n",dt,t);
//    }
//}


static void cubic_create_shape(overlay_theme * theme, cvm_vk_image_atlas_tile ** tile, uint16_t ** selection_grid, uint32_t r, struct cvm_overlay_render_batch * restrict render_batch)///change ri & r0 as 16ths of a pixel??
{
    uint32_t i,rt,threshold,x,y,xc,yc,xm,ym,t,rm;
    uint8_t * data;
    uint16_t * grid;

    assert(r<=40);///CUBIC OVERLAY ELEMENTS OF THIS SIZE NOT SUPPORTED

    *tile = cvm_vk_acquire_image_atlas_tile(render_batch->alpha_atlas, r*2,r*2);
    data = (uint8_t*)cvm_vk_stage_image_atlas_upload(&render_batch->upload_shunt_buffer, &render_batch->alpha_atlas_copy_actions, *tile, r*2, r*2, render_batch->alpha_atlas->bytes_per_pixel).allocation;

    if(*tile)
    {
        threshold=64;

        rt=r*r*r;

        rm=rt*32768;///32^3

        if(selection_grid)*selection_grid=grid=calloc((r*r+3)>>2,sizeof(uint16_t));///r*2*r*2/16 == r*r/4 == r*r>>2, but round up

        for(x=0;x<r;x++)
        {
            for(y=0;y<r;y++)
            {
                t=0;
                if((x*x*x + y*y*y)>=rt)t=0;
                else if(((x+1)*(x+1)*(x+1) + (y+1)*(y+1)*(y+1))<=rt)t=255;
                else
                    for(xm=1;xm<32;xm+=2)for(ym=1;ym<32;ym+=2)///16 increments at pixel centres
                {
                    xc=(x<<5)+xm;
                    yc=(y<<5)+ym;
                    t+= (xc*xc*xc+yc*yc*yc)<rm;
                }
                t-= t==256;

                i=(r+y)*r*2+(r+x);
                data[i]=t;
                if(selection_grid)grid[i>>4]|=(t>threshold)<<(i&0x0F);

                i=(r+y)*r*2+(r-x-1);
                data[i]=t;
                if(selection_grid)grid[i>>4]|=(t>threshold)<<(i&0x0F);

                i=(r-y-1)*r*2+(r+x);
                data[i]=t;
                if(selection_grid)grid[i>>4]|=(t>threshold)<<(i&0x0F);

                i=(r-y-1)*r*2+(r-x-1);
                data[i]=t;
                if(selection_grid)grid[i>>4]|=(t>threshold)<<(i&0x0F);
            }
        }
    }
}

static void cubic_shaded_element_box_constrained_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,overlay_colour colour,int16_t x_off,int16_t y_off,rectangle box_r,uint32_t box_status)
{
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    if(!cubic->foreground_image_tile)cubic_create_shape(theme, &cubic->foreground_image_tile, &cubic->foreground_selection_grid, cubic->foreground_r, render_batch);
    if(!cubic->foreground_image_tile)return;

    int16_t radius=cubic->foreground_r;
    int16_t diameter=cubic->foreground_d;
    int16_t tile_x_pos=cubic->foreground_image_tile->x_pos<<2;
    int16_t tile_y_pos=cubic->foreground_image_tile->y_pos<<2;

    int16_t y_off_p,x2_p,y1_p,y2_p;

    box_r.x1+=cubic->foreground_offset_x * !(box_status&WIDGET_H_FIRST);
    box_r.x2-=cubic->foreground_offset_x * !(box_status&WIDGET_H_LAST);
    box_r.y1+=cubic->foreground_offset_y;
    box_r.y2-=cubic->foreground_offset_y;

    if( !rectangles_overlap(r,box_r) || !rectangles_overlap(r,bounds))return;///early exit, no overlap

    if(r.x1<box_r.x1)
    {
        x_off+=box_r.x1-r.x1;
        r.x1=box_r.x1;
    }

    if(r.y1<box_r.y1)
    {
        y_off+=box_r.y1-r.y1;
        r.y1=box_r.y1;
    }

    if(r.x2>box_r.x2)r.x2=box_r.x2;

    if(r.y2>box_r.y2)r.y2=box_r.y2;

    if(r.x1<box_r.x1+radius && !(box_status&WIDGET_H_FIRST))///left applicable
    {
        y1_p=r.y1;
        y_off_p=y_off;
        x2_p=box_r.x1+radius;
        if(x2_p>r.x2)x2_p=r.x2;

        ///make the single part that changes some tracked variable??
        y2_p=box_r.y1+radius;
        if(r.y1<y2_p)
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_overlap_min_overlay_element(render_batch, bounds, ((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=y2_p}), colour, x_off, y_off_p, tile_x_pos-box_r.x1, tile_y_pos-box_r.y1);

            y_off_p+=y2_p-y1_p;
            y1_p=y2_p;
        }

        y2_p=box_r.y2-radius;
        if(r.y2 > y1_p && y2_p > r.y1)///render middle section normally
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=y2_p}),colour,x_off,y_off_p);

            y_off_p+=y2_p-y1_p;
            y1_p=y2_p;
        }

        if(r.y2>box_r.y2-radius)
        {
            cvm_render_shaded_overlap_min_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=r.y2}),colour,x_off,y_off_p,tile_x_pos-box_r.x1,tile_y_pos+diameter-box_r.y2);
        }

        x_off+=x2_p-r.x1;///move to end?
        r.x1=x2_p;///only happens if less, should be fine
    }

    x2_p=box_r.x2-radius * !(box_status&WIDGET_H_LAST);
    if(r.x2 > r.x1 && x2_p > r.x1)///render middle section normally
    {
        if(x2_p>r.x2)x2_p=r.x2;

        cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=x2_p,.y2=r.y2}),colour,x_off,y_off);

        x_off+=x2_p-r.x1;
        r.x1=x2_p;
    }

    if(r.x2>box_r.x2-radius && !(box_status&WIDGET_H_LAST))
    {
        y2_p=box_r.y1+radius;
        if(r.y1<y2_p)
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_overlap_min_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=r.x2,.y2=y2_p}),colour,x_off,y_off,tile_x_pos+diameter-box_r.x2,tile_y_pos-box_r.y1);

            y_off+=y2_p-r.y1;
            r.y1=y2_p;
        }

        y2_p=box_r.y2-radius;
        if(r.y2 > r.y1 && y2_p > r.y1)///render middle section normally
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=r.x2,.y2=y2_p}),colour,x_off,y_off);

            y_off+=y2_p-r.y1;
            r.y1=y2_p;
        }

        if(r.y2>box_r.y2-radius)
        {
            cvm_render_shaded_overlap_min_overlay_element(render_batch,bounds,r,colour,x_off,y_off,tile_x_pos+diameter-box_r.x2,tile_y_pos+diameter-box_r.y2);
        }
    }
}

static void cubic_shaded_element_fading_box_constrained_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,overlay_colour colour,int16_t x_off,int16_t y_off,
                                                        rectangle fade_bound,rectangle fade_range,rectangle box_r,uint32_t box_status)
{
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    if(!cubic->foreground_image_tile)cubic_create_shape(theme, &cubic->foreground_image_tile, &cubic->foreground_selection_grid, cubic->foreground_r, render_batch);
    if(!cubic->foreground_image_tile)return;

    int16_t radius=cubic->foreground_r;
    int16_t diameter=cubic->foreground_d;
    int16_t tile_x_pos=cubic->foreground_image_tile->x_pos<<2;
    int16_t tile_y_pos=cubic->foreground_image_tile->y_pos<<2;

    int16_t y_off_p,x2_p,y1_p,y2_p;

    box_r.x1+=cubic->foreground_offset_x * !(box_status&WIDGET_H_FIRST);
    box_r.x2-=cubic->foreground_offset_x * !(box_status&WIDGET_H_LAST);
    box_r.y1+=cubic->foreground_offset_y;
    box_r.y2-=cubic->foreground_offset_y;

    if( !rectangles_overlap(r,box_r) || !rectangles_overlap(r,bounds))return;///early exit, no overlap

    if(r.x1<box_r.x1)
    {
        x_off+=box_r.x1-r.x1;
        r.x1=box_r.x1;
    }

    if(r.y1<box_r.y1)
    {
        y_off+=box_r.y1-r.y1;
        r.y1=box_r.y1;
    }

    if(r.x2>box_r.x2)r.x2=box_r.x2;

    if(r.y2>box_r.y2)r.y2=box_r.y2;

    if(r.x1<box_r.x1+radius && !(box_status&WIDGET_H_FIRST))///left applicable
    {
        y1_p=r.y1;
        y_off_p=y_off;
        x2_p=box_r.x1+radius;
        if(x2_p>r.x2)x2_p=r.x2;

        ///make the single part that changes some tracked variable??
        y2_p=box_r.y1+radius;
        if(r.y1<y2_p)
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_fading_overlap_min_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=y2_p}),colour,x_off,y_off_p,fade_bound,fade_range,tile_x_pos-box_r.x1,tile_y_pos-box_r.y1);

            y_off_p+=y2_p-y1_p;
            y1_p=y2_p;
        }

        y2_p=box_r.y2-radius;
        if(r.y2 > y1_p && y2_p > r.y1)///render middle section normally
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_fading_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=y2_p}),colour,x_off,y_off_p,fade_bound,fade_range);

            y_off_p+=y2_p-y1_p;
            y1_p=y2_p;
        }

        if(r.y2>box_r.y2-radius)
        {
            cvm_render_shaded_fading_overlap_min_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=r.y2}),colour,x_off,y_off_p,fade_bound,fade_range,tile_x_pos-box_r.x1,tile_y_pos+diameter-box_r.y2);
        }

        x_off+=x2_p-r.x1;///move to end?
        r.x1=x2_p;///only happens if less, should be fine
    }

    x2_p=box_r.x2-radius * !(box_status&WIDGET_H_LAST);
    if(r.x2 > r.x1 && x2_p > r.x1)///render middle section normally
    {
        if(x2_p>r.x2)x2_p=r.x2;

        cvm_render_shaded_fading_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=x2_p,.y2=r.y2}),colour,x_off,y_off,fade_bound,fade_range);

        x_off+=x2_p-r.x1;
        r.x1=x2_p;
    }

    if(r.x2>box_r.x2-radius && !(box_status&WIDGET_H_LAST))
    {
        y2_p=box_r.y1+radius;
        if(r.y1<y2_p)
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_fading_overlap_min_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=r.x2,.y2=y2_p}),colour,x_off,y_off,fade_bound,fade_range,tile_x_pos+diameter-box_r.x2,tile_y_pos-box_r.y1);

            y_off+=y2_p-r.y1;
            r.y1=y2_p;
        }

        y2_p=box_r.y2-radius;
        if(r.y2 > r.y1 && y2_p > r.y1)///render middle section normally
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_fading_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=r.x2,.y2=y2_p}),colour,x_off,y_off,fade_bound,fade_range);

            y_off+=y2_p-r.y1;
            r.y1=y2_p;
        }

        if(r.y2>box_r.y2-radius)
        {
            cvm_render_shaded_fading_overlap_min_overlay_element(render_batch,bounds,r,colour,x_off,y_off,fade_bound,fade_range,tile_x_pos+diameter-box_r.x2,tile_y_pos+diameter-box_r.y2);
        }
    }
}

static void cubic_fill_element_box_constrained_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,overlay_colour colour,rectangle box_r,uint32_t box_status)
{
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    if(!cubic->foreground_image_tile)cubic_create_shape(theme, &cubic->foreground_image_tile, &cubic->foreground_selection_grid, cubic->foreground_r, render_batch);
    if(!cubic->foreground_image_tile)return;

    int radius=cubic->foreground_r;
    int diameter=cubic->foreground_d;
    int x_off=cubic->foreground_image_tile->x_pos<<2;
    int y_off=cubic->foreground_image_tile->y_pos<<2;

    int y1_p,y2_p,x2_p;

    box_r.x1+=cubic->foreground_offset_x * !(box_status&WIDGET_H_FIRST);
    box_r.x2-=cubic->foreground_offset_x * !(box_status&WIDGET_H_LAST);
    box_r.y1+=cubic->foreground_offset_y;
    box_r.y2-=cubic->foreground_offset_y;

    r=get_rectangle_overlap(r,box_r);

    if( !rectangle_has_positive_area(r) || !rectangles_overlap(r,bounds))return;///early exit, no overlap

    if(r.x1<box_r.x1+radius && !(box_status&WIDGET_H_FIRST))///left applicable
    {
        y1_p=r.y1;
        x2_p=box_r.x1+radius;
        if(x2_p>r.x2)x2_p=r.x2;

        ///make the single part that changes some tracked variable??
        y2_p=box_r.y1+radius;
        if(r.y1<y2_p)
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=y2_p}),colour,x_off+r.x1-box_r.x1,y_off+y1_p-box_r.y1);

            y1_p=y2_p;
        }

        y2_p=box_r.y2-radius;
        if(r.y2 > y1_p && y2_p > r.y1)///render middle section normally
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_fill_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=y2_p}),colour);

            y1_p=y2_p;
        }

        if(r.y2>box_r.y2-radius)
        {
            cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=r.y2}),colour,x_off+r.x1-box_r.x1,y_off+y1_p-box_r.y2+diameter);
        }

        r.x1=x2_p;///only happens if less, should be fine
    }

    x2_p=box_r.x2-radius * !(box_status&WIDGET_H_LAST);
    if(r.x2 > r.x1 && x2_p > r.x1)///render middle section normally
    {
        if(x2_p>r.x2)x2_p=r.x2;

        cvm_render_fill_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=x2_p,.y2=r.y2}),colour);

        r.x1=x2_p;
    }

    if(r.x2>box_r.x2-radius && !(box_status&WIDGET_H_LAST))
    {
        y2_p=box_r.y1+radius;
        if(r.y1<y2_p)
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_overlay_element(render_batch,bounds,(rectangle){.x1=r.x1,.y1=r.y1,.x2=r.x2,.y2=y2_p},colour,x_off+r.x1-box_r.x2+diameter,y_off+r.y1-box_r.y1);

            r.y1=y2_p;
        }

        y2_p=box_r.y2-radius;
        if(r.y2 > r.y1 && y2_p > r.y1)///render middle section normally
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_fill_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=r.x2,.y2=y2_p}),colour);

            r.y1=y2_p;
        }

        if(r.y2>box_r.y2-radius)
        {
            cvm_render_shaded_overlay_element(render_batch,bounds,r,colour,x_off+r.x1-box_r.x2+diameter,y_off+r.y1-box_r.y2+diameter);
        }
    }
}

static void cubic_fill_element_fading_box_constrained_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,overlay_colour colour,
                                                      rectangle fade_bound,rectangle fade_range,rectangle box_r,uint32_t box_status)
{
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    if(!cubic->foreground_image_tile)cubic_create_shape(theme, &cubic->foreground_image_tile, &cubic->foreground_selection_grid, cubic->foreground_r, render_batch);
    if(!cubic->foreground_image_tile)return;

    int radius=cubic->foreground_r;
    int diameter=cubic->foreground_d;
    int x_off=cubic->foreground_image_tile->x_pos<<2;
    int y_off=cubic->foreground_image_tile->y_pos<<2;

    int y1_p,y2_p,x2_p;

    box_r.x1+=cubic->foreground_offset_x * !(box_status&WIDGET_H_FIRST);
    box_r.x2-=cubic->foreground_offset_x * !(box_status&WIDGET_H_LAST);
    box_r.y1+=cubic->foreground_offset_y;
    box_r.y2-=cubic->foreground_offset_y;

    r=get_rectangle_overlap(r,box_r);

    if( !rectangle_has_positive_area(r) || !rectangles_overlap(r,bounds))return;///early exit, no overlap

    if(r.x1<box_r.x1+radius && !(box_status&WIDGET_H_FIRST))///left applicable
    {
        y1_p=r.y1;
        x2_p=box_r.x1+radius;
        if(x2_p>r.x2)x2_p=r.x2;

        ///make the single part that changes some tracked variable??
        y2_p=box_r.y1+radius;
        if(r.y1<y2_p)
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_fading_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=y2_p}),colour,x_off+r.x1-box_r.x1,y_off+y1_p-box_r.y1,fade_bound,fade_range);

            y1_p=y2_p;
        }

        y2_p=box_r.y2-radius;
        if(r.y2 > y1_p && y2_p > r.y1)///render middle section normally
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_fill_fading_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=y2_p}),colour,fade_bound,fade_range);

            y1_p=y2_p;
        }

        if(r.y2>box_r.y2-radius)
        {
            cvm_render_shaded_fading_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=y1_p,.x2=x2_p,.y2=r.y2}),colour,x_off+r.x1-box_r.x1,y_off+y1_p-box_r.y2+diameter,fade_bound,fade_range);
        }

        r.x1=x2_p;///only happens if less, should be fine
    }

    x2_p=box_r.x2-radius * !(box_status&WIDGET_H_LAST);
    if(r.x2 > r.x1 && x2_p > r.x1)///render middle section normally
    {
        if(x2_p>r.x2)x2_p=r.x2;

        cvm_render_fill_fading_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=x2_p,.y2=r.y2}),colour,fade_bound,fade_range);

        r.x1=x2_p;
    }

    if(r.x2>box_r.x2-radius && !(box_status&WIDGET_H_LAST))
    {
        y2_p=box_r.y1+radius;
        if(r.y1<y2_p)
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_shaded_fading_overlay_element(render_batch,bounds,(rectangle){.x1=r.x1,.y1=r.y1,.x2=r.x2,.y2=y2_p},colour,x_off+r.x1-box_r.x2+diameter,y_off+r.y1-box_r.y1,fade_bound,fade_range);

            r.y1=y2_p;
        }

        y2_p=box_r.y2-radius;
        if(r.y2 > r.y1 && y2_p > r.y1)///render middle section normally
        {
            if(y2_p>r.y2)y2_p=r.y2;

            cvm_render_fill_fading_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=r.x2,.y2=y2_p}),colour,fade_bound,fade_range);

            r.y1=y2_p;
        }

        if(r.y2>box_r.y2-radius)
        {
            cvm_render_shaded_fading_overlay_element(render_batch,bounds,r,colour,x_off+r.x1-box_r.x2+diameter,y_off+r.y1-box_r.y2+diameter,fade_bound,fade_range);
        }
    }
}


static bool cubic_square_select(overlay_theme * theme,rectangle r,uint32_t status)
{
    int i,y,x,o;
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    if(!cubic->foreground_selection_grid)return false;

    y=(cubic->foreground_d-r.y1-r.y2)>>1;
    o=(r.x2-r.x1-cubic->foreground_d)>>1;

    if(y<0 || y>=cubic->foreground_d)return false;

    if(!(status&WIDGET_H_FIRST))///not the first
    {
        x= -r.x1-o;

        if(x>=0 && x<cubic->foreground_r)
        {
            i=y*cubic->foreground_d+x;
            if(cubic->foreground_selection_grid[i>>4] & (1<<(i&0x0F))) return true;
        }

        r.x1+=o+cubic->foreground_r;
    }

    if(!(status&WIDGET_H_LAST))///not the last
    {
        x= cubic->foreground_d+o-r.x2;

        if(x>=cubic->foreground_r && x<cubic->foreground_d)
        {
            i=y*cubic->foreground_d+x;
            if(cubic->foreground_selection_grid[i>>4] & (1<<(i&0x0F))) return true;
        }

        r.x2-=o+cubic->foreground_r;
    }

    return r.x1<=0 && r.x2>0;
}

static bool cubic_h_bar_select(overlay_theme * theme,rectangle r,uint32_t status)
{
    int i,y,x;
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    if(!cubic->foreground_selection_grid)return false;

    y=(cubic->foreground_d-r.y1-r.y2)>>1;

    if(y<0 || y>=cubic->foreground_d)return false;

    if(!(status&WIDGET_H_FIRST))///not the first
    {
        x= -r.x1-cubic->foreground_offset_x;

        if(x>=0 && x<cubic->foreground_r)
        {
            i=y*cubic->foreground_d+x;
            if(cubic->foreground_selection_grid[i>>4] & (1<<(i&0x0F))) return true;
        }

        r.x1+=cubic->foreground_offset_x+cubic->foreground_r;
    }

    if(!(status&WIDGET_H_LAST))///not the last
    {
        x= cubic->foreground_d+cubic->foreground_offset_x-r.x2;

        if(x>=cubic->foreground_r && x<cubic->foreground_d)
        {
            i=y*cubic->foreground_d+x;
            if(cubic->foreground_selection_grid[i>>4] & (1<<(i&0x0F))) return true;
        }

        r.x2-=cubic->foreground_offset_x+cubic->foreground_r;
    }

    return r.x1<=0 && r.x2>0;
}

static bool cubic_box_select(overlay_theme * theme,rectangle r,uint32_t status)
{
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    uint16_t * selection_grid = cubic->foreground_selection_grid;
    int radius = cubic->foreground_r;
    int diameter = cubic->foreground_d;

    if(!selection_grid)return false;

    r.y1+=cubic->foreground_offset_y;
    r.y2-=cubic->foreground_offset_y;

    int i,y,x;

    if(!(status&WIDGET_H_FIRST))
    {
        r.x1+=cubic->foreground_offset_x;
        x= -r.x1;
        r.x1+=radius;

        if(x>=0 && x<radius)
        {
            y= -r.y1;
            if(y>=0 && y<radius)
            {
                i=y*diameter+x;
                if(selection_grid[i>>4] & (1<<(i&0x0F))) return true;
            }

            y= diameter-r.y2;
            if(y>=radius && y<diameter)
            {
                i=y*diameter+x;
                if(selection_grid[i>>4] & (1<<(i&0x0F))) return true;
            }

            return r.y1+radius<=0 && r.y2-radius>0;
        }
    }

    if(!(status&WIDGET_H_LAST))
    {
        r.x2-=cubic->foreground_offset_x;
        x= diameter-r.x2;
        r.x2-=radius;

        if(x>=radius && x<diameter)
        {
            y= -r.y1;
            if(y>=0 && y<radius)
            {
                i=y*diameter+x;
                if(selection_grid[i>>4] & (1<<(i&0x0F))) return true;
            }

            y= diameter-r.y2;
            if(y>=radius && y<diameter)
            {
                i=y*diameter+x;
                if(selection_grid[i>>4] & (1<<(i&0x0F))) return true;
            }

            return r.y1+radius<=0 && r.y2-radius>0;
        }
    }

    return rectangle_surrounds_origin(r);
}

static bool cubic_panel_select(overlay_theme * theme,rectangle r,uint32_t status)
{
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    uint16_t * selection_grid = cubic->background_selection_grid;
    int radius = cubic->background_r;
    int diameter = cubic->background_d;

    if(!selection_grid)return false;

    int i,y,x,y1,y2;

    if(!(status&WIDGET_H_FIRST))
    {
        x= -r.x1;
        y1=r.y1;
        y2=r.y2;
        r.x1+=radius;

        if(x>=0 && x<radius)
        {
            if(!(status&WIDGET_V_FIRST))
            {
                y= -y1;

                if(y>=0 && y<radius)
                {
                    i=y*diameter+x;
                    if(selection_grid[i>>4] & (1<<(i&0x0F))) return true;
                }

                y1+=radius;
            }

            if(!(status&WIDGET_V_LAST))
            {
                y= diameter-y2;

                if(y>=radius && y<diameter)
                {
                    i=y*diameter+x;
                    if(selection_grid[i>>4] & (1<<(i&0x0F))) return true;
                }

                y2-=radius;
            }

            return y1<=0 && y2>0;
        }
    }

    if(!(status&WIDGET_H_LAST))
    {
        x= diameter-r.x2;
        y1=r.y1;
        y2=r.y2;
        r.x2-=radius;

        if(x>=radius && x<diameter)
        {
            if(!(status&WIDGET_V_FIRST))
            {
                y= -y1;

                if(y>=0 && y<radius)
                {
                    i=y*diameter+x;
                    if(selection_grid[i>>4] & (1<<(i&0x0F))) return true;
                }

                y1+=radius;
            }

            if(!(status&WIDGET_V_LAST))
            {
                y= diameter-y2;

                if(y>=radius && y<diameter)
                {
                    i=y*diameter+x;
                    if(selection_grid[i>>4] & (1<<(i&0x0F))) return true;
                }

                y2-=radius;
            }

            return y1<=0 && y2>0;
        }
    }

    return rectangle_surrounds_origin(r);
}

static void cubic_square_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,uint32_t status,overlay_colour colour)
{
    cubic_theme_data * cubic=theme->other_data;

    int radius=cubic->foreground_r;

    if(!cubic->foreground_image_tile)cubic_create_shape(theme, &cubic->foreground_image_tile, &cubic->foreground_selection_grid, radius, render_batch);
    if(!cubic->foreground_image_tile)return;

    int x_off=cubic->foreground_image_tile->x_pos<<2;
    int y_off=cubic->foreground_image_tile->y_pos<<2;

    r.y1=(r.y1+r.y2-cubic->foreground_d)>>1;
    r.y2=r.y1+cubic->foreground_d;
    int m=(r.x2+r.x1)>>1;

    if(!(status&WIDGET_H_FIRST))
    {
        cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=m-radius,.y1=r.y1,.x2=m,.y2=r.y2}),colour,x_off,y_off);
        r.x1=m;
    }

    if(!(status&WIDGET_H_LAST))
    {
        cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=m,.y1=r.y1,.x2=m+radius,.y2=r.y2}),colour,x_off+radius,y_off);
        r.x2=m;
    }

    cvm_render_fill_overlay_element(render_batch,bounds,r,colour);
}

static void cubic_h_bar_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,uint32_t status,overlay_colour colour)
{
    cubic_theme_data * cubic=theme->other_data;

    int radius=cubic->foreground_r;

    if(!cubic->foreground_image_tile)cubic_create_shape(theme, &cubic->foreground_image_tile, &cubic->foreground_selection_grid, radius, render_batch);
    if(!cubic->foreground_image_tile)return;

    int x_off=cubic->foreground_image_tile->x_pos<<2;
    int y_off=cubic->foreground_image_tile->y_pos<<2;

    r.y1=(r.y1+r.y2-cubic->foreground_d)>>1;
    r.y2=r.y1+cubic->foreground_d;

    if(!(status&WIDGET_H_FIRST))
    {
        r.x1+= cubic->foreground_offset_x;
        cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=r.x1+radius,.y2=r.y2}),colour,x_off,y_off);
        r.x1+=radius;
    }

    if(!(status&WIDGET_H_LAST))
    {
        r.x2-= cubic->foreground_offset_x;
        cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=r.x2-radius,.y1=r.y1,.x2=r.x2,.y2=r.y2}),colour,x_off+radius,y_off);
        r.x2-=radius;
    }

    cvm_render_fill_overlay_element(render_batch,bounds,r,colour);
}

static rectangle cubic_get_sliderbar_offsets(overlay_theme * theme,uint32_t status)
{
    cubic_theme_data * cubic;
    cubic=theme->other_data;

    return (rectangle)
    {
        .x1=  (cubic->foreground_r + cubic->foreground_offset_x * !(status&WIDGET_H_FIRST)),
        .x2= -(cubic->foreground_r + cubic->foreground_offset_x * !(status&WIDGET_H_LAST)),
        .y1=  (cubic->foreground_r + cubic->foreground_offset_y * !(status&WIDGET_V_FIRST)),
        .y2= -(cubic->foreground_r + cubic->foreground_offset_y * !(status&WIDGET_V_LAST))
    };
}

static void cubic_h_bar_slider_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,uint32_t status,overlay_colour colour,int32_t before,int32_t bar,int32_t after)
{
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    if(!cubic->internal_image_tile)cubic_create_shape(theme, &cubic->internal_image_tile, NULL, cubic->internal_r, render_batch);
    if(!cubic->internal_image_tile)return;

    r.x1+=cubic->foreground_r+cubic->foreground_offset_x * !(status&WIDGET_H_FIRST);
    r.x2-=cubic->foreground_r+cubic->foreground_offset_x * !(status&WIDGET_H_LAST);

    r.y1=(r.y1+r.y2-cubic->internal_d)>>1;
    r.y2=r.y1+cubic->internal_d;

    int32_t range_value=before+bar+after;

    if(range_value<1)range_value=1;
    int32_t range_space=r.x2-r.x1;

    ///to maintain bar size and ensure before/after 0 alignment when needed, use the smaller of before/after to dictate bar placement (along with bar itself)
    if(before<after)
    {
        r.x1+=(range_space*before)/range_value;
        r.x2=r.x1+(range_space*bar)/range_value;
    }
    else
    {
        r.x2-=(range_space*after)/range_value;
        r.x1=r.x2-(range_space*bar)/range_value;
    }

    cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=r.x1-cubic->internal_r,.y1=r.y1,.x2=r.x1,.y2=r.y2}),colour,(cubic->internal_image_tile->x_pos<<2),(cubic->internal_image_tile->y_pos<<2));

    cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=r.x2,.y1=r.y1,.x2=r.x2+cubic->internal_r,.y2=r.y2}),colour,(cubic->internal_image_tile->x_pos<<2)+cubic->internal_r,(cubic->internal_image_tile->y_pos<<2));

    cvm_render_fill_overlay_element(render_batch,bounds,r,colour);
}

static void cubic_box_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,uint32_t status,overlay_colour colour)
{
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    int radius=cubic->foreground_r;

    if(!cubic->foreground_image_tile)cubic_create_shape(theme, &cubic->foreground_image_tile, &cubic->foreground_selection_grid, radius, render_batch);
    if(!cubic->foreground_image_tile)return;

    int x_off=cubic->foreground_image_tile->x_pos<<2;
    int y_off=cubic->foreground_image_tile->y_pos<<2;

    r.y1+=cubic->foreground_offset_y;
    r.y2-=cubic->foreground_offset_y;

    rectangle rv;

    if(!(status&WIDGET_H_FIRST))
    {
        r.x1+=cubic->foreground_offset_x;
        rv=r;
        rv.x2=r.x1+=radius;

        cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=rv.x1,.y1=rv.y1,.x2=rv.x2,.y2=rv.y1+radius}),colour,x_off,y_off);
        rv.y1+=radius;

        cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=rv.x1,.y1=rv.y2-radius,.x2=rv.x2,.y2=rv.y2}),colour,x_off,y_off+radius);
        rv.y2-=radius;

        cvm_render_fill_overlay_element(render_batch,bounds,rv,colour);
    }

    if(!(status&WIDGET_H_LAST))
    {
        r.x2-=cubic->foreground_offset_x;
        rv=r;
        rv.x1=r.x2-=radius;

        cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=rv.x1,.y1=rv.y1,.x2=rv.x2,.y2=rv.y1+radius}),colour,x_off+radius,y_off);
        rv.y1+=radius;

        cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=rv.x1,.y1=rv.y2-radius,.x2=rv.x2,.y2=rv.y2}),colour,x_off+radius,y_off+radius);
        rv.y2-=radius;

        cvm_render_fill_overlay_element(render_batch,bounds,rv,colour);
    }

    cvm_render_fill_overlay_element(render_batch,bounds,r,colour);
}

static void cubic_panel_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,uint32_t status,overlay_colour colour)
{
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    int radius=cubic->background_r;

    if(!cubic->background_image_tile)cubic_create_shape(theme, &cubic->background_image_tile, &cubic->background_selection_grid, radius, render_batch);
    if(!cubic->background_image_tile)return;

    int x_off=cubic->background_image_tile->x_pos<<2;
    int y_off=cubic->background_image_tile->y_pos<<2;

    rectangle rv;

    if(!(status&WIDGET_H_FIRST))
    {
        rv=r;
        rv.x2=r.x1+=radius;

        if(!(status&WIDGET_V_FIRST))
        {
            cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=rv.x1,.y1=rv.y1,.x2=rv.x2,.y2=rv.y1+radius}),colour,x_off,y_off);

            rv.y1+=radius;
        }

        if(!(status&WIDGET_V_LAST))
        {
            cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=rv.x1,.y1=rv.y2-radius,.x2=rv.x2,.y2=rv.y2}),colour,x_off,y_off+radius);

            rv.y2-=radius;
        }

        cvm_render_fill_overlay_element(render_batch,bounds,rv,colour);
    }

    if(!(status&WIDGET_H_LAST))
    {
        rv=r;
        rv.x1=r.x2-=radius;

        if(!(status&WIDGET_V_FIRST))
        {
            cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=rv.x1,.y1=rv.y1,.x2=rv.x2,.y2=rv.y1+radius}),colour,x_off+radius,y_off);

            rv.y1+=radius;
        }

        if(!(status&WIDGET_V_LAST))
        {
            cvm_render_shaded_overlay_element(render_batch,bounds,((rectangle){.x1=rv.x1,.y1=rv.y2-radius,.x2=rv.x2,.y2=rv.y2}),colour,x_off+radius,y_off+radius);

            rv.y2-=radius;
        }

        cvm_render_fill_overlay_element(render_batch,bounds,rv,colour);
    }

    cvm_render_fill_overlay_element(render_batch,bounds,r,colour);
}

static void cubic_square_box_constrained_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,uint32_t status,overlay_colour colour,rectangle box_r,uint32_t box_status)
{
    cubic_theme_data * cubic=theme->other_data;

    int radius=cubic->foreground_r;

    if(!cubic->foreground_image_tile)cubic_create_shape(theme, &cubic->foreground_image_tile, &cubic->foreground_selection_grid, radius, render_batch);
    if(!cubic->foreground_image_tile)return;

    int x_off=cubic->foreground_image_tile->x_pos<<2;
    int y_off=cubic->foreground_image_tile->y_pos<<2;

    r.y1=(r.y1+r.y2-cubic->foreground_d)>>1;
    r.y2=r.y1+cubic->foreground_d;
    int m=(r.x2+r.x1)>>1;

    if(!(status&WIDGET_H_FIRST))
    {
        cubic_shaded_element_box_constrained_render(render_batch,theme,bounds,((rectangle){.x1=m-radius,.y1=r.y1,.x2=m,.y2=r.y2}),colour,x_off,y_off,box_r,box_status);
        r.x1=m;
    }

    if(!(status&WIDGET_H_LAST))
    {
        cubic_shaded_element_box_constrained_render(render_batch,theme,bounds,((rectangle){.x1=m,.y1=r.y1,.x2=m+radius,.y2=r.y2}),colour,x_off+radius,y_off,box_r,box_status);
        r.x2=m;
    }

    ///assume that if widget is first/last then containing box is too, ergo no need to render edge connecting parts with box consideration
    cvm_render_fill_overlay_element(render_batch,bounds,r,colour);
}

static void cubic_h_bar_box_constrained_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,uint32_t status,overlay_colour colour,rectangle box_r,uint32_t box_status)
{
    cubic_theme_data * cubic=theme->other_data;

    int radius=cubic->foreground_r;

    if(!cubic->foreground_image_tile)cubic_create_shape(theme, &cubic->foreground_image_tile, &cubic->foreground_selection_grid, radius, render_batch);
    if(!cubic->foreground_image_tile)return;

    int x_off=cubic->foreground_image_tile->x_pos<<2;
    int y_off=cubic->foreground_image_tile->y_pos<<2;

    r.y1=(r.y1+r.y2-cubic->foreground_d)>>1;
    r.y2=r.y1+cubic->foreground_d;

    if(!(status&WIDGET_H_FIRST))
    {
        r.x1+= cubic->foreground_offset_x;
        cubic_shaded_element_box_constrained_render(render_batch,theme,bounds,((rectangle){.x1=r.x1,.y1=r.y1,.x2=r.x1+radius,.y2=r.y2}),colour,x_off,y_off,box_r,box_status);
        r.x1+=radius;
    }

    if(!(status&WIDGET_H_LAST))
    {
        r.x2-= cubic->foreground_offset_x;
        cubic_shaded_element_box_constrained_render(render_batch,theme,bounds,((rectangle){.x1=r.x2-radius,.y1=r.y1,.x2=r.x2,.y2=r.y2}),colour,x_off+radius,y_off,box_r,box_status);
        r.x2-=radius;
    }

    cubic_fill_element_box_constrained_render(render_batch,theme,bounds,r,colour,box_r,box_status);
}

static void cubic_box_box_constrained_render(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,uint32_t status,overlay_colour colour,rectangle box_r,uint32_t box_status)
{
    cubic_theme_data * cubic;

    cubic=theme->other_data;

    int radius=cubic->foreground_r;

    if(!cubic->foreground_image_tile)cubic_create_shape(theme, &cubic->foreground_image_tile, &cubic->foreground_selection_grid, radius, render_batch);
    if(!cubic->foreground_image_tile)return;

    int x_off=cubic->foreground_image_tile->x_pos<<2;
    int y_off=cubic->foreground_image_tile->y_pos<<2;

    r.y1+=cubic->foreground_offset_y;
    r.y2-=cubic->foreground_offset_y;

    rectangle rv;

    if(!(status&WIDGET_H_FIRST))
    {
        r.x1+=cubic->foreground_offset_x;
        rv=r;
        rv.x2=r.x1+=radius;

        cubic_shaded_element_box_constrained_render(render_batch,theme,bounds,((rectangle){.x1=rv.x1,.y1=rv.y1,.x2=rv.x2,.y2=rv.y1+radius}),colour,x_off,y_off,box_r,box_status);
        rv.y1+=radius;

        cubic_shaded_element_box_constrained_render(render_batch,theme,bounds,((rectangle){.x1=rv.x1,.y1=rv.y2-radius,.x2=rv.x2,.y2=rv.y2}),colour,x_off,y_off+radius,box_r,box_status);
        rv.y2-=radius;

        cubic_fill_element_box_constrained_render(render_batch,theme,bounds,rv,colour,box_r,box_status);
    }

    if(!(status&WIDGET_H_LAST))
    {
        r.x2-=cubic->foreground_offset_x;
        rv=r;
        rv.x1=r.x2-=radius;

        cubic_shaded_element_box_constrained_render(render_batch,theme,bounds,((rectangle){.x1=rv.x1,.y1=rv.y1,.x2=rv.x2,.y2=rv.y1+radius}),colour,x_off+radius,y_off,box_r,box_status);
        rv.y1+=radius;

        cubic_shaded_element_box_constrained_render(render_batch,theme,bounds,((rectangle){.x1=rv.x1,.y1=rv.y2-radius,.x2=rv.x2,.y2=rv.y2}),colour,x_off+radius,y_off+radius,box_r,box_status);
        rv.y2-=radius;

        cubic_fill_element_box_constrained_render(render_batch,theme,bounds,rv,colour,box_r,box_status);
    }

    cubic_fill_element_box_constrained_render(render_batch,theme,bounds,r,colour,box_r,box_status);
}

overlay_theme * create_cubic_theme(const FT_Library * freetype_library)
{
    overlay_theme * theme;
    cubic_theme_data * cubic;

    uint32_t foreground_radius = 10;
    uint32_t foreground_offset_x = 2;
    uint32_t foreground_offset_y = 1;
    uint32_t background_radius = 14;
    uint32_t internal_radius = 8;

    theme=malloc(sizeof(overlay_theme));
    theme->other_data=cubic=malloc(sizeof(cubic_theme_data));

    theme->base_unit_w = 2 * (foreground_radius + foreground_offset_x);
    theme->base_unit_h = 2 * (foreground_radius + foreground_offset_y);

    theme->x_panel_offset=2;
    theme->x_panel_offset_side=0;
    theme->y_panel_offset=3;
    theme->y_panel_offset_side=3;

    theme->h_bar_text_offset=16;

    theme->h_bar_icon_text_offset=12;/// (minimum) separation between icon and text sharing a bar

    #warning try only updating overlay when something in it changes!

    #warning when possible ensure offset from end of at least the fade distance
    theme->h_text_fade_range=16;///24?
    theme->v_text_fade_range=8;///8? 6? 12??

    theme->x_box_offset=16;/// for now only text offset? any other uses?
    theme->y_box_offset=3;/// ??? figure out what this should be
    #warning above have misleading names, "content offsets" instead??

    theme->icon_bar_extra_w=32;///16(icon_size)+ text_offset(16) e.g. same offset as text bar
    theme->separator_w=8;
    theme->separator_h=4;
    theme->popup_separation=8;

    theme->border_resize_selection_range=14;


    theme->base_contiguous_unit_w = 2 * foreground_radius;
    theme->base_contiguous_unit_h = 2 * foreground_radius;
    theme->contiguous_box_x_offset = foreground_offset_x;
    theme->contiguous_box_y_offset = foreground_offset_y;


    theme->contiguous_all_box_x_offset=0;/// unused??
    theme->contiguous_all_box_y_offset=1;
    theme->contiguous_some_box_x_offset=12;///pretty sure this exists to allow "clean" scrolling of partial entries in a contiguous boxes, this doesn't work for horizontal contiguous boxes!
    ///instead use fade range paradigm ?? could/should work on all entry types fill shaded &c. but doesnt allow for widget specific fade ranges ad only 1 entry remains with data for fade...
    /// fade can even be used to indicate that there are more contents to a contiguous box in a particular direction
    /// have contiguous box scroll (when appropriate) when clicked in some range of end, click range should probably match fade range
    /// need horizontal and vertical fade variants and either to actually use offset info or to allow an absolute fade point with negative values (potentially off screen)

    /// better to render non- specialised elements (foreground/background) and make efficient as possible (everything just a box) with contiguous variant?
    /// have theme based variations to size based on other factors though? sizing flags to prevent horizontal/vertical component where desired?


    theme->contiguous_some_box_y_offset=1;///possibly 1 ??
    theme->contiguous_horizintal_bar_h = 2 * foreground_radius;

    theme->square_render=cubic_square_render;
    theme->h_bar_render=cubic_h_bar_render;
    theme->h_bar_slider_render=cubic_h_bar_slider_render;
    theme->box_render=cubic_box_render;
    theme->panel_render=cubic_panel_render;

    theme->square_box_constrained_render=cubic_square_box_constrained_render;
    theme->h_bar_box_constrained_render=cubic_h_bar_box_constrained_render;
    theme->box_box_constrained_render=cubic_box_box_constrained_render;

    theme->fill_box_constrained_render=cubic_fill_element_box_constrained_render;
    theme->fill_fading_box_constrained_render=cubic_fill_element_fading_box_constrained_render;
    theme->shaded_box_constrained_render=cubic_shaded_element_box_constrained_render;
    theme->shaded_fading_box_constrained_render=cubic_shaded_element_fading_box_constrained_render;

    theme->square_select=cubic_square_select;
    theme->h_bar_select=cubic_h_bar_select;
    theme->box_select=cubic_box_select;
    theme->panel_select=cubic_panel_select;

    theme->get_sliderbar_offsets=cubic_get_sliderbar_offsets;

    cubic->foreground_image_tile=NULL;
    cubic->foreground_selection_grid=NULL;

    cubic->foreground_offset_x = foreground_offset_x;
    cubic->foreground_offset_y = foreground_offset_y;
    cubic->foreground_r = foreground_radius;
    cubic->foreground_d = 2 * foreground_radius;

    cubic->background_image_tile=NULL;
    cubic->background_selection_grid=NULL;
    cubic->background_r = background_radius;
    cubic->background_d = 2 * background_radius;

    cubic->internal_image_tile=NULL;
    cubic->internal_r = internal_radius;
    cubic->internal_d = 2 * internal_radius;

    cvm_overlay_create_font(&theme->font, freetype_library,"resources/cvm_font_1.ttf",16);

    return theme;

/// ↑←↓→ 🗙 🔄 💾 📁 📷 📄 🎵 ➕ ➖ 👻 🖋 🔓 🔗 🖼 ✂ 👁 ⚙
}

void destroy_cubic_theme(overlay_theme * theme, cvm_vk_image_atlas * backing_image_atlas)
{
    cubic_theme_data * cubic;
    cubic=theme->other_data;

    if(cubic->foreground_image_tile) cvm_vk_relinquish_image_atlas_tile(backing_image_atlas,cubic->foreground_image_tile);
    if(cubic->foreground_selection_grid) free(cubic->foreground_selection_grid);

    if(cubic->background_image_tile) cvm_vk_relinquish_image_atlas_tile(backing_image_atlas,cubic->background_image_tile);
    if(cubic->background_selection_grid) free(cubic->background_selection_grid);

    if(cubic->internal_image_tile) cvm_vk_relinquish_image_atlas_tile(backing_image_atlas,cubic->internal_image_tile);

    cvm_overlay_destroy_font(&theme->font, backing_image_atlas);

    free(theme->other_data);

    free(theme);
}

