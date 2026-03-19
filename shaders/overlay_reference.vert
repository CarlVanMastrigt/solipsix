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

layout (location=0) in u16vec4 rect;/** note: u16 -- start_x, end_x, start_y, end_y */
layout (location=1) in u16vec4 data1;/** note: u16 */
layout (location=2) in u16vec4 data2;
layout (location=3) in u16vec4 data3;


layout(location=0) flat out u16vec4 out_rect;/** note: u16 */
layout(location=1) flat out u16vec4 d1;
layout(location=2) flat out u16vec4 d2;
layout(location=3) flat out u16vec4 d3;

layout (push_constant) uniform screen_dimensions
{
    layout(offset=0) vec2 inv_screen_size_doubled;
};

void main()
{
    u16vec2 p;
    switch(gl_VertexIndex)
    {
        case 0: p = rect.xz; break;
        case 1: p = rect.yz; break;
        case 2: p = rect.xw; break;
        case 3: p = rect.yw; break;
    }

    gl_Position=vec4(p*inv_screen_size_doubled-1.0, 0.0, 1.0);

    out_rect=rect;
    d1=data1;
    d2=data2;
    d3=data3;
}
