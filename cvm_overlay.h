/**
Copyright 2020,2021,2022,2024 Carl van Mastrigt

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

#ifndef CVM_OVERLAY_H
#define CVM_OVERLAY_H

#include "vk/shunt_buffer.h"
#include "vk/staging_buffer.h"

#define CVM_OVERLAY_ELEMENT_FILL        0x00000000
#define CVM_OVERLAY_ELEMENT_SHADED      0x10000000
#define CVM_OVERLAY_ELEMENT_COLOURED    0x20000000
///having both set could signify something??
#define CVM_OVERLAY_ELEMENT_OVERLAP_MIN 0x40000000
#define CVM_OVERLAY_ELEMENT_OVERLAP_MUL 0x80000000


// struct sol_gui_object;
struct sol_gui_context;

#include "math/s16_rect.h"

typedef struct overlay_theme overlay_theme;

typedef enum
{
    OVERLAY_NO_COLOUR=0,
    OVERLAY_BACKGROUND_COLOUR,
    OVERLAY_MAIN_COLOUR,
    OVERLAY_ALTERNATE_MAIN_COLOUR,
    OVERLAY_HIGHLIGHTING_COLOUR,
//    OVERLAY_MAIN_HIGHLIGHTED_COLOUR,
//    OVERLAY_MAIN_ACTIVE_COLOUR,
//    OVERLAY_MAIN_INACTIVE_COLOUR,
//    OVERLAY_MAIN_PROMINENT_COLOUR,
//    OVERLAY_BORDER_COLOUR,


    OVERLAY_TEXT_HIGHLIGHT_COLOUR,
    OVERLAY_TEXT_COMPOSITION_COLOUR_0,
    OVERLAY_TEXT_COLOUR_0,
//    OVERLAY_TEXT_COLOUR_1,
//    OVERLAY_TEXT_COLOUR_2,
//    OVERLAY_TEXT_COLOUR_3,
//    OVERLAY_TEXT_COLOUR_4,
//    OVERLAY_TEXT_COLOUR_5,
//    OVERLAY_TEXT_COLOUR_6,
//    OVERLAY_TEXT_COLOUR_7,

//    OVERLAY_MISC_COLOUR_0,
//    OVERLAY_MISC_COLOUR_1,
//    OVERLAY_MISC_COLOUR_2,
//    OVERLAY_MISC_COLOUR_3,

    OVERLAY_NUM_COLOURS
}
overlay_colour;




typedef struct cvm_overlay_element_render_data
{
    uint16_t data0[4];///position
    uint32_t data1[2];
    uint16_t data2[4];/// overap_tex_lookup.xy , fade_off_left|fade_off_right , fade_off_top|fade_off_bot
}
cvm_overlay_element_render_data;

#define SOL_STACK_TYPE cvm_overlay_element_render_data
#define SOL_STACK_FUNCTION_PREFIX cvm_overlay_element_render_data_stack
#define SOL_STACK_STRUCT_NAME cvm_overlay_element_render_data_stack
#include "data_structures/stack.h"

struct cvm_overlay_render_batch;


#include "cvm_overlay_text.h"

struct cvm_overlay_theme_element_description
{
    /// constraint of rendering
    rectangle bounds;

    /// the region/size applicable to the element
    rectangle r;

    /// status information associated with widget being rendered
    uint32_t status;

    /// colour to use for rendering
    overlay_colour colour;

    /// sizing information associated with scroll bars
    int32_t before;
    int32_t bar;
    int32_t after;

    /// for if widget is constrained by a generic box widget
    rectangle constraining_box_r;
    uint32_t constraining_box_status;

    /// for if fading off
    /// both could probably be made much more clear..
    rectangle fade_bound;
    rectangle fade_range;
    /// ^ may want to consider moving this logic into element rendering
};


struct overlay_theme
{
    cvm_overlay_font font;

    int base_unit_w;
    int base_unit_h;

    int h_bar_text_offset;

    int h_bar_icon_text_offset;///need a variant of this for contiguous elements (h_bar_icon_text_offset = contigouus_h_bar_icon_text_offset + contiguous_box_x_offset)

    int h_text_fade_range;
    int v_text_fade_range;

    //int h_slider_bar_lost_w;///horizontal space tied up in visual elements (not part of range)
    //int v_slider_bar_lost_h;///vertical space tied up in visual elements (not part of range)
    ///int slider_bar_bar_fraction;

    /// relative positions of box contents (from rect of widget)
    int x_box_offset;
    int y_box_offset;

    int icon_bar_extra_w;///if switching from h_text_bar_render to h_icon_text_bar_render how much extra space to use
    int separator_w;
    int separator_h;
    int popup_separation;

    /// spacing between panel and sides of content
    int x_panel_offset;
    int y_panel_offset;
    /// spacing when against the edge of the screen
    int x_panel_offset_side;
    int y_panel_offset_side;

    /// range of panel deadspace near the edge that allows resizing
    int border_resize_selection_range;


    ///worth assessing whether a lack of separation is even worth it or desirable..., perhaps "contiguous" should just refer to graphical elements
    int base_contiguous_unit_w;
    int base_contiguous_unit_h;

    int contiguous_box_x_offset;
    int contiguous_box_y_offset;


    ///remove these?
    int contiguous_all_box_x_offset;
    int contiguous_all_box_y_offset;
    int contiguous_some_box_x_offset;
    int contiguous_some_box_y_offset;
    int contiguous_horizintal_bar_h;

    void * other_data;

//    void    (*square_render_)           (struct cvm_overlay_render_batch * restrict render_batch, const overlay_theme* theme, const struct cvm_overlay_theme_element_description* description);

    void    (*square_render)            (struct cvm_overlay_render_batch * restrict render_batch, overlay_theme * theme, rectangle bounds, rectangle r, uint32_t status, overlay_colour colour);
    void    (*h_bar_render)             (struct cvm_overlay_render_batch * restrict render_batch, overlay_theme * theme, rectangle bounds, rectangle r, uint32_t status, overlay_colour colour);
    void    (*h_bar_slider_render)      (struct cvm_overlay_render_batch * restrict render_batch, overlay_theme * theme, rectangle bounds, rectangle r, uint32_t status, overlay_colour colour, int32_t before, int32_t bar, int32_t after);
    void    (*h_adjactent_slider_render)(struct cvm_overlay_render_batch * restrict render_batch, overlay_theme * theme, rectangle bounds, rectangle r, uint32_t status, overlay_colour colour, int32_t before, int32_t bar, int32_t after);///usually/always tacked onto box
    void    (*v_adjactent_slider_render)(struct cvm_overlay_render_batch * restrict render_batch, overlay_theme * theme, rectangle bounds, rectangle r, uint32_t status, overlay_colour colour, int32_t before, int32_t bar, int32_t after);///usually/always tacked onto box
    void    (*box_render)               (struct cvm_overlay_render_batch * restrict render_batch, overlay_theme * theme, rectangle bounds, rectangle r, uint32_t status, overlay_colour colour);
    void    (*panel_render)             (struct cvm_overlay_render_batch * restrict render_batch, overlay_theme * theme, rectangle bounds, rectangle r, uint32_t status, overlay_colour colour);
    ///                                 (struct cvm_overlay_render_batch * restrict render_batch, overlay_theme * theme, rectangle bounds, rectangle r, uint32_t status, overlay_colour colour)

    void    (*square_box_constrained_render)(struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,uint32_t status,overlay_colour colour,rectangle box_r,uint32_t box_status);
    void    (*h_bar_box_constrained_render) (struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,uint32_t status,overlay_colour colour,rectangle box_r,uint32_t box_status);
    void    (*box_box_constrained_render)   (struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,uint32_t status,overlay_colour colour,rectangle box_r,uint32_t box_status);
    /// need h_bar_slider_over_box_render as well

    void    (*fill_box_constrained_render)          (struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,overlay_colour colour,rectangle box_r,uint32_t box_status);
    void    (*fill_fading_box_constrained_render)   (struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,overlay_colour colour,rectangle fade_bound,rectangle fade_range,rectangle box_r,uint32_t box_status);
    void    (*shaded_box_constrained_render)        (struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,overlay_colour colour,int16_t x_off,int16_t y_off,rectangle box_r,uint32_t box_status);
    void    (*shaded_fading_box_constrained_render) (struct cvm_overlay_render_batch * restrict render_batch,overlay_theme * theme,rectangle bounds,rectangle r,overlay_colour colour,int16_t x_off,int16_t y_off,rectangle fade_bound,rectangle fade_range,rectangle box_r,uint32_t box_status);

    bool    (*square_select)            (overlay_theme * theme,rectangle r,uint32_t status);
    bool    (*h_bar_select)             (overlay_theme * theme,rectangle r,uint32_t status);
    bool    (*box_select)               (overlay_theme * theme,rectangle r,uint32_t status);
    bool    (*panel_select)             (overlay_theme * theme,rectangle r,uint32_t status);

    rectangle   (*get_sliderbar_offsets)(overlay_theme * theme,uint32_t status);///returns offsets from each respective side
};













struct cvm_overlay_image_atlases
{
    struct sol_vk_image alpha_image;
    cvm_vk_image_atlas alpha_atlas;

    struct sol_vk_image colour_image;
    cvm_vk_image_atlas colour_atlas;
};

VkResult cvm_overlay_image_atlases_initialise(struct cvm_overlay_image_atlases* image_atlases, const struct cvm_vk_device* device, uint32_t alpha_w, uint32_t alpha_h, uint32_t colour_w, uint32_t colour_h, bool multithreaded);
void cvm_overlay_image_atlases_terminate(struct cvm_overlay_image_atlases* image_atlases, const struct cvm_vk_device* device);






/// static resources? base resources?
struct cvm_overlay_rendering_resources
{
    VkDescriptorSetLayout descriptor_set_layout;

    VkDescriptorPool descriptor_pool;

    /// for creating pipelines
    VkPipelineLayout pipeline_layout;
    VkPipelineShaderStageCreateInfo vertex_pipeline_stage;
    VkPipelineShaderStageCreateInfo fragment_pipeline_stage;
};

struct cvm_overlay_pipeline
{
    VkPipeline pipeline;
    VkExtent2D extent;
};

/// data to be passed form setup stage to rendering batch/packet
/// used both in setup/prep and actual_render functions
///     ^ actually... aside from the push constants (which COULD be moved to uniform) this is all only used by the prep stage!


struct cvm_overlay_render_batch
{
    /// preparation resources
    struct cvm_overlay_element_render_data_stack render_elements;// essentailly overlay element instances

    struct sol_vk_shunt_buffer upload_shunt_buffer;// used for putting data in atlases


        // information regarding current rendering stack position (as part of compositing), this will probably need to be handled better
        s16_rect current_render_bounds;



    /// the atlases can be used for other purposes, but the shunt buffer and copy list must be kept in sync (ergo them going together here)

    /// need a good way to track use of tiles in atlas, can have usage mask (u8/u16) that can be reset en-masse?
    ///     ^ or perhaps a linked list per usage frame (max N) that gets shuffled off all together to the LL of removable items
    /// atlases are unowned and merely passed by reference
    cvm_vk_image_atlas* colour_atlas;// -- unowned
    cvm_vk_image_atlas* alpha_atlas;// -- unowned
    /// separation of uploads and the image atlas itself in this way allows frames to be set up in advance (multiple render batches in flight)
    /// i.e. not having the copy stack be part of the image atlas is a net positive

    /// copies to perform from shunt buffer to the atlases
    struct cvm_vk_buffer_image_copy_stack alpha_atlas_copy_actions;
    struct cvm_vk_buffer_image_copy_stack colour_atlas_copy_actions;


    /// following are copies or transient data used only within the stages of overlay batch rendering

    struct sol_vk_staging_buffer_allocation staging_buffer_allocation;
    struct sol_vk_staging_buffer* staging_buffer;/** staging buffer from which the above allocation was acquired, used to simplify interface */

    /// also record staging buffer itself here?
    VkDeviceSize element_offset;
    VkDeviceSize upload_offset;

    /// set and then used in rendering, copied here just to prevent passing it around
    VkDescriptorSet descriptor_set;// -- unowned

    // only used for primary rendering? also need one for intermediaries(? or only allow single intermediary?)
    VkExtent2D final_target_extent;
};

