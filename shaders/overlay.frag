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



/// array, [0] = alpha image, [1] = colour image
layout(set=0,binding=0) uniform sampler2D images[2];

layout(set=0,binding=1) uniform overlay_colours
{
    vec4 colours[16];
};

layout(location=0) flat in uvec4 d0;
layout(location=1) flat in uvec2 d1;
layout(location=2) flat in uvec4 d2;

layout(location=0) out vec4 c;

void main()
{
    uint r;

    switch(d1.x&0x30000000)
    {
        case 0x10000000: c=vec4(1.0.xxx,textureLod(images[0],gl_FragCoord.xy-d0.xy+d2.xy,0).x);break;
        case 0x20000000: c=textureLod(images[1],gl_FragCoord.xy-d0.xy+d2.xy,0);break;
        default:c=c=1.0.xxxx;
    }

    // mask/clip texture
    switch(d1.x&0xC0000000)///need to check assembly for if both paths are taken (and hence both loads!) and if so conditionally(?) load val early to use in each effective branch
    {
        case 0x40000000: c.a=min(c.a,textureLod(images[0],gl_FragCoord.xy-d0.xy+d2.zw,0).x);break;
        case 0x80000000: c.a*=textureLod(images[0],gl_FragCoord.xy-d0.xy+d2.zw,0).x;break;
    }

    r=(d1.y>>18)&0x3F;///left fade
    if(r>0) c.a*=min(1.0,(gl_FragCoord.x-float(d0.x-((d1.x>>18)&0x3F)))/float(r));

    r=(d1.y>>12)&0x3F;///top fade
    if(r>0) c.a*=min(1.0,(gl_FragCoord.y-float(d0.y-((d1.x>>12)&0x3F)))/float(r));

    r=(d1.y>>6)&0x3F;///right fade
    if(r>0) c.a*=min(1.0,(float(d0.z+((d1.x>>6)&0x3F))-gl_FragCoord.x)/float(r));

    r=d1.y&0x3F;///bottom fade
    if(r>0) c.a*=min(1.0,(float(d0.w+(d1.x&0x3F))-gl_FragCoord.y)/float(r));

    if((d1.x&0x30000000)!=0x20000000)c*=colours[d1.y>>24];///if d1.x&0x3000==0x3000 (i.e. changes made such that code is used for something in particular, presently is meaningless) then change this accordingly
}
