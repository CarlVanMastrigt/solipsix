/**
Copyright 2021,2022,2023 Carl van Mastrigt

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


void cvm_vk_managed_buffer_create(cvm_vk_managed_buffer * mb,uint32_t buffer_size,uint32_t min_size_factor,VkBufferUsageFlags usage,bool host_visible)
{
    uint32_t i,next;
    VkMemoryPropertyFlags required_memory_properties,desired_memory_properties;

    usage|=VK_BUFFER_USAGE_TRANSFER_DST_BIT;///for when its necessary to copy into the buffer (device only memory)

    buffer_size=(((buffer_size-1) >> min_size_factor)+1) << min_size_factor;///round to multiple of 1<<min_size_factor
    mb->total_space=buffer_size;
    mb->permanent_offset=buffer_size;
    mb->temporary_offset=0;

    mb->temporary_allocation_data_space=128;
    mb->temporary_allocation_data=malloc(mb->temporary_allocation_data_space*sizeof(cvm_vk_temporary_buffer_allocation_data));

    i=mb->temporary_allocation_data_space;
    next=CVM_INVALID_U32_INDEX;
    while(i--)
    {
        mb->temporary_allocation_data[i].next=next;
#ifndef	NDEBUG
        mb->temporary_allocation_data[i].available=true;///for checking allocation validity
#endif // NDEBUG
        next=i;
    }

    mb->first_unused_temporary_allocation=0;///put all allocations in linked list of unused allocations
    mb->unused_temporary_allocation_count=mb->temporary_allocation_data_space;
    mb->last_used_allocation=CVM_INVALID_U32_INDEX;

    for(i=0;i<32;i++)
    {
        mb->available_temporary_allocations[i].heap=malloc(sizeof(uint32_t)*16);
        mb->available_temporary_allocations[i].space=16;
        mb->available_temporary_allocations[i].count=0;
    }
    mb->available_temporary_allocation_bitmask=0;

    mb->base_temporary_allocation_size_factor=min_size_factor;

    cvm_atomic_lock_create(&mb->allocation_management_spinlock);

    mb->mapping=NULL;
    mb->mapping_coherent=false;

    if(host_visible)
    {
        required_memory_properties=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        desired_memory_properties=0;
    }
    else
    {
        required_memory_properties=0;
        desired_memory_properties=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    cvm_vk_create_buffer(&mb->buffer,&mb->memory,usage,buffer_size,(void**)&mb->mapping,&mb->mapping_coherent,required_memory_properties,desired_memory_properties);
    ///if this is non-null then probably UMA and thus can circumvent staging buffer

    cvm_atomic_lock_create(&mb->copy_spinlock);

    #warning should have a runtime method to check if following is necessary, will be a decent help in setting up module buffer sizes
    mb->staging_buffer=NULL;

    cvm_vk_buffer_copy_stack_initialise(&mb->pending_copies, 64);
    cvm_vk_buffer_barrier_stack_initialise(&mb->copy_release_barriers, 64);

    mb->copy_update_counter=0;
    mb->copy_queue_bitmask=0;
}

void cvm_vk_managed_buffer_destroy(cvm_vk_managed_buffer * mb)
{
    uint32_t i;

    cvm_vk_destroy_buffer(mb->buffer,mb->memory,mb->mapping);
    ///all temporary allocations should be freed at this point
    assert(mb->last_used_allocation==CVM_INVALID_U32_INDEX);///ATTEMPTED TO DESTROY A BUFFER WITH ACTIVE ELEMENTS

    for(i=0;i<32;i++)
    {
        free(mb->available_temporary_allocations[i].heap);
    }

    free(mb->temporary_allocation_data);

    cvm_vk_buffer_copy_stack_terminate(&mb->pending_copies);
    cvm_vk_buffer_barrier_stack_terminate(&mb->copy_release_barriers);
}

bool cvm_vk_managed_buffer_acquire_temporary_allocation(cvm_vk_managed_buffer * mb,uint64_t size,uint32_t * allocation_index,uint64_t * allocation_offset)
{
    uint32_t i,offset,size_bitmask,up,down,end,size_factor,available_bitmask,additional_allocations;///up down current are for binary heap management
    uint32_t *heap;
    uint32_t prev_index,next_index;
    cvm_vk_temporary_buffer_allocation_data *allocations,*prev_data,*next_data;

    assert(allocation_index);

    assert(size < mb->total_space);

    size_factor=cvm_po2_64_gte(size);
    size_factor=(size_factor>mb->base_temporary_allocation_size_factor)*(size_factor-mb->base_temporary_allocation_size_factor);

    assert(size_factor<32);
    if(size_factor>=32)return false;

    cvm_atomic_lock_acquire(&mb->allocation_management_spinlock);

    /// seeing as this function must be as fast and light as possible, we should track unused allocation count to determine if there are enough unused allocations left (with allowance for inaccuracy)
    if(mb->unused_temporary_allocation_count < 32)
    {
        next_index=mb->first_unused_temporary_allocation;

        additional_allocations = mb->temporary_allocation_data_space;
        assert(additional_allocations>=32);
        assert(!(additional_allocations&(additional_allocations-1)));

        mb->temporary_allocation_data = allocations = realloc(mb->temporary_allocation_data,sizeof(cvm_vk_temporary_buffer_allocation_data)*(mb->temporary_allocation_data_space+additional_allocations));

        for(i=mb->temporary_allocation_data_space+additional_allocations-1;i>=mb->temporary_allocation_data_space;i--)
        {
            allocations[i].next=next_index;
#ifndef	NDEBUG
            allocations[i].available=true;///for checking allocation validity
#endif // NDEBUG
            next_index=i;
        }
        mb->first_unused_temporary_allocation=next_index;///same as next

        mb->unused_temporary_allocation_count+=additional_allocations;
        mb->temporary_allocation_data_space+=additional_allocations;
    }
    else allocations=mb->temporary_allocation_data;
    ///above ensures there will be enough unused allocations for any case below (is overly conservative. but is a quick, rarely failing test)

    available_bitmask=mb->available_temporary_allocation_bitmask;

    if(available_bitmask>>size_factor)///there exists an allocation of the desired size or larger
    {
        ///find lowest size with available allocation
        i=cvm_lbs_32(available_bitmask>>size_factor)+size_factor;

        heap=mb->available_temporary_allocations[i].heap;

        prev_index=heap[0];///what we want, the lowest offset available allocation of the required size or smaller
        prev_data=allocations+prev_index;

        prev_data->available=false;
        prev_data->size_factor=size_factor;

        if((end= --mb->available_temporary_allocations[i].count))///after extraction patch up the heap if its nonempty
        {
            up=0;

            while((down=(up<<1)+1)<end)
            {
                down+= (down+1<end) && (allocations[heap[down+1]].offset < allocations[heap[down]].offset);///find smallest of downwards offset
                if(allocations[heap[end]].offset < allocations[heap[down]].offset) break;///if last element (old count) has smaller offset than next down then early exit
                allocations[heap[down]].heap_index=up;///need to know (new) index in heap
                heap[up]=heap[down];
                up=down;
            }

            allocations[heap[end]].heap_index=up;
            heap[up]=heap[end];
        }
        else available_bitmask&= ~(1<<i);///otherwise, if now empty mark the heap as such

        while(i-- > size_factor)
        {
            next_index=mb->first_unused_temporary_allocation;
            next_data=allocations+next_index;

            mb->first_unused_temporary_allocation=next_data->next;
            mb->unused_temporary_allocation_count--;

            ///horizontal linked list stuff
            allocations[prev_data->next].prev=next_index;///next cannot be CVM_INVALID_U32_INDEX. if it were, then this allocation would have been freed back to the pool (last available allocations are recursively freed)
            next_data->next=prev_data->next;
            next_data->prev=prev_index;
            prev_data->next=next_index;

            next_data->size_factor=i;
            next_data->offset=prev_data->offset+(1u<<i);
            next_data->available=true;
            next_data->heap_index=0;///index in heap
            ///if this allocation size wasn't empty, then this code path wouldn't be taken, ergo addition to heap is super simple

            mb->available_temporary_allocations[i].heap[0]=next_index;
            mb->available_temporary_allocations[i].count=1;

            available_bitmask|=1<<i;
        }

        mb->available_temporary_allocation_bitmask=available_bitmask;

        cvm_atomic_lock_release(&mb->allocation_management_spinlock);

        *allocation_index=prev_index;///set the allocation data
        if(allocation_offset)*allocation_offset=prev_data->offset<<mb->base_temporary_allocation_size_factor;
    }
    else///try to allocate from the end of the buffer
    {
        offset=mb->temporary_offset;///store offset locally

        size_bitmask=(1u<<size_factor)-1u;

        ///rounded offset plus allocation size must be less than end of dynamic buffer space
        if( ((uint64_t)((((offset+size_bitmask)>>size_factor)<<size_factor) + (1u<<size_factor))) << mb->base_temporary_allocation_size_factor > mb->permanent_offset)
        {
            ///not enough memory left
            cvm_atomic_lock_release(&mb->allocation_management_spinlock);
            return false;
        }
        /// ^ extra brackets to make compiler shut up :sob:

        prev_index=mb->last_used_allocation;
        next_index=mb->first_unused_temporary_allocation;///rely on next chain already existing here, but next does need to be set on last used allocation
        if(prev_index!=CVM_INVALID_U32_INDEX)allocations[prev_index].next=next_index;
        next_data=allocations+next_index;

        for(i=0;offset&size_bitmask;i++) if(offset & 1<<i)
        {
            mb->unused_temporary_allocation_count--;

            next_data->offset=offset;
            next_data->prev=prev_index;///right/next will get set after the fact (after this loop), prev on available linked list has not been set, so will need to set that
            next_data->size_factor=i;
            next_data->available=true;
            next_data->heap_index=mb->available_temporary_allocations[i].count++;///by virtue of being allocated from the end it will have the largest offset, thus will never need to move up the heap
            ///add to heap

            if(next_data->heap_index==mb->available_temporary_allocations[i].space)
            {
                mb->available_temporary_allocations[i].heap=realloc(mb->available_temporary_allocations[i].heap,sizeof(uint32_t)*(mb->available_temporary_allocations[i].space+=16));
            }
            mb->available_temporary_allocations[i].heap[next_data->heap_index]=next_index;

            available_bitmask|=1<<i;

            offset+=1<<i;///increment offset to align

            prev_index=next_index;
            next_index=next_data->next;
            next_data=allocations+next_index;
        }

        mb->unused_temporary_allocation_count--;
        mb->first_unused_temporary_allocation=next_data->next;

        next_data->offset=offset;
        next_data->prev=prev_index;
        next_data->next=CVM_INVALID_U32_INDEX;///last used allocation now after all
        next_data->size_factor=size_factor;
        next_data->available=false;

        mb->last_used_allocation=next_index;
        mb->temporary_offset=offset+(1<<size_factor);
        mb->available_temporary_allocation_bitmask=available_bitmask;

        cvm_atomic_lock_release(&mb->allocation_management_spinlock);

        *allocation_index=next_index;///set the allocation data
        if(allocation_offset)*allocation_offset=offset<<mb->base_temporary_allocation_size_factor;
    }

    return true;
}

void cvm_vk_managed_buffer_release_temporary_allocation(cvm_vk_managed_buffer * mb,uint32_t allocation_index)
{
    uint32_t up,down,end,size_factor,offset;
    uint32_t * heap;
    uint32_t neighbour_index;///represents neighbouring/"buddy" allocation
    cvm_vk_temporary_buffer_allocation_data * allocations;
    cvm_vk_temporary_buffer_allocation_data *allocation_data,*neighbour_data;

    cvm_atomic_lock_acquire(&mb->allocation_management_spinlock);

    allocations=mb->temporary_allocation_data;

    assert(allocation_index<mb->temporary_allocation_data_space);/// INVALID ALLOCATION includes check that allocation_index!=CVM_INVALID_U32_INDEX
    assert(!allocations[allocation_index].available);/// ALLOCATION WAS ALREADY FREED

    if(allocation_index==mb->last_used_allocation)///could also check data.next==CVM_INVALID_U32_INDEX
    {
        ///free space from end until an unavailable allocation is found
        ///need handling for only 1 alloc, which is rightmost

        allocations[allocation_index].next=mb->first_unused_temporary_allocation;///beyond this we can rely on the extant next
#ifndef NDEBUG
        allocations[allocation_index].available=true;///for checking allocation validity
#endif // NDEBUG

        mb->first_unused_temporary_allocation=allocation_index;
        allocation_index=allocations[allocation_index].prev;
        mb->unused_temporary_allocation_count++;

        while(allocation_index!=CVM_INVALID_U32_INDEX && allocations[allocation_index].available)
        {
            size_factor=allocations[allocation_index].size_factor;
            ///remove from heap, index itself cannot have any children as it's the allocation with the largest offset (it's at the end), but the one we replace it with may, so no need to check down when adding
            if((end= --mb->available_temporary_allocations[size_factor].count))
            {
                heap=mb->available_temporary_allocations[size_factor].heap;

                down=allocations[allocation_index].heap_index;
                while(down && allocations[heap[end]].offset < allocations[heap[(up=(down-1)>>1)]].offset)
                {
                    allocations[heap[up]].heap_index=down;
                    heap[down]=heap[up];
                    down=up;
                }

                allocations[heap[end]].heap_index=down;
                heap[down]=heap[end];
            }
            else mb->available_temporary_allocation_bitmask&= ~(1<<size_factor);///just removed last available allocation of this size

            mb->first_unused_temporary_allocation=allocation_index;
            allocation_index=allocations[allocation_index].prev;
            mb->unused_temporary_allocation_count++;
        }

        if(allocation_index!=CVM_INVALID_U32_INDEX)allocations[allocation_index].next=CVM_INVALID_U32_INDEX;
        mb->last_used_allocation=allocation_index;

        mb->temporary_offset=allocations[mb->first_unused_temporary_allocation].offset;///set based on last added, which will be end of removed, a little hacky but w/e
    }
    else /// the difficult one...
    {
        allocation_data=allocations+allocation_index;
        size_factor=allocation_data->size_factor;
        offset=allocation_data->offset;

        while(size_factor+1u < 32)/// combine allocations in aligned way
        {
            ///neighbouring allocation for this alignment
            neighbour_index = offset & 1<<size_factor ? allocation_data->prev : allocation_data->next;/// will never be NULL (if a was last other branch would be taken, if first (offset is 0) n will always be set to next)
            neighbour_data=allocations+neighbour_index;

            if(!neighbour_data->available || neighbour_data->size_factor!=size_factor)break;///an size factor must be the same size or smaller, can only combine if the same size and available

            ///remove n from its availability heap
            ///n's size factor must be same as size factor here
            if((end= --mb->available_temporary_allocations[size_factor].count))
            {
                heap=mb->available_temporary_allocations[size_factor].heap;

                down=neighbour_data->heap_index;
                ///try moving up
                while(down && allocations[heap[end]].offset < allocations[heap[(up=(down-1)>>1)]].offset)
                {
                    allocations[heap[up]].heap_index=down;
                    heap[down]=heap[up];
                    down=up;
                }
                ///try moving down
                up=down;
                while((down=(up<<1)+1)<end)
                {
                    down+= (down+1<end) && (allocations[heap[down+1]].offset < allocations[heap[down]].offset);
                    if(allocations[heap[end]].offset < allocations[heap[down]].offset) break;///will break on first cycle if above loop did anything
                    allocations[heap[down]].heap_index=up;
                    heap[up]=heap[down];
                    up=down;
                }

                allocations[heap[end]].heap_index=up;
                heap[up]=heap[end];
            }
            else mb->available_temporary_allocation_bitmask&= ~(1<<size_factor);///just removed last available allocation of this size

            ///change linked list/ adjacency information
            if(offset&1<<size_factor) /// a is next/right
            {
                offset-=1<<size_factor;
                ///this should be rare enough to set next and prev during loop and still be the most efficient option
                allocation_data->prev=neighbour_data->prev;
                if(allocation_data->prev!=CVM_INVALID_U32_INDEX) allocations[allocation_data->prev].next=allocation_index;///using prev set above
            }
            else allocations[(allocation_data->next=neighbour_data->next)].prev=allocation_index;///next CANNOT be NULL, as last allocation cannot be available (and thus break above would have been hit)

            size_factor++;

            neighbour_data->next=mb->first_unused_temporary_allocation;///put subsumed adjacent allocation in unused list
            mb->first_unused_temporary_allocation=neighbour_index;
            mb->unused_temporary_allocation_count++;
        }

        allocation_data->size_factor=size_factor;
        allocation_data->offset=offset;
        allocation_data->available=true;

        /// add a to appropriate availability heap
        down=mb->available_temporary_allocations[size_factor].count++;///by virtue of being allocated from the end it will have the largest offset, thus will never need to move up the heap

        heap=mb->available_temporary_allocations[size_factor].heap;
        if(down==mb->available_temporary_allocations[size_factor].space)
        {
            heap=mb->available_temporary_allocations[size_factor].heap=realloc(heap,sizeof(uint32_t)*(mb->available_temporary_allocations[size_factor].space+=16));
        }

        while(down && offset < allocations[heap[(up=(down-1)>>1)]].offset)
        {
            allocations[heap[up]].heap_index=down;
            heap[down]=heap[up];
            down=up;
        }

        allocation_data->heap_index=down;
        heap[down]=allocation_index;

        mb->available_temporary_allocation_bitmask|=1<<size_factor;
    }

    cvm_atomic_lock_release(&mb->allocation_management_spinlock);
}


uint64_t cvm_vk_managed_buffer_acquire_permanent_allocation(cvm_vk_managed_buffer * mb,uint64_t size,uint64_t alignment)
{
    uint64_t permanent_offset,temporary_offset;

    cvm_atomic_lock_acquire(&mb->allocation_management_spinlock);

    ///allocating at desired alignment from end of buffer, can simply "round down"
    ///don't require power of 2 alignment (useful for base vertex use cases, so can index different sections of buffer w/o binding to different slot)

    permanent_offset=mb->permanent_offset;
    ///make sure there's enough space in the buffer to begin with (and avoid underflow errors), set error otherwise
    ///also would be interpreted as an error is 0 space remained so don't allocate ALL of the remaining space
    if(permanent_offset<=size)
    {
        permanent_offset=0;
    }
    else
    {
        permanent_offset-=size;
        permanent_offset-=permanent_offset%alignment;///align allocation properly

        temporary_offset=((uint64_t)mb->temporary_offset)<<mb->base_temporary_allocation_size_factor;///the other end of the buffer contains a dynamic allocator, need to check this allocation wouldn't overlap with that
        if(permanent_offset<temporary_offset) permanent_offset=0;///ensure there's enough space available, set error otherwise
    }

    ///if there's precisely 0 space left that would be interpreted as an error so don't make that wasted/untracked allocation (as well as don't allocate upon error)
    if(permanent_offset)
    {
        mb->permanent_offset=permanent_offset;///if allocation was valid make it official
    }

    cvm_atomic_lock_release(&mb->allocation_management_spinlock);

    return permanent_offset;
}

static inline void stage_copy_action(cvm_vk_managed_buffer * mb,queue_transfer_synchronization_data * transfer_data,VkPipelineStageFlags2 use_stage_mask,VkAccessFlags2 use_access_mask,VkDeviceSize staging_offset,uint64_t dst_offset,uint64_t size,uint16_t * availability_token,uint32_t dst_queue_id)
{
    cvm_atomic_lock_acquire(&mb->copy_spinlock);

    cvm_vk_buffer_copy_stack_append(&mb->pending_copies,(VkBufferCopy)
    {
        .srcOffset=staging_offset,
        .dstOffset=dst_offset,
        .size=size
    });

    mb->copy_queue_bitmask|=1<<dst_queue_id;

    *availability_token=mb->copy_update_counter;///this may need to change if altering allowable drame delay for DMA actions

    if(cvm_vk_get_transfer_queue_family()==transfer_data->associated_queue_family_index)
    {
        *cvm_vk_buffer_barrier_stack_append_ptr(&mb->copy_release_barriers) = (VkBufferMemoryBarrier2)
        {
            .sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext=NULL,
            .srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask=use_stage_mask,
            .dstAccessMask=use_access_mask,
            .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
            .buffer=mb->buffer,
            .offset=dst_offset,
            .size=size
        };

        cvm_atomic_lock_release(&mb->copy_spinlock);
    }
    else
    {
        *cvm_vk_buffer_barrier_stack_append_ptr(&mb->copy_release_barriers) = (VkBufferMemoryBarrier2)
        {
            .sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext=NULL,
            .srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask=0,///these are irrelevant on releasing queue of QFOT
            .dstAccessMask=0,///these are irrelevant on releasing queue of QFOT
            .srcQueueFamilyIndex=cvm_vk_get_transfer_queue_family(),
            .dstQueueFamilyIndex=transfer_data->associated_queue_family_index,
            .buffer=mb->buffer,
            .offset=dst_offset,
            .size=size
        };

        cvm_atomic_lock_release(&mb->copy_spinlock);


        ///there must exist a semaphore between these 2 (GPU synchronisation on QFOT)


        cvm_atomic_lock_acquire(&transfer_data->spinlock);

        *cvm_vk_buffer_barrier_stack_append_ptr(&transfer_data->acquire_barriers) = (VkBufferMemoryBarrier2)
        {
            .sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext=NULL,
            .srcStageMask=0,///these are irrelevant on acquiring queue of QFOT
            .srcAccessMask=0,///these are irrelevant on acquiring queue of QFOT
            .dstStageMask=use_stage_mask,
            .dstAccessMask=use_access_mask,
            .srcQueueFamilyIndex=cvm_vk_get_transfer_queue_family(),
            .dstQueueFamilyIndex=transfer_data->associated_queue_family_index,
            .buffer=mb->buffer,
            .offset=dst_offset,
            .size=size
        };

        transfer_data->wait_stages|=use_stage_mask;

        cvm_atomic_lock_release(&transfer_data->spinlock);
    }
}


void * cvm_vk_managed_buffer_acquire_temporary_allocation_with_mapping(cvm_vk_managed_buffer * mb,uint64_t size,
    queue_transfer_synchronization_data * transfer_data,VkPipelineStageFlags2 use_stage_mask,VkAccessFlags2 use_access_mask,
    uint32_t * allocation_index,uint64_t * allocation_offset,uint16_t * availability_token,uint32_t dst_queue_id)
{
    /**
    would save allocating space from staging buffer unnecessarily to do that first (temporary allocations can give their space back immediately with no issue)
    BUT; its really bad to have run out of managed buffer space anyway (probably want to spit out a warning) and the same CANNOT be done for permanent allocations
    */

    uint8_t * mapping=NULL;
    VkDeviceSize staging_offset;

    if(mb->mapping)///on UMA can access memory directly, so no need to get staging space or add a copy
    {
        if(!cvm_vk_managed_buffer_acquire_temporary_allocation(mb,size,allocation_index,allocation_offset))
        {
            fprintf(stderr,"UNABLE TO ACQUIRE %lu BYTES FROM MANAGED BUFFER\n",size);
            return NULL;
        }

        return mb->mapping + *allocation_offset;
    }
    else
    {
        assert(mb->staging_buffer);

        mapping=cvm_vk_staging_buffer_acquire_space(mb->staging_buffer,size,&staging_offset);


        if(!mapping)return NULL;

        if(!cvm_vk_managed_buffer_acquire_temporary_allocation(mb,size,allocation_index,allocation_offset))
        {
            fprintf(stderr,"UNABLE TO ACQUIRE %lu BYTES FROM MANAGED BUFFER\n",size);
            return NULL;
        }

        stage_copy_action(mb,transfer_data,use_stage_mask,use_access_mask,staging_offset,*allocation_offset,size,availability_token,dst_queue_id);

        return mapping;
    }
}

