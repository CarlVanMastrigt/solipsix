/**
Copyright 2020,2021,2022 Carl van Mastrigt

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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "sol_mesh.h"




int sol_mesh_generate_file_from_objs(const char * name, uint16_t flags)
{
	const uint16_t cvm_mesh_file_signiature = 0x53FFu;
	const uint16_t cvm_mesh_version_number  = 0x0001u;


    uint16_t num_verts,num_positions,num_normals,num_tex_coords,current_matreial,k,pm,pa,n,adj_count;
    uint32_t num_faces,i,j;
    size_t interleaved_vertex_data_size;

    bool has_normals,has_tex_coords;

    uint16_t * v_indices;///indices_0
    //uint16_t * normal_indices;
    //uint16_t * tex_coord_indices;
    uint16_t * indices;///final/resultant
    uint16_t * adjacency;

    uint16_t * materials;

    float * positions;
    //float * norms;
    //float * tex_coords;//male uint16_t (unorm coordinates) ??

    void * interleaved_vertex_data;

    FILE * f;

    char buffer[1024];
    uint16_t vb[3];///support for up to 12
    uint16_t vnb[3];///support for up to 12
    uint16_t vtb[3];///support for up to 12


    if(flags&SOL_MESH_PROPERTY_VERTEX_NORMALS)
    {
        fprintf(stderr,"MESH NORMAL SUPPORT NYI\n");
        return -1;
    }
    if(flags&SOL_MESH_PROPERTY_TEXTURE_COORDS)
    {
        fprintf(stderr,"MESH TEXTURE COORDINATE SUPPORT NYI\n");
        return -1;
    }

    strcpy(buffer,name);
    strcat(buffer,".obj");

    printf("converting to mesh file: %s\n",buffer);

    num_tex_coords=num_normals=num_positions=0;
    num_faces=0;

    f=fopen(buffer,"r");

    if(!f)
    {
        fprintf(stderr,"UNABLE TO LOAD OBJ FILE: %s\n",buffer);
        return -1;
    }

    while(fgets(buffer,1024,f))
    {
        if(buffer[0]=='f' && buffer[1]==' ' && num_faces++ ==0x000FFFFF)
        {
            fprintf(stderr,"TOO MANY FACES IN OBJ: %s\n",name);
            return -1;
        }
        else if(buffer[0]=='v')
        {
            if(buffer[1]==' ' && num_positions++ ==0xFFFF)
            {
                fprintf(stderr,"TOO MANY VERTS IN OBJ: %s\n",name);
                return -1;
            }
            else if(buffer[1]=='n' && buffer[2]==' ' && num_normals++ ==0xFFFF)
            {
                fprintf(stderr,"TOO MANY VERT NORMALS IN OBJ: %s\n",name);
                return -1;
            }
            else if(buffer[1]=='t' && buffer[2]==' ' && num_tex_coords++ ==0xFFFF)
            {
                fprintf(stderr,"TOO MANY TEXTURE COORDINATES IN OBJ: %s\n",name);
                return -1;
            }
        }
    }

    fclose(f);

    if(!num_faces)
    {
        fprintf(stderr,"NO FACES FOUND IN OBJ %s\n",name);
        return -1;
    }
    if(!num_positions)
    {
        fprintf(stderr,"NO VERTS FOUND IN OBJ %s\n",name);
        return -1;
    }
    if(flags&SOL_MESH_PROPERTY_VERTEX_NORMALS && !num_normals)
    {
        fprintf(stderr,"NO NORMALS FOUND IN OBJ %s\n",name);
        return -1;
    }
    if(flags&SOL_MESH_PROPERTY_TEXTURE_COORDS && !num_tex_coords)
    {
        fprintf(stderr,"NO TEXTURE COORDINATES FOUND IN OBJ %s\n",name);
        return -1;
    }

    printf("num_faces: %u\nnum_positions: %hu\nnum_normals: %hu\nnum_texture_coordinates: %hu\n",num_faces,num_positions,num_normals,num_tex_coords);

    has_normals=num_normals>0;
    has_tex_coords=num_tex_coords>0;


    v_indices=malloc(sizeof(uint16_t)*num_faces*3);

    positions=malloc(sizeof(float)*num_positions*3);

    if(flags&SOL_MESH_PROPERTY_ADGACENCY)
    {
        adjacency=malloc(sizeof(uint16_t)*num_faces*6);
    }
    else adjacency=NULL;

    if(flags&SOL_MESH_PROPERTY_PER_FACE_MATERIAL)
    {
        materials=malloc(sizeof(uint16_t)*num_faces);
    }
    else materials=NULL;


    num_tex_coords=num_normals=num_positions=num_faces=0;
    current_matreial=0;

    strcpy(buffer,name);
    strcat(buffer,".obj");

    f=fopen(buffer,"r");

    while(fgets(buffer,1024,f))
    {
        if(buffer[0]=='f' && buffer[1]==' ')
        {
            if(has_normals && has_tex_coords)
            {
                if(sscanf(buffer+2,"%hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu",vb+0,vtb+0,vnb+0,vb+1,vtb+1,vnb+1,vb+2,vtb+2,vnb+2) != 9)
                {
                    fprintf(stderr,"FAILED LOADING INDICES VTN: %s : %s\n",name,buffer);
                    return -1;
                }
            }
            else if(has_tex_coords)
            {
                if(sscanf(buffer+2,"%hu/%hu %hu/%hu %hu/%hu",vb+0,vtb+0,vb+1,vtb+1,vb+2,vtb+2) != 6)
                {
                    fprintf(stderr,"FAILED LOADING INDICES VT: %s : %s\n",name,buffer);
                    return -1;
                }
            }
            else if(has_normals)
            {
                if(sscanf(buffer+2,"%hu//%hu %hu//%hu %hu//%hu",vb+0,vnb+0,vb+1,vnb+1,vb+2,vnb+2) != 6)
                {
                    fprintf(stderr,"FAILED LOADING INDICES VN: %s : %s\n",name,buffer);
                    return -1;
                }
            }
            else if(sscanf(buffer+2,"%hu %hu %hu",vb+0,vb+1,vb+2) != 3)
            {
                fprintf(stderr,"FAILED LOADING INDICES V: %s : %s\n",name,buffer);
                return -1;
            }

            v_indices[num_faces*3+0]=vb[0];
            v_indices[num_faces*3+1]=vb[1];
            v_indices[num_faces*3+2]=vb[2];

            if(flags&SOL_MESH_PROPERTY_PER_FACE_MATERIAL) materials[num_faces]=current_matreial;
            num_faces++;
        }
        else if(buffer[0]=='v')
        {
            if(buffer[1]==' ')
            {
                if(sscanf(buffer+2,"%f %f %f",positions+num_positions*3,positions+num_positions*3+1,positions+num_positions*3+2) != 3)
                {
                    fprintf(stderr,"FAILED LOADING VERTICES : %s : %s\n",name,buffer);
                    return -1;
                }
                num_positions++;
            }
            else if(buffer[1]=='t' && buffer[2]==' ')
            {
                /// texture_coords
            }
            else if(buffer[1]=='n' && buffer[2]==' ')
            {
                /// vertex_normals
            }
        }
        ///check material here!
    }

    ///if handling tex_coords and normals will need to duplicate resources here so that verts match up (NYI)
    ///must be handled before touching adjacency

    ///need to free vertex, normal and texture coordinate indices here

    ///following are hack for now as above is NYI (which would decide size of this buffer)
    indices=v_indices;
    num_verts=num_positions;
    interleaved_vertex_data_size=sizeof(float)*3;
    interleaved_vertex_data=positions;

    fclose(f);

    for(i=0;i<num_faces;i++) indices[i*3+0]-=1,indices[i*3+1]-=1,indices[i*3+2]-=1;

    if(flags & SOL_MESH_PROPERTY_ADGACENCY) for(i=0;i<num_faces;i++)
    {
        adjacency[i*6+0]=indices[i*3+0];
        adjacency[i*6+2]=indices[i*3+1];
        adjacency[i*6+4]=indices[i*3+2];

        adj_count=0;

        for(j=0;j<num_faces;j++) if(i!=j)
        {
            pm=0;pa=3;n=0;
            for(k=0;k<3;k++)
            {
                ///should comfortably be able to be elseif, could do with some more asserts
                if(indices[i*3+0]==indices[j*3+k])      pa-=k,n++,pm+=2;
                else if(indices[i*3+1]==indices[j*3+k]) pa-=k,n++;//,pm+=0;
                else if(indices[i*3+2]==indices[j*3+k]) pa-=k,n++,pm+=4;
            }
            assert(n!=3);///technically this can occur if your geometry has 2 triangles back to back
            if(n==2)
            {
                adj_count+=pm;
                adjacency[i*6+pm-1]=indices[j*3+pa];
            }
        }

        if(adj_count!=12)puts("UNABLE TO CONSTRUCT ADJACENCY MESH FROM MODEL (IS IT NON-MANIFOLD ?)");
    }



    strcpy(buffer,name);
    strcat(buffer,".mesh");

    f=fopen(buffer,"wb");

    if(!f)
    {
        printf("UNABLE TO CREATE MESH FILE");
        return -1;
    }

    ///header/metadta
    printf("%zu\n",fwrite(&cvm_mesh_file_signiature,sizeof(uint16_t),1,f));
    printf("%zu\n",fwrite(&cvm_mesh_version_number,sizeof(uint16_t),1,f));
    printf("%zu\n",fwrite(&flags,sizeof(uint16_t),1,f));
    printf("%zu\n",fwrite(&num_verts,sizeof(uint16_t),1,f));
    printf("%zu\n",fwrite(&num_faces,sizeof(uint32_t),1,f));

    ///contents
    printf("%zu\n",fwrite(indices,sizeof(uint16_t),num_faces*3,f));
    if(flags & SOL_MESH_PROPERTY_ADGACENCY) printf("%zu\n",fwrite(adjacency,sizeof(uint16_t),num_faces*6,f));
    if(flags & SOL_MESH_PROPERTY_PER_FACE_MATERIAL) printf("%zu\n",fwrite(materials,sizeof(uint16_t),num_faces,f));
    printf("%zu\n",fwrite(interleaved_vertex_data,interleaved_vertex_data_size,num_verts,f));
    ///write norms
    ///write texture coordinates

    fclose(f);

    free(interleaved_vertex_data);
    free(indices);
    free(adjacency);
    free(materials);

    return 0;
}