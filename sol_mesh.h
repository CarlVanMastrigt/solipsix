/**
Copyright 2020,2021,2022,2026 Carl van Mastrigt

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

#pragma once

#include <inttypes.h>

#include "vk/buffer_atlas.h"

#define SOL_MESH_PROPERTY_SIMPLE             0x0000u
#define SOL_MESH_PROPERTY_ADGACENCY          0x0001u
#define SOL_MESH_PROPERTY_PER_FACE_MATERIAL  0x0002u
#define SOL_MESH_PROPERTY_VERTEX_NORMALS     0x0004u
#define SOL_MESH_PROPERTY_TEXTURE_COORDS     0x0008u

int sol_mesh_generate_file_from_objs(const char * name, uint16_t property_flags);


/** does this even make sense?
 * handle individual meshes in some independent fashion? where mesh metadata is stored alongside identifier 
 * 
 * if present, would like to render easily/immediately
 * if missing need to re-load lots of data because it may require recalculation? (stored data might not be enough?)
 * ^ variants types with variant handling in the mesh map?
 * 
 * use (id) or load/insert (with correct retain) id ??
 * render
 * on fail request load
 * depending on result use(render) or wait for init
 * render (always works?) must handle upload?
 * need variant that uploads/initialises
 * initialise doesn't work on size
 * */

struct sol_mesh_library;


struct sol_mesh
{
	/** can be loaded from disk (load metadata) or set manually */
	uint64_t buffer_atlas_index;
	uint32_t triangle_count;
	uint16_t mesh_properties;
};

// struct sol_mesh_library* sol_mesh_library_create(struct sol_vk_buffer_atlas* buffer_atlas);
// void sol_mesh_library_destroy(struct sol_mesh_library* mesh_library);

// uint64_t sol_mesh_library_generate_mesh_identifier(struct sol_mesh_library* mesh_library);

// sol_mesh_library_acquire_entry



