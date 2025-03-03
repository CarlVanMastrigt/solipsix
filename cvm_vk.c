/**
Copyright 2020,2021,2022,2023,2024 Carl van Mastrigt

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


static cvm_vk_device cvm_vk_;
static cvm_vk_surface_swapchain cvm_vk_surface_swapchain_;


cvm_vk_device * cvm_vk_device_get(void)
{
    return &cvm_vk_;
    #warning REMOVE THIS FUNCTION
}

cvm_vk_surface_swapchain * cvm_vk_swapchain_get(void)
{
    return &cvm_vk_surface_swapchain_;
    #warning REMOVE THIS FUNCTION
}








VkResult cvm_vk_instance_initialise(struct cvm_vk_instance* instance, const cvm_vk_instance_setup * setup)
{
    uint32_t api_version, major_version, minor_version, patch_version, variant, i, j, extension_count;
    char** extension_names;
    const uint32_t internal_extension_count = 0;
    const char* internal_extension_names[1] =
    {
//        "VK_KHR_present_wait",
//        "VK_EXT_swapchain_maintenance1"
    };

    char ** ext_names;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
    printf("EXT COUNT: %u\n",extension_count);

    VkResult result;

    extension_count = internal_extension_count + setup->extension_count;/// check for duplicates?
    extension_names = malloc(sizeof(char*)*extension_count);
    memcpy(extension_names, internal_extension_names, sizeof(char*)*internal_extension_count);
    memcpy(extension_names+internal_extension_count, setup->extension_names, sizeof(char*)*setup->extension_count);


    instance->host_allocator = setup->host_allocator;

    result = vkEnumerateInstanceVersion(&api_version);

    if(result == VK_SUCCESS)
    {
        variant=api_version>>29;
        major_version=(api_version>>22)&0x7F;
        minor_version=(api_version>>12)&0x3FF;
        patch_version=api_version&0xFFF;
        printf("Vulkan API version: %u.%u.%u - %u\n", major_version, minor_version, patch_version, variant);

        #warning make minimum required version part of setup
        if((major_version<=1 && minor_version<3) || variant)
        {
            #warning print message tp stderr
            result = VK_RESULT_MAX_ENUM;
        }
    }

    if(result == VK_SUCCESS)
    {
        VkApplicationInfo application_info=(VkApplicationInfo)
        {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = NULL,
            .pApplicationName = setup->application_name,
            .applicationVersion = setup->application_version,
            .pEngineName = SOLIPSIX_ENGINE_NAME,
            .engineVersion = SOLIPSIX_ENGINE_VERSION,
            .apiVersion = api_version,
        };

        for(i=0;i<setup->extension_count;i++)
        {
            puts(setup->extension_names[i]);
        }

        VkInstanceCreateInfo instance_creation_info=(VkInstanceCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &application_info,
            .enabledLayerCount = setup->layer_count,
            .ppEnabledLayerNames = setup->layer_names,
            .enabledExtensionCount = extension_count,
            .ppEnabledExtensionNames = extension_names,
        };
        result = vkCreateInstance(&instance_creation_info, setup->host_allocator, &instance->instance);
    }

    free(extension_names);
    if(result != VK_SUCCESS)
    {
        printf("FUCK %d\n",result);
    }

    return result;
}

VkResult cvm_vk_instance_initialise_for_SDL(struct cvm_vk_instance* instance, const cvm_vk_instance_setup * setup)
{
    cvm_vk_instance_setup internal_setup;
    SDL_Window * window;
    unsigned int num_extensions_required_by_SDL;
//    const char * validation = "VK_LAYER_KHRONOS_validation";
    VkResult result = VK_SUCCESS;

    internal_setup.layer_names = setup->layer_names;
    internal_setup.layer_count = setup->layer_count;
    internal_setup.host_allocator = setup->host_allocator;
    internal_setup.application_name = setup->application_name;
    internal_setup.application_version = setup->application_version;

    result = VK_RESULT_MAX_ENUM;/// default result of error

    window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_VULKAN|SDL_WINDOW_HIDDEN);

    if(window)
    {
        if(SDL_Vulkan_GetInstanceExtensions(window,&num_extensions_required_by_SDL,NULL))
        {
            internal_setup.extension_count = setup->extension_count + num_extensions_required_by_SDL;
            internal_setup.extension_names = malloc(sizeof(const char*) * internal_setup.extension_count);
            memcpy(internal_setup.extension_names, setup->extension_names, sizeof(const char*) * setup->extension_count);

            if(SDL_Vulkan_GetInstanceExtensions(window, &num_extensions_required_by_SDL, internal_setup.extension_names + setup->extension_count))
            {
                result = cvm_vk_instance_initialise(instance, &internal_setup);
            }

            free(internal_setup.extension_names);
        }

        SDL_DestroyWindow(window);
    }

    return result;
}

void cvm_vk_instance_terminate(struct cvm_vk_instance* instance)
{
    vkDestroyInstance(instance->instance, instance->host_allocator);
}

VkSurfaceKHR cvm_vk_create_surface_from_SDL_window(const struct cvm_vk_instance* instance, SDL_Window * window)
{
    SDL_bool created_surface;
    VkSurfaceKHR surface;

    created_surface = SDL_Vulkan_CreateSurface(window, instance->instance, &surface);

    return created_surface ? surface : VK_NULL_HANDLE;
}

void cvm_vk_destroy_surface(const struct cvm_vk_instance* instance, VkSurfaceKHR surface)
{
    vkDestroySurfaceKHR(instance->instance, surface, instance->host_allocator);
}





static float cvm_vk_internal_device_feature_validation_function(const VkBaseInStructure* valid_list,
                                                               const VkPhysicalDeviceProperties* device_properties,
                                                               const VkPhysicalDeviceMemoryProperties* memory_properties,
                                                               const VkExtensionProperties* extension_properties,
                                                               uint32_t extension_count,
                                                               const VkQueueFamilyProperties * queue_family_properties,
                                                               uint32_t queue_family_count)
{
    uint32_t i,extensions_found;
    const VkBaseInStructure* entry;
    const VkPhysicalDeviceVulkan12Features * features_12;
    const VkPhysicalDeviceVulkan13Features * features_13;

    #warning SHOULD actually check if presentable with some SDL window (maybe externally? and in default case) because the selected laptop adapter may alter presentability later!

    /// properties unused
    (void)device_properties;
    (void)memory_properties;
    (void)extension_properties;
    (void)extension_count;

    for(entry = valid_list;entry;entry=entry->pNext)
    {
        switch(entry->sType)
        {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
                features_12 = (const VkPhysicalDeviceVulkan12Features*)entry;
                if(features_12->timelineSemaphore==VK_FALSE)return 0.0;
                break;

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES:
                features_13 = (const VkPhysicalDeviceVulkan13Features*)entry;
                if(features_13->synchronization2==VK_FALSE)return 0.0;
                break;

            default: break;
        }
    }

    extensions_found=0;
    for(i=0;i<extension_count;i++)
    {
        if(strcmp(extension_properties[i].extensionName,"VK_KHR_swapchain")==0)
        {
            extensions_found++;
        }
    }
    if(extensions_found!=1)return 0.0;

    switch(device_properties->deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 1.0;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 0.25;
        default: return 0.0625;
    }
}

static void cvm_vk_internal_device_feature_request_function(VkBaseOutStructure* set_list,
                                                            bool* extension_set_table,
                                                            const VkBaseInStructure* valid_list,
                                                            const VkPhysicalDeviceProperties* device_properties,
                                                            const VkPhysicalDeviceMemoryProperties* memory_properties,
                                                            const VkExtensionProperties* extension_properties,
                                                            uint32_t extension_count,
                                                            const VkQueueFamilyProperties * queue_family_properties,
                                                            uint32_t queue_family_count)
{
    uint32_t i;
    VkBaseOutStructure* set_entry;
    VkPhysicalDeviceVulkan12Features * set_features_12;
    VkPhysicalDeviceVulkan13Features * features_13;

    /// properties unused
    (void)valid_list;
    (void)device_properties;
    (void)memory_properties;

    for(set_entry = set_list;set_entry;set_entry = set_entry->pNext)
    {
        switch(set_entry->sType)
        {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
                set_features_12 = (VkPhysicalDeviceVulkan12Features*)set_entry;
                set_features_12->timelineSemaphore=VK_TRUE;
                break;

            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES:
                features_13 = (VkPhysicalDeviceVulkan13Features*)set_entry;
                features_13->synchronization2=VK_TRUE;
                break;

            default: break;
        }
    }

    for(i=0;i<extension_count;i++)
    {
        #warning (somehow) only do this if a surface is provided
        if(strcmp(extension_properties[i].extensionName,"VK_KHR_swapchain")==0)
        {
            extension_set_table[i]=true;
        }
    }
}


static void cvm_vk_internal_device_setup_init(cvm_vk_device_setup * internal, const cvm_vk_device_setup * external)
{
    const cvm_vk_device_setup empty_device_setup = {0};// does this actually need to be static?? what terrible code is this??

    const uint32_t internal_feature_count = 3;
    VkStructureType feature_struct_types[4] =
    {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
//        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR,
    };
    size_t feature_struct_sizes[4] =
    {
        sizeof(VkPhysicalDeviceVulkan11Features),
        sizeof(VkPhysicalDeviceVulkan12Features),
        sizeof(VkPhysicalDeviceVulkan13Features),
//        sizeof(VkPhysicalDevicePresentWaitFeaturesKHR),

    };

    uint32_t i,j;

    if(!external)
    {
        external = &empty_device_setup;
    }

    internal->feature_validation = malloc(sizeof(cvm_vk_device_feature_validation_function*)*(external->feature_validation_count+1));
    internal->feature_validation_count = external->feature_validation_count+1;
    internal->feature_validation[0] = &cvm_vk_internal_device_feature_validation_function;
    memcpy(internal->feature_validation+1,external->feature_validation,sizeof(cvm_vk_device_feature_validation_function*)*external->feature_validation_count);

    internal->feature_request=malloc(sizeof(cvm_vk_device_feature_request_function*)*(external->feature_request_count+1));
    internal->feature_request_count=external->feature_request_count+1;
    internal->feature_request[0]=&cvm_vk_internal_device_feature_request_function;
    memcpy(internal->feature_request+1,external->feature_request,sizeof(cvm_vk_device_feature_request_function*)*external->feature_request_count);


    ///following ones shouldnt contain duplicates
    internal->device_feature_struct_types = malloc(sizeof(VkStructureType)*(external->device_feature_struct_count+internal_feature_count));
    internal->device_feature_struct_sizes = malloc(sizeof(size_t)*(external->device_feature_struct_count+internal_feature_count));
    internal->device_feature_struct_count = internal_feature_count;
    memcpy(internal->device_feature_struct_types, feature_struct_types, sizeof(VkStructureType)*internal_feature_count);
    memcpy(internal->device_feature_struct_sizes, feature_struct_sizes, sizeof(size_t)*internal_feature_count);

    for(i=0;i<external->device_feature_struct_count;i++)
    {
        for(j=0;j<internal->device_feature_struct_count;j++)
        {
            if(external->device_feature_struct_types[i]==internal->device_feature_struct_types[j] || external->device_feature_struct_types[i]==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
            {
                break;
            }
        }
        if(j==internal->device_feature_struct_count)
        {
            internal->device_feature_struct_types[j] = external->device_feature_struct_types[i];
            internal->device_feature_struct_sizes[j] = external->device_feature_struct_sizes[i];
            internal->device_feature_struct_count++;
        }
    }

    internal->desired_graphics_queues = CVM_CLAMP(external->desired_graphics_queues,1,8);
    internal->desired_transfer_queues = CVM_CLAMP(external->desired_transfer_queues,1,8);
    internal->desired_async_compute_queues = CVM_CLAMP(external->desired_async_compute_queues,1,8);

    internal->instance = external->instance;


    internal->pipeline_cache_file_name = external->pipeline_cache_file_name;
}

static void cvm_vk_internal_device_setup_destroy(cvm_vk_device_setup * setup)
{
    free(setup->feature_validation);
    free(setup->feature_request);
    free(setup->device_feature_struct_types);
    free(setup->device_feature_struct_sizes);
}

static VkPhysicalDeviceFeatures2* cvm_vk_create_device_feature_structure_list(const cvm_vk_device_setup * device_setup)
{
    uint32_t i;
    VkBaseInStructure * last_feature_struct;
    VkBaseInStructure * feature_struct;
    VkPhysicalDeviceFeatures2 * features;

    /// set up chain of features, used to ensure
    /// init to zero, which is false for when this will be setting up only required features
    features = calloc(1,sizeof(VkPhysicalDeviceFeatures2));
    features->sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features->pNext=NULL;

    last_feature_struct=(VkBaseInStructure*)features;

    for(i=0;i<device_setup->device_feature_struct_count;i++)
    {
        /// init to zero, which is false for when this will be setting up only required features
        feature_struct = calloc(1,device_setup->device_feature_struct_sizes[i]);
        feature_struct->sType = device_setup->device_feature_struct_types[i];
        feature_struct->pNext = NULL;

        last_feature_struct->pNext = feature_struct;
        last_feature_struct = feature_struct;
    }

    return features;
}

static void cvm_vk_destroy_device_feature_structure_list(VkPhysicalDeviceFeatures2 * features)
{
    VkBaseInStructure * s_next;
    VkBaseInStructure * s;

    for(s=(VkBaseInStructure*)features; s; s=s_next)
    {
        s_next=(VkBaseInStructure*)s->pNext;
        free(s);
    }
}

static float cvm_vk_test_physical_device_capabilities(VkPhysicalDevice physical_device_to_test, const cvm_vk_device_setup * device_setup)
{
    uint32_t i,queue_family_count,extension_count;
    VkQueueFamilyProperties * queue_family_properties;
    VkBool32 surface_supported;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkPhysicalDeviceFeatures2 * features;
    VkExtensionProperties * extensions;
    float score = 1.0;


    vkGetPhysicalDeviceProperties(physical_device_to_test, &properties);
    vkGetPhysicalDeviceMemoryProperties(physical_device_to_test,&memory_properties);

    features = cvm_vk_create_device_feature_structure_list(device_setup);
    vkGetPhysicalDeviceFeatures2(physical_device_to_test, features);

    vkEnumerateDeviceExtensionProperties(physical_device_to_test, NULL, &extension_count, NULL);
    extensions=malloc(sizeof(VkExtensionProperties)*extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device_to_test, NULL, &extension_count, extensions);

    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_to_test, &queue_family_count, NULL);
    queue_family_properties=malloc(sizeof(VkQueueFamilyProperties)*queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_to_test, &queue_family_count, queue_family_properties);

    /// external feature requirements
    for(i=0;i<device_setup->feature_validation_count;i++)
    {
        #warning pass in queue_family_satisfactory_bits and queue families!
        score *= device_setup->feature_validation[i]((VkBaseInStructure*)features, &properties, &memory_properties, extensions, extension_count, queue_family_properties, queue_family_count);
    }

    cvm_vk_destroy_device_feature_structure_list(features);
    free(extensions);
    free(queue_family_properties);

    printf("testing GPU : %s : d:%u v:%u : %f\n",properties.deviceName, properties.deviceID, properties.vendorID, score);

    return score;
}


/// returns the available device features and extensions
static VkPhysicalDevice cvm_vk_create_physical_device(VkInstance vk_instance, const cvm_vk_device_setup * device_setup)
{
    ///pick best physical device with all required features

    uint32_t i,device_count;
    VkPhysicalDevice * physical_devices;
    SDL_bool created_surface;
    float score,best_score;
    VkPhysicalDevice best_device;

    best_device = VK_NULL_HANDLE;

    CVM_VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &device_count, NULL));
    physical_devices = malloc(sizeof(VkPhysicalDevice)*device_count);
    CVM_VK_CHECK(vkEnumeratePhysicalDevices(vk_instance, &device_count, physical_devices));

    best_score = 0.0;
    for(i=0;i<device_count;i++)///check for dedicated gfx cards first
    {
        score = cvm_vk_test_physical_device_capabilities(physical_devices[i], device_setup);
        if(score > best_score)
        {
            best_device = physical_devices[i];
            best_score = score;
        }
    }

    free(physical_devices);

    return best_device;
}

static void cvm_vk_initialise_device_queue(cvm_vk_device * device,cvm_vk_device_queue * queue,uint32_t queue_family_index,uint32_t queue_index)
{
    cvm_vk_timeline_semaphore_initialise(device,&queue->timeline);
    vkGetDeviceQueue(device->device,queue_family_index,queue_index,&queue->queue);
}

static void cvm_vk_terminate_device_queue(cvm_vk_device * device,cvm_vk_device_queue * queue)
{
    cvm_vk_timeline_semaphore_terminate(device,&queue->timeline);
}

static void cvm_vk_initialise_device_queue_family(cvm_vk_device * device,cvm_vk_device_queue_family * queue_family,uint32_t queue_family_index,uint32_t queue_count)
{
    uint32_t i;

    VkCommandPoolCreateInfo command_pool_create_info=(VkCommandPoolCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .queueFamilyIndex=queue_family_index,
    };

    CVM_VK_CHECK(vkCreateCommandPool(device->device, &command_pool_create_info, device->host_allocator, &queue_family->internal_command_pool));

    queue_family->queues=malloc(sizeof(cvm_vk_device_queue)*queue_count);
    queue_family->queue_count=queue_count;
    for(i=0;i<queue_count;i++)
    {
        cvm_vk_initialise_device_queue(device,queue_family->queues+i,queue_family_index,i);
    }
}

static void cvm_vk_terminate_device_queue_family(cvm_vk_device * device,cvm_vk_device_queue_family * queue_family)
{
    uint32_t i;

    vkDestroyCommandPool(device->device, queue_family->internal_command_pool, device->host_allocator);

    for(i=0;i<queue_family->queue_count;i++)
    {
        cvm_vk_terminate_device_queue(device,queue_family->queues+i);
    }
    free(queue_family->queues);
}

static int cvm_vk_create_logical_device(cvm_vk_device * device, const cvm_vk_device_setup * device_setup)
{
    uint32_t i,j,queue_count,enabled_extension_count,available_extension_count;
    bool * enabled_extensions_table;
    VkExtensionProperties * enabled_extensions;
    VkPhysicalDeviceFeatures2 * enabled_features;
    VkExtensionProperties * available_extensions;
    const char ** enabled_extension_names;
    VkPhysicalDeviceFeatures2 * available_features;
    VkDeviceQueueCreateInfo * device_queue_creation_infos;
    VkQueueFlags queue_flags;
    bool graphics, transfer, compute;
    float * priorities;
    uint32_t queue_counts[CVM_VK_MAX_QUEUE_FAMILY_COUNT];

    /// being a little sneaky to circumvent const here
    vkGetPhysicalDeviceProperties(device->physical_device, (VkPhysicalDeviceProperties*)&device->properties);
    vkGetPhysicalDeviceMemoryProperties(device->physical_device, (VkPhysicalDeviceMemoryProperties*)&device->memory_properties);

    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &device->queue_family_count, NULL);
    if(device->queue_family_count>CVM_VK_MAX_QUEUE_FAMILY_COUNT)return -1;
    device->queue_family_properties=malloc(sizeof(VkQueueFamilyProperties)*device->queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, &device->queue_family_count, (VkQueueFamilyProperties*)device->queue_family_properties);

    available_features = cvm_vk_create_device_feature_structure_list(device_setup);
    vkGetPhysicalDeviceFeatures2(device->physical_device,available_features);

    vkEnumerateDeviceExtensionProperties(device->physical_device,NULL,&available_extension_count,NULL);
    available_extensions=malloc(sizeof(VkExtensionProperties)*available_extension_count);
    vkEnumerateDeviceExtensionProperties(device->physical_device,NULL,&available_extension_count,available_extensions);


    enabled_features = cvm_vk_create_device_feature_structure_list(device_setup);
    enabled_extensions_table=calloc(available_extension_count,sizeof(bool));

    for(i=0;i<device_setup->feature_request_count;i++)
    {
        device_setup->feature_request[i]((VkBaseOutStructure*)enabled_features,
                                         enabled_extensions_table,
                                         (VkBaseInStructure*)available_features,
                                         &device->properties,
                                         &device->memory_properties,
                                         available_extensions,
                                         available_extension_count,
                                         device->queue_family_properties,
                                         device->queue_family_count);
    }

    enabled_extension_count=0;
    for(i=0;i<available_extension_count;i++)
    {
        if(enabled_extensions_table[i])
        {
            enabled_extension_count++;
        }
    }

    enabled_extension_names=malloc(sizeof(const char*)*enabled_extension_count);
    enabled_extensions=malloc(sizeof(VkExtensionProperties)*enabled_extension_count);

    enabled_extension_count=0;
    for(i=0;i<available_extension_count;i++)
    {
        if(enabled_extensions_table[i])
        {
            enabled_extensions[enabled_extension_count]=available_extensions[i];
            enabled_extension_names[enabled_extension_count]=available_extensions[i].extensionName;
            enabled_extension_count++;
        }
    }

    device->features = enabled_features;
    device->extensions = enabled_extensions;
    device->extension_count = enabled_extension_count;


    device->graphics_queue_family_index=CVM_INVALID_U32_INDEX;
    device->transfer_queue_family_index=CVM_INVALID_U32_INDEX;
    device->fallback_present_queue_family_index=CVM_INVALID_U32_INDEX;
    device->async_compute_queue_family_index=CVM_INVALID_U32_INDEX;



    for(i=0;i<device->queue_family_count;i++)
    {
        queue_counts[i]=1;
        /// default of 1 per queue family
    }

    i=device->queue_family_count;
    while(i--)///assume lowest is best fit for anything in particular
    {
        queue_flags=device->queue_family_properties[i].queueFlags;
        graphics=queue_flags & VK_QUEUE_GRAPHICS_BIT;
        transfer=queue_flags & VK_QUEUE_TRANSFER_BIT;
        compute =queue_flags & VK_QUEUE_COMPUTE_BIT;

        if(graphics)
        {
            device->graphics_queue_family_index=i;
            queue_counts[i] = CVM_MIN(device->queue_family_properties[i].queueCount,device_setup->desired_graphics_queues);
        }

        if(transfer && !graphics && !compute)
        {
            device->transfer_queue_family_index=i;
            queue_counts[i] = CVM_MIN(device->queue_family_properties[i].queueCount,device_setup->desired_transfer_queues);
        }

        if(compute && !graphics)
        {
            device->async_compute_queue_family_index=i;
            queue_counts[i] = CVM_MIN(device->queue_family_properties[i].queueCount,device_setup->desired_async_compute_queues);
        }
    }


    #warning REMOVE THESE
    device->fallback_present_queue_family_index = device->async_compute_queue_family_index;
//    device->fallback_present_queue_family_index = device->graphics_queue_family_index;
#warning this is temporary while transfers get back up and running!
    device->transfer_queue_family_index = device->graphics_queue_family_index;
    if(device->transfer_queue_family_index==CVM_INVALID_U32_INDEX)device->transfer_queue_family_index=device->graphics_queue_family_index;



    device_queue_creation_infos = malloc(sizeof(VkDeviceQueueCreateInfo)*device->queue_family_count);
    for(i=0;i<device->queue_family_count;i++)
    {
        queue_count=queue_counts[i];
        priorities=malloc(sizeof(float)*queue_count);

        for(j=0;j<queue_count;j++)
        {
            priorities[j] = 1.0;
        }

        device_queue_creation_infos[i]=(VkDeviceQueueCreateInfo)
        {
            .sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .queueFamilyIndex=i,
            .queueCount=queue_count,
            .pQueuePriorities=priorities,
        };
    }

    VkDeviceCreateInfo device_creation_info=(VkDeviceCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext=enabled_features,
        .flags=0,
        .queueCreateInfoCount=device->queue_family_count,
        .pQueueCreateInfos= device_queue_creation_infos,
        .enabledExtensionCount=enabled_extension_count,
        .ppEnabledExtensionNames=enabled_extension_names,
        .pEnabledFeatures=NULL,///using features2 in pNext chain instead
    };

    CVM_VK_CHECK(vkCreateDevice(device->physical_device, &device_creation_info, device_setup->instance->host_allocator, &device->device));

    device->queue_families = malloc(sizeof(cvm_vk_device_queue_family)*device->queue_family_count);

    for(i=0;i<device->queue_family_count;i++)
    {
        cvm_vk_initialise_device_queue_family(device,device->queue_families+i,i,queue_counts[i]);
    }



    for(i=0;i<device->queue_family_count;i++)
    {
        free((void*)device_queue_creation_infos[i].pQueuePriorities);
    }

    cvm_vk_destroy_device_feature_structure_list(available_features);
    free(available_extensions);

    free(enabled_extensions_table);
    free(enabled_extension_names);
    free(device_queue_creation_infos);
}

static void cvm_vk_destroy_logical_device(cvm_vk_device * device)
{
    uint32_t i;
    for(i=0;i<device->queue_family_count;i++)
    {
        cvm_vk_terminate_device_queue_family(device,device->queue_families+i);
    }
    free(device->queue_families);
    free((void*)device->queue_family_properties);

    vkDestroyDevice(device->device, device->host_allocator);
    cvm_vk_destroy_device_feature_structure_list((void*)device->features);
    free((void*)device->extensions);
}



static void cvm_vk_create_transfer_chain(void)
{
    //
}

static void cvm_vk_destroy_transfer_chain(void)
{
    //
}





VkSemaphoreSubmitInfo cvm_vk_binary_semaphore_submit_info(VkSemaphore semaphore,VkPipelineStageFlags2 stages)
{
    return (VkSemaphoreSubmitInfo)
    {
        .sType=VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext=NULL,
        .semaphore=semaphore,
        .value=0,///not needed, is binary
        .stageMask=stages,
        .deviceIndex=0
    };
}

VkResult cvm_vk_create_descriptor_set_layout_registering_requirements(VkDescriptorSetLayout* set_layout, const VkDescriptorSetLayoutCreateInfo* set_layout_create_info, const struct cvm_vk_device* device, struct cvm_vk_descriptor_pool_requirements* pool_requirements, uint32_t sets_using_this_layout)
{
    VkResult r;
    uint32_t i,j;
    const VkDescriptorSetLayoutBinding* binding;

    r = vkCreateDescriptorSetLayout(device->device, set_layout_create_info, device->host_allocator, set_layout);

    if(r==VK_SUCCESS)
    {
        for(i=0;i<set_layout_create_info->bindingCount;i++)
        {
            binding = set_layout_create_info->pBindings + i;

            for(j=0;j<pool_requirements->type_count;j++)
            {
                if(pool_requirements->type_sizes[j].type == binding->descriptorType)
                {
                    pool_requirements->type_sizes[j].descriptorCount += binding->descriptorCount * sets_using_this_layout;
                    break;
                }
            }
            if(j == pool_requirements->type_count)
            {
                pool_requirements->type_sizes[pool_requirements->type_count++]=(VkDescriptorPoolSize)
                {
                    .type = binding->descriptorType,
                    .descriptorCount = binding->descriptorCount * sets_using_this_layout,
                };
            }

            pool_requirements->set_count += sets_using_this_layout;
        }
    }

    return r;
}


VkResult cvm_vk_create_descriptor_pool_for_sizes(VkDescriptorPool* pool, const struct cvm_vk_device* device, struct cvm_vk_descriptor_pool_requirements* pool_requirements)
{
    VkDescriptorPoolCreateInfo create_info =
    {
        .sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext=NULL,
        .flags=0,///by not specifying individual free must reset whole pool (which is fine)
        .maxSets=pool_requirements->set_count,
        .poolSizeCount=pool_requirements->type_count,
        .pPoolSizes=pool_requirements->type_sizes
    };

    return vkCreateDescriptorPool(device->device, &create_info, device->host_allocator, pool);
}







VkFormat cvm_vk_get_screen_format(void)
{
    return 0;
}

uint32_t cvm_vk_get_swapchain_image_count(void)
{
    return 0;
}

VkImageView cvm_vk_get_swapchain_image_view(uint32_t index)
{
    return VK_NULL_HANDLE;
}


static inline void cvm_vk_defaults_initialise(struct cvm_vk_defaults* defaults, const cvm_vk_device * device)
{
    VkResult created;

    VkSamplerCreateInfo fetch_sampler_create_info =
    {
        .sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .magFilter=VK_FILTER_NEAREST,
        .minFilter=VK_FILTER_NEAREST,
        .mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias=0.0f,
        .anisotropyEnable=VK_FALSE,
        .maxAnisotropy=0.0f,
        .compareEnable=VK_FALSE,
        .compareOp=VK_COMPARE_OP_NEVER,
        .minLod=0.0f,
        .maxLod=0.0f,
        .borderColor=VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates=VK_TRUE,
    };

    created = vkCreateSampler(device->device, &fetch_sampler_create_info, device->host_allocator, &defaults->fetch_sampler);
    assert(created == VK_SUCCESS);
}

static inline void cvm_vk_defaults_terminate(struct cvm_vk_defaults* defaults, const cvm_vk_device * device)
{
    vkDestroySampler(device->device, defaults->fetch_sampler, device->host_allocator);
}


static inline void cvm_vk_pipeline_cache_initialise(struct cvm_vk_pipeline_cache* pipeline_cache, const cvm_vk_device * device, const char* cache_file_name)
{
    VkResult result;
    FILE * cache_file;
    size_t cache_size;
    size_t count;
    uint64_t cache_size_disk;
    void* cache_data;
    bool success;

    // use "default" value on failure
    pipeline_cache->cache = VK_NULL_HANDLE;
    pipeline_cache->file_name = NULL;

    if(cache_file_name == NULL)
    {
        fprintf(stderr, "no pipeline cache file provided, consider using one to improve startup performance\n");
        return;
    }

    pipeline_cache->file_name = cvm_strdup(cache_file_name);

    cache_file = fopen(cache_file_name,"rb");

    cache_size = 0;
    cache_data = NULL;

    if(cache_file)
    {
        #warning also write identifier to know whether to ignore
        success = false;
        count = fread(&cache_size_disk, sizeof(uint64_t), 1, cache_file);
        /// could/should also read/write identifier/version here...
        if(count == 1)
        {
            cache_size = cache_size_disk;
            cache_data = malloc(cache_size);
            count = fread(cache_data, 1, cache_size, cache_file);

            if(count != cache_size)
            {
                /// couldn't read whole cache so invalidate it
                cache_size = 0;
            }
            else
            {
                success = true;
            }
        }

        if(!success)
        {
            fprintf(stderr, "cound not read pipeline cache file %s, it appeared to be corrupted\n", pipeline_cache->file_name);
        }

        fclose(cache_file);
    }

    VkPipelineCacheCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .initialDataSize = cache_size,
        .pInitialData =  cache_data,
    };

    result = vkCreatePipelineCache(device->device, &create_info, device->host_allocator, &pipeline_cache->cache);

    free(cache_data);
}

static inline void cvm_vk_pipeline_cache_terminate(struct cvm_vk_pipeline_cache* pipeline_cache, const cvm_vk_device * device)
{
    VkResult result;
    FILE * cache_file;
    size_t cache_size;
    size_t count;
    void* cache_data;
    uint64_t cache_size_disk;

    if(pipeline_cache->file_name && pipeline_cache->cache != VK_NULL_HANDLE)
    {
        result = vkGetPipelineCacheData(device->device, device->pipeline_cache.cache, &cache_size, NULL);

        if(result == VK_SUCCESS && cache_size)
        {
            cache_data = malloc(cache_size);

            result = vkGetPipelineCacheData(device->device, device->pipeline_cache.cache, &cache_size, cache_data);

            cache_file = fopen(pipeline_cache->file_name,"wb");

            if(cache_file && result == VK_SUCCESS)
            {
                cache_size_disk = cache_size;
                count = fwrite(&cache_size_disk, sizeof(uint64_t), 1, cache_file);
                if(count == 1)
                {
                    count = fwrite(cache_data, 1, cache_size, cache_file);
                }

                fclose(cache_file);

                if(count != cache_size)
                {
                    fprintf(stderr, "cound not write pipeline cache file %s", pipeline_cache->file_name);
                    remove(pipeline_cache->file_name);
                }
            }

            free(cache_data);
        }
    }

    if(pipeline_cache->cache != VK_NULL_HANDLE)
    {
        vkDestroyPipelineCache(device->device, device->pipeline_cache.cache, device->host_allocator);
    }

    // filename owned (created with strdup) so free
    free(pipeline_cache->file_name);
}


int cvm_vk_device_initialise(cvm_vk_device * device, const cvm_vk_device_setup * external_device_setup)
{
    cvm_vk_device_setup device_setup;

    cvm_vk_internal_device_setup_init(&device_setup, external_device_setup);// should be able to handle external being NULL

    device->host_allocator = device_setup.instance->host_allocator;

    device->physical_device = cvm_vk_create_physical_device(device_setup.instance->instance, &device_setup);
    if(device->physical_device==VK_NULL_HANDLE)return -1;

    cvm_vk_create_logical_device(device, &device_setup);

    #warning generally could do with cleanup, perhaps separating logical and physical devices?

    device->resource_identifier_monotonic = malloc(sizeof(atomic_uint_least64_t));
    atomic_init(device->resource_identifier_monotonic, 1);/// nonzero for debugging zero is invalid while testing

    cvm_vk_pipeline_cache_initialise(&device->pipeline_cache, device, device_setup.pipeline_cache_file_name);

    cvm_vk_create_transfer_chain();///make conditional on separate transfer queue?

    cvm_vk_create_defaults_old();
    cvm_vk_defaults_initialise(&device->defaults, device);

    cvm_vk_internal_device_setup_destroy(&device_setup);

    return 0;
}

void cvm_vk_device_terminate(cvm_vk_device * device)
{
    /// is the following necessary???
    /// shouldnt we have waited on all things using this device before we terminate it?
    vkDeviceWaitIdle(device->device);

    cvm_vk_destroy_defaults_old();
    cvm_vk_defaults_terminate(&device->defaults, device);

    cvm_vk_pipeline_cache_terminate(&device->pipeline_cache, device);

    cvm_vk_destroy_transfer_chain();///make conditional on separate transfer queue?

    cvm_vk_destroy_logical_device(device);

    free(device->resource_identifier_monotonic);
}












void cvm_vk_submit_graphics_work(cvm_vk_module_work_payload * payload,cvm_vk_payload_flags flags)
{
    #warning REMOVE
}

VkFence cvm_vk_create_fence(const cvm_vk_device * device,bool initially_signalled)
{
    VkFence fence;
    VkResult r;

    VkFenceCreateInfo create_info=
    {
        .sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext=NULL,
        .flags=initially_signalled ? VK_FENCE_CREATE_SIGNALED_BIT : 0,
    };

    r=vkCreateFence(device->device, &create_info, device->host_allocator, &fence);
    assert(r==VK_SUCCESS);

    return r==VK_SUCCESS ? fence : VK_NULL_HANDLE;
}

void cvm_vk_destroy_fence(const cvm_vk_device * device,VkFence fence)
{
    vkDestroyFence(device->device, fence, device->host_allocator);
}

void cvm_vk_wait_on_fence_and_reset(const cvm_vk_device * device, VkFence fence)
{
    CVM_VK_CHECK(vkWaitForFences(device->device, 1, &fence, VK_TRUE,CVM_VK_DEFAULT_TIMEOUT));
    vkResetFences(device->device, 1, &fence);
}

VkSemaphore cvm_vk_create_binary_semaphore(const cvm_vk_device * device)
{
    VkSemaphore semaphore;
    VkResult r;

    VkSemaphoreCreateInfo create_info=(VkSemaphoreCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext=NULL,
        .flags=0
    };

    r=vkCreateSemaphore(device->device, &create_info, device->host_allocator, &semaphore);
    assert(r==VK_SUCCESS);

    return r==VK_SUCCESS ? semaphore : VK_NULL_HANDLE;
}

void cvm_vk_destroy_binary_semaphore(const cvm_vk_device * device,VkSemaphore semaphore)
{
    vkDestroySemaphore(device->device, semaphore, NULL);
}



void cvm_vk_create_render_pass(VkRenderPass * render_pass,VkRenderPassCreateInfo * info)
{
    CVM_VK_CHECK(vkCreateRenderPass(cvm_vk_.device,info,NULL,render_pass));
}

void cvm_vk_destroy_render_pass(VkRenderPass render_pass)
{
    vkDestroyRenderPass(cvm_vk_.device,render_pass,NULL);
}

void cvm_vk_create_framebuffer(VkFramebuffer * framebuffer,VkFramebufferCreateInfo * info)
{
    CVM_VK_CHECK(vkCreateFramebuffer(cvm_vk_.device,info,NULL,framebuffer));
}

void cvm_vk_destroy_framebuffer(VkFramebuffer framebuffer)
{
    vkDestroyFramebuffer(cvm_vk_.device,framebuffer,NULL);
}


void cvm_vk_create_pipeline_layout(VkPipelineLayout * pipeline_layout,VkPipelineLayoutCreateInfo * info)
{
    CVM_VK_CHECK(vkCreatePipelineLayout(cvm_vk_.device,info,NULL,pipeline_layout));
}

void cvm_vk_destroy_pipeline_layout(VkPipelineLayout pipeline_layout)
{
    vkDestroyPipelineLayout(cvm_vk_.device,pipeline_layout,NULL);
}


void cvm_vk_create_graphics_pipeline(VkPipeline * pipeline,VkGraphicsPipelineCreateInfo * info)
{
    #warning use pipeline cache!?
    CVM_VK_CHECK(vkCreateGraphicsPipelines(cvm_vk_.device,VK_NULL_HANDLE,1,info,NULL,pipeline));
}

void cvm_vk_destroy_pipeline(VkPipeline pipeline)
{
    vkDestroyPipeline(cvm_vk_.device,pipeline,NULL);
}


///return VkPipelineShaderStageCreateInfo, but hold on to VkShaderModule (passed by ptr) for deletion at program cleanup ( that module can be kept inside the creation info! )
void cvm_vk_create_shader_stage_info(VkPipelineShaderStageCreateInfo * stage_info, const struct cvm_vk_device* device, const char * filename,VkShaderStageFlagBits stage)
{
    if(device==NULL) device = &cvm_vk_;
    static char * entrypoint="main";

    *stage_info=(VkPipelineShaderStageCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext=NULL,
        .flags=0,///not supported
        .stage=stage,
        .module=VK_NULL_HANDLE,// set later
        .pName=entrypoint,///always use main as entrypoint, use a static string such that this address is still valid after function return
        .pSpecializationInfo=NULL
    };

    FILE * f;
    size_t length;
    char * data_buffer;

    f=fopen(filename,"rb");

    assert(f || !fprintf(stderr,"COULD NOT LOAD SHADER: %s\n",filename));

    if(f)
    {
        fseek(f,0,SEEK_END);
        length=ftell(f);
        data_buffer=malloc(length);
        rewind(f);
        fread(data_buffer,1,length,f);
        fclose(f);

        VkShaderModuleCreateInfo shader_module_create_info=(VkShaderModuleCreateInfo)
        {
            .sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext=NULL,
            .flags=0,
            .codeSize=length,
            .pCode=(uint32_t*)data_buffer
        };

        VkResult r= vkCreateShaderModule(device->device, &shader_module_create_info, device->host_allocator, &stage_info->module);
        assert(r==VK_SUCCESS || !fprintf(stderr,"ERROR CREATING SHADER MODULE FROM FILE: %s\n",filename));

        free(data_buffer);
    }
}


void cvm_vk_destroy_shader_stage_info(VkPipelineShaderStageCreateInfo * stage_info, const struct cvm_vk_device* device)
{
    if(device==NULL) device = &cvm_vk_;
    vkDestroyShaderModule(device->device, stage_info->module, device->host_allocator);
}


void cvm_vk_create_descriptor_set_layout(VkDescriptorSetLayout * descriptor_set_layout,VkDescriptorSetLayoutCreateInfo * info)
{
    CVM_VK_CHECK(vkCreateDescriptorSetLayout(cvm_vk_.device,info,NULL,descriptor_set_layout));
}

void cvm_vk_destroy_descriptor_set_layout(VkDescriptorSetLayout descriptor_set_layout)
{
    vkDestroyDescriptorSetLayout(cvm_vk_.device,descriptor_set_layout,NULL);
}

void cvm_vk_create_descriptor_pool(VkDescriptorPool * descriptor_pool,VkDescriptorPoolCreateInfo * info)
{
    CVM_VK_CHECK(vkCreateDescriptorPool(cvm_vk_.device,info,NULL,descriptor_pool));
}

void cvm_vk_destroy_descriptor_pool(VkDescriptorPool descriptor_pool)
{
    vkDestroyDescriptorPool(cvm_vk_.device,descriptor_pool,NULL);
}

void cvm_vk_allocate_descriptor_sets(VkDescriptorSet * descriptor_sets,VkDescriptorSetAllocateInfo * info)
{
    CVM_VK_CHECK(vkAllocateDescriptorSets(cvm_vk_.device,info,descriptor_sets));
}

void cvm_vk_write_descriptor_sets(VkWriteDescriptorSet * writes,uint32_t count)
{
    vkUpdateDescriptorSets(cvm_vk_.device,count,writes,0,NULL);
}

void cvm_vk_create_image(VkImage * image,VkImageCreateInfo * info)
{
    CVM_VK_CHECK(vkCreateImage(cvm_vk_.device,info,NULL,image));
}

void cvm_vk_destroy_image(VkImage image)
{
    vkDestroyImage(cvm_vk_.device,image,NULL);
}

static uint32_t sol_vk_find_appropriate_memory_type(const cvm_vk_device * device, uint32_t supported_type_bits, VkMemoryPropertyFlags required_properties)
{
    uint32_t i;
    for(i=0; i<device->memory_properties.memoryTypeCount; i++)
    {
        if(( supported_type_bits & 1<<i ) && ((device->memory_properties.memoryTypes[i].propertyFlags & required_properties) == required_properties))
        {
            return i;
        }
    }
    return CVM_INVALID_U32_INDEX;
}


void cvm_vk_set_view_create_info_using_image_create_info(VkImageViewCreateInfo* view_create_info, const VkImageCreateInfo* image_create_info, VkImage image)
{
    VkImageViewType view_type;
    VkImageAspectFlags aspect;

    /// cannot create a cube array like this unfortunately
    switch(image_create_info->imageType)
    {
    case VK_IMAGE_TYPE_1D:
        view_type = (image_create_info->arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        break;
    case VK_IMAGE_TYPE_2D:
        view_type = (image_create_info->arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        break;
    case VK_IMAGE_TYPE_3D:
        assert(image_create_info->arrayLayers == 1);
        view_type = VK_IMAGE_VIEW_TYPE_3D;
        break;
    default:
        assert(false);// this is unhandled
        view_type = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    }

    /// remember these are DEFAULTS, none of the exotic aspects need be considered
    switch(image_create_info->format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        break;
    case VK_FORMAT_S8_UINT:
        aspect = VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
    default:
        aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        break;
    }

    *view_create_info = (VkImageViewCreateInfo)
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = image,
        .viewType = view_type,
        .format = image_create_info->format,
        .components = (VkComponentMapping)
        {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = (VkImageSubresourceRange)
        {
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = image_create_info->mipLevels,
            .baseArrayLayer = 0,
            .layerCount = image_create_info->arrayLayers,
        }
    };
}

VkResult cvm_vk_allocate_and_bind_memory_for_images(VkDeviceMemory * memory,VkImage * images,uint32_t image_count,VkMemoryPropertyFlags required_properties,VkMemoryPropertyFlags desired_properties)
{
    #warning REMOVE THIS FUNCTION
    VkDeviceSize offsets[image_count];
    VkDeviceSize current_offset;

    VkMemoryDedicatedRequirements dedicated_requirements =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
        .pNext = NULL,
    };
    VkMemoryRequirements2 memory_requirements =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .pNext = &dedicated_requirements,
    };
    VkImageMemoryRequirementsInfo2 image_requirements_info =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
        .pNext = NULL,
        // .image = image
    };
    uint32_t i,memory_type_index,supported_type_bits;
    VkResult result = VK_SUCCESS;

    *memory = VK_NULL_HANDLE;

    current_offset = 0;
    supported_type_bits = 0xFFFFFFFF;
    for(i=0;i<image_count;i++)
    {
        image_requirements_info.image = images[i];
        vkGetImageMemoryRequirements2(cvm_vk_.device, &image_requirements_info, &memory_requirements);
        printf("==== %d : %d @ %lu\n", dedicated_requirements.prefersDedicatedAllocation, dedicated_requirements.requiresDedicatedAllocation,  memory_requirements.memoryRequirements.size);
        #warning probably want to consider dedicated allocations as above, possibly worthwhile to allocate images singularly, or use a memory stack to handle images!
        #warning instead probably best to change strategy, have general image allocator (from heap) and image struct that can hold memory in cases where that is best
        #warning possibly device based image memory set?

        offsets[i] = cvm_vk_align(current_offset, memory_requirements.memoryRequirements.alignment);
        current_offset = offsets[i] + memory_requirements.memoryRequirements.size;
        supported_type_bits &= memory_requirements.memoryRequirements.memoryTypeBits;
    }

    if(!supported_type_bits)
    {
        //fprintf(stderr,"CVM VK ERROR - images have no singular supported memory types, consider splitting backing memory\n");
        result = VK_RESULT_MAX_ENUM;
    }

    if(result == VK_SUCCESS)
    {
        memory_type_index = sol_vk_find_appropriate_memory_type(&cvm_vk_, supported_type_bits, desired_properties|required_properties);

        if(memory_type_index == CVM_INVALID_U32_INDEX)
        {
            memory_type_index = sol_vk_find_appropriate_memory_type(&cvm_vk_, supported_type_bits, required_properties);
        }

        if(memory_type_index == CVM_INVALID_U32_INDEX)
        {
            //fprintf(stderr,"CVM VK ERROR - images have no singular supported memory types with required properties, consider splitting backing memory, or perhaps using less stringent requirements\n");
            result = VK_RESULT_MAX_ENUM;
        }
    }

    if(result == VK_SUCCESS)
    {
        VkMemoryAllocateInfo memory_allocate_info=(VkMemoryAllocateInfo)
        {
            .sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext=NULL,
            .allocationSize = current_offset,
            .memoryTypeIndex = memory_type_index
        };

        result = vkAllocateMemory(cvm_vk_.device, &memory_allocate_info, cvm_vk_.host_allocator, memory);
    }

    for(i=0 ; result == VK_SUCCESS && i < image_count ; i++)
    {
        result = vkBindImageMemory(cvm_vk_.device, images[i], *memory, offsets[i]);
    }

    if(result != VK_SUCCESS)
    {
        if(*memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(cvm_vk_.device, *memory, cvm_vk_.host_allocator);
        }
    }

    return result;
}

#warning unify the approach used here and in buffer creation, either pass in all variables or pass in struct(s) for in/out
VkResult cvm_vk_create_images(const cvm_vk_device* device, const VkImageCreateInfo* image_create_infos, uint32_t image_count, VkDeviceMemory* memory,VkImage* images, VkImageView* default_image_views)
{
    #warning REMOVE THIS FUNCTION
    uint32_t i;
    VkResult result = VK_SUCCESS;
    VkImageViewCreateInfo view_create_info;
    #warning have this unwind resources created upon failure

    *memory = VK_NULL_HANDLE;

    for(i=0;i<image_count;i++)
    {
        images[i] = VK_NULL_HANDLE;
    }

    if(default_image_views)
    {
        for(i=0;i<image_count;i++)
        {
            default_image_views[i] = VK_NULL_HANDLE;
        }
    }

    for(i=0;i<image_count;i++)
    {
        result = vkCreateImage(device->device, image_create_infos+i, device->host_allocator, images+i);
        if(result != VK_SUCCESS)
        {
            break;
        }
    }

    if(result == VK_SUCCESS)
    {
        result = cvm_vk_allocate_and_bind_memory_for_images(memory, images, image_count, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if(default_image_views && result == VK_SUCCESS)
        {
            for(i=0;i<image_count;i++)
            {
                cvm_vk_set_view_create_info_using_image_create_info(&view_create_info, image_create_infos+i, images[i]);
                result = vkCreateImageView(device->device, &view_create_info, device->host_allocator, default_image_views+i);
                if(result != VK_SUCCESS)
                {
                    break;
                }
            }
        }
    }

    if(result != VK_SUCCESS)
    {
        for(i=0;i<image_count;i++)
        {
            if(images[i] != VK_NULL_HANDLE)
            {
                vkDestroyImage(device->device, images[i], device->host_allocator);
            }
            if(default_image_views && default_image_views[i] != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device->device, default_image_views[i], device->host_allocator);
            }
        }
        if(*memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device->device, *memory, device->host_allocator);
        }
    }

    return result;
}


VkResult sol_vk_image_create(struct sol_vk_image* image, const cvm_vk_device* device, const VkImageCreateInfo* image_create_info, bool create_default_view)
{
    VkResult result;
    uint32_t memory_type_index;

    VkImageViewCreateInfo view_create_info;
    VkMemoryDedicatedRequirements dedicated_requirements =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
        .pNext = NULL,
    };
    VkMemoryRequirements2 memory_requirements =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .pNext = &dedicated_requirements,
    };


    #warning merge this concept with managed image?
    *image = (struct sol_vk_image)
    {
        .image        = VK_NULL_HANDLE,
        .default_view = VK_NULL_HANDLE,
        .memory       = VK_NULL_HANDLE,
    };



    result = vkCreateImage(device->device, image_create_info, device->host_allocator, &image->image);

    if(result == VK_SUCCESS)
    {
        const VkImageMemoryRequirementsInfo2 image_requirements_info =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
            .pNext = NULL,
            .image = image->image,
        };

        vkGetImageMemoryRequirements2(device->device, &image_requirements_info, &memory_requirements);

        printf("sol_vk_image_create: %d : %d @ %lu\n", dedicated_requirements.prefersDedicatedAllocation, dedicated_requirements.requiresDedicatedAllocation,  memory_requirements.memoryRequirements.size);

        memory_type_index = sol_vk_find_appropriate_memory_type(device, memory_requirements.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if(memory_type_index == CVM_INVALID_U32_INDEX)
        {
            memory_type_index = sol_vk_find_appropriate_memory_type(device, memory_requirements.memoryRequirements.memoryTypeBits, 0);
        }

        if(memory_type_index == CVM_INVALID_U32_INDEX)
        {
            result = VK_ERROR_UNKNOWN;
        }
    }

    if(result == VK_SUCCESS)
    {
        #warning unless dedicated allocation is required; can probably use an existing memory allocation with some offset (same w/ binding)
        #warning VkMemoryDedicatedAllocateInfo in next!
        const VkMemoryAllocateInfo memory_allocate_info =
        {
            .sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext=NULL,
            .allocationSize = memory_requirements.memoryRequirements.size,
            .memoryTypeIndex = memory_type_index
        };

        result = vkAllocateMemory(device->device, &memory_allocate_info, device->host_allocator, &image->memory);
    }

    if(result == VK_SUCCESS)
    {
        result = vkBindImageMemory(cvm_vk_.device, image->image, image->memory, 0);
    }

    if(result == VK_SUCCESS && create_default_view)
    {
        cvm_vk_set_view_create_info_using_image_create_info(&view_create_info, image_create_info, image->image);
        result = vkCreateImageView(device->device, &view_create_info, device->host_allocator, &image->default_view);
    }

    if(result != VK_SUCCESS)
    {
        sol_vk_image_destroy(image, device);
    }

    return result;
}

void sol_vk_image_destroy(struct sol_vk_image* image, const cvm_vk_device* device)
{
    if(image->image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device->device, image->image, device->host_allocator);
        image->image = VK_NULL_HANDLE;
    }
    if(image->default_view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->device, image->default_view, device->host_allocator);
        image->default_view = VK_NULL_HANDLE;
    }
    if(image->memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->device, image->memory, device->host_allocator);
        image->memory = VK_NULL_HANDLE;
    }
}


void cvm_vk_create_image_view(VkImageView * image_view,VkImageViewCreateInfo * info)
{
    CVM_VK_CHECK(vkCreateImageView(cvm_vk_.device,info,NULL,image_view));
}

void cvm_vk_destroy_image_view(VkImageView image_view)
{
    vkDestroyImageView(cvm_vk_.device,image_view,NULL);
}

void cvm_vk_create_sampler(VkSampler * sampler,VkSamplerCreateInfo * info)
{
    CVM_VK_CHECK(vkCreateSampler(cvm_vk_.device,info,NULL,sampler));
}

void cvm_vk_destroy_sampler(VkSampler sampler)
{
    vkDestroySampler(cvm_vk_.device,sampler,NULL);
}

void cvm_vk_free_memory(VkDeviceMemory memory)
{
    vkFreeMemory(cvm_vk_.device,memory,NULL);
}

///unlike other functions, this one takes abstract/resultant data rather than just generic creation info
void cvm_vk_create_buffer(VkBuffer * buffer,VkDeviceMemory * memory,VkBufferUsageFlags usage,VkDeviceSize size,void ** mapping,bool * mapping_coherent,VkMemoryPropertyFlags required_properties,VkMemoryPropertyFlags desired_properties)
{
    #warning remove
    uint32_t memory_type_index,supported_type_bits;

    VkBufferCreateInfo buffer_create_info=(VkBufferCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext=NULL,
        .flags=0,
        .size=size,
        .usage=usage,
        .sharingMode=VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount=0,
        .pQueueFamilyIndices=NULL
    };

    CVM_VK_CHECK(vkCreateBuffer(cvm_vk_.device,&buffer_create_info,NULL,buffer));

    VkMemoryRequirements buffer_memory_requirements;
    vkGetBufferMemoryRequirements(cvm_vk_.device,*buffer,&buffer_memory_requirements);

    memory_type_index = sol_vk_find_appropriate_memory_type(&cvm_vk_, buffer_memory_requirements.memoryTypeBits, desired_properties|required_properties);

    if(memory_type_index == CVM_INVALID_U32_INDEX)
    {
        memory_type_index = sol_vk_find_appropriate_memory_type(&cvm_vk_, buffer_memory_requirements.memoryTypeBits, required_properties);
        assert(memory_type_index != CVM_INVALID_U32_INDEX);
    }


    VkMemoryAllocateInfo memory_allocate_info=(VkMemoryAllocateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext=NULL,
        .allocationSize=buffer_memory_requirements.size,
        .memoryTypeIndex=memory_type_index
    };

    CVM_VK_CHECK(vkAllocateMemory(cvm_vk_.device,&memory_allocate_info,NULL,memory));

    CVM_VK_CHECK(vkBindBufferMemory(cvm_vk_.device,*buffer,*memory,0));///offset/alignment kind of irrelevant because of 1 buffer per allocation paradigm

    if(cvm_vk_.memory_properties.memoryTypes[memory_type_index].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        CVM_VK_CHECK(vkMapMemory(cvm_vk_.device,*memory,0,VK_WHOLE_SIZE,0,mapping));

        *mapping_coherent=!!(cvm_vk_.memory_properties.memoryTypes[memory_type_index].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    else
    {
        *mapping=NULL;
        *mapping_coherent=false;
    }
}

void cvm_vk_destroy_buffer(VkBuffer buffer,VkDeviceMemory memory,void * mapping)
{
    if(mapping)
    {
        vkUnmapMemory(cvm_vk_.device,memory);
    }

    vkDestroyBuffer(cvm_vk_.device,buffer,NULL);
    vkFreeMemory(cvm_vk_.device,memory,NULL);
}

void cvm_vk_flush_buffer_memory_range(VkMappedMemoryRange * flush_range)
{
    ///is this thread safe??
    CVM_VK_CHECK(vkFlushMappedMemoryRanges(cvm_vk_.device,1,flush_range));
}

uint32_t cvm_vk_get_buffer_alignment_requirements(VkBufferUsageFlags usage)
{
    #warning remove
    uint32_t alignment=1;

    /// need specialised functions for vertex buffers and index buffers (or leave it up to user)
    /// vertex: alignment = size largest primitive/type used in vertex inputs - need way to handle this as it isnt really specified, perhaps assume 16? perhaps rely on user to ensure this is satisfied
    /// index: size of index type used
    /// indirect: 4

    if(usage & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT) && alignment < cvm_vk_.properties.limits.optimalBufferCopyOffsetAlignment)
        alignment=cvm_vk_.properties.limits.optimalBufferCopyOffsetAlignment;

    if(usage & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) && alignment < cvm_vk_.properties.limits.minTexelBufferOffsetAlignment)
        alignment = cvm_vk_.properties.limits.minTexelBufferOffsetAlignment;

    if(usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT && alignment < cvm_vk_.properties.limits.minUniformBufferOffsetAlignment)
        alignment = cvm_vk_.properties.limits.minUniformBufferOffsetAlignment;

    if(usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT && alignment < cvm_vk_.properties.limits.minStorageBufferOffsetAlignment)
        alignment = cvm_vk_.properties.limits.minStorageBufferOffsetAlignment;

    if(alignment<cvm_vk_.properties.limits.nonCoherentAtomSize)
        alignment=cvm_vk_.properties.limits.nonCoherentAtomSize;

    assert((alignment & (alignment-1))==0);///alignments must all be a power of 2 according to the vulkan spec

//    printf("ALIGNMENT %u\n",alignment);

    return alignment;
}

VkDeviceSize cvm_vk_buffer_alignment_requirements(const cvm_vk_device * device, VkBufferUsageFlags usage)
{
    VkDeviceSize alignment=device->properties.limits.nonCoherentAtomSize;

    if(usage & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT))
    {
        alignment = CVM_MAX(alignment,device->properties.limits.optimalBufferCopyOffsetAlignment);
    }

    if(usage & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
    {
        alignment = CVM_MAX(alignment,device->properties.limits.minTexelBufferOffsetAlignment);
    }

    if(usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
    {
        alignment = CVM_MAX(alignment,device->properties.limits.minUniformBufferOffsetAlignment);
    }

    if(usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
    {
        alignment = CVM_MAX(alignment,device->properties.limits.minStorageBufferOffsetAlignment);
    }

    assert((alignment & (alignment-1))==0);///alignments must all be a power of 2 according to the vulkan spec

    return alignment;
}

VkResult cvm_vk_buffer_memory_pair_create(const cvm_vk_device * device, cvm_vk_buffer_memory_pair_setup * setup)
{
    VkMemoryRequirements memory_requirements;
    uint32_t memory_type_index;
    VkResult result;
    VkMemoryPropertyFlags required_properties, desired_properties;

    required_properties = setup->required_properties | (setup->map_memory ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : 0);
    desired_properties = required_properties | setup->desired_properties;

    /// set mapping fallback values
    setup->buffer = VK_NULL_HANDLE;
    setup->memory = VK_NULL_HANDLE;
    setup->mapping = NULL;
    setup->mapping_coherent = false;
    setup->acquired_properties = 0;



    VkBufferCreateInfo buffer_create_info=(VkBufferCreateInfo)
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = setup->buffer_size,
        .usage = setup->usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
    };

    result = vkCreateBuffer(device->device, &buffer_create_info, device->host_allocator, &setup->buffer);

    if(result == VK_SUCCESS)
    {
        vkGetBufferMemoryRequirements(device->device, setup->buffer, &memory_requirements);

        memory_type_index = sol_vk_find_appropriate_memory_type(device, memory_requirements.memoryTypeBits, desired_properties);

        if(memory_type_index == CVM_INVALID_U32_INDEX)
        {
            ///try again with only required properties
            memory_type_index = sol_vk_find_appropriate_memory_type(device, memory_requirements.memoryTypeBits, required_properties);
        }

        if(memory_type_index == CVM_INVALID_U32_INDEX)
        {
            //fprintf(stderr,"CVM VK ERROR - memory with required properties not found in buffer allocation\n");
            result = VK_RESULT_MAX_ENUM;
        }
        else
        {
            setup->acquired_properties = device->memory_properties.memoryTypes[memory_type_index].propertyFlags;
        }
    }

    if(result == VK_SUCCESS)
    {
        VkMemoryAllocateInfo memory_allocate_info=(VkMemoryAllocateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = NULL,
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = memory_type_index
        };

        result = vkAllocateMemory(device->device, &memory_allocate_info, device->host_allocator, &setup->memory);
    }

    if(result == VK_SUCCESS)
    {
        result = vkBindBufferMemory(device->device, setup->buffer, setup->memory, 0);
    }

    if(result == VK_SUCCESS && setup->map_memory)
    {
        result = vkMapMemory(device->device, setup->memory, 0, VK_WHOLE_SIZE, 0, &setup->mapping);

        if(result == VK_SUCCESS)
        {
            setup->mapping_coherent = (cvm_vk_.memory_properties.memoryTypes[memory_type_index].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }
    }

    /// failure handling
    if(result != VK_SUCCESS)
    {
        if(setup->buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device->device, setup->buffer, NULL);
            setup->buffer = VK_NULL_HANDLE;
        }
        if(setup->memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device->device, setup->memory, NULL);
            setup->memory = VK_NULL_HANDLE;
        }

        setup->mapping = NULL;
        setup->mapping_coherent = false;
        setup->acquired_properties = 0;
    }

    return result;
}

void cvm_vk_buffer_memory_pair_destroy(const cvm_vk_device * device, VkBuffer buffer, VkDeviceMemory memory, bool memory_was_mapped)
{
    if(memory_was_mapped)
    {
        vkUnmapMemory(device->device,memory);
    }

    vkDestroyBuffer(device->device,buffer,NULL);
    vkFreeMemory(device->device,memory,NULL);
}



bool cvm_vk_format_check_optimal_feature_support(VkFormat format,VkFormatFeatureFlags flags)
{
    VkFormatProperties prop;
    vkGetPhysicalDeviceFormatProperties(cvm_vk_.physical_device,format,&prop);
    return (prop.optimalTilingFeatures&flags)==flags;
}


































static void cvm_vk_create_module_sub_batch(cvm_vk_module_sub_batch * msb)
{
    VkCommandPoolCreateInfo command_pool_create_info=(VkCommandPoolCreateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext=NULL,
        .flags= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex=cvm_vk_.graphics_queue_family_index
    };

    CVM_VK_CHECK(vkCreateCommandPool(cvm_vk_.device,&command_pool_create_info,NULL,&msb->graphics_pool));

    msb->graphics_scbs=NULL;
    msb->graphics_scb_space=0;
    msb->graphics_scb_count=0;
}

static void cvm_vk_destroy_module_sub_batch(cvm_vk_module_sub_batch * msb)
{
    if(msb->graphics_scb_space)
    {
        vkFreeCommandBuffers(cvm_vk_.device,msb->graphics_pool,msb->graphics_scb_space,msb->graphics_scbs);
    }

    free(msb->graphics_scbs);

    vkDestroyCommandPool(cvm_vk_.device,msb->graphics_pool,NULL);
}

static void cvm_vk_create_module_batch(cvm_vk_module_batch * mb,uint32_t sub_batch_count)
{
    uint32_t i;
    mb->sub_batches=malloc(sizeof(cvm_vk_module_sub_batch)*sub_batch_count);

    ///required to go first as 0th serves at main threads pool
    for(i=0;i<sub_batch_count;i++)
    {
        cvm_vk_create_module_sub_batch(mb->sub_batches+i);
    }

    mb->graphics_pcbs=NULL;
    mb->graphics_pcb_space=0;
    mb->graphics_pcb_count=0;

    if(cvm_vk_.transfer_queue_family_index!=cvm_vk_.graphics_queue_family_index)
    {
        VkCommandPoolCreateInfo command_pool_create_info=(VkCommandPoolCreateInfo)
        {
            .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext=NULL,
            .flags= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex=cvm_vk_.transfer_queue_family_index,
        };

        CVM_VK_CHECK(vkCreateCommandPool(cvm_vk_.device,&command_pool_create_info,NULL,&mb->transfer_pool));
    }
    else
    {
        mb->transfer_pool=mb->sub_batches[0].graphics_pool;
    }

    VkCommandBufferAllocateInfo command_buffer_allocate_info=(VkCommandBufferAllocateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext=NULL,
        .commandPool=mb->transfer_pool,
        .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount=1
    };

    CVM_VK_CHECK(vkAllocateCommandBuffers(cvm_vk_.device,&command_buffer_allocate_info,&mb->transfer_cb));///only need 1
}

static void cvm_vk_destroy_module_batch(cvm_vk_module_batch * mb,uint32_t sub_batch_count)
{
    uint32_t i;
//    assert(mb->graphics_pcb_space);///module was destroyed without ever being used
    if(mb->graphics_pcb_space)
    {
        ///hijack first sub batch's command pool for primary command buffers
        vkFreeCommandBuffers(cvm_vk_.device,mb->sub_batches[0].graphics_pool,mb->graphics_pcb_space,mb->graphics_pcbs);
    }

    free(mb->graphics_pcbs);

    vkFreeCommandBuffers(cvm_vk_.device,mb->transfer_pool,1,&mb->transfer_cb);

    if(cvm_vk_.transfer_queue_family_index!=cvm_vk_.graphics_queue_family_index)
    {
        vkDestroyCommandPool(cvm_vk_.device,mb->transfer_pool,NULL);
    }

    for(i=0;i<sub_batch_count;i++)
    {
        cvm_vk_destroy_module_sub_batch(mb->sub_batches+i);
    }

    free(mb->sub_batches);
}

void cvm_vk_resize_module_graphics_data(cvm_vk_module_data * module_data)
{
    #warning REMOVE
}

void cvm_vk_create_module_data(cvm_vk_module_data * module_data,uint32_t sub_batch_count)
{
    uint32_t i;
    module_data->batch_count=0;
    module_data->batches=NULL;
    assert(sub_batch_count>0);///must have at least 1 for "primary" batch

    module_data->sub_batch_count=sub_batch_count;
    module_data->batch_index=0;

    for(i=0;i<CVM_VK_MAX_QUEUES;i++)
    {
        cvm_atomic_lock_create(&module_data->transfer_data[i].spinlock);
        cvm_vk_buffer_barrier_stack_initialise(&module_data->transfer_data[i].acquire_barriers);
        module_data->transfer_data[i].transfer_semaphore_wait_value=0;
        module_data->transfer_data[i].wait_stages=VK_PIPELINE_STAGE_2_NONE;
        module_data->transfer_data[i].associated_queue_family_index = (i==CVM_VK_GRAPHICS_QUEUE_INDEX)?cvm_vk_.graphics_queue_family_index:cvm_vk_.graphics_queue_family_index;
        #warning NEED TO SET ABOVE COMPUTE VARIANT APPROPRIATELY! USING cvm_vk_graphics_queue_family WILL NOT DO!
    }
}

void cvm_vk_destroy_module_data(cvm_vk_module_data * module_data)
{
    uint32_t i;

    for(i=0;i<module_data->batch_count;i++)
    {
        cvm_vk_destroy_module_batch(module_data->batches+i,module_data->sub_batch_count);
    }

    free(module_data->batches);

    for(i=0;i<CVM_VK_MAX_QUEUES;i++)
    {
        cvm_vk_buffer_barrier_stack_terminate(&module_data->transfer_data[i].acquire_barriers);
    }
}

cvm_vk_module_batch * cvm_vk_get_module_batch(cvm_vk_module_data * module_data,uint32_t * swapchain_image_index)
{
    #warning REMOVE
}

void cvm_vk_end_module_batch(cvm_vk_module_batch * batch)
{
    #warning REMOVE
}

VkCommandBuffer cvm_vk_access_batch_transfer_command_buffer(cvm_vk_module_batch * batch,uint32_t affected_queue_bitbask)
{
    if(!batch->has_begun_transfer)
    {
        batch->has_begun_transfer=true;

        VkCommandBufferBeginInfo command_buffer_begin_info=(VkCommandBufferBeginInfo)
        {
            .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext=NULL,
            .flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,/// VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT necessary?
            .pInheritanceInfo=NULL
        };

        CVM_VK_CHECK(vkBeginCommandBuffer(batch->transfer_cb,&command_buffer_begin_info));
    }

    assert(!batch->has_ended_transfer);

    batch->transfer_affceted_queues_bitmask|=affected_queue_bitbask;

    return batch->transfer_cb;
}

void cvm_vk_setup_new_graphics_payload_from_batch(cvm_vk_module_work_payload * payload,cvm_vk_module_batch * batch)
{
    static const uint32_t pcb_allocation_count=4;
    uint32_t cb_index,queue_index;
    queue_transfer_synchronization_data * transfer_data;

    queue_index=CVM_VK_GRAPHICS_QUEUE_INDEX;

    cb_index=batch->graphics_pcb_count++;

    if(cb_index==batch->graphics_pcb_space)///out of command buffers (or not yet init)
    {
        batch->graphics_pcbs=realloc(batch->graphics_pcbs,sizeof(VkCommandBuffer)*(batch->graphics_pcb_space+pcb_allocation_count));

        VkCommandBufferAllocateInfo command_buffer_allocate_info=(VkCommandBufferAllocateInfo)
        {
            .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext=NULL,
            .commandPool=batch->sub_batches[0].graphics_pool,
            .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount=pcb_allocation_count
        };

        CVM_VK_CHECK(vkAllocateCommandBuffers(cvm_vk_.device,&command_buffer_allocate_info,batch->graphics_pcbs+batch->graphics_pcb_space));

        batch->graphics_pcb_space+=pcb_allocation_count;
    }

    transfer_data=batch->transfer_data+queue_index;

    payload->command_buffer=batch->graphics_pcbs[cb_index];
    payload->wait_count=0;
    payload->destination_queue=queue_index;
    payload->transfer_data=transfer_data;

    payload->signal_stages=VK_PIPELINE_STAGE_2_NONE;

    VkCommandBufferBeginInfo command_buffer_begin_info=(VkCommandBufferBeginInfo)
    {
        .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext=NULL,
        .flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,/// VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT necessary?
        .pInheritanceInfo=NULL
    };

    CVM_VK_CHECK(vkBeginCommandBuffer(payload->command_buffer,&command_buffer_begin_info));

    #warning probably want to enforce that acquisition order is the same as submission order, is easient way to ensure following condition is met
    if(cb_index==0) ///if first (for this queue/queue-family) then inject barriers (must be first SUBMITTED not just first acquired)
    {
        assert( !(batch->queue_submissions_this_batch&1u<<queue_index));

        ///shouldnt need spinlock as this should be appropriately threaded
        if(transfer_data->acquire_barriers.count)///should always be zero if no QFOT is required, (see assert below)
        {
            assert(cvm_vk_.graphics_queue_family_index!=cvm_vk_.transfer_queue_family_index);

            ///add resource acquisition side of QFOT
            VkDependencyInfo copy_aquire_dependencies=
            {
                .sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext=NULL,
                .dependencyFlags=0,
                .memoryBarrierCount=0,
                .pMemoryBarriers=NULL,
                .bufferMemoryBarrierCount=transfer_data->acquire_barriers.count,
                .pBufferMemoryBarriers=transfer_data->acquire_barriers.data,
                .imageMemoryBarrierCount=0,
                .pImageMemoryBarriers=NULL
            };
            vkCmdPipelineBarrier2(payload->command_buffer,&copy_aquire_dependencies);

            #warning assert queue families are actually different! before performing barriers (perhaps if instead of asserting, not sure yet)

            //assert(transfer_data->associated_queue_family_index==cvm_vk_transfer_queue_family);

            ///add transfer waits as appropriate
            #warning make wait addition a function
//            payload->waits[0].value=transfer_data->transfer_semaphore_wait_value;
//            payload->waits[0].semaphore=cvm_vk_transfer_timeline.semaphore;
//            payload->wait_stages[0]=transfer_data->wait_stages;
//            payload->wait_count++;

            ///rest data now that we're done with it
            transfer_data->wait_stages=VK_PIPELINE_STAGE_2_NONE;

            cvm_vk_buffer_barrier_stack_reset(&transfer_data->acquire_barriers);
        }
    }
    else
    {
        assert(batch->queue_submissions_this_batch&1u<<queue_index);
    }
}

uint32_t cvm_vk_get_transfer_queue_family(void)
{
    return cvm_vk_.transfer_queue_family_index;
}

uint32_t cvm_vk_get_graphics_queue_family(void)
{
    return cvm_vk_.graphics_queue_family_index;
}
