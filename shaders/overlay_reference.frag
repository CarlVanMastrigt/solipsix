/**
Copyright 2021,2022,2025 Carl van Mastrigt

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

#version 450

// #extension GL_EXT_shader_explicit_arithmetic_types_int16 : enable
#define u16vec4 uvec4
#define u16vec2 uvec2
#define i16vec3 ivec3
#define uint16_t uint


/// array, [0] = alpha image, [1] = colour image
layout(set=0,binding=0) uniform sampler2DArray images[3];
// layout(set=0,binding=0) uniform sampler2D images[3];

layout(set=0,binding=1) uniform overlay_colours
{
    vec4 colours[8];
};

/** rect can be passed in as relative... */

layout(location=0) flat in u16vec4 rect;/** note: u16 -- start_x, end_x, start_y, end_y */
layout(location=1) flat in u16vec4 d1;
layout(location=2) flat in u16vec4 d2;
layout(location=3) flat in u16vec4 d3;

layout(location=0) out vec4 c;

void main()
{
    uint16_t render_type = d1.x & uint16_t(0x000Fu);
    uint16_t colour_index = d1.x >> 4;

    uint16_t array_layer = d1.y & uint16_t(0x00FFu);
    i16vec3 atlas_coords = i16vec3(gl_FragCoord.xy - rect.xz + d1.zw, array_layer);


    switch(int(render_type))
    {
    case 0:
        c = colours[colour_index];
        break;
    case 1:
        c = colours[colour_index];
        c.a *= texelFetch(images[0], atlas_coords, 0).x;
        break;
    case 2:
        c = colours[colour_index];
        c.a *= texelFetch(images[1], atlas_coords, 0).x;
        break;
    case 3:
        c = texelFetch(images[2], atlas_coords, 0);
        break;
    }
}