/// this could reasonably also create the descriptor sets which could then simply be accessed/acquired later
VkResult cvm_overlay_rendering_resources_initialise(struct cvm_overlay_rendering_resources* rendering_resources, const struct cvm_vk_device* device, uint32_t active_render_count);
void cvm_overlay_rendering_resources_terminate (struct cvm_overlay_rendering_resources* rendering_resources, const struct cvm_vk_device* device);

VkResult cvm_overlay_descriptor_set_fetch(const struct cvm_vk_device* device, const struct cvm_overlay_rendering_resources* rendering_resources, VkDescriptorSet* set);
/// not sure if we should be releasing as its not strictly necessary

VkResult cvm_overlay_render_pipeline_initialise(struct cvm_overlay_pipeline* pipeline, const struct cvm_vk_device* device, const struct cvm_overlay_rendering_resources* rendering_resources, VkRenderPass render_pass, VkExtent2D extent, uint32_t subpass);
void cvm_overlay_render_pipeline_terminate(struct cvm_overlay_pipeline* pipeline, const struct cvm_vk_device* device);


void cvm_overlay_render_batch_initialise(struct cvm_overlay_render_batch* batch, const struct cvm_vk_device* device, VkDeviceSize shunt_buffer_max_size);
void cvm_overlay_render_batch_terminate(struct cvm_overlay_render_batch* batch);


