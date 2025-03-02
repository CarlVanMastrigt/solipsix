/**
Copyright 2021,2022 Carl van Mastrigt

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

layout (location=0) in uvec4 data0;
layout (location=1) in uvec2 data1;
layout (location=2) in uvec4 data2;


layout(location=0) flat out uvec4 d0;
layout(location=1) flat out uvec2 d1;
layout(location=2) flat out uvec4 d2;

layout (push_constant) uniform screen_dimensions
{
	layout(offset=0) vec2 inv_screen_size_doubled;
};

void main()
{
    uvec2 p;
    switch(gl_VertexIndex)
    {
        case 0: p=data0.xy;break;
        case 1: p=data0.xw;break;
        case 2: p=data0.zy;break;
        default: p=data0.zw;
    }

    gl_Position=vec4(p*inv_screen_size_doubled-1.0, 0.0, 1.0);

    d0=data0;
    d1=data1;
    d2=data2;
}
