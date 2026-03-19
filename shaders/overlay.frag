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

#extension GL_EXT_shader_explicit_arithmetic_types_int16 : enable

/// array, [0] = alpha image, [1] = colour image
layout(set=0,binding=0) uniform sampler2DArray images[3];
// layout(set=0,binding=0) uniform sampler2D images[3];

layout(set=0,binding=1) uniform overlay_colours
{
    vec4 colours[8];
};

/** rect can be passed in as relative... */

layout(location=0) flat in u16vec4 rect;/** note: u16 -- start_x, start_y, end_x, end_y */
layout(location=1) flat in u16vec4 d1;
layout(location=2) flat in u16vec4 d2;
layout(location=3) flat in u16vec4 d3;

layout(location=0) out vec4 c;

/**
 * descriptions of inputs:
 */

void main()
{
    uint16_t render_type = d1.x & uint16_t(0x000Fu);
    uint16_t colour_index = d1.x >> 4;

    uint16_t array_layer = d1.y & uint16_t(0x00FFu);
    i16vec3 atlas_coords = i16vec3(gl_FragCoord.xy - rect.xy + d1.zw, array_layer);


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

    // mask/clip texture
    // switch(d1.x&0xC0000000)///need to check assembly for if both paths are taken (and hence both loads!) and if so conditionally(?) load val early to use in each effective branch
    // {
    //     case 0x40000000: c.a=min(c.a, texelFetch(images[0], atlas_coords, 0).x);break;
    //     case 0x80000000: c.a*=texelFetch(images[0], atlas_coords, 0).x;break;
    // }

    // r=(d1.y>>18)&0x3F;///left fade
    // if(r>0) c.a*=min(1.0,(gl_FragCoord.x-float(rect.x-((d1.x>>18)&0x3F)))/float(r));

    // r=(d1.y>>12)&0x3F;///top fade
    // if(r>0) c.a*=min(1.0,(gl_FragCoord.y-float(rect.y-((d1.x>>12)&0x3F)))/float(r));

    // r=(d1.y>>6)&0x3F;///right fade
    // if(r>0) c.a*=min(1.0,(float(rect.z+((d1.x>>6)&0x3F))-gl_FragCoord.x)/float(r));

    // r=d1.y&0x3F;///bottom fade
    // if(r>0) c.a*=min(1.0,(float(rect.w+(d1.x&0x3F))-gl_FragCoord.y)/float(r));

}