void * cvm_vk_managed_buffer_acquire_permanent_allocation_with_mapping(cvm_vk_managed_buffer * mb,uint64_t size,uint64_t alignment,
            queue_transfer_synchronization_data * transfer_data,VkPipelineStageFlags2 use_stage_mask,VkAccessFlags2 use_access_mask,
            uint64_t * allocation_offset,uint16_t * availability_token,uint32_t dst_queue_id)
{
    uint8_t * mapping=NULL;
    VkDeviceSize staging_offset;

    if(mb->mapping)///on UMA can access memory directly, so no need to get staging space or add a copy
    {
        if(!(*allocation_offset=cvm_vk_managed_buffer_acquire_permanent_allocation(mb,size,alignment)))
        {
            fprintf(stderr,"UNABLE TO ACQUIRE %lu BYTES FROM MANAGED BUFFER\n",size);
            return NULL;
        }

        return mb->mapping + *allocation_offset;
    }
    else
    {
        assert(mb->staging_buffer);

        mapping=cvm_vk_staging_buffer_acquire_space(mb->staging_buffer,size,&staging_offset);

        if(!mapping)return NULL;

        if(!(*allocation_offset=cvm_vk_managed_buffer_acquire_permanent_allocation(mb,size,alignment)))
        {
            fprintf(stderr,"UNABLE TO ACQUIRE %lu BYTES FROM MANAGED BUFFER\n",size);
            return NULL;
        }

        stage_copy_action(mb,transfer_data,use_stage_mask,use_access_mask,staging_offset,*allocation_offset,size,availability_token,dst_queue_id);

        return mapping;
    }
}