/// there are 6 stages to rendering the overlay, they must be externally synchronised both on cpu and GPU (submitting in order to the same command buffer is enough to ensure GPU ordering)



/// `root_widget` must have been organised for an `extent` the same as the render_pass & pipeline this batch will be used with
/// we should know resolution of presentable image/target/viewport of pipeline to be used here, and organise menu widget accordingly
void cvm_overlay_render_batch_build(struct cvm_overlay_render_batch* batch, struct sol_gui_context* gui_context, struct cvm_overlay_image_atlases* image_atlases, VkExtent2D target_extent);

/// `descriptor_set` must have been fetched with `cvm_overlay_descriptor_set_fetch`
#warning these need better names, "stage" implies its the batching stage, instead:  `render_batch_step_<step_name>` , this being render_batch_step_element_staging, step_image_upload, image_transition,
void cvm_overlay_render_batch_stage(struct cvm_overlay_render_batch* batch, const struct cvm_vk_device* device, struct sol_vk_staging_buffer* staging_buffer, const float* colour_array, VkDescriptorSet descriptor_set);

/// copies staged data into atlases used by rendering
void cvm_overlay_render_batch_upload(struct cvm_overlay_render_batch* batch, VkCommandBuffer command_buffer);

/// transitions the rendering resources so must be called after all changes to image atlases (for example if some parts of the atlas are rendered to)
void cvm_overlay_render_batch_ready(struct cvm_overlay_render_batch* batch, VkCommandBuffer command_buffer);

