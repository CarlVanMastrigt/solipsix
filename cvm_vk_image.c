/**
Copyright 2021,2022,2024 Carl van Mastrigt

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

#include "solipsix.h"


void cvm_vk_supervised_image_initialise(struct cvm_vk_supervised_image* supervised_image, VkImage image, VkImageView view)
{
    supervised_image->image = image;
    supervised_image->view = view;

    supervised_image->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    supervised_image->write_stage_mask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    supervised_image->write_access_mask = VK_ACCESS_2_NONE;

    supervised_image->read_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    supervised_image->read_access_mask = VK_ACCESS_2_NONE;
}

void cvm_vk_supervised_image_terminate(struct cvm_vk_supervised_image* supervised_image)
{
    /// nothing to clean up!
}

void cvm_vk_supervised_image_barrier(struct cvm_vk_supervised_image* supervised_image, VkCommandBuffer cb, VkImageLayout new_layout, VkPipelineStageFlagBits2 dst_stage_mask, VkAccessFlagBits2 dst_access_mask)
{
    /// thse definitions do restrict future use, but there isnt a good way around that, would be good to specify these values on the device...
    static const VkAccessFlagBits2 all_image_read_access_mask =
        VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT |
        VK_ACCESS_2_SHADER_READ_BIT |
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
        VK_ACCESS_2_TRANSFER_READ_BIT |
        VK_ACCESS_2_HOST_READ_BIT |
        VK_ACCESS_2_MEMORY_READ_BIT |
        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT |
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
        VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR |
        VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR;

    static const VkAccessFlagBits2 all_image_write_access_mask =
        VK_ACCESS_2_SHADER_WRITE_BIT |
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_2_TRANSFER_WRITE_BIT |
        VK_ACCESS_2_HOST_WRITE_BIT |
        VK_ACCESS_2_MEMORY_WRITE_BIT |
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
        VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR |
        VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR;



    /// need to wait on prior writes, waiting on reads (even in write case) is not necessary if there exists an execution dependency (this barrier is the execution dependency)
    const VkPipelineStageFlagBits2 src_stage_mask  = supervised_image->write_stage_mask;
    const VkAccessFlagBits2        src_access_mask = supervised_image->write_access_mask;

    assert((dst_access_mask & (all_image_read_access_mask | all_image_write_access_mask)) == dst_access_mask);
    /// unknown/incompatible access mask used!


    if(dst_access_mask & all_image_write_access_mask)
    {
        /// is a write op

        /// reset all barriers, fresh slate, assumes this barrier transitively includes prior read & write scope
        supervised_image->write_stage_mask  = dst_stage_mask;
        supervised_image->write_access_mask = dst_access_mask;

        supervised_image->read_stage_mask = VK_PIPELINE_STAGE_2_NONE;
        supervised_image->read_access_mask = VK_ACCESS_2_NONE;
    }
    else
    {
        /// is a read op

        const bool stages_already_barriered   = (dst_stage_mask  & supervised_image->read_stage_mask ) == dst_stage_mask;
        const bool accesses_already_barriered = (dst_access_mask & supervised_image->read_access_mask) == dst_access_mask;

        if(stages_already_barriered && accesses_already_barriered && supervised_image->current_layout == new_layout)
        {
            /// barrier has already happened
            return;
        }

        /// for ease of future checks on access-stage combinations we must barrier on the union of this and prior stage-access masks
        /// is suboptimal, but shouldn't be an issue under most usage patterns
        dst_stage_mask  |= supervised_image->read_stage_mask;
        dst_access_mask |= supervised_image->read_access_mask;

        supervised_image->read_stage_mask  = dst_stage_mask;
        supervised_image->read_access_mask = dst_access_mask;
    }


    VkDependencyInfo barrier_dependencies =
    {
        .sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext=NULL,
        .dependencyFlags=0,
        .memoryBarrierCount=0,
        .pMemoryBarriers=NULL,
        .bufferMemoryBarrierCount=0,
        .pBufferMemoryBarriers=NULL,
        .imageMemoryBarrierCount=1,
        .pImageMemoryBarriers=(VkImageMemoryBarrier2[1])
        {
            {
                .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext=NULL,
                .srcStageMask = src_stage_mask,
                .srcAccessMask = src_access_mask,
                .dstStageMask = dst_stage_mask,
                .dstAccessMask = dst_access_mask,
                .oldLayout = supervised_image->current_layout,
                .newLayout = new_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = supervised_image->image,
                .subresourceRange=(VkImageSubresourceRange)
                {
                    .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel=0,
                    .levelCount=1,
                    .baseArrayLayer=0,
                    .layerCount=1
                }
            }
        }
    };

    vkCmdPipelineBarrier2(cb, &barrier_dependencies);

    supervised_image->current_layout = new_layout;
}






void cvm_vk_create_image_atlas(cvm_vk_image_atlas* atlas,VkImage image,VkImageView image_view,size_t bytes_per_pixel,uint32_t width,uint32_t height,bool multithreaded)
{
    uint32_t i,j;

    assert(width > 1<<CVM_VK_BASE_TILE_SIZE_FACTOR && height > 1<<CVM_VK_BASE_TILE_SIZE_FACTOR);///IMAGE ATLAS TOO SMALL

    mtx_init(&atlas->structure_mutex,mtx_plain);
    atlas->multithreaded = multithreaded;

    cvm_vk_supervised_image_initialise(&atlas->supervised_image, image, image_view);

    atlas->bytes_per_pixel = bytes_per_pixel;

    assert((width>=(1<<CVM_VK_BASE_TILE_SIZE_FACTOR)) && (height>=(1<<CVM_VK_BASE_TILE_SIZE_FACTOR)));/// IMAGE ATLAS TOO SMALL
    assert(!(width & (width-1)) && !(height & (height-1)));///IMAGE ATLAS MUST BE CREATED AT POWER OF 2 WIDTH AND HEIGHT

    atlas->num_tile_sizes_h=cvm_lbs_32(width)+1-CVM_VK_BASE_TILE_SIZE_FACTOR;
    atlas->num_tile_sizes_v=cvm_lbs_32(height)+1-CVM_VK_BASE_TILE_SIZE_FACTOR;



    cvm_vk_image_atlas_tile * tiles=malloc(CVM_VK_RESERVED_IMAGE_ATLAS_TILE_COUNT*sizeof(cvm_vk_image_atlas_tile));

    cvm_vk_image_atlas_tile * next=NULL;
    i=CVM_VK_RESERVED_IMAGE_ATLAS_TILE_COUNT;
    while(i--)
    {
        tiles[i].next_h=next;
        tiles[i].is_base=!i;
        next=tiles+i;
    }

    atlas->first_unused_tile=tiles;
    atlas->unused_tile_count=CVM_VK_RESERVED_IMAGE_ATLAS_TILE_COUNT;

    atlas->available_tiles=malloc(atlas->num_tile_sizes_h*sizeof(cvm_vk_available_atlas_tile_heap*));

    for(i=0;i<atlas->num_tile_sizes_h;i++)
    {
        atlas->available_tiles[i]=malloc(atlas->num_tile_sizes_v*sizeof(cvm_vk_available_atlas_tile_heap));

        for(j=0;j<atlas->num_tile_sizes_v;j++)
        {
            atlas->available_tiles[i][j].heap=malloc(4*sizeof(cvm_vk_image_atlas_tile*));
            atlas->available_tiles[i][j].space=4;
            atlas->available_tiles[i][j].count=0;
        }
    }

    for(i=0;i<16;i++)atlas->available_tiles_bitmasks[i]=0;

    ///starts off as one big tile

    cvm_vk_image_atlas_tile * t=atlas->first_unused_tile;
    atlas->first_unused_tile=t->next_h;
    atlas->unused_tile_count--;

    t->prev_h=NULL;
    t->next_h=NULL;

    t->prev_v=NULL;
    t->next_v=NULL;

    t->x_pos=0;
    t->y_pos=0;

    t->heap_index=0;
    t->size_factor_h=atlas->num_tile_sizes_h-1;
    t->size_factor_v=atlas->num_tile_sizes_v-1;

    t->available=true;

    atlas->available_tiles[t->size_factor_h][t->size_factor_v].heap[0]=t;
    atlas->available_tiles[t->size_factor_h][t->size_factor_v].count=1;

    atlas->available_tiles_bitmasks[t->size_factor_h]=1<<t->size_factor_v;
}

void cvm_vk_destroy_image_atlas(cvm_vk_image_atlas* atlas)
{
    uint32_t i,j;
    cvm_vk_image_atlas_tile *first,*last,*current,*next;

    ///one tile of max size is below, represent fully relinquished / empty atlas
    assert(atlas->available_tiles[atlas->num_tile_sizes_h-1][atlas->num_tile_sizes_v-1].count==1);///TRYING TO DESTROY AN IMAGE ATLAS WITH ACTIVE TILES

    first=atlas->available_tiles[atlas->num_tile_sizes_h-1][atlas->num_tile_sizes_v-1].heap[0];///return the last, base tile to the list to be processed (unused tiles)
    first->next_h=atlas->first_unused_tile;
    atlas->first_unused_tile=first;

    for(i=0;i<atlas->num_tile_sizes_h;i++)
    {
        for(j=0;j<atlas->num_tile_sizes_v;j++)
        {
            free(atlas->available_tiles[i][j].heap);
        }

        free(atlas->available_tiles[i]);
    }

    free(atlas->available_tiles);

    first=last=NULL;
    ///get all base tiles in their own linked list, discarding all others
    for(current=atlas->first_unused_tile;current;current=next)
    {
        next=current->next_h;
        if(current->is_base)
        {
            if(last) last=last->next_h=current;
            else first=last=current;
        }
    }
    last->next_h=NULL;

    for(current=first;current;current=next)
    {
        next=current->next_h;
        free(current);///free all base allocations (to mathch appropriate allocs)
    }

    cvm_vk_supervised_image_terminate(&atlas->supervised_image);

    mtx_destroy(&atlas->structure_mutex);
}

///probably worth having this as function, allows calling from branches

//static void split_tile();

cvm_vk_image_atlas_tile * cvm_vk_acquire_image_atlas_tile(cvm_vk_image_atlas* atlas,uint32_t width,uint32_t height)
{
    uint32_t i,desired_size_factor_h,desired_size_factor_v,size_factor_h,size_factor_v,current,up,down,offset,offset_mid,offset_end;
    cvm_vk_image_atlas_tile *base,*partner,*adjacent;
    cvm_vk_image_atlas_tile ** heap;


    desired_size_factor_h=(width<CVM_VK_BASE_TILE_SIZE)?0:cvm_po2_32_gte(width)-CVM_VK_BASE_TILE_SIZE_FACTOR;
    desired_size_factor_v=(height<CVM_VK_BASE_TILE_SIZE)?0:cvm_po2_32_gte(height)-CVM_VK_BASE_TILE_SIZE_FACTOR;

    if(desired_size_factor_h>=atlas->num_tile_sizes_h || desired_size_factor_v>=atlas->num_tile_sizes_v)return NULL;///i dont expect this to get hit so dont bother doing earlier easier test

    if(atlas->multithreaded)
    {
        mtx_lock(&atlas->structure_mutex);
    }

    if(atlas->unused_tile_count < atlas->num_tile_sizes_h+ atlas->num_tile_sizes_v)
    {
        base=malloc(CVM_VK_RESERVED_IMAGE_ATLAS_TILE_COUNT*sizeof(cvm_vk_image_atlas_tile));

        partner=atlas->first_unused_tile;
        i=CVM_VK_RESERVED_IMAGE_ATLAS_TILE_COUNT;
        while(i--)
        {
            base[i].next_h=partner;
            base[i].is_base=!i;
            partner=base+i;
        }

        atlas->first_unused_tile=base;
        atlas->unused_tile_count+=CVM_VK_RESERVED_IMAGE_ATLAS_TILE_COUNT;
    }


    base=NULL;

    for(size_factor_h=desired_size_factor_h;size_factor_h<atlas->num_tile_sizes_h;size_factor_h++)
    {
        if(atlas->available_tiles_bitmasks[size_factor_h]>=1u<<desired_size_factor_v)
        {
//            size_factor_v=__builtin_ctz(atlas->available_tiles_bitmasks[size_factor_h]&~((1<<desired_size_factor_v)-1));
            size_factor_v=__builtin_ctz(atlas->available_tiles_bitmasks[size_factor_h]>>desired_size_factor_v)+desired_size_factor_v;

            if(!base || (uint32_t)base->size_factor_h+(uint32_t)base->size_factor_v > size_factor_h+size_factor_v)
            {
                base=atlas->available_tiles[size_factor_h][size_factor_v].heap[0];
            }
        }
    }

    if(!base)///a tile to split was not found!
    {
        if(atlas->multithreaded)
        {
            mtx_unlock(&atlas->structure_mutex);
        }
        return NULL;///no tile large enough exists
    }

    ///extract from heap
    heap=atlas->available_tiles[base->size_factor_h][base->size_factor_v].heap;

    if((current= --atlas->available_tiles[base->size_factor_h][base->size_factor_v].count))
    {
        up=0;

        while((down=(up<<1)+1)<current)
        {
            down+= (down+1<current) && (heap[down+1]->offset < heap[down]->offset);
            if(heap[current]->offset < heap[down]->offset) break;
            heap[down]->heap_index=up;///need to know (new) index in heap
            heap[up]=heap[down];
            up=down;
        }

        heap[current]->heap_index=up;
        heap[up]=heap[current];
    }
    else atlas->available_tiles_bitmasks[base->size_factor_h]&= ~(1<<base->size_factor_v);///took last tile of this size


    while(base->size_factor_h>desired_size_factor_h || base->size_factor_v>desired_size_factor_v)
    {
        partner=atlas->first_unused_tile;
        atlas->first_unused_tile=partner->next_h;
        atlas->unused_tile_count--;

        ///here if offset isnt equal to relevant adjacent position then adjacent is larger and starts before the relevant (v/h) block that contains the tile we're splitting

        if(base->size_factor_h != desired_size_factor_h && (base->size_factor_v==desired_size_factor_v || base->size_factor_h>=base->size_factor_v))///try to keep tiles square where possible, with slight bias towards wider tiles
        {
            ///split vertically
            base->size_factor_h--;

            partner->offset=base->offset;
            partner->x_pos+=1<<base->size_factor_h;

            adjacent=partner->next_h=base->next_h;
            partner->prev_h=base;
            base->next_h=partner;

            ///adjust adjacency info, traverse all edges finding tiles that should link to partner now

            ///traverse right edge
            offset=base->y_pos;
            offset_end=offset+(1<<base->size_factor_v);
            if(adjacent && adjacent->y_pos == offset)
            {
                while(offset < offset_end)
                {
                    adjacent->prev_h=partner;
                    offset+=1<<adjacent->size_factor_v;
                    adjacent=adjacent->next_v;
                }
            }

            ///traverse bottom edge
            adjacent=partner->next_v=base->next_v;
            offset=base->x_pos;
            offset_mid=offset+(1<<base->size_factor_h);
            offset_end=offset_mid+(1<<base->size_factor_h);
            if(adjacent && adjacent->x_pos == offset)
            {
                while(offset < offset_end)
                {
                    if(offset==offset_mid)partner->next_v=adjacent;
                    if(offset>=offset_mid)adjacent->prev_v=partner;
                    offset+=1<<adjacent->size_factor_h;
                    adjacent=adjacent->next_h;
                }
            }

            ///traverse top edge
            adjacent=partner->prev_v=base->prev_v;
            offset=base->x_pos;
            /// o_mid and o_end unchanged
            if(adjacent && adjacent->x_pos == offset)
            {
                while(offset < offset_end)
                {
                    while(adjacent->next_v!=base) adjacent=adjacent->next_v;
                    if(offset==offset_mid)partner->prev_v=adjacent;
                    if(offset>=offset_mid)adjacent->next_v=partner;
                    offset+=1<<adjacent->size_factor_h;
                    adjacent=adjacent->next_h;
                }
            }
        }
        else
        {
            ///split horizontally
            base->size_factor_v--;

            partner->offset=base->offset;
            partner->y_pos+=1<<base->size_factor_v;

            adjacent=partner->next_v=base->next_v;
            partner->prev_v=base;
            base->next_v=partner;

            ///adjust adjacency info, traverse all edges finding tiles that should link to partner now

            ///traverse bottom edge
            offset=base->x_pos;
            offset_end=offset+(1<<base->size_factor_h);
            if(adjacent && adjacent->x_pos == offset)
            {
                while(offset < offset_end)
                {
                    adjacent->prev_v=partner;
                    offset+=1<<adjacent->size_factor_h;
                    adjacent=adjacent->next_h;
                }
            }

            ///traverse right edge
            adjacent=partner->next_h=base->next_h;
            offset=base->y_pos;
            offset_mid=offset+(1<<base->size_factor_v);
            offset_end=offset_mid+(1<<base->size_factor_v);
            if(adjacent && adjacent->y_pos == offset)
            {
                while(offset < offset_end)
                {
                    if(offset==offset_mid)partner->next_h=adjacent;
                    if(offset>=offset_mid)adjacent->prev_h=partner;
                    offset+=1<<adjacent->size_factor_v;
                    adjacent=adjacent->next_v;
                }
            }

            ///traverse left edge
            adjacent=partner->prev_h=base->prev_h;
            offset=base->y_pos;
            /// o_mid and o_end unchanged
            if(adjacent && adjacent->y_pos == offset)
            {
                while(offset < offset_end)
                {
                    while(adjacent->next_h!=base) adjacent=adjacent->next_h;
                    if(offset==offset_mid)partner->prev_h=adjacent;
                    if(offset>=offset_mid)adjacent->next_h=partner;
                    offset+=1<<adjacent->size_factor_v;
                    adjacent=adjacent->next_v;
                }
            }
        }

        partner->heap_index=0;///must be no tiles already in heap this tile will be put in (otherwise we'd be splitting that one)
        atlas->available_tiles[base->size_factor_h][base->size_factor_v].heap[0]=partner;
        atlas->available_tiles[base->size_factor_h][base->size_factor_v].count=1;

        atlas->available_tiles_bitmasks[base->size_factor_h] |= 1<<base->size_factor_v;

        partner->available=true;
        partner->size_factor_h=base->size_factor_h;
        partner->size_factor_v=base->size_factor_v;
    }

    base->available=false;

    if(atlas->multithreaded)
    {
        mtx_unlock(&atlas->structure_mutex);
    }

    return base;
}

static inline void remove_tile_from_availability_heap(cvm_vk_image_atlas* atlas,cvm_vk_image_atlas_tile * t,uint32_t size_factor_h,uint32_t size_factor_v)
{
    cvm_vk_image_atlas_tile ** heap;
    uint32_t current,up,down;

    if((current= --atlas->available_tiles[size_factor_h][size_factor_v].count))
    {
        heap=atlas->available_tiles[size_factor_h][size_factor_v].heap;

        down=t->heap_index;
        while(down && heap[current]->offset < heap[(up=(down-1)>>1)]->offset)///try moving up heap
        {
            heap[up]->heap_index=down;
            heap[down]=heap[up];
            down=up;
        }
        up=down;
        while((down=(up<<1)+1)<current)///try moving down heap
        {
            down+= down+1<current && heap[down+1]->offset < heap[down]->offset;
            if(heap[current]->offset < heap[down]->offset) break;
            heap[down]->heap_index=up;
            heap[up]=heap[down];
            up=down;
        }

        heap[current]->heap_index=up;
        heap[up]=heap[current];
    }
    else atlas->available_tiles_bitmasks[size_factor_h]&= ~(1<<size_factor_v);///partner was last available tile of this size


    ///add to list of unused tiles
    t->next_h=atlas->first_unused_tile;
    atlas->first_unused_tile=t;
    atlas->unused_tile_count++;
}

void cvm_vk_relinquish_image_atlas_tile(cvm_vk_image_atlas* atlas,cvm_vk_image_atlas_tile * base)
{
    uint32_t up,down,offset,offset_end;
    cvm_vk_image_atlas_tile *partner,*adjacent;
    cvm_vk_image_atlas_tile ** heap;
    bool finished_h,finished_v;

    if(!base)return;

    ///base,partner,test

    if(atlas->multithreaded)
    {
        mtx_lock(&atlas->structure_mutex);
    }

    finished_h=finished_v=false;

    do
    {
        ///combine horizontally adjacent
        while(!finished_h && (base->size_factor_h<=base->size_factor_v || finished_v))
        {
            if(base->x_pos & 1<<base->size_factor_h)///base is right of current horizontal tile pair
            {
                if((partner=base->prev_h) && partner->size_factor_h==base->size_factor_h && partner->size_factor_v==base->size_factor_v && partner->available)
                {
                    base->offset=partner->offset;
                    ///traverse left side
                    adjacent=base->prev_h=partner->prev_h;
                    offset=partner->y_pos;
                    offset_end=offset+(1<<base->size_factor_v);
                    if(adjacent && adjacent->y_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            while(adjacent->next_h!=partner)adjacent=adjacent->next_h;
                            adjacent->next_h=base;
                            offset+=1<<adjacent->size_factor_v;
                            adjacent=adjacent->next_v;
                        }
                    }

                    ///traverse bottom side
                    adjacent=base->next_v=partner->next_v;
                    offset=partner->x_pos;
                    offset_end=offset+(1<<base->size_factor_h);
                    if(adjacent && adjacent->x_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            adjacent->prev_v=base;
                            offset+=1<<adjacent->size_factor_h;
                            adjacent=adjacent->next_h;
                        }
                    }

                    ///traverse top side
                    adjacent=base->prev_v=partner->prev_v;
                    offset=partner->x_pos;
                    ///o_end hasn't changed
                    if(adjacent && adjacent->x_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            while(adjacent->next_v!=partner)adjacent=adjacent->next_v;
                            adjacent->next_v=base;
                            offset+=1<<adjacent->size_factor_h;
                            adjacent=adjacent->next_h;
                        }
                    }

                    remove_tile_from_availability_heap(atlas,partner,base->size_factor_h,base->size_factor_v);

                    base->size_factor_h++;

                    finished_v=false;///new combination potential
                }
                else finished_h=true;
            }
            else///base is left of current horizontal tile pair
            {
                if((partner=base->next_h) && partner->size_factor_h==base->size_factor_h && partner->size_factor_v==base->size_factor_v && partner->available)
                {
                    ///traverse right side
                    adjacent=base->next_h=partner->next_h;
                    offset=partner->y_pos;
                    offset_end=offset+(1<<base->size_factor_v);
                    if(adjacent && adjacent->y_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            adjacent->prev_h=base;
                            offset+=1<<adjacent->size_factor_v;
                            adjacent=adjacent->next_v;
                        }
                    }

                    ///traverse bottom side
                    adjacent=partner->next_v;
                    offset=partner->x_pos;
                    offset_end=offset+(1<<base->size_factor_h);
                    if(adjacent && adjacent->x_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            adjacent->prev_v=base;
                            offset+=1<<adjacent->size_factor_h;
                            adjacent=adjacent->next_h;
                        }
                    }

                    ///traverse top side
                    adjacent=partner->prev_v;
                    offset=partner->x_pos;
                    ///o_end hasn't changed
                    if(adjacent && adjacent->x_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            while(adjacent->next_v!=partner)adjacent=adjacent->next_v;
                            adjacent->next_v=base;
                            offset+=1<<adjacent->size_factor_h;
                            adjacent=adjacent->next_h;
                        }
                    }

                    remove_tile_from_availability_heap(atlas,partner,base->size_factor_h,base->size_factor_v);

                    base->size_factor_h++;

                    finished_v=false;///new combination potential
                }
                else finished_h=true;
            }
        }

        ///combine vertically adjacent
        while(!finished_v && (base->size_factor_v<=base->size_factor_h || finished_h))
        {
            if(base->y_pos & 1<<base->size_factor_v)///base is bottom of current vertical tile pair
            {
                if((partner=base->prev_v) && partner->size_factor_h==base->size_factor_h && partner->size_factor_v==base->size_factor_v && partner->available)
                {
                    base->offset=partner->offset;
                    ///traverse top side
                    adjacent=base->prev_v=partner->prev_v;
                    offset=partner->x_pos;
                    offset_end=offset+(1<<base->size_factor_h);
                    if(adjacent && adjacent->x_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            while(adjacent->next_v!=partner)adjacent=adjacent->next_v;
                            adjacent->next_v=base;
                            offset+=1<<adjacent->size_factor_h;
                            adjacent=adjacent->next_h;
                        }
                    }

                    ///traverse left side
                    adjacent=base->prev_h=partner->prev_h;
                    offset=partner->y_pos;
                    offset_end=offset+(1<<base->size_factor_v);
                    if(adjacent && adjacent->y_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            while(adjacent->next_h!=partner)adjacent=adjacent->next_h;
                            adjacent->next_h=base;
                            offset+=1<<adjacent->size_factor_v;
                            adjacent=adjacent->next_v;
                        }
                    }

                    ///traverse right side
                    adjacent=base->next_h=partner->next_h;
                    offset=partner->y_pos;
                    ///o_end hasn't changed
                    if(adjacent && adjacent->y_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            adjacent->prev_h=base;
                            offset+=1<<adjacent->size_factor_v;
                            adjacent=adjacent->next_v;
                        }
                    }

                    remove_tile_from_availability_heap(atlas,partner,base->size_factor_h,base->size_factor_v);

                    base->size_factor_v++;

                    finished_h=false;///new combination potential
                }
                else finished_v=true;
            }
            else///base is top of current vertical tile pair
            {
                if((partner=base->next_v) && partner->size_factor_h==base->size_factor_h && partner->size_factor_v==base->size_factor_v && partner->available)
                {
                    ///traverse bottom side
                    adjacent=base->next_v=partner->next_v;
                    offset=partner->x_pos;
                    offset_end=offset+(1<<base->size_factor_h);
                    if(adjacent && adjacent->x_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            adjacent->prev_v=base;
                            offset+=1<<adjacent->size_factor_h;
                            adjacent=adjacent->next_h;
                        }
                    }

                    ///traverse left side
                    adjacent=partner->prev_h;
                    offset=partner->y_pos;
                    offset_end=offset+(1<<base->size_factor_v);
                    if(adjacent && adjacent->y_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            while(adjacent->next_h!=partner)adjacent=adjacent->next_h;
                            adjacent->next_h=base;
                            offset+=1<<adjacent->size_factor_v;
                            adjacent=adjacent->next_v;
                        }
                    }

                    ///traverse right side
                    adjacent=partner->next_h;
                    offset=partner->y_pos;
                    ///o_end hasn't changed
                    if(adjacent && adjacent->y_pos==offset)
                    {
                        while(offset < offset_end)
                        {
                            adjacent->prev_h=base;
                            offset+=1<<adjacent->size_factor_v;
                            adjacent=adjacent->next_v;
                        }
                    }

                    remove_tile_from_availability_heap(atlas,partner,base->size_factor_h,base->size_factor_v);

                    base->size_factor_v++;

                    finished_h=false;///new combination potential
                }
                else finished_v=true;
            }
        }

    }
    while(!finished_h || !finished_v);


    down=atlas->available_tiles[base->size_factor_h][base->size_factor_v].count++;
    heap=atlas->available_tiles[base->size_factor_h][base->size_factor_v].heap;

    if(down==atlas->available_tiles[base->size_factor_h][base->size_factor_v].space)
    {
        heap=atlas->available_tiles[base->size_factor_h][base->size_factor_v].heap=realloc(heap,sizeof(cvm_vk_image_atlas_tile*)*(atlas->available_tiles[base->size_factor_h][base->size_factor_v].space*=2));
    }

    while(down && heap[(up=(down-1)>>1)]->offset > base->offset)
    {
        heap[up]->heap_index=down;
        heap[down]=heap[up];
        down=up;
    }

    base->heap_index=down;
    base->available=true;
    heap[down]=base;

    atlas->available_tiles_bitmasks[base->size_factor_h]|=1<<base->size_factor_v;///partner was last available tile of this size

    if(atlas->multithreaded)
    {
        mtx_unlock(&atlas->structure_mutex);
    }
}



/// needs better name
/// should move ACTUAL width and height into the tile (how pressed for space is it actually?) -- well, it does try pretty hard, perhaps too hard to be space efficient
/// bytes_per_pixel an even bigger pain to handle, perhaps byte size of tile? perhaps let it remain implied by user? put it in some spare bits?
struct sol_buffer_segment cvm_vk_stage_image_atlas_upload(struct sol_buffer* upload_buffer, struct cvm_vk_buffer_image_copy_stack* copy_buffer, const struct cvm_vk_image_atlas_tile * atlas_tile, uint32_t width,uint32_t height, uint32_t bytes_per_pixel)
{
    struct sol_buffer_segment segment;

    #warning implement using abse image

    segment = sol_buffer_fetch_aligned_segment(upload_buffer, bytes_per_pixel * width * height, 16);

    *cvm_vk_buffer_image_copy_stack_append_ptr(copy_buffer) = (VkBufferImageCopy)
    {
        .bufferOffset=segment.offset,
        .bufferRowLength=width,
        .bufferImageHeight=height,
        .imageSubresource=(VkImageSubresourceLayers)
        {
            .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel=0,
            .baseArrayLayer=0,
            .layerCount=1
        },
        .imageOffset=(VkOffset3D)
        {
            .x=atlas_tile->x_pos<<CVM_VK_BASE_TILE_SIZE_FACTOR,
            .y=atlas_tile->y_pos<<CVM_VK_BASE_TILE_SIZE_FACTOR,
            .z=0
        },
        .imageExtent=(VkExtent3D)
        {
            .width=width,
            .height=height,
            .depth=1,
        }
    };

    return segment;
}



/// should REALLY have an implicit version of this to allow the render pass to do layout transitions
void cvm_vk_image_atlas_barrier(cvm_vk_image_atlas* atlas, VkCommandBuffer cb, VkImageLayout new_layout, VkPipelineStageFlagBits2 dst_stage_mask, VkAccessFlagBits2 dst_access_mask)
{
    cvm_vk_supervised_image_barrier(&atlas->supervised_image, cb, new_layout, dst_stage_mask, dst_access_mask);
}

void cvm_vk_image_atlas_submit_all_pending_copy_actions(cvm_vk_image_atlas* atlas, VkCommandBuffer transfer_cb, VkBuffer staging_buffer, VkDeviceSize staging_base_offset, struct cvm_vk_buffer_image_copy_stack* pending_copy_actions)
{
    uint32_t i, copy_count;
    VkBufferImageCopy* copy_actions;

    copy_count = pending_copy_actions->count;
    copy_actions = pending_copy_actions->data;

    if(copy_count)
    {
        cvm_vk_image_atlas_barrier(atlas, transfer_cb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

        for(i=0;i<copy_count;i++)
        {
            copy_actions[i].bufferOffset += staging_base_offset;
        }
        vkCmdCopyBufferToImage(transfer_cb, staging_buffer, atlas->supervised_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy_count ,copy_actions);
    }

    /// can reset stack as all copies were performed
    cvm_vk_buffer_image_copy_stack_reset(pending_copy_actions);
}