void cvm_vk_managed_buffer_submit_all_batch_copy_actions(cvm_vk_managed_buffer * mb,cvm_vk_module_batch * batch)
{
    mb->copy_update_counter++;

    if(mb->copy_queue_bitmask)///should indicate whether any copies happened
    {
        if(mb->mapping)/// is UMA system, no staging copies necessary
        {
            if(!mb->mapping_coherent)///if its coherent on a UMA system nothing needs to be done!
            {
                VkMappedMemoryRange flush_range=(VkMappedMemoryRange)
                {
                    .sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                    .pNext=NULL,
                    .memory=mb->memory,
                    .offset=0,
                    .size=VK_WHOLE_SIZE
                };

                cvm_vk_flush_buffer_memory_range(&flush_range);
            }
        }
        else
        {
            VkCommandBuffer transfer_cb=cvm_vk_access_batch_transfer_command_buffer(batch,mb->copy_queue_bitmask);

            mb->copy_queue_bitmask=0;

            assert(mb->pending_copies.count);
            assert(mb->copy_release_barriers.count);

            ///multithreaded consideration shouldn't be necessary as this should only ever be called from the main thread relevant to this resource while no worker threads are operating on the resource

            vkCmdCopyBuffer(transfer_cb,mb->staging_buffer->buffer,mb->buffer,mb->pending_copies.count,mb->pending_copies.data);

            cvm_vk_buffer_copy_stack_reset(&mb->pending_copies);

            VkDependencyInfo copy_release_dependencies=
            {
                .sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext=NULL,
                .dependencyFlags=0,
                .memoryBarrierCount=0,
                .pMemoryBarriers=NULL,
                .bufferMemoryBarrierCount=mb->copy_release_barriers.count,
                .pBufferMemoryBarriers=mb->copy_release_barriers.data,
                .imageMemoryBarrierCount=0,
                .pImageMemoryBarriers=NULL
            };
            vkCmdPipelineBarrier2(transfer_cb,&copy_release_dependencies);

            cvm_vk_buffer_barrier_stack_reset(&mb->copy_release_barriers);
        }


    }
    else
    {
        assert(mb->pending_copies.count==0);
        assert(mb->copy_release_barriers.count==0);
    }
}