/// `pipeline` must have been created with `cvm_overlay_render_pipeline_create` and be called inside the render pass and subpass used to create it, must also have the same extent as what was passed to batch_build
void cvm_overlay_render_batch_render(struct cvm_overlay_render_batch* batch, struct cvm_overlay_rendering_resources* rendering_resources, struct cvm_overlay_pipeline* pipeline, VkCommandBuffer command_buffer);

/// used to schedule the release of resources used/held by this batch, must ensure completion moment occurs after `render` has completed
void cvm_overlay_render_batch_finish(struct cvm_overlay_render_batch* batch, struct sol_vk_timeline_semaphore_moment completion_moment);

/// stage, upload and ready could reasonably be combined if all the contents of the atlases are uploaded rather than rendered













/// overlay render helper functions follow, move them to a helper h file...
#warning move these to a helper (.h) file


/// x/y_off are the texture space coordinates to read data from at position r, i.e. at r the texture coordinates looked up would be x_off,y_off
static inline void cvm_render_shaded_overlay_element(struct cvm_overlay_render_batch * restrict render_batch,rectangle b,rectangle r,overlay_colour colour,int16_t x_off,int16_t y_off)
{
    cvm_overlay_element_render_data* render_data;
    
    b=get_rectangle_overlap(r,b);

    if(rectangle_has_positive_area(b))
    {
        render_data = cvm_overlay_element_render_data_stack_append_ptr(&render_batch->render_elements);
        *render_data =(cvm_overlay_element_render_data)
        {
            {b.x1,b.y1,b.x2,b.y2},
            {CVM_OVERLAY_ELEMENT_SHADED,colour<<24},
            {b.x1-r.x1+x_off,b.y1-r.y1+y_off,0,0}
        };
    }
}

