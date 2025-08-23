/**
Copyright 2020,2021,2022,2023,2024,2025 Carl van Mastrigt

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

#ifndef solipsix_H
#include "solipsix.h"
#endif


#ifndef CVM_VK_H
#define CVM_VK_H

#include "vk/timeline_semaphore.h"
#include "vk/sync_manager.h"

#ifndef CVM_VK_CHECK
#define CVM_VK_CHECK(f)                                                         \
{                                                                               \
    VkResult r=f;                                                               \
    assert(r==VK_SUCCESS ||                                                     \
           !fprintf(stderr,"VULKAN FUNCTION FAILED : %d : %s\n",r,#f));         \
}

#endif


#define SOL_VK_DEFAULT_TIMEOUT ((uint64_t)1000000000)
/// ^ in nanoseconds (1 second)

#define CVM_VK_MAX_QUEUE_FAMILY_COUNT 64




#define SOL_STACK_ENTRY_TYPE VkBufferMemoryBarrier2
#define SOL_STACK_FUNCTION_PREFIX cvm_vk_buffer_barrier_stack
#define SOL_STACK_STRUCT_NAME cvm_vk_buffer_barrier_stack
#include "data_structures/stack.h"

#define SOL_STACK_ENTRY_TYPE VkBufferCopy
#define SOL_STACK_FUNCTION_PREFIX cvm_vk_buffer_copy_stack
#define SOL_STACK_STRUCT_NAME cvm_vk_buffer_copy_stack
#include "data_structures/stack.h"

#define SOL_STACK_ENTRY_TYPE VkBufferImageCopy
#define SOL_STACK_FUNCTION_PREFIX cvm_vk_buffer_image_copy_stack
#define SOL_STACK_STRUCT_NAME cvm_vk_buffer_image_copy_stack
#include "data_structures/stack.h"

#define SOL_STACK_ENTRY_TYPE VkSemaphore
#define SOL_STACK_FUNCTION_PREFIX sol_vk_semaphore_stack
#define SOL_STACK_STRUCT_NAME sol_vk_semaphore_stack
#include "data_structures/stack.h"


typedef struct cvm_vk_managed_buffer cvm_vk_managed_buffer;
typedef struct cvm_vk_managed_buffer_dismissal_list cvm_vk_managed_buffer_dismissal_list;

/**
features to be cognisant of:

fragmentStoresAndAtomics



(maybe)

vertexPipelineStoresAndAtomics

could move swapchain/resource rebuilding outside of critical section if atomic locking mechanism is employed
    ^ cas to gain control, VK thread must gain absolute control (a==0 -> a=0xFFFFFFFF) whereas any thread using those resources can gain read permissions (a!=0xFFFFFFFF -> a=a+1)
        ^ this probably isnt woth it as some systems might handle busy waiting poorly, also swapchain/render_resource recreation should be a rare operation anyway!

going to have to rely on acquire op failing to know when to recreate swapchain
    ^ also need to take settings changes into account, i.e. manually do same thing as out of date would and also prevent swapchain image acquisition
*/

///won't be supporting modules having msaa output

///render system, upon swapchain creation/recreation should forward acquire swapchain images
///move this to be thread static declaration?

///use swapchain image to index into these? yes
///this should encompass swapchain images? (or at least it could...)

///could move these structs to the c file? - they should only ever be used by cvm_vk internally
///this struct contains the data needed to be known upfront when acquiring swapchain image (cvm_vk_swapchain_frame), some data could go in either struct though...


typedef struct cvm_vk_device cvm_vk_device;

typedef uint_least64_t cvm_vk_resource_identifier;

static inline VkDeviceSize cvm_vk_align(VkDeviceSize size, VkDeviceSize alignment)
{
    return (size+alignment-1) & ~(alignment-1);
}





typedef struct cvm_vk_instance_setup
{
    const VkAllocationCallbacks* host_allocator;

    const char** layer_names;
    uint32_t layer_count;

    const char** extension_names;
    uint32_t extension_count;

    const char* application_name;
    uint32_t application_version;
}
cvm_vk_instance_setup;

struct cvm_vk_instance
{
    VkInstance instance;
    /// consider removing above, separeating it from the device

