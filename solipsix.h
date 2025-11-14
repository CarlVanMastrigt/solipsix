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

#ifndef SOLIPSIX_H
#define SOLIPSIX_H

///needed for sincosf, may want to re-evaluate usage, at least move to math??
// #define _GNU_SOURCE

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
#include <stdalign.h>

#include <immintrin.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H

#include <vulkan/vulkan.h>


#define SOLIPSIX_ENGINE_NAME "solipsix"
#define SOLIPSIX_ENGINE_VERSION 1


typedef union widget widget; // find a better place to put this?


#include "cvm_utils.h"
#include "cvm_math.h"
#include "cvm_random.h"
#include "cvm_directory.h"
#include "cvm_vk.h"
#include "cvm_overlay.h"
#include "cvm_widget.h"
#include "cvm_transform.h"
#include "sol_camera.h"
#include "cvm_mesh.h"
#include "sol_lockfree.h"
#include "cvm_thread.h"
#include "sol_sync.h"


#include "cvm_debug_render.h"

#include "cvm_independent_overlay.h"


#endif

