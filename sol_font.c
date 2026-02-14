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

#include "tests/test_utils.h"


#include <assert.h>
#include <stdlib.h>
#include <threads.h>

#include <inttypes.h>

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include <freetype/ftbbox.h>



#include "sol_utils.h"


#include "data_structures/indices_stack.h"

#include "overlay/render.h"
#include "overlay/enums.h"



// #include <unicode/ubidi.h>
// #include <unicode/ubrk.h>

// #include <fribidi/fribidi.h>

// #include <unicode/ubrk.h>
#warning "need" ubrk to handle multiline and "ctrl" behaviour


#include "sol_font.h"

// #include "sol_font_internals.h"


#define KB_TEXT_SHAPE_IMPLEMENTATION
#define KB_TEXT_SHAPE_STATIC
// #define KB_TEXT_SHAPE_NO_CRT
#include "kb/kb_text_shape.h"


#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>




struct sol_font_library
{
	FT_Library freetype_library;
	// add pointer to image atlas here?
	uint32_t vended_font_count;// identifier passed into hash map
	struct sol_indices_stack available_font_indices;
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

#define SOL_FONT_GLYPH_INDEX_MAX     65535u
#define SOL_FONT_GLYPH_OFFSET_BIAS   511
#define SOL_FONT_GLYPH_OFFSET_MAX    1023
#define SOL_FONT_GLYPH_DIMENSION_MAX 1023



#warning NO, this is not enough, also want subpixel positioning!
struct sol_font_glyph_map_entry
{
	/** this is the packing of glyph_id and subpixel_advance : `glyph_id | (subpixel_offset << 16)`
	 * glyph_id is the glyth index as returned by harfbuzz, it should be less than 2^16 based on opentype and freetype file formats
	 * for rendering glyphs with better positioning, allow subpixel offset along the advance direction (horizontal OR vertical)
	 * note: this must be considered in key for image atlas lookup (i.e. render glyph at subpixel offset)
	 * there are 64 subpixel positions possible along the primary axis 
	 * 
	 * because there are 64 subpixel positions possible: 0:[-7 -> 8] 1:[9 -> 24] 2:[25 -> 40] 3:[41 -> 56] to correctly round */
	uint32_t key : 22;

	/** the positional information for the glyph, derived from the bounding box as returned by freetype
	 * the offsets should have -511 applied to account for negative offsets */
	uint32_t size_x   : 10;
	uint32_t size_y   : 10;
	uint32_t offset_x : 10;
	uint32_t offset_y : 10;

	/** which image atlas to use, may want to consider expanding this in future */
	uint32_t atlas_type : 2;

	/** identifier for the location in the image atlas where the pixel data for this glyph is stored */
	uint64_t id_in_atlas;
};
// fuck it, make this 32 bytes rather than packing into 16


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
#include "data_structures/hash_map_implement.h"





struct sol_font
{
	struct sol_font_library* parent_library;
	// could avoid overlap and have these be ref-counted...

	/** freetype components, used for rendering */
    struct
    {
    	FT_Face face;
    	int64_t x_scale;
    	int64_t y_scale;
    }
    ft;


    struct
    {
		hb_font_t* font;
		hb_segment_properties_t default_properties;
	}
	hb;


	struct
	{
		kbts_font font;
		kbts_shape_state* state;
		kbts_shape_config config;
		kbts_direction direction;

		kbts_glyph* glyphs;
		uint32_t glyph_space;
	}
	kb;

    // int16_t glyph_size;
    int16_t baseline_offset;
    int16_t normalised_orthogonal_size;

    int16_t max_advance;

    bool subpixel_offset_render;


    struct sol_font_glyph_map glyph_map;




    // ///not a great solution but bubble sort new glyphs from end?, is rare enough op and gives low enough cache use to justify?
    // cvm_overlay_glyph * glyphs;
    // uint32_t glyph_count;
    // uint32_t glyph_space;
};








struct sol_font_library* sol_font_library_create(void)
{
	int freetype_error;
	struct sol_font_library* font_library;

	font_library = malloc(sizeof(struct sol_font_library));
	freetype_error = FT_Init_FreeType(&font_library->freetype_library);
	/** conside `FT_New_Library` with an allocator... */
    assert(!freetype_error);