void cvm_vk_managed_buffer_bind_as_vertex(VkCommandBuffer cmd_buf,cvm_vk_managed_buffer * mb,uint32_t binding)
{
    VkDeviceSize offset=0;
    vkCmdBindVertexBuffers(cmd_buf,binding,1,&mb->buffer,&offset);
}

void cvm_vk_managed_buffer_bind_as_index(VkCommandBuffer cmd_buf,cvm_vk_managed_buffer * mb,VkIndexType type)
{
    vkCmdBindIndexBuffer(cmd_buf,mb->buffer,0,type);
}



void cvm_vk_managed_buffer_dismissal_list_create(cvm_vk_managed_buffer_dismissal_list * dismissal_list)
{
    cvm_atomic_lock_create(&dismissal_list->spinlock);
    dismissal_list->frame_count=0;
    dismissal_list->frame_index=CVM_INVALID_U32_INDEX;
    dismissal_list->allocation_indices=NULL;
}

void cvm_vk_managed_buffer_dismissal_list_update(cvm_vk_managed_buffer_dismissal_list * dismissal_list,uint32_t frame_count)
{
    uint32_t i;

    assert(dismissal_list->frame_index==CVM_INVALID_U32_INDEX);

    if(dismissal_list->frame_count==frame_count)return;

    for(i=frame_count;i<dismissal_list->frame_count;i++)
    {
        assert(dismissal_list->allocation_indices[i].count==0);
        u32_stack_terminate(dismissal_list->allocation_indices+i);
    }

    dismissal_list->allocation_indices=realloc(dismissal_list->allocation_indices,sizeof(struct u32_stack)*frame_count);

    for(i=dismissal_list->frame_count;i<frame_count;i++)
    {
        u32_stack_initialise(dismissal_list->allocation_indices+i, 64);
    }

    dismissal_list->frame_count=frame_count;
}

