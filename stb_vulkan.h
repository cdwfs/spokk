/* stb_vulkan - v0.01 - public domain Vulkan helper
                                     no warranty implied; use at your own risk

   Do this:
      #define STB_VULKAN_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.

   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define STB_VULKAN_IMPLEMENTATION
   #include "stb_vulkan.h"

   You can #define STBVK_ASSERT(x) before the #include to avoid using assert.h.
   And #define STBVK_MALLOC, STBVK_REALLOC, and STBVK_FREE to avoid using malloc,realloc,free



LICENSE

This software is in the public domain. Where that dedication is not
recognized, you are granted a perpetual, irrevocable license to copy,
distribute, and modify this file as you see fit.

*/

#ifndef STBVK_INCLUDE_STB_VULKAN_H
#define STBVK_INCLUDE_STB_VULKAN_H

#include <vulkan/vulkan.h>

#ifndef STBVK_NO_STDIO
#   include <stdio.h>
#endif // STBVK_NO_STDIO

#define STBVK_VERSION 1

typedef unsigned char stbvk_uc;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef STB_VULKAN_STATIC
#   define STBVKDEF static
#else
#   define STBVKDEF extern
#endif

//////////////////////////////////////////////////////////////////////////////
//
// PUBLIC API
//

    typedef struct
    {
        VkAllocationCallbacks *allocation_callbacks;

        VkInstance instance;
        VkDebugReportCallbackEXT debug_report_callback;

        VkPhysicalDevice physical_device;
        VkPhysicalDeviceProperties physical_device_properties;
        VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
        VkPhysicalDeviceFeatures physical_device_features;
        uint32_t queue_family_index;
        VkQueueFamilyProperties queue_family_properties;
        VkDevice device;
        VkQueue *queues;

        VkCommandPool command_pool;
    } stbvk_context;


    typedef struct
    {
        VkAllocationCallbacks *allocation_callbacks;
        VkBool32 enable_standard_validation_layers;
        const VkApplicationInfo *application_info; // Used to initialize VkInstance. Optional; set to NULL for default values.
        PFN_vkDebugReportCallbackEXT debug_report_callback; // Optional; set to NULL to disable debug reports.
        void *debug_report_callback_user_data; // Optional; passed to debug_report_callback, if enabled.
    } stbvk_context_create_info;
    STBVKDEF VkResult stbvk_init_context(stbvk_context_create_info const *createInfo, stbvk_context *c);
    STBVKDEF void stbvk_destroy_context(stbvk_context *c);

    typedef struct
    {
       int      (*read)  (void *user,char *data,int size);   // fill 'data' with 'size' bytes.  return number of bytes actually read
       void     (*skip)  (void *user,int n);                 // skip the next 'n' bytes, or 'unget' the last -n bytes if negative
       int      (*eof)   (void *user);                       // returns nonzero if we are at end of file/data
    } stbvk_io_callbacks;

    STBVKDEF VkShaderModule stbvk_load_shader_from_memory(stbvk_context *c, stbvk_uc const *buffer, int len);
    STBVKDEF VkShaderModule stbvk_load_shader_from_callbacks(stbvk_context *c, stbvk_io_callbacks const *clbk, void *user);
#ifndef STBVK_NO_STDIO
    STBVKDEF VkShaderModule stbvk_load_shader(stbvk_context *c, char const *filename);
    STBVKDEF VkShaderModule stbvk_load_shader_from_file(stbvk_context *c, FILE *f, int len);
#endif

    STBVKDEF void stbvk_set_image_layout(VkCommandBuffer cmd_buf, VkImage image,
        VkImageSubresourceRange subresource_range, VkImageLayout old_layout, VkImageLayout new_layout,
        VkAccessFlags src_access_mask);

#ifdef __cplusplus
}
#endif

//
//
////   end header file   /////////////////////////////////////////////////////
#endif // STBVK_INCLUDE_STB_VULKAN_H

#ifdef STB_VULKAN_IMPLEMENTATION

#ifndef STBVK_NO_STDIO
#   include <stdio.h>
#endif

#ifndef STBVK_ASSERT
#   include <assert.h>
#   define STBVK_ASSERT(x) assert(x)
#endif

