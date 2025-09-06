/**
Copyright 2025 Carl van Mastrigt

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





#include <assert.h>
#include <stdlib.h>
#include <threads.h>

#include <inttypes.h>

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include <freetype/ftbbox.h>

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>



#include "data_structures/available_indices_stack.h"

#include "overlay/render.h"



// #include <unicode/ubidi.h>
// #include <unicode/ubrk.h>

// #include <fribidi/fribidi.h>

// #include <unicode/ubrk.h>
#warning "need" ubrk to handle multiline and "ctrl" behaviour


#include "sol_font.h"

// #include "sol_font_internals.h"





struct sol_font_library
{
	FT_Library freetype_library;
	// add pointer to image atlas here?
	uint32_t vended_font_count;// identifier passed into hash map
	struct sol_available_indices_stack available_font_indices;
	hb_segment_properties_t default_properties;
	#warning really not happy with the management of above

	mtx_t mutex;
	hb_buffer_t** buffers;
	uint32_t buffer_count;
	uint32_t buffer_space;
};


/** this is used to access a thread local singleton while recording it for later destruction
 * best to try and ensure properties that are the same backing memory (have the same pointer)
 * passing NULL properties promises the properties will be set to something sensible externally */
hb_buffer_t* sol_font_get_hb_buffer(struct sol_font_library* library)
{
	static thread_local hb_buffer_t* buffer = NULL;

	if(buffer == NULL)
	{
		buffer = hb_buffer_create();
		hb_buffer_set_content_type(buffer, HB_BUFFER_CONTENT_TYPE_UNICODE);

		mtx_lock(&library->mutex);
		if(library->buffer_count == library->buffer_space)
		{
			library->buffer_space += 16;
			library->buffers = realloc(library->buffers, sizeof(hb_buffer_t*) * library->buffer_space);
		}
		library->buffers[library->buffer_count++] = buffer;
		mtx_unlock(&library->mutex);
	}
	return buffer;
}

/** this should be subtracted from any offset value before being used */

#define SOL_FONT_GLYPH_INDEX_MAX 65535u
#define SOL_FONT_GLYPH_OFFSET_BIAS 511
#define SOL_FONT_GLYPH_OFFSET_MAX 1023



#warning NO, this is not enough, also want subpixel positioning!
struct sol_font_glyph_map_entry
{
	// pack it in based on max unicode encoding possible, then distribute this space amongst the following
	/** this is the packing of glyph_id and subpixel_advance : `glyph_id | (subpixel_advance << 16)`
	 * glyph_id is the glyth index as returned by harfbuzz, it should be less than 2^16 based on opentype and freetype file formats
	 * for rendering glyphs with better positioning, allow subpixel offset along the advance direction (horizontal OR vertical)
	 * note: this must be considered in key for image atlas lookup (i.e. render glyph at subpixel offset)
	 * because there are 64 subpixel positions possible: 0:[-7 -> 8] 1:[9 -> 24] 2:[25 -> 40] 3:[41 -> 56] to correctly round */
	uint32_t key : 19;

	/** which image atlas to use, may want to consider expanding this in future */
	uint32_t atlas_type : 3;

	/** the positional information for the glyph, derived from the bounding box as returned by freetype
	 * the offsets should have -511 applied to account for negative offsets */
	uint32_t size_x   : 10;
	uint32_t size_y   : 10;
	uint32_t offset_x : 10;
	uint32_t offset_y : 10;

	/** identifier for the location in the image atlas where the pixel data for this glyph is stored */
	uint64_t id_in_atlas;
};


static inline uint64_t sol_font_glyph_hash(uint32_t key)
{
	return (uint64_t)key * 0x5851F42D4C957F2D + 0x1337A5511CCE25;
}

/** note: key will be storable in 16 bits despite using u64, which is for packing type compatibility resons */
#define SOL_HASH_MAP_STRUCT_NAME sol_font_glyph_map
#define SOL_HASH_MAP_FUNCTION_PREFIX sol_font_glyph_map
#define SOL_HASH_MAP_KEY_TYPE uint32_t
#define SOL_HASH_MAP_ENTRY_TYPE struct sol_font_glyph_map_entry
#define SOL_HASH_MAP_FUNCTION_KEYWORDS static
#define SOL_HASH_MAP_KEY_ENTRY_CMP_EQUAL(K,E) ((K) == (E->key))
#define SOL_HASH_MAP_KEY_FROM_ENTRY(E) (E->key)
#define SOL_HASH_MAP_KEY_HASH(K) (sol_font_glyph_hash(K))
#define SOL_HASH_MAP_ENTRY_HASH(E) (sol_font_glyph_hash(E->key))
#include "data_structures/hash_map_implement.h"