static inline void cvm_render_shaded_fading_overlay_element(struct cvm_overlay_render_batch * restrict render_batch,rectangle b,rectangle r,overlay_colour colour,int x_off,int y_off,rectangle fade_bound,rectangle fade_range)
{
    cvm_overlay_element_render_data* render_data;

    if(r.x1<fade_bound.x1)///beyond this opacity is 0 (completely transparent)
    {
        x_off+=fade_bound.x1-r.x1;
        r.x1=fade_bound.x1;
    }
    fade_bound.x1=r.x1-fade_bound.x1;///convert bound to distance from side
    if(fade_bound.x1>fade_range.x1) fade_bound.x1=fade_range.x1=0;

    if(r.x2>fade_bound.x2) r.x2=fade_bound.x2;
    fade_bound.x2=fade_bound.x2-r.x2;///convert bound to distance from side
    if(fade_bound.x2>fade_range.x2) fade_bound.x2=fade_range.x2=0;


    if(r.y1<fade_bound.y1)
    {
        y_off+=fade_bound.y1-r.y1;
        r.y1=fade_bound.y1;
    }
    fade_bound.y1=r.y1-fade_bound.y1;///convert bound to distance from side
    if(fade_bound.y1>fade_range.y1) fade_bound.y1=fade_range.y1=0;

    if(r.y2>fade_bound.y2)r.y2=fade_bound.y2;///beyond this opacity is 0 (completely transparent)
    fade_bound.y2=fade_bound.y2-r.y2;///convert bound to distance from side
    if(fade_bound.y2>fade_range.y2) fade_bound.y2=fade_range.y2=0;


    b=get_rectangle_overlap(r,b);

    if(rectangle_has_positive_area(b))
    {
        render_data = cvm_overlay_element_render_data_stack_append_ptr(&render_batch->render_elements);
        *render_data =(cvm_overlay_element_render_data)
        {
            {b.x1,b.y1,b.x2,b.y2},
            {CVM_OVERLAY_ELEMENT_SHADED | fade_bound.x1<<18 | fade_bound.y1<<12 | fade_bound.x2<<6 | fade_bound.y2 , colour<<24 | fade_range.x1<<18 | fade_range.y1<<12 | fade_range.x2<<6 | fade_range.y2},
            {b.x1-r.x1+x_off,b.y1-r.y1+y_off,0,0}
        };
    }
}

/// x/y_over_b equates to combination of, screen space coordinates of base of "overlap" element (negative) with texture coordinates of the tile the "overlap" element uses
static inline void cvm_render_shaded_overlap_min_overlay_element(struct cvm_overlay_render_batch * restrict render_batch,rectangle b,rectangle r,overlay_colour colour,int x_off,int y_off,int x_over_b,int y_over_b)
{
    cvm_overlay_element_render_data* render_data;

    b=get_rectangle_overlap(r,b);

    if(rectangle_has_positive_area(b))
    {
        render_data = cvm_overlay_element_render_data_stack_append_ptr(&render_batch->render_elements);
        *render_data =(cvm_overlay_element_render_data)
        {
            {b.x1,b.y1,b.x2,b.y2},
            {CVM_OVERLAY_ELEMENT_SHADED|CVM_OVERLAY_ELEMENT_OVERLAP_MIN,colour<<24},
            {b.x1-r.x1+x_off,b.y1-r.y1+y_off , b.x1+x_over_b,b.y1+y_over_b}
        };
    }
}

