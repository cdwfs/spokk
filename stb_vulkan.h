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

        VkSurfaceKHR present_surface;
        uint32_t graphics_queue_family_index;
        uint32_t present_queue_family_index;
        VkQueueFamilyProperties graphics_queue_family_properties;
        VkQueueFamilyProperties present_queue_family_properties;

        VkDevice device;
        VkQueue graphics_queue;
        VkQueue present_queue;

        VkCommandPool command_pool;
        VkCommandBuffer command_buffer_primary;

        VkSwapchainKHR swapchain;
        uint32_t swapchain_image_count;
        uint32_t swapchain_image_index;
        VkSurfaceFormatKHR swapchain_surface_format;
        VkImage *swapchain_images;
        VkImageView *swapchain_image_views;

        VkPhysicalDevice *all_physical_devices;
    } stbvk_context;


    typedef struct
    {
        VkAllocationCallbacks *allocation_callbacks;
        
        const char **required_instance_layer_names;
        uint32_t required_instance_layer_count;
        const char **required_instance_extension_names;
        uint32_t required_instance_extension_count;
        const char **required_device_extension_names;
        uint32_t required_device_extension_count;
 
        const VkApplicationInfo *application_info; // Used to initialize VkInstance. Optional; set to NULL for default values.
        PFN_vkDebugReportCallbackEXT debug_report_callback; // Optional; set to NULL to disable debug reports.
        void *debug_report_callback_user_data; // Optional; passed to debug_report_callback, if enabled.
    } stbvk_context_create_info;
    STBVKDEF VkResult stbvk_init_instance(stbvk_context_create_info const *create_info, stbvk_context *c);
    STBVKDEF VkResult stbvk_init_device(stbvk_context_create_info const *create_info, VkSurfaceKHR present_surface, stbvk_context *c);
    STBVKDEF VkResult stbvk_init_command_pool(stbvk_context_create_info const *create_info, stbvk_context *c);
    STBVKDEF VkResult stbvk_init_swapchain(stbvk_context_create_info const *create_info, stbvk_context *c, VkSwapchainKHR old_swapchain);
    STBVKDEF void stbvk_destroy_context(stbvk_context *c);

    typedef struct
    {
        VkImageType image_type;
        VkFormat format;
        VkExtent3D extent;
        uint32_t mip_levels;
        uint32_t array_layers;
        VkSampleCountFlagBits samples;
        VkImageTiling tiling;
        VkImageUsageFlags usage;
        VkImageLayout final_layout; // not including the staging process
        VkImageViewType view_type;
    } stbvk_image_create_info;
    typedef struct
    {
        stbvk_image_create_info create_info;
        VkImageCreateInfo image_create_info;
        VkImageViewCreateInfo image_view_create_info;
        VkImage image;
        VkDeviceMemory device_memory;
        VkMemoryRequirements memory_requirements;
        VkImageView image_view; // Default view based on create_info; users can create additional views at their leisure.
    } stbvk_image;
    STBVKDEF VkResult stbvk_create_image(stbvk_context const *context, stbvk_image_create_info const *create_info, stbvk_image *out_image);
    STBVKDEF VkResult stbvk_get_image_subresource_source_layout(stbvk_context const *context, stbvk_image const *image,
        VkImageSubresource subresource, VkSubresourceLayout *out_layout);
    STBVKDEF VkResult stbvk_load_image_subresource(stbvk_context const *context, stbvk_image const *image,
        VkImageSubresource subresource, VkSubresourceLayout subresource_layout, void const *pixels);
    STBVKDEF void stbvk_destroy_image(stbvk_context const *context, stbvk_image *image);

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

#if defined(STB_VULKAN_IMPLEMENTATION)

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
#   include <stdlib.h>
#   define STBVK_MALLOC(sz)           malloc(sz)
#   define STBVK_REALLOC(p,newsz)     realloc((p),(newsz))
#   define STBVK_FREE(p)              free( (void*)(p) )
#endif

#if !defined(STBVK_LOG)
# if !defined(STBVK_NO_STDIO)
#   include <stdarg.h>
static void stbvk__log_default(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}
#   define STBVK_LOG(...) stbvk__log_default(__VA_ARGS__)
# else
#   define STBVK_LOG(...) (void)(__VA_ARGS)__)
# endif
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
            STBVK_LOG("%s(%d): error in %s() -- %s returned %d", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
            __debugbreak();                                                   \
        }                                                                   \
        assert(err == (expected));                                          \
        __pragma(warning(push))                                             \
        __pragma(warning(disable:4127))                                 \
        } while(0)                                                      \
    __pragma(warning(pop))