struct sol_font
{
	struct sol_font_library* parent_library;
	// could avoid overlap and have these be ref-counted...

	/** freetype components, used for rendering */
    FT_Face face;

	hb_font_t* font;

    int16_t glyph_size;
    int16_t orthogonal_subpixel_offset;
    int16_t baseline_offset;
    int16_t normalised_orthogonal_size;

    int16_t line_spacing;// vertical advance?

    int16_t max_advance;


    struct sol_font_glyph_map glyph_map;



    // ///not a great solution but bubble sort new glyphs from end?, is rare enough op and gives low enough cache use to justify?
    // cvm_overlay_glyph * glyphs;
    // uint32_t glyph_count;
    // uint32_t glyph_space;
};








struct sol_font_library* sol_font_library_create(const char* default_script_id, const char* default_language_id, const char* default_direction_id)
{
	int freetype_error;
	struct sol_font_library* font_library;

	hb_language_t language;
	hb_script_t script;
	hb_direction_t direction;

	font_library = malloc(sizeof(struct sol_font_library));
	freetype_error = FT_Init_FreeType(&font_library->freetype_library);
	/** conside `FT_New_Library` with an allocator... */
    assert(!freetype_error);

    mtx_init(&font_library->mutex, mtx_plain);
    font_library->buffers = NULL;
    font_library->buffer_count = 0;
    font_library->buffer_space = 0;


	language = default_language_id ? hb_language_from_string(default_language_id, -1) : HB_LANGUAGE_INVALID;
	language = language == HB_LANGUAGE_INVALID ? hb_language_from_string("en", -1) : language;

	script = default_script_id ? hb_script_from_string(default_script_id, -1) : HB_SCRIPT_INVALID;
	script = script == HB_SCRIPT_INVALID ? HB_SCRIPT_LATIN : script;

	direction = default_direction_id ? hb_direction_from_string(default_direction_id, -1) : HB_DIRECTION_INVALID;
	direction = direction == HB_DIRECTION_INVALID ? HB_DIRECTION_LTR : direction;

	font_library->default_properties = (hb_segment_properties_t)
	{
		.language = language,
		.script = script,
		.direction = direction,
	};

    return font_library;
}

void sol_font_library_destroy(struct sol_font_library* font_library)
{
	uint32_t i;
	int freetype_error;

	for(i=0;i<font_library->buffer_count;i++)
	{
		hb_buffer_destroy(font_library->buffers[i]);
	}
	if(font_library->buffer_space)
	{
		free(font_library->buffers);
	}


	freetype_error = FT_Done_FreeType(font_library->freetype_library);
    assert(!freetype_error);

	free(font_library);
}




struct sol_font* sol_font_create(struct sol_font_library* font_library, const char* ttf_filename, int pixel_size)
{
	struct sol_font* font = malloc(sizeof(struct sol_font));
	int error;
	int16_t ascender, descender;

	assert(ttf_filename);


	font->font = NULL;

	font->glyph_size = pixel_size;

	font->parent_library = font_library;


	error = FT_New_Face(font_library->freetype_library, ttf_filename, 0, &font->face);
	if(error)
	{
		free(font);
		return NULL;
	}

	error = FT_Set_Pixel_Sizes(font->face, 0, pixel_size);
	if(error)
	{
		sol_font_destroy(font);
		return NULL;
	}

	font->font = hb_ft_font_create_referenced(font->face);
	if(font->font == NULL)
	{
		sol_font_destroy(font);
		return NULL;
	}

#warning assumes horizontal text
	ascender = font->face->size->metrics.ascender;
	descender = font->face->size->metrics.descender;

	printf("a:%d d:%d\n",ascender,descender);