#ifndef _MSC_VER
#   ifdef __cplusplus
#       define stbvk_inline inline
#   else
#       define stbvk_inline
#   endif
#else
#   define stbvk_inline __forceinline
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1700)
typedef UINT16 stbvk__uint16;
typedef  INT16 stbvk__int16;
typedef UINT32 stbvk__uint32;
typedef  INT32 stbvk__int32;
#else
#include <stdint.h>
typedef uint16_t stbvk__uint16;
typedef int16_t  stbvk__int16;
typedef uint32_t stbvk__uint32;
typedef int32_t  stbvk__int32;
#endif
// should produce compiler error if size is wrong
typedef unsigned char validate_uint32[sizeof(stbvk__uint32)==4 ? 1 : -1];

#ifdef _MSC_VER
#   define STBVK_NOTUSED(v)  (void)(v)
#else
#   define STBVK_NOTUSED(v)  (void)sizeof(v)
#endif

#if defined(STBVK_MALLOC) && defined(STBVK_FREE) && (defined(STBVK_REALLOC) || defined(STBVK_REALLOC_SIZED))
// ok
#elif !defined(STBVK_MALLOC) && !defined(STBVK_FREE) && !defined(STBVK_REALLOC) && !defined(STBVK_REALLOC_SIZED)
// ok
#else
#   error "Must define all or none of STBVK_MALLOC, STBVK_FREE, and STBVK_REALLOC (or STBVK_REALLOC_SIZED)."
#endif

#ifndef STBVK_MALLOC
#   define STBVK_MALLOC(sz)           malloc(sz)
#   define STBVK_REALLOC(p,newsz)     realloc((p),(newsz))
#   define STBVK_FREE(p)              free( (void*)(p) )
#endif

#ifndef STBVK_REALLOC_SIZED
#   define STBVK_REALLOC_SIZED(p,oldsz,newsz) STBVK_REALLOC(p,newsz)
#endif

// x86/x64 detection
#if defined(__x86_64__) || defined(_M_X64)
#   define STBVK__X64_TARGET
#elif defined(__i386) || defined(_M_IX86)
#   define STBVK__X86_TARGET
#endif