    mtx_init(&font_library->mutex, mtx_plain);
    font_library->buffers = NULL;
    font_library->buffer_count = 0;
    font_library->buffer_space = 0;

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



#warning if language not provided perhaps implement guessing in "simple"?
struct sol_font* sol_font_create(struct sol_font_library* font_library, const char* ttf_filename, int pixel_size, bool subpixel_offset_render, const char* default_script_id, const char* default_language_id, const char* default_direction_id)
{
	struct sol_font* font = malloc(sizeof(struct sol_font));
	int error, i;
	int16_t ascender, descender;



	assert(ttf_filename);

	font->hb.font = NULL;
	font->kb.state = NULL;
	font->subpixel_offset_render = subpixel_offset_render;
	font->parent_library = font_library;


	error = FT_New_Face(font_library->freetype_library, ttf_filename, 0, &font->ft.face);
	if(error)
	{
		fprintf(stderr, "error: unable to load font file (%s)", ttf_filename);
		free(font);
		return NULL;
	}

	// error = FT_Set_Pixel_Sizes(font->face, 0, pixel_size);
	error = FT_Set_Char_Size(font->ft.face, 0, pixel_size, 0, 0);
	if(error)
	{
		fprintf(stderr, "error: font file seems to be empty (%s)", ttf_filename);
		sol_font_destroy(font);
		return NULL;
	}

	font->ft.x_scale = font->ft.face->size->metrics.x_scale;
	font->ft.y_scale = font->ft.face->size->metrics.y_scale;

	kb_setup:
	{
		char language_tag[4] = {' ',' ',' ',' '};
		char script_tag[4] = {' ',' ',' ',' '};
		kbts_script script;
		kbts_language language;
		kbts_direction direction;

		for(i = 0; default_script_id   && default_script_id  [i] && i < 4; i++) script_tag  [i] = tolower(default_script_id  [i]);
		for(i = 0; default_language_id && default_language_id[i] && i < 4; i++) language_tag[i] = toupper(default_language_id[i]);


		font->kb.font = kbts_FontFromFile(ttf_filename);

		assert(kbts_FontIsValid(&font->kb.font));

		font->kb.state = kbts_CreateShapeState(&font->kb.font);
		if(font->kb.state == NULL)
		{
			fprintf(stderr, "error: unable to create (kb) shape state for font file (%s)", ttf_filename);
			sol_font_destroy(font);
			return NULL;
		}

		font->kb.glyph_space = 16;
		font->kb.glyphs = malloc(sizeof(kbts_glyph) * font->kb.glyph_space);

		script = kbts_ScriptTagToScript(KBTS_FOURCC(script_tag[0],script_tag[1],script_tag[2],script_tag[3]));
		language = KBTS_FOURCC(language_tag[0],language_tag[1],language_tag[2],language_tag[3]);
		direction = (default_direction_id && sol_strcasecmp(default_direction_id, "rtl") == 0) ? KBTS_DIRECTION_RTL : KBTS_DIRECTION_LTR;

		font->kb.config = kbts_ShapeConfig(&font->kb.font, script, language);
		font->kb.direction = direction;
	}

	hb_setup:
	{
		hb_language_t language;
		hb_script_t script;
		hb_direction_t direction;

		font->hb.font = hb_ft_font_create_referenced(font->ft.face);
		if(font->hb.font == NULL)
		{
			fprintf(stderr, "error: unable to create hb font for font file (%s)", ttf_filename);
			sol_font_destroy(font);
			return NULL;
		}

		language = default_language_id ? hb_language_from_string(default_language_id, -1) : HB_LANGUAGE_INVALID;
		language = language == HB_LANGUAGE_INVALID ? hb_language_from_string("eng", -1) : language;

		script = default_script_id ? hb_script_from_string(default_script_id, -1) : HB_SCRIPT_INVALID;
		script = script == HB_SCRIPT_INVALID ? HB_SCRIPT_LATIN : script;

		direction = default_direction_id ? hb_direction_from_string(default_direction_id, -1) : HB_DIRECTION_INVALID;
		direction = direction == HB_DIRECTION_INVALID ? HB_DIRECTION_LTR : direction;

		font->hb.default_properties = (hb_segment_properties_t)
		{
			.language = language,
			.script = script,
			.direction = direction,
		};
	}



#warning assumes horizontal text
	ascender = font->ft.face->size->metrics.ascender;
	descender = font->ft.face->size->metrics.descender;