#elif defined(__ANDROID__)
#   define STBVK__RETVAL_CHECK(expected, expr) \
    do {  \
        int err = (expr);                                                   \
        if (err != (expected)) {                                            \
            STBVK_LOG("%s(%d): error in %s() -- %s returned %d", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
            /*__asm__("int $3"); */                 \
        }                                                                   \
        assert(err == (expected));                                          \
    } while(0)
#else
#   define STBVK__RETVAL_CHECK(expected, expr) \
    do {  \
        int err = (expr);                                                   \
        if (err != (expected)) {                                            \
            STBVK_LOG("%s(%d): error in %s() -- %s returned %d", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
            /*__asm__("int $3"); */                 \
        }                                                                   \
        assert(err == (expected));                                          \
    } while(0)
#endif
#define STBVK__CHECK(expr) STBVK__RETVAL_CHECK(VK_SUCCESS, expr)

#define STBVK__CLAMP(x, xmin, xmax) ( ((x)<(xmin)) ? (xmin) : ( ((x)>(xmax)) ? (xmax) : (x) ) )

template<typename T>
const T& stbvk__min(const T& a, const T& b) { return (a<b) ? a : b; }
template<typename T>
const T& stbvk__max(const T& a, const T& b) { return (a>b) ? a : b; }

#include <algorithm>
#include <string>
#include <vector>

STBVKDEF VkResult stbvk_init_instance(stbvk_context_create_info const *create_info, stbvk_context *context)
{
    uint32_t instance_extension_count = 0;
    STBVK__CHECK( vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr) );
    std::vector<VkExtensionProperties> instance_extension_properties(instance_extension_count);
    STBVK__CHECK( vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extension_properties.data()) );
    std::vector<std::string> all_instance_extension_names(instance_extension_count);
    for(uint32_t iExt=0; iExt<instance_extension_count; iExt+=1)
    {
        all_instance_extension_names[iExt] = std::string(instance_extension_properties[iExt].extensionName);
    }

    uint32_t instance_layer_count = 0;
    STBVK__CHECK( vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr) );
    std::vector<VkLayerProperties> instance_layer_properties(instance_layer_count);
    STBVK__CHECK( vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layer_properties.data()) );
    std::vector<std::string> all_instance_layer_names(instance_layer_count);
    for(uint32_t iLayer=0; iLayer<instance_layer_count; iLayer+=1)
    {
        all_instance_layer_names[iLayer] = std::string(instance_layer_properties[iLayer].layerName);

        uint32_t layer_extension_count = 0;
        STBVK__CHECK( vkEnumerateInstanceExtensionProperties(instance_layer_properties[iLayer].layerName,
            &layer_extension_count, nullptr) );
        std::vector<VkExtensionProperties> layer_extension_properties(layer_extension_count);
        STBVK__CHECK( vkEnumerateInstanceExtensionProperties(instance_layer_properties[iLayer].layerName,
            &layer_extension_count, layer_extension_properties.data()) );
        for(const auto &ext_props : layer_extension_properties)
        {
            if (std::find(all_instance_extension_names.cbegin(), all_instance_extension_names.cend(), ext_props.extensionName)
                == all_instance_extension_names.cend())
            {
                all_instance_extension_names.push_back( std::string(ext_props.extensionName) );
            }
        }
    }

    // TODO(cort): extension/layer filtering still needs major work. Required vs. requested? querying what was actually available?
    std::vector<const char*> extension_names_c;
    extension_names_c.reserve(all_instance_extension_names.size());
    bool found_debug_report_extension = false;
    for(uint32_t iExt = 0; iExt<create_info->required_instance_extension_count; iExt += 1)
    {
        if (std::find(all_instance_extension_names.cbegin(), all_instance_extension_names.cend(),
            create_info->required_instance_extension_names[iExt])
            == all_instance_extension_names.cend())
        {
            break;
        }
        if (std::string(create_info->required_instance_extension_names[iExt]) == std::string(VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
            found_debug_report_extension = true;

        extension_names_c.push_back( create_info->required_instance_extension_names[iExt] );
    }
    std::vector<const char*> layer_names_c;
    layer_names_c.reserve(all_instance_layer_names.size());
    for(uint32_t iLayer = 0; iLayer<create_info->required_instance_layer_count; iLayer += 1)
    {
        if (std::find(all_instance_layer_names.begin(), all_instance_layer_names.end(),
            create_info->required_instance_layer_names[iLayer])
            == all_instance_layer_names.end())
        {
            break;
        }
        layer_names_c.push_back( create_info->required_instance_layer_names[iLayer] );
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
    instance_create_info.enabledLayerCount       = uint32_t(layer_names_c.size());
    instance_create_info.ppEnabledLayerNames     = layer_names_c.data();
    instance_create_info.enabledExtensionCount   = uint32_t(extension_names_c.size());
    instance_create_info.ppEnabledExtensionNames = extension_names_c.data();

    STBVK__CHECK( vkCreateInstance(&instance_create_info, create_info->allocation_callbacks, &context->instance) );

    // Set up debug report callback
    if (create_info->debug_report_callback && found_debug_report_extension)
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

STBVKDEF VkResult stbvk_init_device(stbvk_context_create_info const * create_info, VkSurfaceKHR present_surface, stbvk_context *context)
{
    uint32_t physical_device_count = 0;
    STBVK__CHECK( vkEnumeratePhysicalDevices(context->instance, &physical_device_count, NULL) );
    STBVK_ASSERT(physical_device_count > 0);
    context->all_physical_devices = (VkPhysicalDevice*)STBVK_MALLOC(physical_device_count * sizeof(VkPhysicalDevice));
    STBVK__CHECK( vkEnumeratePhysicalDevices(context->instance, &physical_device_count, context->all_physical_devices) );

    // Select a physical device.
    // Loop over all physical devices and all queue families of each device, looking for at least
    // one queue that supports graphics & at least one that can present to the present surface. Preferably
    // the same quque, but not always.
    // TODO(cort): Right now, we use the first physical device that suits our needs. It may be worth trying harder
    // to identify the *best* GPU in a multi-GPU system, >1 of which may fully support Vulkan.
    // Maybe let the caller pass in an optional device name (which they've queried for themselves, or prompted a user for)
    // and skip devices that don't match it?
    bool found_graphics_queue_family = false;
    bool found_present_queue_family = false;
    uint32_t graphics_queue_family_index = UINT32_MAX;
    uint32_t present_queue_family_index = UINT32_MAX;
    VkDeviceQueueCreateInfo device_queue_create_infos[2] = {};
    uint32_t device_queue_create_info_count = 0;
    for(uint32_t iPD=0; iPD< physical_device_count; ++iPD)
    {
        found_graphics_queue_family = false;
        found_present_queue_family = false;
        graphics_queue_family_index = UINT32_MAX;
        present_queue_family_index = UINT32_MAX;

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(context->all_physical_devices[iPD], &queue_family_count, NULL);
        VkQueueFamilyProperties *queue_family_properties_all = (VkQueueFamilyProperties*)STBVK_MALLOC(queue_family_count * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(context->all_physical_devices[iPD], &queue_family_count, queue_family_properties_all);

        for(uint32_t iQF=0; iQF<queue_family_count; ++iQF)
        {
            bool queue_supports_graphics = (queue_family_properties_all[iQF].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            VkBool32 queue_supports_present = VK_FALSE;
            STBVK__CHECK( vkGetPhysicalDeviceSurfaceSupportKHR(context->all_physical_devices[iPD], iQF,
                present_surface, &queue_supports_present) );
            if (queue_supports_graphics && queue_supports_present)
            {
                // prefer a queue that supports both if possible.
                graphics_queue_family_index = present_queue_family_index = iQF;
                found_graphics_queue_family = found_present_queue_family = true;
                context->graphics_queue_family_properties = queue_family_properties_all[iQF];
                context->present_queue_family_properties  = queue_family_properties_all[iQF];
            }
            else
            {
                if (!found_present_queue_family && queue_supports_present)
                {
                    present_queue_family_index = iQF;
                    found_present_queue_family = true;
                    context->present_queue_family_properties = queue_family_properties_all[iQF];
                }
                if (!found_graphics_queue_family && queue_supports_graphics)
                {
                    graphics_queue_family_index = iQF;
                    found_graphics_queue_family = true;
                    context->graphics_queue_family_properties = queue_family_properties_all[iQF];
                }
            }
            if (found_graphics_queue_family && found_present_queue_family)
            {
                break;
            }
        }
        STBVK_FREE(queue_family_properties_all);
        queue_family_properties_all = NULL;

        if (found_present_queue_family && found_graphics_queue_family)
        {
            context->physical_device = context->all_physical_devices[iPD];
            context->graphics_queue_family_index = graphics_queue_family_index;
            context->present_queue_family_index  = present_queue_family_index;

            uint32_t graphics_queue_count = context->graphics_queue_family_properties.queueCount;
            float *graphics_queue_priorities = (float*)STBVK_MALLOC(graphics_queue_count * sizeof(float));
            for(uint32_t iQ=0; iQ<graphics_queue_count; ++iQ) {
                graphics_queue_priorities[iQ] = 1.0f;
            }
            device_queue_create_info_count += 1;
            device_queue_create_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            device_queue_create_infos[0].pNext = NULL;
            device_queue_create_infos[0].flags = 0;
            device_queue_create_infos[0].queueFamilyIndex = graphics_queue_family_index;
            device_queue_create_infos[0].queueCount = graphics_queue_count;
            device_queue_create_infos[0].pQueuePriorities = graphics_queue_priorities;

            if (present_queue_family_index != graphics_queue_family_index)
            {
                uint32_t present_queue_count = context->present_queue_family_properties.queueCount;
                float *present_queue_priorities = (float*)STBVK_MALLOC(present_queue_count * sizeof(float));
                for(uint32_t iQ=0; iQ<present_queue_count; ++iQ) {
                    present_queue_priorities[iQ] = 1.0f;
                }
                device_queue_create_info_count += 1;
                device_queue_create_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                device_queue_create_infos[1].pNext = NULL;
                device_queue_create_infos[1].flags = 0;
                device_queue_create_infos[1].queueFamilyIndex = present_queue_family_index;
                device_queue_create_infos[1].queueCount = present_queue_count;
                device_queue_create_infos[1].pQueuePriorities = present_queue_priorities;
            }
        }
    }
    STBVK_ASSERT(found_graphics_queue_family && found_present_queue_family);
    context->present_surface = present_surface;

    vkGetPhysicalDeviceProperties(context->physical_device, &context->physical_device_properties);
#if 0
    STBVK_LOG("Physical device #%u: '%s', API version %u.%u.%u",
        0,
        context->physical_device_properties.deviceName,
        VK_VERSION_MAJOR(context->physical_device_properties.apiVersion),
        VK_VERSION_MINOR(context->physical_device_properties.apiVersion),
        VK_VERSION_PATCH(context->physical_device_properties.apiVersion));
#endif

    vkGetPhysicalDeviceMemoryProperties(context->physical_device, &context->physical_device_memory_properties);

    vkGetPhysicalDeviceFeatures(context->physical_device, &context->physical_device_features);

    uint32_t device_extension_count = 0;
    STBVK__CHECK( vkEnumerateDeviceExtensionProperties(context->physical_device, nullptr, &device_extension_count, nullptr) );
    std::vector<VkExtensionProperties> device_extension_properties(device_extension_count);
    STBVK__CHECK( vkEnumerateDeviceExtensionProperties(context->physical_device, nullptr, &device_extension_count, device_extension_properties.data()) );
    std::vector<std::string> all_device_extension_names(device_extension_count);
    for(uint32_t iExt=0; iExt<device_extension_count; iExt+=1)
    {
        all_device_extension_names[iExt] = std::string(device_extension_properties[iExt].extensionName);
    }
    std::vector<const char*> extension_names_c;
    extension_names_c.reserve(all_device_extension_names.size());
    for(uint32_t iExt = 0; iExt<create_info->required_device_extension_count; iExt += 1)
    {
        if (std::find(all_device_extension_names.cbegin(), all_device_extension_names.cend(),
            create_info->required_device_extension_names[iExt])
            == all_device_extension_names.cend())
        {
            break;
        }
        extension_names_c.push_back( create_info->required_device_extension_names[iExt] );
    }

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = NULL;
    device_create_info.flags = 0;
    device_create_info.queueCreateInfoCount = device_queue_create_info_count;
    device_create_info.pQueueCreateInfos = device_queue_create_infos;
    device_create_info.enabledLayerCount = 0;
    device_create_info.ppEnabledLayerNames = NULL;
    device_create_info.enabledExtensionCount = (uint32_t)extension_names_c.size();
    device_create_info.ppEnabledExtensionNames = extension_names_c.data();
    device_create_info.pEnabledFeatures = &context->physical_device_features;
    STBVK__CHECK( vkCreateDevice(context->physical_device, &device_create_info, context->allocation_callbacks, &context->device) );
    STBVK_FREE(device_create_info.pQueueCreateInfos[0].pQueuePriorities);
    STBVK_FREE(device_create_info.pQueueCreateInfos[1].pQueuePriorities);
#if 0
    STBVK_LOG("Created Vulkan logical device with extensions:");
    for(uint32_t iExt=0; iExt<device_create_info.enabledExtensionCount; iExt+=1) {
        STBVK_LOG("- %s", device_create_info.ppEnabledExtensionNames[iExt]);
    }
#endif

    STBVK_ASSERT(context->present_queue_family_properties.queueCount > 0);
    vkGetDeviceQueue(context->device, context->present_queue_family_index, 0, &context->present_queue);
    STBVK_ASSERT(context->graphics_queue_family_properties.queueCount > 0);
    vkGetDeviceQueue(context->device, context->graphics_queue_family_index, 0, &context->graphics_queue);

    return VK_SUCCESS;
}

STBVKDEF VkResult stbvk_init_command_pool(stbvk_context_create_info const * /*createInfo*/, stbvk_context *context)
{
    // Create command pool
    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.pNext = NULL;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allows reseting individual command buffers from this pool
    command_pool_create_info.queueFamilyIndex = context->graphics_queue_family_index;
    STBVK__CHECK( vkCreateCommandPool(context->device, &command_pool_create_info, context->allocation_callbacks, &context->command_pool) );

    // Allocate primary command buffer
    VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.pNext = NULL;
    command_buffer_allocate_info.commandPool = context->command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;
    STBVK__CHECK( vkAllocateCommandBuffers(context->device, &command_buffer_allocate_info, &context->command_buffer_primary) );

    return VK_SUCCESS;
}

STBVKDEF VkResult stbvk_init_swapchain(stbvk_context_create_info const * /*create_info*/, stbvk_context *context, VkSwapchainKHR old_swapchain)
{
    VkSurfaceCapabilitiesKHR surface_capabilities;
    STBVK__CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physical_device, context->present_surface, &surface_capabilities) );
    VkExtent2D swapchain_extent;
    if ( (int32_t)surface_capabilities.currentExtent.width == -1 )
    {
        STBVK_ASSERT( (int32_t)surface_capabilities.currentExtent.height == -1 );
        // TODO(cort): better defaults here, when we can't detect the present surface extent?
        swapchain_extent.width  = STBVK__CLAMP(1280, surface_capabilities.minImageExtent.width,  surface_capabilities.maxImageExtent.width);
        swapchain_extent.height = STBVK__CLAMP( 720, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    }
    else
    {
        swapchain_extent = surface_capabilities.currentExtent;
    }
    uint32_t device_surface_format_count = 0;
    STBVK__CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->present_surface, &device_surface_format_count, NULL) );
    VkSurfaceFormatKHR *device_surface_formats = (VkSurfaceFormatKHR*)STBVK_MALLOC(device_surface_format_count * sizeof(VkSurfaceFormatKHR));
    STBVK__CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->present_surface, &device_surface_format_count, device_surface_formats) );
    if (device_surface_format_count == 1 && device_surface_formats[0].format == VK_FORMAT_UNDEFINED)
    {
        // No preferred format.
        context->swapchain_surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
        context->swapchain_surface_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    }
    else
    {
        assert(device_surface_format_count >= 1);
        context->swapchain_surface_format = device_surface_formats[0];
    }
    STBVK_FREE(device_surface_formats);

    uint32_t device_present_mode_count = 0;
    STBVK__CHECK( vkGetPhysicalDeviceSurfacePresentModesKHR(context->physical_device, context->present_surface, &device_present_mode_count, NULL) );
    VkPresentModeKHR *device_present_modes = (VkPresentModeKHR*)STBVK_MALLOC(device_present_mode_count * sizeof(VkPresentModeKHR));
    STBVK__CHECK( vkGetPhysicalDeviceSurfacePresentModesKHR(context->physical_device, context->present_surface, &device_present_mode_count, device_present_modes) );
    VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    STBVK_FREE(device_present_modes);

    uint32_t desired_swapchain_image_count = surface_capabilities.minImageCount+1;
    if (	surface_capabilities.maxImageCount > 0
        &&	desired_swapchain_image_count > surface_capabilities.maxImageCount)
    {
        desired_swapchain_image_count = surface_capabilities.maxImageCount;
    }
    VkSurfaceTransformFlagBitsKHR swapchain_surface_transform;
    if (0 != (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR))
    {
        swapchain_surface_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        swapchain_surface_transform = surface_capabilities.currentTransform;
    }

    VkImageUsageFlags swapchain_image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        swapchain_image_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; // used for image clears
    }

    STBVK_ASSERT(surface_capabilities.supportedCompositeAlpha != 0);
    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ( !(surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) )
    {
      composite_alpha = VkCompositeAlphaFlagBitsKHR(
            int(surface_capabilities.supportedCompositeAlpha) &
            -int(surface_capabilities.supportedCompositeAlpha) );
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.pNext = NULL;
    swapchain_create_info.surface = context->present_surface;
    swapchain_create_info.minImageCount = desired_swapchain_image_count;
    swapchain_create_info.imageFormat = context->swapchain_surface_format.format;
    swapchain_create_info.imageColorSpace = context->swapchain_surface_format.colorSpace;
    swapchain_create_info.imageExtent.width = swapchain_extent.width;
    swapchain_create_info.imageExtent.height = swapchain_extent.height;
    swapchain_create_info.imageUsage = swapchain_image_usage;
    swapchain_create_info.preTransform = swapchain_surface_transform;
    swapchain_create_info.compositeAlpha = composite_alpha;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 0;
    swapchain_create_info.pQueueFamilyIndices = NULL;
    swapchain_create_info.presentMode = swapchain_present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = old_swapchain;
    STBVK__CHECK( vkCreateSwapchainKHR(context->device, &swapchain_create_info, context->allocation_callbacks, &context->swapchain) );
    if (old_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(context->device, old_swapchain, context->allocation_callbacks);
    }

    STBVK__CHECK( vkGetSwapchainImagesKHR(context->device, context->swapchain, &context->swapchain_image_count, NULL) );
    context->swapchain_images = (VkImage*)malloc(context->swapchain_image_count * sizeof(VkImage));
    STBVK__CHECK( vkGetSwapchainImagesKHR(context->device, context->swapchain, &context->swapchain_image_count, context->swapchain_images) );

    VkImageViewCreateInfo image_view_create_info = {};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.pNext = NULL;
    image_view_create_info.flags = 0;
    image_view_create_info.format = context->swapchain_surface_format.format;
    image_view_create_info.components = {};
    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.subresourceRange = {};
    image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.image = VK_NULL_HANDLE; // filled in below
    context->swapchain_image_views = (VkImageView*)malloc(context->swapchain_image_count * sizeof(VkImageView));
    for(uint32_t iSCI=0; iSCI<context->swapchain_image_count; iSCI+=1)
    {
        image_view_create_info.image = context->swapchain_images[iSCI];
        STBVK__CHECK( vkCreateImageView(context->device, &image_view_create_info, context->allocation_callbacks, &context->swapchain_image_views[iSCI]) );
    }

    return VK_SUCCESS;
}