    const VkAllocationCallbacks* host_allocator;
};

typedef struct cvm_vk_device_setup
{
    const struct cvm_vk_instance* instance;

    float (*validation_function)(const VkPhysicalDeviceFeatures2*, const VkPhysicalDeviceProperties*, const VkPhysicalDeviceMemoryProperties*, const VkExtensionProperties*, uint32_t, const VkQueueFamilyProperties*, uint32_t);

    // sets desired features and extensions
    void (*request_function)(VkPhysicalDeviceFeatures2*, bool*, const VkPhysicalDeviceFeatures2*, const VkPhysicalDeviceProperties*, const VkPhysicalDeviceMemoryProperties*, const VkExtensionProperties*, uint32_t, const VkQueueFamilyProperties*, uint32_t);

    const char* preferred_device_name_substring;

    VkStructureType* device_feature_struct_types;
    size_t* device_feature_struct_sizes;
    uint32_t device_feature_struct_count;

    uint32_t desired_graphics_queues;
    uint32_t desired_transfer_queues;
    uint32_t desired_async_compute_queues;
    ///remove above and default to having just 2 (if possible) : low priority and high priority?

    const char* pipeline_cache_file_name;
}
cvm_vk_device_setup;


typedef struct cvm_vk_device_queue
{
    struct sol_vk_timeline_semaphore timeline;
    VkQueue queue;
    uint32_t family_index;
}
cvm_vk_device_queue;

typedef struct cvm_vk_device_queue_family
{
    const VkQueueFamilyProperties properties;

    cvm_vk_device_queue* queues;
    uint32_t queue_count;
}
cvm_vk_device_queue_family;

struct cvm_vk_defaults
{
    VkSampler fetch_sampler;
};

struct cvm_vk_pipeline_cache
{
    VkPipelineCache cache;
    void* data;
    char* file_name;
};

struct sol_vk_object_pools
{
    struct sol_vk_semaphore_stack semaphores;
    mtx_t semaphore_mutex;
};

VkSemaphore sol_vk_device_object_pool_semaphore_acquire(const struct cvm_vk_device* device);
void sol_vk_device_object_pool_semaphore_release(const struct cvm_vk_device* device, VkSemaphore semaphore);

struct cvm_vk_device
{
    #warning need to add some serious multithreading consideration to this struct, probably with a recursive mutex
    const VkAllocationCallbacks* host_allocator;

    VkPhysicalDevice physical_device;
    VkDevice device;///"logical" device

    /// capabilities (properties & features)
    const VkPhysicalDeviceProperties properties;
    const VkPhysicalDeviceMemoryProperties memory_properties;

    const VkPhysicalDeviceFeatures2 features;// enabled features
    bool feature_swapchain_maintainence;

    const VkExtensionProperties* extensions;//enabled extensions
    uint32_t extension_count;

    // const VkQueueFamilyProperties* queue_family_properties;

    cvm_vk_device_queue_family* queue_families;
    uint32_t queue_family_count;


    /// these can be the same, though this should probably change
    /// should have fallbacks...
    /// rename to defaults as thats what they are really...
    uint32_t graphics_queue_family_index;
    uint32_t transfer_queue_family_index;///rename to host_device_transfer ??
    uint32_t async_compute_queue_family_index;

    struct cvm_vk_pipeline_cache pipeline_cache;

    struct cvm_vk_defaults defaults;

    atomic_uint_least64_t * resource_identifier_monotonic;
    #warning make above track/manage all resources as well as their backing memory
    #warning add image backing blocks per heap type (start with 0 blocks, used for small, non-dedicated allocations)

    // move this somewhere else maybe?
    // set of default data types that are recyclable within different objects in the device
    struct sol_vk_object_pools* object_pools;

    struct sol_vk_sync_manager sync_manager;
};

#include "vk/command_pool.h"
#include "vk/swapchain.h"

static inline cvm_vk_resource_identifier cvm_vk_resource_unique_identifier_acquire(const cvm_vk_device * device)
{
    return atomic_fetch_add_explicit(device->resource_identifier_monotonic, 1, memory_order_relaxed);
}