	font->orthogonal_subpixel_offset = ascender > 0 ? ascender & 63 : -(-ascender & 63);
	font->baseline_offset = ascender >> 6;
	font->normalised_orthogonal_size = (ascender - descender + 31) >> 6;

	printf("vert subpixel offset %d\n",font->orthogonal_subpixel_offset);

	// uint32_t a = font->face->ascender+31;
	// int32_t d = font->face->descender;
	// printf("bbox ymax %d\n", font->face->bbox.yMax>>6);
	// printf("bbox ymin %d\n", font->face->bbox.yMin>>6);
	// printf("ascent %d.%d\n", (a)>>6, a&63);
	// printf("descent %d. %d\n", d>>6, d&63);
	printf("height %d\n", font->normalised_orthogonal_size);

    font->max_advance   = font->face->size->metrics.max_advance >> 6;
    font->line_spacing  = font->face->size->metrics.height >> 6;


	printf("line spacing %d\n", font->line_spacing );

	struct sol_hash_map_descriptor map_descriptor =
	{
		.entry_space_exponent_initial = 11,
		.entry_space_exponent_limit = 17,
		.resize_fill_factor = 160,
		.limit_fill_factor = 192,
	};

	sol_font_glyph_map_initialise(&font->glyph_map, map_descriptor);

	return font;
}

void sol_font_destroy(struct sol_font* font)
{
	sol_font_glyph_map_terminate(&font->glyph_map);

	FT_Done_Face(font->face);

	if(font->font)
	{
		hb_font_destroy(font->font);
	}

	free(font);
}

#warning might be able to use sdl key input to determine/separate chunks of text by language!? if true this is amazing (this is good news)
#warning  NOPE ^ you cannot, need to re-assess each break (ubrk) on "dynamic" UI strings (user input, filename &c.)