void cvm_vk_managed_buffer_dismissal_list_destroy(cvm_vk_managed_buffer_dismissal_list * dismissal_list)
{
    uint32_t i;

    assert(dismissal_list->frame_index==CVM_INVALID_U32_INDEX);

    for(i=0;i<dismissal_list->frame_count;i++)
    {
        assert(dismissal_list->allocation_indices[i].count==0);
        u32_stack_terminate(dismissal_list->allocation_indices+i);
    }

    free(dismissal_list->allocation_indices);
}

void cvm_vk_managed_buffer_dismissal_list_begin_frame(cvm_vk_managed_buffer_dismissal_list * dismissal_list,uint32_t frame_index)
{
    assert(frame_index<dismissal_list->frame_count);
    assert(dismissal_list->allocation_indices[frame_index].count==0);

    dismissal_list->frame_index=frame_index;
}

void cvm_vk_managed_buffer_dismissal_list_end_frame(cvm_vk_managed_buffer_dismissal_list * dismissal_list)
{
    dismissal_list->frame_index=CVM_INVALID_U32_INDEX;
}

void cvm_vk_managed_buffer_dismissal_list_enqueue_allocation(cvm_vk_managed_buffer_dismissal_list * dismissal_list,uint32_t allocation_index)
{
    cvm_atomic_lock_acquire(&dismissal_list->spinlock);///rename to release spinlock? use a different paradigm?

    assert(dismissal_list->frame_index < dismissal_list->frame_count);

    u32_stack_append(dismissal_list->allocation_indices + dismissal_list->frame_index,allocation_index);

    cvm_atomic_lock_release(&dismissal_list->spinlock);
}