VkResult cvm_vk_instance_initialise(struct cvm_vk_instance* instance,const cvm_vk_instance_setup * setup);
/// need a way to include extension names, include setup which gets combined?
VkResult cvm_vk_instance_initialise_for_SDL(struct cvm_vk_instance* instance, const cvm_vk_instance_setup * setup);
///above extra is the max extra used by any module
void cvm_vk_instance_terminate(struct cvm_vk_instance* instance);

VkSurfaceKHR cvm_vk_create_surface_from_SDL_window(const struct cvm_vk_instance* instance, SDL_Window * window);
void cvm_vk_destroy_surface(const struct cvm_vk_instance* instance, VkSurfaceKHR surface);

/** perhaps should make this conditional on sol_sync_primitive being included/defined ? */
static inline void sol_vk_devive_impose_timeline_semaphore_moment_condition(struct cvm_vk_device* device, struct sol_vk_timeline_semaphore_moment moment, struct sol_sync_primitive* successor)
{
    sol_vk_sync_manager_impose_timeline_semaphore_moment_condition(&device->sync_manager, moment, successor);
}



int cvm_vk_device_initialise(cvm_vk_device * device, const cvm_vk_device_setup* device_setup);
///above extra is the max extra used by any module
void cvm_vk_device_terminate(cvm_vk_device * device);


#warning following are placeholders for interoperability with old approach
cvm_vk_device * cvm_vk_device_get(void);
cvm_vk_surface_swapchain * cvm_vk_swapchain_get(void);

VkFence cvm_vk_create_fence(const cvm_vk_device * device,bool initially_signalled);
void cvm_vk_destroy_fence(const cvm_vk_device * device,VkFence fence);
void cvm_vk_wait_on_fence_and_reset(const cvm_vk_device * device, VkFence fence);

VkSemaphore cvm_vk_create_binary_semaphore(const cvm_vk_device * device);
void cvm_vk_destroy_binary_semaphore(const cvm_vk_device * device,VkSemaphore semaphore);

void cvm_vk_create_render_pass(VkRenderPass * render_pass,VkRenderPassCreateInfo * info);
void cvm_vk_destroy_render_pass(VkRenderPass render_pass);

void cvm_vk_create_framebuffer(VkFramebuffer * framebuffer,VkFramebufferCreateInfo * info);
void cvm_vk_destroy_framebuffer(VkFramebuffer framebuffer);

void cvm_vk_create_pipeline_layout(VkPipelineLayout * pipeline_layout,VkPipelineLayoutCreateInfo * info);
void cvm_vk_destroy_pipeline_layout(VkPipelineLayout pipeline_layout);

void cvm_vk_create_graphics_pipeline(VkPipeline * pipeline,VkGraphicsPipelineCreateInfo * info);
void cvm_vk_destroy_pipeline(VkPipeline pipeline);

void cvm_vk_create_shader_stage_info(VkPipelineShaderStageCreateInfo * stage_info,const struct cvm_vk_device* device,const char * filename,VkShaderStageFlagBits stage);
void cvm_vk_destroy_shader_stage_info(VkPipelineShaderStageCreateInfo * stage_info,const struct cvm_vk_device* device);

void cvm_vk_create_descriptor_set_layout(VkDescriptorSetLayout * descriptor_set_layout,VkDescriptorSetLayoutCreateInfo * info);
void cvm_vk_destroy_descriptor_set_layout(VkDescriptorSetLayout descriptor_set_layout);

void cvm_vk_create_descriptor_pool(VkDescriptorPool * descriptor_pool,VkDescriptorPoolCreateInfo * info);
void cvm_vk_destroy_descriptor_pool(VkDescriptorPool descriptor_pool);

void cvm_vk_allocate_descriptor_sets(VkDescriptorSet * descriptor_sets,VkDescriptorSetAllocateInfo * info);
void cvm_vk_write_descriptor_sets(VkWriteDescriptorSet * writes,uint32_t count);

void cvm_vk_create_image(VkImage * image,VkImageCreateInfo * info);
void cvm_vk_destroy_image(VkImage image);

void cvm_vk_set_view_create_info_using_image_create_info(VkImageViewCreateInfo* view_create_info, const VkImageCreateInfo* image_create_info, VkImage image);