STBVKDEF void stbvk_destroy_context(stbvk_context *context)
{
    vkDeviceWaitIdle(context->device);

    for(uint32_t iSCI=0; iSCI<context->swapchain_image_count; ++iSCI)
    {
        vkDestroyImageView(context->device, context->swapchain_image_views[iSCI], context->allocation_callbacks);
    }
    STBVK_FREE(context->swapchain_image_views);
    context->swapchain_image_views = NULL;
    STBVK_FREE(context->swapchain_images);
    context->swapchain_images = NULL;
    vkDestroySwapchainKHR(context->device, context->swapchain, context->allocation_callbacks);

    vkFreeCommandBuffers(context->device, context->command_pool, 1, &context->command_buffer_primary);

    vkDestroyCommandPool(context->device, context->command_pool, context->allocation_callbacks);
    context->command_pool = NULL;

    vkDestroyDevice(context->device, context->allocation_callbacks);
    context->device = VK_NULL_HANDLE;
    STBVK_FREE(context->all_physical_devices);

    if (context->debug_report_callback != VK_NULL_HANDLE)
    {
        PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback =
            (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(context->instance, "vkDestroyDebugReportCallbackEXT");
        DestroyDebugReportCallback(context->instance, context->debug_report_callback, context->allocation_callbacks);
    }

    vkDestroySurfaceKHR(context->instance, context->present_surface, context->allocation_callbacks);

    vkDestroyInstance(context->instance, context->allocation_callbacks);
    context->instance = VK_NULL_HANDLE;

    context->allocation_callbacks = NULL;
    *context = {};
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
#endif

static VkBool32 get_memory_type_from_properties(VkPhysicalDeviceMemoryProperties const *memory_properties,
    uint32_t memory_type_bits, VkFlags requirements_mask, uint32_t *out_memory_type_index)
{
    STBVK_ASSERT(sizeof(memory_type_bits)*8 == VK_MAX_MEMORY_TYPES);
    for(uint32_t iMemType=0; iMemType<VK_MAX_MEMORY_TYPES; iMemType+=1)
    {
        if (	(memory_type_bits & (1<<iMemType)) != 0
            &&	(memory_properties->memoryTypes[iMemType].propertyFlags & requirements_mask) == requirements_mask)
        {
            *out_memory_type_index = iMemType;
            return VK_TRUE;
        }
    }
    return VK_FALSE;
}



STBVKDEF VkResult stbvk_create_image(stbvk_context const *context, stbvk_image_create_info const *create_info, stbvk_image *out_image)
{
    // TODO: take a bitfield of requested format features, and make sure the physical device supports them in tiled mode.
    VkFormatProperties texture_format_properties = {};
    vkGetPhysicalDeviceFormatProperties(context->physical_device, create_info->format, &texture_format_properties);
    if (0 == (texture_format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        fprintf(stderr, "ERROR: physical device does not support sampling format %d with optimal tiling.\n", create_info->format);
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    VkImageCreateFlags image_create_flags = (VkImageCreateFlags)0;
    if (create_info->view_type == VK_IMAGE_VIEW_TYPE_CUBE ||
        create_info->view_type == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
    {
        image_create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }
    VkImageFormatProperties image_format_properties = {};
    STBVK__CHECK( vkGetPhysicalDeviceImageFormatProperties(context->physical_device,
        create_info->format, create_info->image_type, create_info->tiling,
        create_info->usage, image_create_flags, &image_format_properties) );
    if (create_info->array_layers  > image_format_properties.maxArrayLayers ||
        create_info->mip_levels    > image_format_properties.maxMipLevels ||
        create_info->extent.width  > image_format_properties.maxExtent.width ||
        create_info->extent.height > image_format_properties.maxExtent.height ||
        create_info->extent.depth  > image_format_properties.maxExtent.depth)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if ( (VkSampleCountFlagBits)(create_info->samples & image_format_properties.sampleCounts) != create_info->samples )
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    out_image->create_info = *create_info;

    out_image->image_create_info = {};
    out_image->image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    out_image->image_create_info.pNext = NULL;
    out_image->image_create_info.flags = image_create_flags;
    out_image->image_create_info.imageType = create_info->image_type;
    out_image->image_create_info.format = create_info->format;
    out_image->image_create_info.extent = create_info->extent;
    out_image->image_create_info.mipLevels = create_info->mip_levels;
    out_image->image_create_info.arrayLayers = create_info->array_layers;
    out_image->image_create_info.samples = create_info->samples;
    out_image->image_create_info.tiling = create_info->tiling;
    out_image->image_create_info.usage = create_info->usage;
    out_image->image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    out_image->image_create_info.queueFamilyIndexCount = 0;
    out_image->image_create_info.pQueueFamilyIndices = NULL;
    out_image->image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    STBVK__CHECK( vkCreateImage(context->device, &out_image->image_create_info, context->allocation_callbacks, &out_image->image) );

    vkGetImageMemoryRequirements(context->device, out_image->image, &out_image->memory_requirements);
    if (out_image->memory_requirements.size > image_format_properties.maxResourceSize)
    {
        vkDestroyImage(context->device, out_image->image, context->allocation_callbacks);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryAllocateInfo memory_allocate_info = {};
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = out_image->memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = 0; // filled in below
    VkBool32 found_memory_type = get_memory_type_from_properties(
        &context->physical_device_memory_properties,
        out_image->memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &memory_allocate_info.memoryTypeIndex);
    STBVK_ASSERT(found_memory_type);
    STBVK__CHECK( vkAllocateMemory(context->device, &memory_allocate_info, context->allocation_callbacks, &out_image->device_memory) );
    VkDeviceSize memory_offset = 0;
    STBVK__CHECK( vkBindImageMemory(context->device, out_image->image, out_image->device_memory, memory_offset) );

    out_image->image_view_create_info = {};
    out_image->image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    out_image->image_view_create_info.pNext = NULL;
    out_image->image_view_create_info.flags = 0;
    out_image->image_view_create_info.image = out_image->image;
    out_image->image_view_create_info.viewType = create_info->view_type;
    out_image->image_view_create_info.format = create_info->format;
    out_image->image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    out_image->image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    out_image->image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    out_image->image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    out_image->image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // TODO(cort): generalize?
    out_image->image_view_create_info.subresourceRange.baseMipLevel = 0;
    out_image->image_view_create_info.subresourceRange.levelCount = create_info->mip_levels;
    out_image->image_view_create_info.subresourceRange.baseArrayLayer = 0;
    out_image->image_view_create_info.subresourceRange.layerCount = create_info->array_layers;
    STBVK__CHECK( vkCreateImageView(context->device, &out_image->image_view_create_info, context->allocation_callbacks, &out_image->image_view) );

    return VK_SUCCESS;
}

static VkResult stbvk__create_staging_image(stbvk_context const *context, stbvk_image const *image,
    VkImageSubresource subresource, VkImage *out_staging_image)
{
    VkImageCreateInfo staging_image_create_info = image->image_create_info;
    staging_image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
    staging_image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    staging_image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    staging_image_create_info.queueFamilyIndexCount = 0;
    staging_image_create_info.pQueueFamilyIndices = NULL;
    staging_image_create_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    staging_image_create_info.arrayLayers = 1;
    staging_image_create_info.mipLevels = 1;
    staging_image_create_info.extent.width  = stbvk__max(image->image_create_info.extent.width  >> subresource.mipLevel, 1U);
    staging_image_create_info.extent.height = stbvk__max(image->image_create_info.extent.height >> subresource.mipLevel, 1U);
    staging_image_create_info.extent.depth  = stbvk__max(image->image_create_info.extent.depth  >> subresource.mipLevel, 1U);
    STBVK__CHECK( vkCreateImage(context->device, &staging_image_create_info, context->allocation_callbacks, out_staging_image) );
    return VK_SUCCESS;
}

STBVKDEF VkResult stbvk_get_image_subresource_source_layout(stbvk_context const *context, stbvk_image const *image, VkImageSubresource subresource,
    VkSubresourceLayout *out_layout)
{
    // TODO(cort): validate subresource against image bounds
    VkImage staging_image_temp = VK_NULL_HANDLE;
    STBVK__CHECK( stbvk__create_staging_image(context, image, subresource, &staging_image_temp) );
    vkGetImageSubresourceLayout(context->device, staging_image_temp, &subresource, out_layout);
    vkDestroyImage(context->device, staging_image_temp, context->allocation_callbacks);
    return VK_SUCCESS;
}

STBVKDEF VkResult stbvk_load_image_subresource(stbvk_context const *context, stbvk_image const *image,
    VkImageSubresource subresource, VkSubresourceLayout subresource_layout, void const *pixels)
{
    VkImage staging_image;
    STBVK__CHECK( stbvk__create_staging_image(context, image, subresource, &staging_image) );

    VkMemoryRequirements memory_requirements = {};
    vkGetImageMemoryRequirements(context->device, staging_image, &memory_requirements);
    VkMemoryAllocateInfo memory_allocate_info = {};
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = 0; // filled in below
    VkBool32 found_memory_type = get_memory_type_from_properties(&context->physical_device_memory_properties,
        memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &memory_allocate_info.memoryTypeIndex);
    STBVK_ASSERT(found_memory_type);
    VkDeviceMemory staging_device_memory = VK_NULL_HANDLE;
    STBVK__CHECK( vkAllocateMemory(context->device, &memory_allocate_info, context->allocation_callbacks, &staging_device_memory) );
    VkDeviceSize memory_offset = 0;
    STBVK__CHECK( vkBindImageMemory(context->device, staging_image, staging_device_memory, memory_offset) );

    VkSubresourceLayout layout_sanity_check = {};
    vkGetImageSubresourceLayout(context->device, staging_image, &subresource, &layout_sanity_check);
    STBVK_ASSERT(
        layout_sanity_check.offset     == subresource_layout.offset &&
        layout_sanity_check.size       == subresource_layout.size &&
        layout_sanity_check.rowPitch   == subresource_layout.rowPitch &&
        layout_sanity_check.arrayPitch == subresource_layout.arrayPitch &&
        layout_sanity_check.depthPitch == subresource_layout.depthPitch);

    void *mapped_subresource_data = NULL;
    VkMemoryMapFlags memory_map_flags = 0;
    STBVK__CHECK( vkMapMemory(context->device, staging_device_memory, memory_offset,
        memory_requirements.size, memory_map_flags, &mapped_subresource_data) );
    memcpy(mapped_subresource_data, pixels, subresource_layout.size);
    vkUnmapMemory(context->device, staging_device_memory);

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.pNext = nullptr;
    fence_create_info.flags = 0;
    VkFence staging_fence = VK_NULL_HANDLE;
    STBVK__CHECK( vkCreateFence(context->device, &fence_create_info, context->allocation_callbacks, &staging_fence) );

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.pNext = NULL;
    command_buffer_allocate_info.commandPool = context->command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;
    VkCommandBuffer cmd_buf_staging = {};
    STBVK__CHECK( vkAllocateCommandBuffers(context->device, &command_buffer_allocate_info, &cmd_buf_staging) );
    VkCommandBufferBeginInfo cmd_buf_begin_info = {};
    cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_begin_info.pNext = nullptr;
    cmd_buf_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT ;
    cmd_buf_begin_info.pInheritanceInfo = nullptr;
    STBVK__CHECK( vkBeginCommandBuffer(cmd_buf_staging, &cmd_buf_begin_info) );

    VkImageSubresourceRange staging_image_subresource_range = {};
    staging_image_subresource_range.aspectMask = image->image_view_create_info.subresourceRange.aspectMask;
    staging_image_subresource_range.baseMipLevel = 0;
    staging_image_subresource_range.levelCount = 1;
    staging_image_subresource_range.baseArrayLayer = 0;
    staging_image_subresource_range.layerCount = 1;
    stbvk_set_image_layout(cmd_buf_staging, staging_image, staging_image_subresource_range,
        VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0);
    VkImageSubresourceRange dst_image_subresource_range = {};
    dst_image_subresource_range.aspectMask = image->image_view_create_info.subresourceRange.aspectMask;
    dst_image_subresource_range.baseMipLevel = subresource.mipLevel;
    dst_image_subresource_range.levelCount = 1;
    dst_image_subresource_range.baseArrayLayer = subresource.arrayLayer;
    dst_image_subresource_range.layerCount = 1;
    stbvk_set_image_layout(cmd_buf_staging, image->image, dst_image_subresource_range,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0);

    VkImageCopy copy_region = {};
    copy_region.srcSubresource = {};
    copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.srcSubresource.baseArrayLayer = 0;
    copy_region.srcSubresource.layerCount = 1;
    copy_region.srcSubresource.mipLevel = 0;
    copy_region.srcOffset = {0,0,0};
    copy_region.dstSubresource = {};
    copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.dstSubresource.baseArrayLayer = subresource.arrayLayer;
    copy_region.dstSubresource.layerCount = 1;
    copy_region.dstSubresource.mipLevel = subresource.mipLevel;
    copy_region.dstOffset = {0,0,0};
    copy_region.extent.width  = stbvk__max(image->image_create_info.extent.width  >> subresource.mipLevel, 1U);
    copy_region.extent.height = stbvk__max(image->image_create_info.extent.height >> subresource.mipLevel, 1U);
    copy_region.extent.depth  = stbvk__max(image->image_create_info.extent.depth  >> subresource.mipLevel, 1U);
    vkCmdCopyImage(cmd_buf_staging,
        staging_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    stbvk_set_image_layout(cmd_buf_staging, image->image, dst_image_subresource_range,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image->create_info.final_layout, 0);

    STBVK__CHECK( vkEndCommandBuffer(cmd_buf_staging) );
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf_staging;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = NULL;
    STBVK__CHECK( vkQueueSubmit(context->graphics_queue, 1, &submit_info, staging_fence) );
    STBVK__CHECK( vkWaitForFences(context->device, 1, &staging_fence, VK_TRUE, UINT64_MAX) );

    vkFreeCommandBuffers(context->device, context->command_pool, 1, &cmd_buf_staging);
    vkDestroyFence(context->device, staging_fence, context->allocation_callbacks);
    vkFreeMemory(context->device, staging_device_memory, context->allocation_callbacks);
    vkDestroyImage(context->device, staging_image, context->allocation_callbacks);

    return VK_SUCCESS;
}

STBVKDEF void stbvk_destroy_image(stbvk_context const *context, stbvk_image *image)
{
    vkFreeMemory(context->device, image->device_memory, context->allocation_callbacks);
    vkDestroyImageView(context->device, image->image_view, context->allocation_callbacks);
    vkDestroyImage(context->device, image->image, context->allocation_callbacks);
}


#ifndef STBVK_NO_STDIO
STBVKDEF VkShaderModule stbvk_load_shader_from_file(stbvk_context *c, FILE *f, int len)
{
    void *shader_bin = STBVK_MALLOC(len);
    size_t bytes_read = fread(shader_bin, 1, len, f);
    if ( (int)bytes_read != len)
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
    default:
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
        // Make sure any Copy or CPU writes to image are flushed
        img_memory_barrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
        img_memory_barrier.dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        break;
    default:
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