/// should only be called in critical section
/// maybe look at conditionally stripping use of spinlocks on delete function
void cvm_vk_managed_buffer_dismissal_list_release_frame(cvm_vk_managed_buffer_dismissal_list * dismissal_list,cvm_vk_managed_buffer * managed_buffer,uint32_t frame_index)
{
    struct u32_stack * stack;
    uint32_t allocation_index;

    assert(frame_index<dismissal_list->frame_count);
    assert(dismissal_list->frame_index==CVM_INVALID_U32_INDEX);

    stack=dismissal_list->allocation_indices+frame_index;

    while(u32_stack_remove(stack,&allocation_index))
    {
        cvm_vk_managed_buffer_release_temporary_allocation(managed_buffer,allocation_index);
    }
}








void cvm_vk_staging_buffer_create(cvm_vk_staging_buffer * sb,VkBufferUsageFlags usage)
{
    atomic_init(&sb->active_offset,0);
    sb->total_space=0;
    sb->mapping=NULL;
    sb->mapping_coherent=false;
    sb->buffer=VK_NULL_HANDLE;
    sb->memory=VK_NULL_HANDLE;
    sb->usage=usage;

    sb->active_offset=0;
    sb->active_region.start=0;
    sb->active_region.end=0;

    sb->available_region_count=0;
    sb->acquired_regions=NULL;
    sb->available_regions=NULL;

    uint32_t required_alignment=cvm_vk_get_buffer_alignment_requirements(usage);

    sb->alignment_size_factor=cvm_po2_32_gte(required_alignment);

    assert(1u<<sb->alignment_size_factor == required_alignment);///NON POWER OF 2 ALIGNMENTS NOT SUPPORTED
}