VkResult cvm_vk_allocate_and_bind_memory_for_images(VkDeviceMemory * memory,VkImage * images,uint32_t image_count,VkMemoryPropertyFlags required_properties,VkMemoryPropertyFlags desired_properties);

void cvm_vk_create_image_view(VkImageView * image_view,VkImageViewCreateInfo * info);
void cvm_vk_destroy_image_view(VkImageView image_view);

void cvm_vk_create_sampler(VkSampler* sampler,VkSamplerCreateInfo * info);
void cvm_vk_destroy_sampler(VkSampler sampler);

void cvm_vk_free_memory(VkDeviceMemory memory);


struct sol_vk_image
{
    // VkImageCreateInfo image_create_info; // may want to keep properties around
    VkImage image;
    VkImageView base_view;
    VkDeviceMemory memory;// may be VK_NULL_HANDLE if backed by
};

/** if view_create_info is NULL, the default view will be created 
 * otherwise the image field will be set after the image has been created */
VkResult sol_vk_image_create(struct sol_vk_image* image, struct cvm_vk_device* device, const VkImageCreateInfo* image_create_info, const VkImageViewCreateInfo* view_create_info);
void sol_vk_image_destroy(struct sol_vk_image* image, struct cvm_vk_device* device);




// buffers can probably be handled better
void cvm_vk_create_buffer(VkBuffer* buffer,VkDeviceMemory * memory,VkBufferUsageFlags usage,VkDeviceSize size,void ** mapping,bool * mapping_coherent,VkMemoryPropertyFlags required_properties,VkMemoryPropertyFlags desired_properties);
void cvm_vk_destroy_buffer(VkBuffer buffer,VkDeviceMemory memory,void * mapping);
void cvm_vk_flush_buffer_memory_range(VkMappedMemoryRange * flush_range);
uint32_t cvm_vk_get_buffer_alignment_requirements(VkBufferUsageFlags usage);

VkDeviceSize cvm_vk_buffer_alignment_requirements(const cvm_vk_device * device, VkBufferUsageFlags usage);




struct sol_vk_backed_buffer_description
{
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    /** if VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT is set in required_properties the buffer will be mapped*/
    VkMemoryPropertyFlags required_properties;
    VkMemoryPropertyFlags desired_properties;
};

struct sol_vk_backed_buffer
{
    struct sol_vk_backed_buffer_description description;

    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapping;
    VkMemoryPropertyFlags memory_properties;
    uint32_t memory_type_index;
};

VkResult sol_vk_backed_buffer_create(const cvm_vk_device * device, const struct sol_vk_backed_buffer_description* description, struct sol_vk_backed_buffer* buffer);
void sol_vk_backed_buffer_destroy(const cvm_vk_device * device, struct sol_vk_backed_buffer* buffer);

void sol_vk_backed_buffer_flush_range(const cvm_vk_device * device, const struct sol_vk_backed_buffer* buffer, VkDeviceSize offset, VkDeviceSize size);



VkFormat cvm_vk_get_screen_format(void);///can remove?
uint32_t cvm_vk_get_swapchain_image_count(void);
VkImageView cvm_vk_get_swapchain_image_view(uint32_t index);

VkSemaphoreSubmitInfo cvm_vk_binary_semaphore_submit_info(VkSemaphore semaphore,VkPipelineStageFlags2 stages);


/// little bit of a utility function set here
#define CVM_VK_MAX_DESCRIPTOR_POOL_TYPES 16
struct cvm_vk_descriptor_pool_requirements
{
    VkDescriptorPoolSize type_sizes[CVM_VK_MAX_DESCRIPTOR_POOL_TYPES];
    uint32_t type_count;
    uint32_t set_count;
};
VkResult cvm_vk_create_descriptor_set_layout_registering_requirements(VkDescriptorSetLayout* set_layout, const VkDescriptorSetLayoutCreateInfo* set_layout_create_info, const struct cvm_vk_device* device, struct cvm_vk_descriptor_pool_requirements* pool_requirements, uint32_t sets_using_this_layout);
VkResult cvm_vk_create_descriptor_pool_for_sizes(VkDescriptorPool* pool, const struct cvm_vk_device* device, struct cvm_vk_descriptor_pool_requirements* pool_requirements);













#define CVM_VK_MAX_QUEUES 16