#warning need a colour
static inline void sol_font_render_overlay_text_segment_ltr(hb_buffer_t* buffer, struct sol_font* font, enum sol_overlay_colour colour, s16_vec2 base_offset, struct sol_overlay_render_batch* render_batch)
{
	struct sol_font_glyph_map_entry* glyph_map_entry;
	hb_glyph_info_t* glyph_info;
	hb_glyph_position_t* glyph_positions;
	struct sol_image_atlas* image_atlas;
	uint32_t i, glyph_count, glyph_codepoint, glyph_key;
	enum sol_map_result glyph_obtain_result;
	enum sol_image_atlas_result atlas_obtain_result;
	struct sol_image_atlas_location glyph_atlas_location;
	struct sol_buffer_allocation pixel_upload_allocation;
	struct sol_overlay_render_element* render_data;
	int ft_result;
	FT_GlyphSlot glyph_slot;
	bool glyph_requires_load;
	int32_t offset_x, offset_y, subpixel_offset, subpixel_position, cursor_x, cursor_y;
	u16_vec2 glyph_size;
	unsigned char* pixels_dst;
	unsigned char* pixels_src;
	uint16_t row;
	int src_pitch;



	// u16_vec2 base_offset = u16_vec2_set(16, 16);// input value?

	glyph_info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
	glyph_positions = hb_buffer_get_glyph_positions(buffer, &i);

	/** glyph count should match between positioning and rendering */
	assert(i == glyph_count);

	cursor_x = 0;
	cursor_y = 0;

	for(i = 0; i < glyph_count; i++)
	{
		glyph_requires_load = true;
		glyph_codepoint = glyph_info[i].codepoint;

		/** -7 to 8 is "subpixel offset" of 0 and this increases with each 16 point step (4 total, 0-3), must multiply this by 16 when rendering*/

		/** this is segment that assumes LTR font */
		{
			subpixel_position = cursor_x + glyph_positions[i].x_offset + 3;
			subpixel_offset = (subpixel_position & 63) >> 3;
			offset_x = subpixel_position >> 6;
			offset_y = (cursor_y + glyph_positions[i].y_offset) >> 6;
		}

		#warning need to know if this glyph is a bitmap (somehow) before attempting to load with a given subpixel offset!
		cursor_x += glyph_positions[i].x_advance;
		cursor_y += glyph_positions[i].y_advance;


		/** obtain the glyph render data (glyph_map_entry) */

		assert(glyph_codepoint <= SOL_FONT_GLYPH_INDEX_MAX);
		assert(subpixel_offset < 8);


		glyph_key = glyph_codepoint | (subpixel_offset << 16);

		glyph_obtain_result = sol_font_glyph_map_obtain(&font->glyph_map, glyph_key, &glyph_map_entry);

		switch (glyph_obtain_result)
		{
		case SOL_MAP_SUCCESS_INSERTED:
			/** populate `glyph_map_entry` which requires rendering */

			/** TODO: determine image_atlas_type_index (probably just R8 lol, but may wish to support colour for emoji...) */

			glyph_requires_load = false;
			ft_result = FT_Load_Glyph(font->face, glyph_codepoint, 0);
			assert(ft_result == 0);

			glyph_slot = font->face->glyph;
			/** this is segment that assumes LTR font */
			{
				assert(glyph_slot->format == FT_GLYPH_FORMAT_OUTLINE);
				FT_Outline_Translate(&glyph_slot->outline, subpixel_offset << 3, font->orthogonal_subpixel_offset);
			}

			FT_Render_Glyph(glyph_slot, FT_RENDER_MODE_NORMAL);

			assert(glyph_slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
			assert(glyph_slot->bitmap.num_grays == 256);

			image_atlas = render_batch->rendering_resources->atlases[SOL_OVERLAY_IMAGE_ATLAS_TYPE_R8_UNORM];

			assert(glyph_slot->bitmap_left >= -SOL_FONT_GLYPH_OFFSET_BIAS);
			assert(glyph_slot->bitmap_top  >= -SOL_FONT_GLYPH_OFFSET_BIAS);
			assert(glyph_slot->bitmap_left + SOL_FONT_GLYPH_OFFSET_BIAS <= SOL_FONT_GLYPH_OFFSET_MAX);
			assert(glyph_slot->bitmap_top  + SOL_FONT_GLYPH_OFFSET_BIAS <= SOL_FONT_GLYPH_OFFSET_MAX);

			if(glyph_slot->bitmap.width > 0 && glyph_slot->bitmap.rows > 0)
			{
				*glyph_map_entry = (struct sol_font_glyph_map_entry)
				{
					.key = glyph_key,
					/** NOTE: using R8 here is a temporary measure and should be evaluated properly */
					.atlas_type = SOL_OVERLAY_IMAGE_ATLAS_TYPE_R8_UNORM,
					.offset_x = glyph_slot->bitmap_left + SOL_FONT_GLYPH_OFFSET_BIAS,
					.offset_y = glyph_slot->bitmap_top  + SOL_FONT_GLYPH_OFFSET_BIAS,
					.size_x = glyph_slot->bitmap.width,
					.size_y = glyph_slot->bitmap.rows,
					.id_in_atlas = sol_image_atlas_acquire_entry_identifier(image_atlas, false),
				};
			}
			else
			{
				*glyph_map_entry = (struct sol_font_glyph_map_entry)
				{
					.key = glyph_key,
					.id_in_atlas = 0,
				};
			}

			/** note: intentional fallthrough */
		case SOL_MAP_SUCCESS_FOUND:
			/** render (structured to put this outside switch for clarity) */
			break;

		default:
			fprintf(stderr, "unexpected map result (%d)", glyph_obtain_result);
			continue;
		}

		/** this indicates an empty glyph, so don't render it */
		if(glyph_map_entry->id_in_atlas == 0)
		{
			continue;
		}

		/** render the glyph */

		glyph_size = u16_vec2_set(glyph_map_entry->size_x, glyph_map_entry->size_y);
		image_atlas = render_batch->rendering_resources->atlases[glyph_map_entry->atlas_type];

		atlas_obtain_result = sol_image_atlas_entry_obtain(image_atlas, glyph_map_entry->id_in_atlas, glyph_size, SOL_IMAGE_ATLAS_OBTAIN_FLAG_UPLOAD, &glyph_atlas_location, &pixel_upload_allocation);

		switch (atlas_obtain_result)
		{
		case SOL_IMAGE_ATLAS_SUCCESS_INSERTED:
			/** get glyph pixels if not present in image atlas */
			if(glyph_requires_load)
			{
				ft_result = FT_Load_Glyph(font->face, glyph_codepoint, 0);
				assert(ft_result == 0);

				glyph_slot = font->face->glyph;
				/** this is segment that assumes LTR font */
				{
					assert(glyph_slot->format == FT_GLYPH_FORMAT_OUTLINE);
					FT_Outline_Translate(&glyph_slot->outline, subpixel_offset << 3, font->orthogonal_subpixel_offset);
				}

				FT_Render_Glyph(glyph_slot, FT_RENDER_MODE_NORMAL);

				assert(glyph_map_entry->offset_x == glyph_slot->bitmap_left - SOL_FONT_GLYPH_OFFSET_BIAS);
				assert(glyph_map_entry->offset_y == glyph_slot->bitmap_top  - SOL_FONT_GLYPH_OFFSET_BIAS);
				assert(glyph_map_entry->size_x == glyph_slot->bitmap.width);
				assert(glyph_map_entry->size_y == glyph_slot->bitmap.rows );
				assert(glyph_slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
				assert(glyph_slot->bitmap.num_grays  == 256);
			}
			pixels_src = (unsigned char*)glyph_slot->bitmap.buffer;
			src_pitch = glyph_slot->bitmap.pitch;

			/** R8 */
			#warning am assuming R8 here...
			pixels_dst = pixel_upload_allocation.allocation;
			assert(pixel_upload_allocation.size == sizeof(unsigned char) * glyph_size.x * glyph_size.y);

			for(row = 0; row < glyph_size.y; row++)
			{
				memcpy(pixels_dst, pixels_src, sizeof(unsigned char) * glyph_size.x);
				pixels_dst += glyph_size.x;
				pixels_src += src_pitch;
			}

			/** note: intentional fallthrough */
		case SOL_IMAGE_ATLAS_SUCCESS_FOUND:

			offset_x += base_offset.x + (glyph_map_entry->offset_x - SOL_FONT_GLYPH_OFFSET_BIAS);
			offset_y += base_offset.y - (glyph_map_entry->offset_y - SOL_FONT_GLYPH_OFFSET_BIAS);

			assert(glyph_map_entry->atlas_type == SOL_OVERLAY_IMAGE_ATLAS_TYPE_R8_UNORM);


			#warning clean this shit up (give it a general implementation!??) needs to clamp these values to the rect also!
			render_data = sol_overlay_render_element_list_append_ptr(&render_batch->elements);
			assert((uint16_t)colour < 256);
			uint16_t array_layer_and_colour = colour | (glyph_atlas_location.array_layer << 8);
			uint16_t render_type = glyph_map_entry->atlas_type + 1;

			*render_data =(struct sol_overlay_render_element)
		    {
		        {offset_x, offset_y, offset_x + glyph_map_entry->size_x, offset_y + glyph_map_entry->size_y},
		        {0, 0, glyph_atlas_location.offset.x, glyph_atlas_location.offset.y},
		        {render_type, array_layer_and_colour, 0, colour}
		    };
		default:
			/** do nothing */
		}
	}
}

void sol_font_render_overlay_text_simple(const char* text, struct sol_font* font, enum sol_overlay_colour colour, s16_rect position, struct sol_overlay_render_batch* render_batch)
{
	hb_buffer_t* buffer;



	// puts(text);


	#warning  need to pass in position to render!


	// accumulated_subpixel_advance = 0;
	buffer = sol_font_get_hb_buffer(font->parent_library);
	#warning should font have an over-riding set of default segment properties provided > font > library

	hb_buffer_clear_contents(buffer);

	/** assume simple text uses default properties, this must be set every time buffer is recycled */
	hb_buffer_set_segment_properties(buffer, &font->parent_library->default_properties);

	hb_buffer_add_utf8(buffer, text, -1, 0, -1);

	hb_shape(font->font, buffer, NULL, 0);

	//s16_rect position
	s16_vec2 offset = position.start;
	offset.y += ((position.end.y - position.start.y) - font->normalised_orthogonal_size) / 2;
	offset.y += font->baseline_offset;

	#warning could/should work out padding based on font? - removes control from design, use more accurate

	sol_font_render_overlay_text_segment_ltr(buffer, font, colour, offset, render_batch);

	/** hash map per font??, no font identifiers, just fonts themselves ? */
}












