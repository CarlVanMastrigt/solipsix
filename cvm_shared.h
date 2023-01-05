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

#ifndef CVM_SHARED_H
#define CVM_SHARED_H


///change to only include external headers that are actually needed try to reduce count overall
#include <stdbool.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <threads.h>
#include <time.h>
#include <assert.h>

#include <immintrin.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_vulkan.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H

#include <vulkan/vulkan.h>

///own headers

#define CVM_INVALID_U32_INDEX 0xFFFFFFFF
typedef union widget widget;

///following functions to expose intrinsice might want to be put in a separate header, will possibly/probably end up being platform specific
static inline uint32_t cvm_po2_gte(uint32_t v){ return __bsrd(v-1)+1; }
static inline uint32_t cvm_po2_lt(uint32_t v){ return __bsrd(v-1); }
static inline uint32_t cvm_allocation_increase_step(uint32_t current_size)
{
    assert(__bsrd(current_size)>=2u);
    return 1u<<(__bsrd(current_size)-2u);
}



#include "cvm_math.h"
#include "cvm_data_structures.h"
#include "cvm_random.h"
#include "cvm_vk.h"
#include "cvm_overlay.h"
#include "cvm_widget.h"
#include "cvm_transform.h"
#include "cvm_camera.h"
#include "cvm_mesh.h"
#include "cvm_thread.h"


#include "cvm_debug_render.h"


static inline char * cvm_strdup(const char * in)
{
    size_t len=(strlen(in)+1)*sizeof(char);
    char * out=malloc(len);
    return memcpy(out,in,len);
}

#endif