bool cvm_vk_format_check_optimal_feature_support(VkFormat format,VkFormatFeatureFlags flags);

typedef struct queue_transfer_synchronization_data
{
    atomic_uint_fast32_t spinlock;
    ///COULD have some extra data alongside this that marks data as available, sets an uint8_t from 0 to 1 (available) and marks actual dynamic buffer (if extant) as available
    struct cvm_vk_buffer_barrier_stack acquire_barriers;
    VkPipelineStageFlags2 wait_stages;///stages that need to waited upon wrt transfer semaphore (stages that have dependence upon QFOT synchronised by semaphore)
    ///is above necessary?? are the stage barriers in the acquire barriers enough?

    ///this is handled by submit only, so could easily be handled by an array in the module/batch itself
    uint64_t transfer_semaphore_wait_value;///transfer semaphore value, value set upon transfer work submission!
    ///need to set this up!

    ///set only at creation time
    uint32_t associated_queue_family_index;///this should probably be on the payload instead... though IS only used in setting up barriers...
}
queue_transfer_synchronization_data;

typedef enum
{
    CVM_VK_PAYLOAD_FIRST_QUEUE_USE=0x00000001,///could technically work this one out...
    CVM_VK_PAYLOAD_LAST_QUEUE_USE=0x00000002,
}
cvm_vk_payload_flags;

#define CVM_VK_PAYLOAD_MAX_WAITS 8

#define CVM_VK_GRAPHICS_QUEUE_INDEX 0
#define CVM_VK_COMPUTE_QUEUE_OFFSET 1
/// ^ such that they come after graphics


///subset of this (CB and transfer_data) required for secondary command buffers!
typedef struct cvm_vk_module_work_payload
{
    ///can reasonably use queue id, stages and semaphore value here instead of a copy of the semaphore itself...

    ///can also store secondary command buffer?
    /// have flags/enum (to assert on) regarding nature of CB, primary? inline? secondary? compute?

    struct sol_vk_timeline_semaphore waits[CVM_VK_PAYLOAD_MAX_WAITS];///this also allows host side semaphores so isn't that bad tbh...
    VkPipelineStageFlags2 wait_stages[CVM_VK_PAYLOAD_MAX_WAITS];
    uint32_t wait_count;

    VkPipelineStageFlags2 signal_stages;///singular signal semaphore (the queue's one) supported for now

    VkCommandBuffer command_buffer;

    uint32_t destination_queue:8;///should really be known at init time anyway (for barrier purposes), might as well store for simplicities sake, must be large enough to store CVM_VK_MAX_QUEUES-1
    uint32_t is_graphics:1;/// if false(0) then is asynchronous compute (is this even necessary w/ above queue id??)

    ///presently only support generating 1 signal and submitting 1 command buffer, can easily change should there be justification to do so

    ///makes sense not to have transfer queue in this list as transfer-transfer dependencies make no sense
    queue_transfer_synchronization_data * transfer_data;///the data for THIS queue
}
cvm_vk_module_work_payload;///break this struct up based on intended queue type? (have variants)

typedef struct cvm_vk_module_sub_batch
{
    VkCommandPool graphics_pool;
    VkCommandBuffer * graphics_scbs;///secondary command buffers
    uint32_t graphics_scb_space;
    uint32_t graphics_scb_count;

    ///should not need to query number of scbs allocated, max that ever tries to be used should match number allocated
}
cvm_vk_module_sub_batch;