	assert((ascender & 0x3F) == 0 && (descender & 0x3F) == 0);

#warning permit 2 othogonal offsets (for odd and even sized differences between bounds and font orthogonal size)
	#warning this can be done much better: working ENTIRELY in 26.6 units! and placing the basline ON a pixel boundary (i.e. remove orthogonal_subpixel_offset entirely!)
	font->baseline_offset = ascender >> 6;
	font->normalised_orthogonal_size = (ascender - descender) >> 6;

    font->max_advance   = font->ft.face->size->metrics.max_advance >> 6;

	// printf("height %d\n", font->normalised_orthogonal_size);
	// printf("line spacing %d\n", font->line_spacing );

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

	FT_Done_Face(font->ft.face);

	if(font->hb.font)
	{
		hb_font_destroy(font->hb.font);
	}

	free(font->kb.glyphs);
	kbts_FreeShapeState(font->kb.state);
	kbts_FreeFont(&font->kb.font);

	free(font);
}

#warning might be able to use sdl key input to determine/separate chunks of text by language!? if true this is amazing (this is good news)
#warning  NOPE ^ you cannot, need to re-assess each break (ubrk) on "dynamic" UI strings (user input, filename &c.)


static inline bool sol_font_obtain_glyph_map_entry(struct sol_font* font, uint32_t glyph_codepoint, uint32_t subpixel_offset, struct sol_overlay_render_batch* render_batch, struct sol_font_glyph_map_entry** glyph_map_entry_result)
{
	enum sol_map_operation_result obtain_result;
	FT_GlyphSlot glyph_slot;
	int ft_result;
	uint32_t glyph_key;
	struct sol_image_atlas* image_atlas;

	assert(subpixel_offset < 64);

	#warning this encoding of subpixel position in key should be different for horizontal vs vertical text, orthogonal direction only needs a single bit of offset (usless shaping is expected to actually move the y cursor)

	// subpixel_offset_y = 0;
	// printf("SPy: %u\n", subpixel_offset_y);

	/** note: glyph codepoint is restricted to 16 bits for most fonts */
	glyph_key = glyph_codepoint | (subpixel_offset << 16);

	obtain_result = sol_font_glyph_map_obtain(&font->glyph_map, glyph_key, glyph_map_entry_result);

	switch (obtain_result)
	{
	case SOL_MAP_SUCCESS_INSERTED:
		/** populate `glyph_map_entry` which requires rendering */

		/** TODO: determine image_atlas_type_index (probably just R8 lol, but may wish to support colour for emoji...) */
		/** glyph_slot->format == FT_GLYPH_FORMAT_BITMAP...
		 * glyph_slot->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA; <- this is actually REALLY painful,
		 * different memory layot AND is premultiplied, meaning it cannot be rendered the same way as any custom approach
		 * overlay will need to output premultiplied AND account for other output NOT being premultiplied
		 * also handling emoji in general will be difficult as current implementation uses a 1:1 mapping
		 * */

		ft_result = FT_Load_Glyph(font->ft.face, glyph_codepoint, 0);
		assert(ft_result == 0);

		glyph_slot = font->ft.face->glyph;

		assert(glyph_slot->format == FT_GLYPH_FORMAT_OUTLINE);
		FT_Outline_Translate(&glyph_slot->outline, subpixel_offset, 0);

		FT_Render_Glyph(glyph_slot, FT_RENDER_MODE_NORMAL);

		assert(glyph_slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
		assert(glyph_slot->bitmap.num_grays == 256);

		image_atlas = render_batch->rendering_resources->atlases[SOL_OVERLAY_IMAGE_ATLAS_TYPE_R8_UNORM];

		assert(glyph_slot->bitmap_left >= -SOL_FONT_GLYPH_OFFSET_BIAS);
		assert(glyph_slot->bitmap_top  >= -SOL_FONT_GLYPH_OFFSET_BIAS);
		assert(glyph_slot->bitmap_left + SOL_FONT_GLYPH_OFFSET_BIAS <= SOL_FONT_GLYPH_OFFSET_MAX);
		assert(glyph_slot->bitmap_top  + SOL_FONT_GLYPH_OFFSET_BIAS <= SOL_FONT_GLYPH_OFFSET_MAX);
		assert(glyph_slot->bitmap.width <= SOL_FONT_GLYPH_DIMENSION_MAX);
		assert(glyph_slot->bitmap.rows  <= SOL_FONT_GLYPH_DIMENSION_MAX);

		if(glyph_slot->bitmap.width > 0 && glyph_slot->bitmap.rows > 0)
		{
			**glyph_map_entry_result = (struct sol_font_glyph_map_entry)
			{
				.key = glyph_key,
				/** NOTE: using R8 here is a temporary measure and should be evaluated properly */
				.atlas_type = SOL_OVERLAY_IMAGE_ATLAS_TYPE_R8_UNORM,
				.offset_x = glyph_slot->bitmap_left + SOL_FONT_GLYPH_OFFSET_BIAS,
				.offset_y = glyph_slot->bitmap_top  + SOL_FONT_GLYPH_OFFSET_BIAS,
				.size_x = glyph_slot->bitmap.width,
				.size_y = glyph_slot->bitmap.rows,
				.id_in_atlas = sol_image_atlas_generate_entry_identifier(image_atlas),
			};
		}
		else
		{
			**glyph_map_entry_result = (struct sol_font_glyph_map_entry)
			{
				.key = glyph_key,
				.id_in_atlas = 0,
			};
		}

		/** note: intentional fallthrough */
	case SOL_MAP_SUCCESS_FOUND:
		/** render (structured to put this outside switch for clarity) */
		return true;

	default:
		fprintf(stderr, "unexpected glyph map result (%d)", obtain_result);
		return false;
	}
}

static inline bool sol_font_obtain_glyph_atlas_location(struct sol_font* font, const struct sol_font_glyph_map_entry* glyph_map_entry, struct sol_overlay_render_batch* render_batch, struct sol_image_atlas_location* glyph_atlas_location_result)
{
	struct sol_buffer_segment pixel_upload_segment;
	struct sol_image_atlas* image_atlas;
	u16_vec2 glyph_size;
	unsigned char* pixels_dst;
	unsigned char* pixels_src;
	enum sol_image_atlas_result find_result, obtain_result;
	FT_GlyphSlot glyph_slot;
	int ft_result, src_pitch;
	uint32_t subpixel_offset, glyph_index;
	uint16_t row;
	VkDeviceSize required_upload_bytes, required_uplaod_alignment;
	struct sol_vk_image* atlas_raw_image;


	image_atlas = render_batch->rendering_resources->atlases[glyph_map_entry->atlas_type];

	find_result = sol_image_atlas_find_identified_entry(image_atlas, glyph_map_entry->id_in_atlas, glyph_atlas_location_result);

	switch (find_result)
	{
	case SOL_IMAGE_ATLAS_FAIL_ABSENT:
		/** setup glyph pixels (entry) if not present in image atlas */

		glyph_size = u16_vec2_set(glyph_map_entry->size_x, glyph_map_entry->size_y);
		
		atlas_raw_image = &sol_image_atlas_access_supervised_image(image_atlas)->image;

		sol_vk_image_calculate_copy_space_requirements_simple(atlas_raw_image, glyph_size, &required_upload_bytes, &required_uplaod_alignment);

		if(!sol_buffer_can_accomodate_aligned_allocation(&render_batch->upload_buffer, required_upload_bytes, required_uplaod_alignment))
		{
			/** not enough space available in upload buffer */
			return false;
		}

		obtain_result = sol_image_atlas_obtain_identified_entry(image_atlas, glyph_map_entry->id_in_atlas, glyph_size, glyph_atlas_location_result);

		if(obtain_result != SOL_IMAGE_ATLAS_SUCCESS_INSERTED)
		{
			/** not enough space in the map */
			assert(obtain_result != SOL_IMAGE_ATLAS_SUCCESS_FOUND);/* should have been detected as found earlier */
			assert(obtain_result != SOL_IMAGE_ATLAS_FAIL_ABSENT);/* obtain should not return absent */
			return false;
		}

		pixel_upload_segment = sol_vk_image_prepare_copy_simple(
			atlas_raw_image,
			render_batch->atlas_copy_lists + glyph_map_entry->atlas_type,
			&render_batch->upload_buffer,
			glyph_atlas_location_result->offset,
			glyph_size,
			glyph_atlas_location_result->array_layer);

		/** required space was checked against available space BEFORE attempting upload, a null buffer should never be returned */
		assert(pixel_upload_segment.ptr != NULL);

		glyph_index = glyph_map_entry->key & 0xFFFF;
		#warning this decoding of subpixel position in key should be different for horizontal vs vertical text
		subpixel_offset = (glyph_map_entry->key >> 16) & 0x3F;
		ft_result = FT_Load_Glyph(font->ft.face, glyph_index, 0);

		assert(ft_result == 0);

		glyph_slot = font->ft.face->glyph;

		assert(glyph_slot->format == FT_GLYPH_FORMAT_OUTLINE);
		FT_Outline_Translate(&glyph_slot->outline, subpixel_offset, 0);

		FT_Render_Glyph(glyph_slot, FT_RENDER_MODE_NORMAL);

		assert(glyph_map_entry->offset_x == glyph_slot->bitmap_left + SOL_FONT_GLYPH_OFFSET_BIAS);
		assert(glyph_map_entry->offset_y == glyph_slot->bitmap_top  + SOL_FONT_GLYPH_OFFSET_BIAS);
		assert(glyph_map_entry->size_x == glyph_slot->bitmap.width);
		assert(glyph_map_entry->size_y == glyph_slot->bitmap.rows );
		assert(glyph_slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
		assert(glyph_slot->bitmap.num_grays  == 256);

		pixels_src = (unsigned char*)glyph_slot->bitmap.buffer;
		src_pitch = glyph_slot->bitmap.pitch;

		/** R8 */
		#warning am assuming R8 here...
		pixels_dst = pixel_upload_segment.ptr;
		assert(pixel_upload_segment.size == sizeof(unsigned char) * glyph_size.x * glyph_size.y);

		for(row = 0; row < glyph_size.y; row++)
		{
			memcpy(pixels_dst, pixels_src, sizeof(unsigned char) * glyph_size.x);
			pixels_dst += glyph_size.x;
			pixels_src += src_pitch;
		}
		return true;

	case SOL_IMAGE_ATLAS_SUCCESS_FOUND:
		return true;
		
	default:
		return false;
	}
}

static inline void sol_font_render_overlay_glyph(uint32_t glyph_codepoint, int32_t cursor_x, int32_t cursor_y, struct sol_font* font, enum sol_overlay_colour colour, struct sol_overlay_render_batch* render_batch)
{
	struct sol_font_glyph_map_entry* glyph_map_entry;
	unsigned int i, glyph_count;
	struct sol_image_atlas_location glyph_atlas_location;
	struct sol_overlay_render_element* render_data;
	int32_t offset_x, offset_y, rounded_cursor_x;
	uint32_t subpixel_offset_x;
	bool glyph_entry_present;



	/** this is segment that assumes LTR font */
	{
		/** -7 to 8 is "subpixel offset" of 0 and this increases with each 16 point step (4 total, 0-3), must multiply this by 16 when rendering*/
		assert(cursor_x >= 0);
		assert(cursor_y >= 0);

		if(font->subpixel_offset_render)
		{
			subpixel_offset_x = cursor_x & 0x3F;// % 64
			offset_x = cursor_x >> 6;
		}
		else
		{
			rounded_cursor_x = cursor_x + 31;
			subpixel_offset_x = 0;
			offset_x = rounded_cursor_x >> 6;
		}

		offset_y = cursor_y >> 6;
	}

	#warning need to know if this glyph is a bitmap (somehow) before attempting to load with a given subpixel offset!
	#warning attempting to load SVG/bitmap when these don't permit subpixel offset will result in unnecessary image atlas waste -- subpixel offset can be used for SVG but rendering SVG format font is not supported by freetype
	#warning need a bitfield for glyphs types (max 4k memory per font to store it though) to indicate outline vs SVG ??

	/** obtain the glyph render data (glyph_map_entry) */

	/** have a standardised / uniform subpixel offset in y, which this should be expected to match */

	assert(glyph_codepoint <= SOL_FONT_GLYPH_INDEX_MAX);
	assert(subpixel_offset_x < 64);

	glyph_entry_present = sol_font_obtain_glyph_map_entry(font, glyph_codepoint, subpixel_offset_x, render_batch, &glyph_map_entry);

	/** render the glyph if its pixel data is obtainable */
	if(glyph_entry_present && glyph_map_entry->id_in_atlas != 0 && sol_font_obtain_glyph_atlas_location(font, glyph_map_entry, render_batch, &glyph_atlas_location))
	{
		/** note: y offset is negative, ergo subtraction */
		offset_x += (glyph_map_entry->offset_x - SOL_FONT_GLYPH_OFFSET_BIAS);
		offset_y -= (glyph_map_entry->offset_y - SOL_FONT_GLYPH_OFFSET_BIAS);

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
	}
}

static inline void sol_font_render_text_simple_harfbuzz(const char* text, struct sol_font* font, enum sol_overlay_colour colour, s16_rect position, enum sol_overlay_alignment alignment, enum sol_overlay_orientation orientation, struct sol_overlay_render_batch* render_batch)
{
	hb_buffer_t* buffer;
	hb_glyph_info_t* glyph_info;
	hb_glyph_position_t* glyph_positions;
	unsigned int i, glyph_count;
	int32_t cursor_x, cursor_y, accumulated_subpixel_advance;


	buffer = sol_font_get_hb_buffer(font->parent_library);
	#warning should font have an over-riding set of default segment properties provided > font > library

	hb_buffer_clear_contents(buffer);

	/** assume simple text uses default properties, this must be set every time buffer is recycled */
	hb_buffer_set_segment_properties(buffer, &font->hb.default_properties);

	hb_buffer_add_utf8(buffer, text, -1, 0, -1);

	hb_shape(font->hb.font, buffer, NULL, 0);

	glyph_info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
	glyph_positions = hb_buffer_get_glyph_positions(buffer, &i);

	/** glyph count should match between positioning and rendering */
	assert(i == glyph_count);

	accumulated_subpixel_advance = 0;
	for(i = 0; i < glyph_count; i++)
	{
		accumulated_subpixel_advance += glyph_positions[i].x_advance;
	}

	#warning calculate length here, including number (and position?) of breaks -- should be very quick

	/** get cursor position, in subpixels, of the font baseline centred vertically and at the start horizontally, of the provided rectangle */
	cursor_x =  (int32_t)(position.start.x) << 6;
	cursor_y = ((int32_t)(position.end.y + position.start.y - font->normalised_orthogonal_size) << 5) + ((int32_t)(font->baseline_offset) << 6);
	// printf("y: %u %u\n",cursor_y, cursor_y&63);

	for(i = 0; i < glyph_count; i++)
	{
		sol_font_render_overlay_glyph(glyph_info[i].codepoint,
			cursor_x + glyph_positions[i].x_offset,
			cursor_y + glyph_positions[i].y_offset,
			font, colour, render_batch);

		cursor_x += glyph_positions[i].x_advance;
		cursor_y += glyph_positions[i].y_advance;
	}
}


static inline void sol_font_render_text_simple_kb(const char* text, struct sol_font* font, enum sol_overlay_colour colour, s16_rect position, enum sol_overlay_alignment alignment, enum sol_overlay_orientation orientation, struct sol_overlay_render_batch* render_batch)
{
	int32_t cursor_x, cursor_y, x_base, y_base;
	kbts_decode decode;
	size_t len, remaining_len;
	uint32_t i, glyph_count;
	kbts_glyph glyph;
	kbts_cursor cursor;

	len = strlen(text);
	remaining_len = len;
	glyph_count = 0;

	while(remaining_len)
	{
		decode = kbts_DecodeUtf8(text, remaining_len);
		text += decode.SourceCharactersConsumed;
		remaining_len -= decode.SourceCharactersConsumed;
		if(decode.Valid)
		{
			if(glyph_count == font->kb.glyph_space)
			{
				font->kb.glyph_space *= 2;
				font->kb.glyphs = realloc(font->kb.glyphs, sizeof(kbts_glyph) * font->kb.glyph_space);
			}
			font->kb.glyphs[glyph_count++] = kbts_CodepointToGlyph(&font->kb.font, decode.Codepoint);
		}
		else
		{
			break;
		}
	}

	while(kbts_Shape(font->kb.state, &font->kb.config, font->kb.direction, font->kb.direction, font->kb.glyphs, &glyph_count, font->kb.glyph_space))
	{
		font->kb.glyph_space *= 2;
		font->kb.glyphs = realloc(font->kb.glyphs, sizeof(kbts_glyph) * font->kb.glyph_space);
	}

	/** get cursor position, in subpixels, of the font baseline centred vertically and at the start horizontally, of the provided rectangle */

	#warning this needs alignment to be as adaptable as desired
	x_base =  (font->kb.direction == KBTS_DIRECTION_RTL) ? ((int32_t)(position.end.x) << 6) : ((int32_t)(position.start.x) << 6);

	/** << 5 here is basically division by 2, as its working in subpixel offsets (64ths of a pixel) this allows font to be offset by half a pixel vertically */
	// cursor_y = ((int32_t)(position.end.y + position.start.y - font->normalised_orthogonal_size) << 5) + ((int32_t)(font->baseline_offset) << 6);
	y_base = ((int32_t)(position.end.y + position.start.y - font->normalised_orthogonal_size) << 5) + ((int32_t)(font->baseline_offset) << 6);


	cursor = kbts_Cursor(font->kb.direction);
	for(i = 0; i < glyph_count; i++)
	{
		kbts_PositionGlyph(&cursor, &font->kb.glyphs[i], &cursor_x, &cursor_y);

		/** convert font units (that KB works in) into 26.6 units which freetype and rendering works in, this requires expanding the range to ensure values over 32px dont overflow... */
		cursor_x = (int32_t)(((int64_t)cursor_x * font->ft.x_scale) >> 16);
		cursor_y = (int32_t)(((int64_t)cursor_y * font->ft.y_scale) >> 16);

		sol_font_render_overlay_glyph(font->kb.glyphs[i].Id, x_base+cursor_x, y_base+cursor_y, font, colour, render_batch);
	}
}


void sol_font_render_text_simple(const char* text, struct sol_font* font, enum sol_overlay_colour colour, s16_rect position, struct sol_overlay_render_batch* render_batch)
{
	/** get these from theme ? */
	enum sol_overlay_alignment alignment = SOL_OVERLAY_ALIGNMENT_START;
	enum sol_overlay_orientation orientation = SOL_OVERLAY_ORIENTATION_HORIZONTAL;
	// sol_font_render_text_simple_harfbuzz(text, font, colour, position, alignment, orientation, render_batch);
	sol_font_render_text_simple_kb(text, font, colour, position, alignment, orientation, render_batch);
}


s16_vec2 sol_font_size_text_simple_hb(const char* text, struct sol_font* font)
{
	hb_buffer_t* buffer;
	unsigned int i, glyph_count;
	int32_t accumulated_subpixel_advance, advance;
	hb_glyph_position_t* glyph_positions;

	buffer = sol_font_get_hb_buffer(font->parent_library);

	hb_buffer_clear_contents(buffer);

	/** assume simple text uses default properties, this must be set every time buffer is recycled */
	hb_buffer_set_segment_properties(buffer, &font->hb.default_properties);

	hb_buffer_add_utf8(buffer, text, -1, 0, -1);

	hb_shape(font->hb.font, buffer, NULL, 0);

	glyph_positions = hb_buffer_get_glyph_positions(buffer, &glyph_count);
	accumulated_subpixel_advance = 0;
	for(i = 0; i < glyph_count; i++)
	{
		accumulated_subpixel_advance += glyph_positions[i].x_advance;
	}

	advance = accumulated_subpixel_advance >> 6;
	return s16_vec2_set(advance, font->normalised_orthogonal_size);
}

s16_vec2 sol_font_size_text_simple(const char* text, struct sol_font* font)
{
	return sol_font_size_text_simple_hb(text, font);
}


static inline void sol_font_render_centred_glyph(struct sol_font* font, uint16_t glyph_codepoint, enum sol_overlay_colour colour, s16_rect position, struct sol_overlay_render_batch* render_batch)
{
	struct sol_font_glyph_map_entry* glyph_map_entry;
	struct sol_image_atlas_location glyph_atlas_location;
	struct sol_overlay_render_element* render_data;
	uint16_t offset_x, offset_y;

	if(sol_font_obtain_glyph_map_entry(font, glyph_codepoint, 0, render_batch, &glyph_map_entry))
	{
		if(sol_font_obtain_glyph_atlas_location(font, glyph_map_entry, render_batch, &glyph_atlas_location))
		{
			assert(glyph_map_entry->atlas_type == SOL_OVERLAY_IMAGE_ATLAS_TYPE_R8_UNORM);

			#warning clean this shit up (give it a general implementation!??) needs to clamp these values to the rect also!
			render_data = sol_overlay_render_element_list_append_ptr(&render_batch->elements);
			assert((uint16_t)colour < 256);
			uint16_t array_layer_and_colour = colour | (glyph_atlas_location.array_layer << 8);
			uint16_t render_type = glyph_map_entry->atlas_type + 1;


			offset_x = (position.start.x + position.end.x - glyph_map_entry->size_x) >> 1;
			offset_y = (position.start.y + position.end.y - glyph_map_entry->size_y) >> 1;

			*render_data =(struct sol_overlay_render_element)
		    {
		        {offset_x, offset_y, offset_x + glyph_map_entry->size_x, offset_y + glyph_map_entry->size_y},
		        {0, 0, glyph_atlas_location.offset.x, glyph_atlas_location.offset.y},
		        {render_type, array_layer_and_colour, 0, colour}
		    };
		}
	}}


static inline void sol_font_render_glyph_simple_kb(const char* utf8_glyph, struct sol_font* font, enum sol_overlay_colour colour, s16_rect position, struct sol_overlay_render_batch* render_batch)
{
	int32_t cursor_x, cursor_y, x_base, y_base;
	kbts_decode decode;
	size_t len, remaining_len;
	uint32_t i, glyph_count;
	kbts_glyph glyph;
	kbts_cursor cursor;

	len = strlen(utf8_glyph);
	remaining_len = len;
	glyph_count = 0;

	while(remaining_len)
	{
		decode = kbts_DecodeUtf8(utf8_glyph, remaining_len);
		utf8_glyph += decode.SourceCharactersConsumed;
		remaining_len -= decode.SourceCharactersConsumed;
		if(decode.Valid)
		{
			if(glyph_count == font->kb.glyph_space)
			{
				font->kb.glyph_space *= 2;
				font->kb.glyphs = realloc(font->kb.glyphs, sizeof(kbts_glyph) * font->kb.glyph_space);
			}
			font->kb.glyphs[glyph_count++] = kbts_CodepointToGlyph(&font->kb.font, decode.Codepoint);
		}
		else
		{
			break;
		}
	}

	while(kbts_Shape(font->kb.state, &font->kb.config, font->kb.direction, font->kb.direction, font->kb.glyphs, &glyph_count, font->kb.glyph_space))
	{
		font->kb.glyph_space *= 2;
		font->kb.glyphs = realloc(font->kb.glyphs, sizeof(kbts_glyph) * font->kb.glyph_space);
	}

	/** text passed into this function should only produce a single glyph */
	assert(glyph_count == 1);

	sol_font_render_centred_glyph(font, font->kb.glyphs[0].Id, colour, position, render_batch);
}

void sol_font_render_glyph_simple_hb(const char* utf8_glyph, struct sol_font* font, enum sol_overlay_colour colour, s16_rect position, struct sol_overlay_render_batch* render_batch)
{
	#warning rewrite to extract harfbuzz

	hb_buffer_t* buffer;
	hb_glyph_info_t* glyph_info;
	unsigned int i, glyph_count;
	uint16_t offset_x, offset_y;

	buffer = sol_font_get_hb_buffer(font->parent_library);
	#warning should font have an over-riding set of default segment properties provided > font > library

	hb_buffer_clear_contents(buffer);

	/** assume simple text uses default properties, this must be set every time buffer is recycled */
	hb_buffer_set_segment_properties(buffer, &font->hb.default_properties);

	hb_buffer_add_utf8(buffer, utf8_glyph, -1, 0, -1);

	hb_shape(font->hb.font, buffer, NULL, 0);

	glyph_info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
	assert(glyph_count == 1);

	sol_font_render_centred_glyph(font, glyph_info->codepoint, colour, position, render_batch);
}

void sol_font_render_glyph_simple(const char* utf8_glyph, struct sol_font* font, enum sol_overlay_colour colour, s16_rect position, struct sol_overlay_render_batch* render_batch)
{
	// sol_font_render_glyph_simple_hb(utf8_glyph, font, colour, position, render_batch);
	sol_font_render_glyph_simple_kb(utf8_glyph, font, colour, position, render_batch);
}

s16_vec2 sol_font_size_glyph_simple(const char* utf8_glyph, struct sol_font* font)
{
	uint16_t s = font->normalised_orthogonal_size;
	return s16_vec2_set(s, s);
}