// TODO: proper return-value test
#if defined(_MSC_VER)
#   define STBVK__RETVAL_CHECK(expected, expr) \
    do {  \
        int err = (expr);                             \
        if (err != (expected)) {                                            \
            printf("%s(%d): error in %s() -- %s returned %d\n", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
            __debugbreak();                                                   \
        }                                                                   \
        assert(err == (expected));                                          \
        __pragma(warning(push))                                             \
        __pragma(warning(disable:4127))                                 \
        } while(0)                                                      \
    __pragma(warning(pop))
#else
#   define STBVK__RETVAL_CHECK(expected, expr) \
    do {  \
        int err = (expr);                                                   \
        if (err != (expected)) {                                            \
            printf("%s(%d): error in %s() -- %s returned %d\n", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
            __asm__("int $3"); /*__debugbreak();*/                 \
        }                                                                   \
        assert(err == (expected));                                          \
    } while(0)
#endif
#define STBVK__CHECK(expr) STBVK__RETVAL_CHECK(VK_SUCCESS, expr)

static VkResult stbvk__init_instance(stbvk_context_create_info const *create_info, stbvk_context *context)
{
    const char *extension_layer_name = NULL;
    uint32_t extension_count = 0;
    STBVK__CHECK( vkEnumerateInstanceExtensionProperties(extension_layer_name, &extension_count, NULL) );
    VkExtensionProperties *extension_properties = (VkExtensionProperties*)malloc(extension_count * sizeof(VkExtensionProperties));
    STBVK__CHECK( vkEnumerateInstanceExtensionProperties(extension_layer_name, &extension_count, extension_properties) );
    const char **extension_names = (const char**)STBVK_MALLOC(extension_count * sizeof(const char**));
    for(uint32_t iExt=0; iExt<extension_count; iExt+=1)
    {
        extension_names[iExt] = extension_properties[iExt].extensionName;
    }

#if 0 // Enable to query the instance's supported layers
    {
        uint32_t instance_layer_count = 0;
        STBVK__CHECK( vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL) );
        VkLayerProperties *instance_layer_properties = (VkLayerProperties*)STBVK_MALLOC(instance_layer_count * sizeof(VkLayerProperties));
        STBVK__CHECK( vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layer_properties) );
        STBVK_FREE(instance_layer_properties);
    }
#endif

    uint32_t requested_layer_count = 0;
    const char **requested_layer_names = NULL;
    const char *standard_validation_layer = "VK_LAYER_LUNARG_standard_validation";
    if (create_info->enable_standard_validation_layers)
    {
        requested_layer_names = &standard_validation_layer;
        requested_layer_count += 1;
    }

    VkApplicationInfo application_info_default = {};
    application_info_default.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info_default.pNext = NULL;
    application_info_default.pApplicationName = "Default Application Name";
    application_info_default.applicationVersion = 0x1000;
    application_info_default.pEngineName = "Default Engine Name";
    application_info_default.engineVersion = 0x1000;
    application_info_default.apiVersion = VK_MAKE_VERSION(1,0,0);

    VkInstanceCreateInfo instance_create_info = {};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pNext = NULL;
    instance_create_info.flags = 0;
    instance_create_info.pApplicationInfo = create_info->application_info ? create_info->application_info : &application_info_default;
    instance_create_info.enabledLayerCount = requested_layer_count;
    instance_create_info.ppEnabledLayerNames = requested_layer_names;
    instance_create_info.enabledExtensionCount = extension_count;
    instance_create_info.ppEnabledExtensionNames = extension_names;

    STBVK__CHECK( vkCreateInstance(&instance_create_info, create_info->allocation_callbacks, &context->instance) );
    STBVK_FREE(extension_names);

    // Set up debug report callback
    if (create_info->debug_report_callback)
    {
        PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback =
            (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(context->instance, "vkCreateDebugReportCallbackEXT");
        VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo = {};
        debugReportCallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        debugReportCallbackCreateInfo.pNext = NULL;
        debugReportCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        debugReportCallbackCreateInfo.pfnCallback = create_info->debug_report_callback;
        debugReportCallbackCreateInfo.pUserData = create_info->debug_report_callback_user_data;
        context->debug_report_callback = VK_NULL_HANDLE;
        STBVK__CHECK( CreateDebugReportCallback(context->instance, &debugReportCallbackCreateInfo, context->allocation_callbacks, &context->debug_report_callback) );
    }

    return VK_SUCCESS;
}

static VkResult stbvk__init_device(stbvk_context_create_info const *create_info, stbvk_context *context)
{
    uint32_t physical_device_count = 0;
    STBVK__CHECK( vkEnumeratePhysicalDevices(context->instance, &physical_device_count, NULL) );
    STBVK_ASSERT(physical_device_count > 0);
    VkPhysicalDevice *physical_devices = (VkPhysicalDevice*)STBVK_MALLOC(physical_device_count * sizeof(VkPhysicalDevice));
    STBVK__CHECK( vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices) );
    context->physical_device = physical_devices[0];
    STBVK_FREE(physical_devices);

    vkGetPhysicalDeviceProperties(context->physical_device, &context->physical_device_properties);
#if 0
    printf("Physical device #%u: '%s', API version %u.%u.%u\n",
        0,
        context->physical_device_properties.deviceName,
        VK_VERSION_MAJOR(context->physical_device_properties.apiVersion),
        VK_VERSION_MINOR(context->physical_device_properties.apiVersion),
        VK_VERSION_PATCH(context->physical_device_properties.apiVersion));
#endif

    vkGetPhysicalDeviceMemoryProperties(context->physical_device, &context->physical_device_memory_properties);

    vkGetPhysicalDeviceFeatures(context->physical_device, &context->physical_device_features);

#if 0 // Enable to query the device's supported layers
    {
        uint32_t device_layer_count = 0;
        STBVK__CHECK( vkEnumerateDeviceLayerProperties(context->physical_device, &device_layer_count, NULL) );
        VkLayerProperties *device_layer_properties = (VkLayerProperties*)STBVK_MALLOC(device_layer_count * sizeof(VkLayerProperties));
        STBVK__CHECK( vkEnumerateDeviceLayerProperties(context->physical_device, &device_layer_count, device_layer_properties) );
        STBVK_FREE(device_layer_properties);
    }
#endif

    uint32_t requested_layer_count = 0;
    const char **requested_layer_names = NULL;
    const char *standard_validation_layer = "VK_LAYER_LUNARG_standard_validation";
    if (create_info->enable_standard_validation_layers)
    {
        requested_layer_names = &standard_validation_layer;
        requested_layer_count += 1;
    }

    uint32_t extension_count = 0;
    const char *extension_layer_name = NULL;
    STBVK__CHECK( vkEnumerateDeviceExtensionProperties(context->physical_device, extension_layer_name, &extension_count, NULL) );
    VkExtensionProperties *extension_properties = (VkExtensionProperties*)STBVK_MALLOC(extension_count*sizeof(VkExtensionProperties));
    STBVK__CHECK( vkEnumerateDeviceExtensionProperties(context->physical_device, extension_layer_name, &extension_count, extension_properties) );
    const char **extension_names = (const char**)STBVK_MALLOC(extension_count * sizeof(const char**));
    for(uint32_t iExt=0; iExt<extension_count; iExt+=1)
    {
        extension_names[iExt] = extension_properties[iExt].extensionName;
    }

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context->physical_device, &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_family_properties_all = (VkQueueFamilyProperties*)STBVK_MALLOC(queue_family_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(context->physical_device, &queue_family_count, queue_family_properties_all);
    VkDeviceQueueCreateInfo device_queue_create_info = {};
    VkBool32 found_graphics_queue_family = VK_FALSE;
    for(uint32_t iQF=0; iQF<queue_family_count; iQF+=1) {
        if ( (queue_family_properties_all[iQF].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            continue;
        float *queue_priorities = (float*)STBVK_MALLOC(queue_family_properties_all[iQF].queueCount * sizeof(float));
        for(uint32_t iQ=0; iQ<queue_family_properties_all[iQF].queueCount; ++iQ) {
            queue_priorities[iQ] = 1.0f;
        }
        device_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        device_queue_create_info.pNext = NULL;
        device_queue_create_info.flags = 0;
        device_queue_create_info.queueFamilyIndex = iQF;
        device_queue_create_info.queueCount = queue_family_properties_all[iQF].queueCount;
        device_queue_create_info.pQueuePriorities = queue_priorities;

        context->queue_family_index = iQF;
        context->queue_family_properties = queue_family_properties_all[iQF];
        found_graphics_queue_family = VK_TRUE;
        break;
    }
    STBVK_ASSERT(found_graphics_queue_family);
    STBVK_FREE(queue_family_properties_all);

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = NULL;
    device_create_info.flags = 0;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &device_queue_create_info;
    device_create_info.enabledLayerCount = requested_layer_count;
    device_create_info.ppEnabledLayerNames = requested_layer_names;
    device_create_info.enabledExtensionCount = extension_count;
    device_create_info.ppEnabledExtensionNames = extension_names;
    device_create_info.pEnabledFeatures = &context->physical_device_features;
    STBVK__CHECK( vkCreateDevice(context->physical_device, &device_create_info, context->allocation_callbacks, &context->device) );
    STBVK_FREE(extension_names);
    STBVK_FREE(extension_properties);
    STBVK_FREE(device_create_info.pQueueCreateInfos->pQueuePriorities);
#if 0
    printf("Created Vulkan logical device with extensions:\n");
    for(uint32_t iExt=0; iExt<device_create_info.enabledExtensionCount; iExt+=1) {
        printf("- %s\n", device_create_info.ppEnabledExtensionNames[iExt]);
    }
    printf("and device layers:\n");
    for(uint32_t iLayer=0; iLayer<device_create_info.enabledLayerCount; iLayer+=1) {
        printf("- %s\n", device_create_info.ppEnabledLayerNames[iLayer]);
    }
#endif

    context->queues = (VkQueue*)STBVK_MALLOC(context->queue_family_properties.queueCount * sizeof(VkQueue));
    for(uint32_t iQ=0; iQ<context->queue_family_properties.queueCount; iQ+=1) {
        vkGetDeviceQueue(context->device, context->queue_family_index, iQ, &context->queues[iQ]);
    }

    return VK_SUCCESS;
}

static VkResult stbvk__init_command_pool(stbvk_context_create_info const * /*createInfo*/, stbvk_context *context)
{
    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.pNext = NULL;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allows reseting individual command buffers from this pool
    command_pool_create_info.queueFamilyIndex = context->queue_family_index;
    STBVK__CHECK( vkCreateCommandPool(context->device, &command_pool_create_info, context->allocation_callbacks, &context->command_pool) );
    return VK_SUCCESS;
}

STBVKDEF VkResult stbvk_init_context(stbvk_context_create_info const *createInfo, stbvk_context *c)
{
    VkResult result = VK_SUCCESS;

    c->allocation_callbacks = createInfo->allocation_callbacks;

    result = stbvk__init_instance(createInfo, c);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    stbvk__init_device(createInfo, c);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    stbvk__init_command_pool(createInfo, c);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    return VK_SUCCESS;
}

STBVKDEF void stbvk_destroy_context(stbvk_context *context)
{
    STBVK_FREE(context->queues);
    context->queues = NULL;

    vkDestroyCommandPool(context->device, context->command_pool, context->allocation_callbacks);
    context->command_pool = NULL;

    vkDestroyDevice(context->device, context->allocation_callbacks);
    context->device = VK_NULL_HANDLE;
    if (context->debug_report_callback != VK_NULL_HANDLE)
    {
        PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback =
            (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(context->instance, "vkDestroyDebugReportCallbackEXT");
        DestroyDebugReportCallback(context->instance, context->debug_report_callback, context->allocation_callbacks);
    }


    vkDestroyInstance(context->instance, context->allocation_callbacks);
    context->instance = VK_NULL_HANDLE;

    context->allocation_callbacks = NULL;
}


#ifndef STBVK_NO_STDIO
static FILE *stbvk__fopen(char const *filename, char const *mode)
{
   FILE *f;
#if defined(_MSC_VER) && _MSC_VER >= 1400
   if (0 != fopen_s(&f, filename, mode))
      f=0;
#else
   f = fopen(filename, mode);
#endif
   return f;
}

STBVKDEF VkShaderModule stbvk_load_shader_from_file(stbvk_context *c, FILE *f, int len)
{
    void *shader_bin = STBVK_MALLOC(len);
    size_t bytes_read = fread(shader_bin, 1, len, f);
    if (bytes_read != len)
    {
        free(shader_bin);
        return VK_NULL_HANDLE;
    }
    VkShaderModule shader_module = stbvk_load_shader_from_memory(c, (const stbvk_uc*)shader_bin, len);
    STBVK_FREE(shader_bin);
    return shader_module;
}

STBVKDEF VkShaderModule stbvk_load_shader(stbvk_context *c, char const *filename)
{
    FILE *spv_file = stbvk__fopen(filename, "rb");
    if (!spv_file)
    {
        return VK_NULL_HANDLE;
    }
    fseek(spv_file, 0, SEEK_END);
    long spv_file_size = ftell(spv_file);
    fseek(spv_file, 0, SEEK_SET);
    VkShaderModule shader_module = stbvk_load_shader_from_file(c, spv_file, spv_file_size);
    fclose(spv_file);
    return shader_module;
}

#endif

STBVKDEF VkShaderModule stbvk_load_shader_from_memory(stbvk_context *c, stbvk_uc const *buffer, int len)
{
    VkShaderModuleCreateInfo smci = {};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.pNext = NULL;
    smci.flags = 0;
    smci.codeSize = len;
    smci.pCode = (uint32_t*)buffer;
    VkShaderModule shader_module = VK_NULL_HANDLE;
    STBVK__CHECK( vkCreateShaderModule(c->device, &smci, c->allocation_callbacks, &shader_module) );
    
    return shader_module;
}
STBVKDEF VkShaderModule stbvk_load_shader_from_callbacks(stbvk_context * /*c*/, stbvk_io_callbacks const * /*clbk*/, void * /*user*/)
{
    return VK_NULL_HANDLE;
}


STBVKDEF void stbvk_set_image_layout(VkCommandBuffer cmd_buf, VkImage image,
        VkImageSubresourceRange subresource_range, VkImageLayout old_layout, VkImageLayout new_layout,
        VkAccessFlags src_access_mask)
{
    VkImageMemoryBarrier img_memory_barrier = {};
    img_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_memory_barrier.pNext = NULL;
    img_memory_barrier.srcAccessMask = src_access_mask;
    img_memory_barrier.dstAccessMask = 0; // overwritten below
    img_memory_barrier.oldLayout = old_layout;
    img_memory_barrier.newLayout = new_layout;
    img_memory_barrier.image = image;
    img_memory_barrier.subresourceRange = subresource_range;

    switch(old_layout)
    {
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        img_memory_barrier.srcAccessMask |= VK_ACCESS_HOST_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        img_memory_barrier.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        img_memory_barrier.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    }

    switch(new_layout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        img_memory_barrier.dstAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        img_memory_barrier.dstAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        img_memory_barrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        img_memory_barrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        img_memory_barrier.srcAccessMask |= VK_ACCESS_HOST_WRITE_BIT;
        img_memory_barrier.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
        // Make sure any Copy or CPU writes to image are flushed
        img_memory_barrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
        img_memory_barrier.dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        break;
    }

    VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    // TODO(cort): 
    VkDependencyFlags dependency_flags = 0;
    uint32_t memory_barrier_count = 0;
    const VkMemoryBarrier *memory_barriers = NULL;
    uint32_t buffer_memory_barrier_count = 0;
    const VkBufferMemoryBarrier *buffer_memory_barriers = NULL;
    uint32_t image_memory_barrier_count = 1;
    vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, dependency_flags,
        memory_barrier_count, memory_barriers,
        buffer_memory_barrier_count, buffer_memory_barriers,
        image_memory_barrier_count, &img_memory_barrier);
}

#endif // STB_VULKAN_IMPLEMENTATION