typedef struct cvm_vk_module_batch
{
    cvm_vk_module_sub_batch * sub_batches;///for distribution to other threads

    VkCommandBuffer * graphics_pcbs;///primary command buffes, allocated from the "main" (0th) sub_batch's pool
    uint32_t graphics_pcb_space;
    uint32_t graphics_pcb_count;

    ///above must be processed at start of frame BEFORE anything might add to it
    ///having one to process and one to fill doesnt sound like a terrible idea to be honest...

    /// have a buffer of pending high priority copies to be executed on the next PCB before any SCB's are generated, this allows SCB only threads to generate copy actions!
    /// would multiple threads writing into the same staging buffer cause issues?
    ///     ^ have staging action that also copies to alleviate this?
    VkCommandPool transfer_pool;/// cannot have transfers in secondary command buffer (scb's need renderpass/subpass) and they arent needed anyway, so have 1 per module batch
    ///all transfer commands actually generated from stored ops in managed buffer(s) and managed texture pool(s)
    VkCommandBuffer transfer_cb;///only for low priority uploads, high priority uploads should go in command buffer/queue that will use them
    uint32_t has_begun_transfer:1;///has begun the transfer command buffer, dont begin the command buffer if it isn't necessary this frame, begin upon first use
    uint32_t has_ended_transfer:1;///has submitted transfer operations
    ///assert that above is only acquired and submitted once a frame!
    uint32_t transfer_affceted_queues_bitmask;


    /// also keep in mind VK_SHARING_MODE_CONCURRENT where viable and profile it
    /// also keep in mind transfer on graphics queue is possible and profile that as well

    queue_transfer_synchronization_data * transfer_data;///array of size CVM_VK_MAX_QUEUES, gets passed down the chain of command to individual payloads via batches

    uint32_t queue_submissions_this_batch:CVM_VK_MAX_QUEUES;///each bit represents which queues have been acquired this frame, upon first acquisition execute barriers immediately
    ///above does bring up an issue, system REQUIRES that every batch is acquired, may be able to circumvent by copying unhandled barriers into next pending group for that queue
}
cvm_vk_module_batch;

typedef struct cvm_vk_module_data
{
    cvm_vk_module_batch * batches;///one per swapchain image

    uint32_t batch_count;///same as swapchain image count used to initialise this
    uint32_t batch_index;

    uint32_t sub_batch_count;///effectively max number of threads this module will use, must have at least 1 (index 0 is the primary thread)

    ///wait a second! -- transfer queue can technically be run alongside graphics! 2 frame delay isnt actually necessary (do we still want it though? does it really add much complexity?)
    queue_transfer_synchronization_data transfer_data[CVM_VK_MAX_QUEUES];/// pending acquire barriers per queue, must be cycled to ensure correct semaphore dependencies exist between transfer ops and these acquires (QFOT's)
}
cvm_vk_module_data;
///must be called after cvm_vk_initialise
void cvm_vk_create_module_data(cvm_vk_module_data * module_data,uint32_t sub_batch_count);///sub batches are basically the number of threads rendering will use
void cvm_vk_resize_module_graphics_data(cvm_vk_module_data * module_data);///this must be called in critical section, rename to resize_module_data
void cvm_vk_destroy_module_data(cvm_vk_module_data * module_data);

cvm_vk_module_batch * cvm_vk_get_module_batch(cvm_vk_module_data * module_data,uint32_t * swapchain_image_index);///have this return bool? (VkCommandBuffer through ref.)
void cvm_vk_end_module_batch(cvm_vk_module_batch * batch);///handles all pending operations (presently only the transfer operation stuff), should be last thing called that uses the batch or its derivatives
VkCommandBuffer cvm_vk_access_batch_transfer_command_buffer(cvm_vk_module_batch * batch,uint32_t affected_queue_bitbask);///returned value should NOT be submitted directly, instead it should be handled by cvm_vk_end_module_batch


void cvm_vk_work_payload_add_wait(cvm_vk_module_work_payload * payload, struct sol_vk_timeline_semaphore semaphore,VkPipelineStageFlags2 stages);

void cvm_vk_setup_new_graphics_payload_from_batch(cvm_vk_module_work_payload * payload,cvm_vk_module_batch * batch);
void cvm_vk_submit_graphics_work(cvm_vk_module_work_payload * payload,cvm_vk_payload_flags flags);
VkCommandBuffer cvm_vk_obtain_secondary_command_buffer_from_batch(cvm_vk_module_sub_batch * msb,VkFramebuffer framebuffer,VkRenderPass render_pass,uint32_t sub_pass);

///same as above but for compute (including compute?)

uint32_t cvm_vk_get_transfer_queue_family(void);
uint32_t cvm_vk_get_graphics_queue_family(void);
uint32_t cvm_vk_get_asynchronous_compute_queue_family(void);

#warning move these to the top of this file when possible!
#include "vk/memory.h"
#include "cvm_vk_image.h"
#include "cvm_vk_defaults.h"


#endif