void cvm_vk_staging_buffer_update(cvm_vk_staging_buffer * sb,uint32_t space_per_frame, uint32_t frame_count)
{
    assert(sb->available_region_count==0 || (sb->available_region_count==1 && sb->available_regions[0].start==0 && sb->available_regions[0].end==sb->total_space));///NOT ALL STAGING BUFFER SPACE RELINQUISHED BEFORE RECREATION
    assert(!(space_per_frame & ((1<<sb->alignment_size_factor)-1)));///REQUESTED UNALIGNED STAGING SIZE

    uint32_t i,buffer_size=space_per_frame*frame_count;

    if(frame_count)
    {
        sb->acquired_regions=realloc(sb->acquired_regions,sizeof(cvm_vk_staging_buffer_region)*frame_count);
        for(i=0;i<frame_count;i++)
        {
            sb->acquired_regions[i]=(cvm_vk_staging_buffer_region){.start=0,.end=0};
        }
        sb->available_regions=realloc(sb->available_regions,sizeof(cvm_vk_staging_buffer_region)*frame_count);
        sb->available_region_count=1;
        sb->available_regions[0].start=0;
        sb->available_regions[0].end=buffer_size;
    }

    if(buffer_size != sb->total_space)
    {
        if(sb->total_space)
        {
            cvm_vk_destroy_buffer(sb->buffer,sb->memory,sb->mapping);
        }

        cvm_vk_create_buffer(&sb->buffer,&sb->memory,sb->usage,buffer_size,(void**)&sb->mapping,&sb->mapping_coherent,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        atomic_store(&sb->active_offset,0);
        sb->active_region.start=0;///doesn't really need to be set here
        sb->active_region.end=0;///doesn't really need to be set here
        sb->total_space=buffer_size;
    }
}

void cvm_vk_staging_buffer_destroy(cvm_vk_staging_buffer * sb)
{
    assert(sb->available_region_count==0 || (sb->available_region_count==1 && sb->available_regions[0].start==0 && sb->available_regions[0].end==sb->total_space));///NOT ALL STAGING BUFFER SPACE RELINQUISHED BEFORE RECREATION

    free(sb->acquired_regions);
    free(sb->available_regions);

    cvm_vk_destroy_buffer(sb->buffer,sb->memory,sb->mapping);
}

uint32_t cvm_vk_staging_buffer_get_rounded_allocation_size(cvm_vk_staging_buffer * sb,uint32_t allocation_size)
{
    return (((allocation_size-1)>>sb->alignment_size_factor)+1)<<sb->alignment_size_factor;
}

void cvm_vk_staging_buffer_begin(cvm_vk_staging_buffer * sb)
{
    uint32_t current_region_size,largest_region_size=0;
    cvm_vk_staging_buffer_region *largest,*current,*end;
    largest=NULL;

    for(current=sb->available_regions,end=sb->available_regions+sb->available_region_count ; current<end ; current++)
    {
        assert(current->start!=current->end);/// CANNOT HAVE NULL REGION OR LOOPING REGION THAT STARTS MIDWAY

        if(current->end > current->start) current_region_size = current->end - current->start;
        else current_region_size = sb->total_space + current->end - current->start;

        if(current_region_size > largest_region_size)
        {
            largest=current;
            largest_region_size=current_region_size;
        }
    }

    if(largest_region_size)
    {
        sb->active_region=*largest;
        *largest=sb->available_regions[--sb->available_region_count];
    }
    else
    {
        sb->active_region.start=0;
        sb->active_region.end=0;
    }

    atomic_store(&sb->active_offset,sb->active_region.start);
}

void cvm_vk_staging_buffer_end(cvm_vk_staging_buffer * sb,uint32_t frame_index)
{
    uint_fast32_t offset;
    uint32_t start;

    offset=atomic_load(&sb->active_offset);
    start=sb->active_region.start;

    if(offset < start && !sb->mapping_coherent)///if the buffer wrapped between begin and end
    {
        VkMappedMemoryRange flush_range=(VkMappedMemoryRange)
        {
            .sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .pNext=NULL,
            .memory=sb->memory,
            .offset=start,
            .size=sb->total_space-start
        };

        cvm_vk_flush_buffer_memory_range(&flush_range);
        start=0;
    }

    if(start<offset && !sb->mapping_coherent)///if anything was written that wasn't handled by the previous branch (i.e. we've looped around to the start o the staging buffer this frame)
    {
        VkMappedMemoryRange flush_range=(VkMappedMemoryRange)
        {
            .sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .pNext=NULL,
            .memory=sb->memory,
            .offset=start,
            .size=offset
        };

        cvm_vk_flush_buffer_memory_range(&flush_range);
    }


    if(offset==sb->active_region.start) sb->acquired_regions[frame_index]=(cvm_vk_staging_buffer_region){.start=0,.end=0};
    else sb->acquired_regions[frame_index]=(cvm_vk_staging_buffer_region){.start=sb->active_region.start,.end=offset};

    if(offset!=sb->active_region.end)sb->available_regions[sb->available_region_count++]=(cvm_vk_staging_buffer_region){.start=offset,.end=sb->active_region.end};

    sb->active_region.start=0;
    sb->active_region.end=0;
}

void * cvm_vk_staging_buffer_acquire_space(cvm_vk_staging_buffer * sb,uint32_t allocation_size,VkDeviceSize * acquired_offset)
{
    allocation_size=(((allocation_size-1)>>sb->alignment_size_factor)+1)<<sb->alignment_size_factor;///round as required

    uint_fast32_t old_offset,new_offset,offset;

    old_offset=atomic_load(&sb->active_offset);
    do
    {
        if(sb->active_region.end < sb->active_region.start)
        {
            if(old_offset+allocation_size > sb->total_space)
            {
                if(allocation_size>sb->active_region.end)return NULL;
                offset=0;
                new_offset=allocation_size;
            }
            else
            {
                offset=old_offset;
                new_offset=old_offset+allocation_size;
            }
        }
        else
        {
            if(old_offset+allocation_size > sb->active_region.end)return NULL;
            offset=old_offset;
            new_offset=old_offset+allocation_size;
        }
    }
    while(!atomic_compare_exchange_weak(&sb->active_offset,&old_offset,new_offset));

    *acquired_offset=offset;
    return sb->mapping+offset;
}

///should only ever be called during cleanup, not thread safe
void cvm_vk_staging_buffer_release_space(cvm_vk_staging_buffer * sb,uint32_t frame_index)
{
    cvm_vk_staging_buffer_region region;
    uint32_t i;

    region=sb->acquired_regions[frame_index];

    sb->acquired_regions[frame_index]=(cvm_vk_staging_buffer_region){.start=0,.end=0};

    if(region.end!=region.start)
    {
        i=0;
        while(i<sb->available_region_count)
        {
            if(sb->available_regions[i].start==region.end)
            {
                region.end=sb->available_regions[i].end;
                sb->available_regions[i]=sb->available_regions[--sb->available_region_count];
                i=0;
            }
            else if(sb->available_regions[i].end==region.start)
            {
                region.start=sb->available_regions[i].start;
                sb->available_regions[i]=sb->available_regions[--sb->available_region_count];
                i=0;
            }
            else i++;
        }

        ///cannot be a false complete allocation because required non-empty region to start with
        if(region.end==region.start)
        {
            assert(!sb->available_region_count);///FALSE COMPLETE REGION DETECTED
            region=(cvm_vk_staging_buffer_region){.start=0,.end=sb->total_space};
        }

        sb->available_regions[sb->available_region_count++]=region;
    }
}







void cvm_vk_transient_buffer_create(cvm_vk_transient_buffer * tb,VkBufferUsageFlags usage)
{
    atomic_init(&tb->current_offset,0);
    tb->total_space=0;
    tb->space_per_frame=0;
    tb->max_offset=0;
    tb->mapping=NULL;
    tb->mapping_coherent=false;
    tb->buffer=VK_NULL_HANDLE;
    tb->memory=VK_NULL_HANDLE;
    tb->usage=usage;

    uint32_t required_alignment=cvm_vk_get_buffer_alignment_requirements(usage);

    tb->alignment_size_factor=cvm_po2_32_gte(required_alignment);

    assert(1u<<tb->alignment_size_factor == required_alignment);///NON POWER OF 2 ALIGNMENTS NOT SUPPORTED (AND SHOULDN"T BE RETURNED BY VULKAN)
}

void cvm_vk_transient_buffer_update(cvm_vk_transient_buffer * tb,uint32_t space_per_frame, uint32_t frame_count)
{
    uint32_t total_space=space_per_frame*frame_count;

    assert(!tb->max_offset);///TRYING TO RECREATE TRANSIENT BUFFER THAT IS STILL IN USE
    assert(!(space_per_frame & ((1<<tb->alignment_size_factor)-1)));///REQUESTED UNALIGNED TRANSIENT STAGING SIZE

    if(total_space != tb->total_space)///recreate buffer
    {
        if(tb->total_space)
        {
            cvm_vk_destroy_buffer(tb->buffer,tb->memory,tb->mapping);
            tb->mapping=NULL;
            tb->mapping_coherent=false;
        }

        if(total_space)
        {
            cvm_vk_create_buffer(&tb->buffer,&tb->memory,tb->usage,total_space,(void**)&tb->mapping,&tb->mapping_coherent,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
    }

    atomic_store(&tb->current_offset,0);
    tb->total_space=total_space;
    tb->space_per_frame=space_per_frame;
    tb->max_offset=0;
}

void cvm_vk_transient_buffer_destroy(cvm_vk_transient_buffer * tb)
{
    assert(!tb->max_offset);///TRYING TO DESTROY TRANSIENT BUFFER THAT IS STILL IN USE

    cvm_vk_destroy_buffer(tb->buffer,tb->memory,tb->mapping);
}

uint32_t cvm_vk_transient_buffer_get_rounded_allocation_size(cvm_vk_transient_buffer * tb,uint32_t allocation_size,uint32_t alignment)
{
    /// if aligned to base (alignment==0) then just need enough to round up as that should cover itself
    /// if aligned to own alignment, need enough to align to oneself and to align next (potentially base aligned) allocation

    ///base alignment requirements handle specific alignment requirements when base alignment is a multiple of specific alignment, and base alignment is always a Po2 so in this case specific must also be smaller Po2
    if( !(alignment&(alignment-1)) && alignment<(1u<<tb->alignment_size_factor) )alignment=0;

    return (((allocation_size+alignment-1)>>tb->alignment_size_factor)+1)<<tb->alignment_size_factor;
}

void cvm_vk_transient_buffer_begin(cvm_vk_transient_buffer * tb,uint32_t frame_index)
{
    tb->max_offset=(frame_index+1)*tb->space_per_frame;
    atomic_store(&tb->current_offset,frame_index*tb->space_per_frame);
}

void cvm_vk_transient_buffer_end(cvm_vk_transient_buffer * tb)
{
    uint32_t acquired_space=tb->space_per_frame - (tb->max_offset-atomic_load(&tb->current_offset)); /// max space - space remaining

    if(acquired_space && tb->mapping_coherent)
    {
        acquired_space=(((acquired_space-1)>>tb->alignment_size_factor)+1)<<tb->alignment_size_factor;

        VkMappedMemoryRange flush_range=(VkMappedMemoryRange)
        {
            .sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .pNext=NULL,
            .memory=tb->memory,
            .offset=tb->max_offset-tb->space_per_frame,
            .size=acquired_space
        };

        cvm_vk_flush_buffer_memory_range(&flush_range);
    }

    tb->max_offset=0;///indicates this buffer is no longer in use
}

void * cvm_vk_transient_buffer_get_allocation(cvm_vk_transient_buffer * tb,uint32_t allocation_size,uint32_t alignment,VkDeviceSize * acquired_offset)
{
    VkDeviceSize offset;
    uint_fast32_t old_offset,new_offset;

    old_offset=atomic_load(&tb->current_offset);
    do
    {
        offset=(old_offset+alignment-1);
        if(alignment) offset-=offset%alignment;
        else offset=((offset>>tb->alignment_size_factor)+1)<<tb->alignment_size_factor;

        new_offset=offset+allocation_size;
        if(new_offset>tb->max_offset)
        {
            return NULL;
        }
    }
    while(!atomic_compare_exchange_weak(&tb->current_offset,&old_offset,new_offset));

    *acquired_offset=offset;
    return tb->mapping+offset;
}

void cvm_vk_transient_buffer_bind_as_vertex(VkCommandBuffer cmd_buf,cvm_vk_transient_buffer * tb,uint32_t binding,VkDeviceSize offset)
{
    vkCmdBindVertexBuffers(cmd_buf,binding,1,&tb->buffer,&offset);
}

void cvm_vk_transient_buffer_bind_as_index(VkCommandBuffer cmd_buf,cvm_vk_transient_buffer * tb,VkIndexType type,VkDeviceSize offset)
{
    vkCmdBindIndexBuffer(cmd_buf,tb->buffer,offset,type);
}