static inline void cvm_render_shaded_fading_overlap_min_overlay_element(struct cvm_overlay_render_batch * restrict render_batch,rectangle b,rectangle r,overlay_colour colour,int x_off,int y_off,
                                                                        rectangle fade_bound,rectangle fade_range,int x_over_b,int y_over_b)
{
    cvm_overlay_element_render_data* render_data;

    if(r.x1<fade_bound.x1)///beyond this opacity is 0 (completely transparent)
    {
        x_off+=fade_bound.x1-r.x1;
        r.x1=fade_bound.x1;
    }
    fade_bound.x1=r.x1-fade_bound.x1;///convert bound to distance from side
    if(fade_bound.x1>fade_range.x1) fade_bound.x1=fade_range.x1=0;

    if(r.x2>fade_bound.x2) r.x2=fade_bound.x2;
    fade_bound.x2=fade_bound.x2-r.x2;///convert bound to distance from side
    if(fade_bound.x2>fade_range.x2) fade_bound.x2=fade_range.x2=0;


    if(r.y1<fade_bound.y1)
    {
        y_off+=fade_bound.y1-r.y1;
        r.y1=fade_bound.y1;
    }
    fade_bound.y1=r.y1-fade_bound.y1;///convert bound to distance from side
    if(fade_bound.y1>fade_range.y1) fade_bound.y1=fade_range.y1=0;

    if(r.y2>fade_bound.y2)r.y2=fade_bound.y2;///beyond this opacity is 0 (completely transparent)
    fade_bound.y2=fade_bound.y2-r.y2;///convert bound to distance from side
    if(fade_bound.y2>fade_range.y2) fade_bound.y2=fade_range.y2=0;


    b=get_rectangle_overlap(r,b);

    if(rectangle_has_positive_area(b))
    {
        render_data = cvm_overlay_element_render_data_stack_append_ptr(&render_batch->render_elements);
        *render_data =(cvm_overlay_element_render_data)
        {
            {b.x1,b.y1,b.x2,b.y2},
            {CVM_OVERLAY_ELEMENT_SHADED|CVM_OVERLAY_ELEMENT_OVERLAP_MIN | fade_bound.x1<<18 | fade_bound.y1<<12 | fade_bound.x2<<6 | fade_bound.y2 , colour<<24 | fade_range.x1<<18 | fade_range.y1<<12 | fade_range.x2<<6 | fade_range.y2},
            {b.x1-r.x1+x_off,b.y1-r.y1+y_off , b.x1+x_over_b,b.y1+y_over_b}
        };
    }
}

static inline void cvm_render_fill_overlay_element(struct cvm_overlay_render_batch * restrict render_batch,rectangle b,rectangle r,overlay_colour colour)
{
    cvm_overlay_element_render_data* render_data;

    b=get_rectangle_overlap(r,b);

    if(rectangle_has_positive_area(b))
    {
        render_data = cvm_overlay_element_render_data_stack_append_ptr(&render_batch->render_elements);
        *render_data =(cvm_overlay_element_render_data)
        {
            {b.x1,b.y1,b.x2,b.y2},
            {CVM_OVERLAY_ELEMENT_FILL,colour<<24},
            {0,0,0,0}
        };
    }
}

static inline void cvm_render_fill_fading_overlay_element(struct cvm_overlay_render_batch * restrict render_batch,rectangle b,rectangle r,overlay_colour colour,rectangle fade_bound,rectangle fade_range)
{
    cvm_overlay_element_render_data* render_data;

    if(r.x1<fade_bound.x1)r.x1=fade_bound.x1;///beyond this opacity is 0 (completely transparent)
    fade_bound.x1=r.x1-fade_bound.x1;///convert bound to distance from side
    if(fade_bound.x1>fade_range.x1) fade_bound.x1=fade_range.x1=0;

    if(r.x2>fade_bound.x2) r.x2=fade_bound.x2;
    fade_bound.x2=fade_bound.x2-r.x2;///convert bound to distance from side
    if(fade_bound.x2>fade_range.x2) fade_bound.x2=fade_range.x2=0;


    if(r.y1<fade_bound.y1) r.y1=fade_bound.y1;
    fade_bound.y1=r.y1-fade_bound.y1;///convert bound to distance from side
    if(fade_bound.y1>fade_range.y1) fade_bound.y1=fade_range.y1=0;

    if(r.y2>fade_bound.y2)r.y2=fade_bound.y2;///beyond this opacity is 0 (completely transparent)
    fade_bound.y2=fade_bound.y2-r.y2;///convert bound to distance from side
    if(fade_bound.y2>fade_range.y2) fade_bound.y2=fade_range.y2=0;


    b=get_rectangle_overlap(r,b);

    if(rectangle_has_positive_area(b))
    {
        render_data = cvm_overlay_element_render_data_stack_append_ptr(&render_batch->render_elements);
        *render_data =(cvm_overlay_element_render_data)
        {
            {b.x1,b.y1,b.x2,b.y2},
            {CVM_OVERLAY_ELEMENT_FILL | fade_bound.x1<<18 | fade_bound.y1<<12 | fade_bound.x2<<6 | fade_bound.y2 , colour<<24 | fade_range.x1<<18 | fade_range.y1<<12 | fade_range.x2<<6 | fade_range.y2},
            {0,0,0,0}
        };
    }
}

#include "themes/cubic.h"


#endif


