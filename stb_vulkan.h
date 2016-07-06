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
        VkPipelineCache pipeline_cache;

        VkSwapchainKHR swapchain;
        uint32_t swapchain_image_count;
        VkSurfaceFormatKHR swapchain_surface_format;
        VkImage *swapchain_images;
        VkImageView *swapchain_image_views;
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

    STBVKDEF VkBool32 stbvk_get_memory_type_from_properties(VkPhysicalDeviceMemoryProperties const *memory_properties,
        uint32_t memory_type_bits, VkMemoryPropertyFlags memory_properties_mask, uint32_t *out_memory_type_index);

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
        VkImageLayout initial_layout;
        VkMemoryPropertyFlags memory_properties_mask;
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
    STBVKDEF VkResult stbvk_image_create(stbvk_context const *context, stbvk_image_create_info const *create_info, stbvk_image *out_image);
    STBVKDEF VkResult stbvk_image_get_subresource_source_layout(stbvk_context const *context, stbvk_image const *image,
        VkImageSubresource subresource, VkSubresourceLayout *out_layout);
    STBVKDEF VkResult stbvk_image_load_subresource(stbvk_context const *context, stbvk_image const *image,
        VkImageSubresource subresource, VkSubresourceLayout subresource_layout, VkImageLayout final_image_layout,
        void const *pixels);
    STBVKDEF void stbvk_image_destroy(stbvk_context const *context, stbvk_image *image);

    STBVKDEF int stbvk_image_load_from_dds_file(stbvk_context const *context, char const *dds_file_path, stbvk_image *out_image);
    STBVKDEF int stbvk_image_load_from_dds_buffer(stbvk_context const *context, void const *dds_file_data, size_t dds_file_size, stbvk_image *out_image);

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

    STBVKDEF VkResult stbvk_create_descriptor_pool(stbvk_context const *c, const VkDescriptorSetLayoutCreateInfo *layout_ci, uint32_t max_sets,
        VkDescriptorPoolCreateFlags flags, VkDescriptorPool *out_pool);

    STBVKDEF void stbvk_set_image_layout(VkCommandBuffer cmd_buf, VkImage image,
        VkImageSubresourceRange subresource_range, VkImageLayout old_layout, VkImageLayout new_layout,
        VkAccessFlags src_access_mask);

    typedef struct
    {
        uint32_t stride;
        uint32_t attribute_count;
        VkVertexInputAttributeDescription attributes[16];
    } stbvk_vertex_buffer_layout;
    typedef struct
    {
        stbvk_vertex_buffer_layout vertex_buffer_layout; // assumed to be bound at slot 0
        uint32_t dynamic_state_mask;
        VkPrimitiveTopology primitive_topology;
        VkViewport viewport;   // ignored if dynamic_state_mask & (1<<VK_DYNAMIC_STATE_VIEWPORT)
        VkRect2D scissor_rect; // ignored if dynamic_state_mask & (1<<VK_DYNAMIC_STATE_SCISSOR)
        VkPipelineLayout pipeline_layout;
        VkRenderPass render_pass;
        uint32_t subpass;
        uint32_t subpass_color_attachment_count;
        VkShaderModule vertex_shader;
        VkShaderModule fragment_shader;
    } stbvk_graphics_pipeline_settings_vsps;
    typedef struct
    {
	    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info;

	    VkPipelineShaderStageCreateInfo shader_stage_create_infos[5]; // TODO(cort): >5 shader stages?
        VkVertexInputBindingDescription vertex_input_binding_descriptions[4];
        VkVertexInputAttributeDescription vertex_input_attribute_descriptions[16];
        VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info;
	    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info;
        VkPipelineTessellationStateCreateInfo tessellation_state_create_info;
        VkViewport viewports[8];
        VkRect2D scissor_rects[8];
	    VkPipelineViewportStateCreateInfo viewport_state_create_info;
	    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info;
	    VkPipelineMultisampleStateCreateInfo multisample_state_create_info;
	    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info;
	    VkPipelineColorBlendAttachmentState color_blend_attachment_states[8]; // TODO(cort): >8 color attachments?
	    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info;
        VkDynamicState dynamic_states[VK_DYNAMIC_STATE_RANGE_SIZE];
	    VkPipelineDynamicStateCreateInfo dynamic_state_create_info;
    } stbvk_graphics_pipeline_create_info;
    STBVKDEF int stbvk_prepare_graphics_pipeline_create_info_vsps(
        stbvk_graphics_pipeline_settings_vsps const *settings,
        stbvk_graphics_pipeline_create_info *out_create_info);

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
    VkPhysicalDevice *all_physical_devices = (VkPhysicalDevice*)STBVK_MALLOC(physical_device_count * sizeof(VkPhysicalDevice));
    STBVK__CHECK( vkEnumeratePhysicalDevices(context->instance, &physical_device_count, all_physical_devices) );

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
        vkGetPhysicalDeviceQueueFamilyProperties(all_physical_devices[iPD], &queue_family_count, NULL);
        VkQueueFamilyProperties *queue_family_properties_all = (VkQueueFamilyProperties*)STBVK_MALLOC(queue_family_count * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(all_physical_devices[iPD], &queue_family_count, queue_family_properties_all);

        for(uint32_t iQF=0; iQF<queue_family_count; ++iQF)
        {
            bool queue_supports_graphics = (queue_family_properties_all[iQF].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            VkBool32 queue_supports_present = VK_FALSE;
            STBVK__CHECK( vkGetPhysicalDeviceSurfaceSupportKHR(all_physical_devices[iPD], iQF,
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
            context->physical_device = all_physical_devices[iPD];
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
    STBVK_FREE(all_physical_devices);
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

    VkPipelineCacheCreateInfo pipeline_cache_create_info = {};
    pipeline_cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipeline_cache_create_info.pNext = NULL;
    pipeline_cache_create_info.flags = 0;
    pipeline_cache_create_info.initialDataSize = 0;
    pipeline_cache_create_info.pInitialData = NULL;
    STBVK__CHECK( vkCreatePipelineCache(context->device, &pipeline_cache_create_info, context->allocation_callbacks, &context->pipeline_cache) );

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
    VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
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
    STBVK_ASSERT( (swapchain_image_usage & surface_capabilities.supportedUsageFlags) == swapchain_image_usage );

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

    vkDestroyCommandPool(context->device, context->command_pool, context->allocation_callbacks);
    context->command_pool = NULL;

    vkDestroyPipelineCache(context->device, context->pipeline_cache, context->allocation_callbacks);

    vkDestroyDevice(context->device, context->allocation_callbacks);
    context->device = VK_NULL_HANDLE;

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

STBVKDEF VkBool32 stbvk_get_memory_type_from_properties(VkPhysicalDeviceMemoryProperties const *memory_properties,
    uint32_t memory_type_bits, VkMemoryPropertyFlags memory_properties_mask, uint32_t *out_memory_type_index)
{
    STBVK_ASSERT(sizeof(memory_type_bits)*8 == VK_MAX_MEMORY_TYPES);
    for(uint32_t iMemType=0; iMemType<VK_MAX_MEMORY_TYPES; iMemType+=1)
    {
        if (	(memory_type_bits & (1<<iMemType)) != 0
            &&	(memory_properties->memoryTypes[iMemType].propertyFlags & memory_properties_mask) == memory_properties_mask)
        {
            *out_memory_type_index = iMemType;
            return VK_TRUE;
        }
    }
    return VK_FALSE;
}



STBVKDEF VkResult stbvk_image_create(stbvk_context const *context, stbvk_image_create_info const *create_info, stbvk_image *out_image)
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

    VkImageLayout create_image_layout = (create_info->initial_layout == VK_IMAGE_LAYOUT_PREINITIALIZED)
        ? VK_IMAGE_LAYOUT_PREINITIALIZED
        : VK_IMAGE_LAYOUT_UNDEFINED;
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
    out_image->image_create_info.initialLayout = create_image_layout;

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
    VkBool32 found_memory_type = stbvk_get_memory_type_from_properties(
        &context->physical_device_memory_properties,
        out_image->memory_requirements.memoryTypeBits,
        create_info->memory_properties_mask,
        &memory_allocate_info.memoryTypeIndex);
    STBVK_ASSERT(found_memory_type);
    STBVK__CHECK( vkAllocateMemory(context->device, &memory_allocate_info, context->allocation_callbacks, &out_image->device_memory) );
    VkDeviceSize memory_offset = 0;
    STBVK__CHECK( vkBindImageMemory(context->device, out_image->image, out_image->device_memory, memory_offset) );

    VkImageAspectFlags image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    switch(create_info->format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
        image_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        break;
    default:
        break;
    }

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
    out_image->image_view_create_info.subresourceRange.aspectMask = image_aspect;
    out_image->image_view_create_info.subresourceRange.baseMipLevel = 0;
    out_image->image_view_create_info.subresourceRange.levelCount = create_info->mip_levels;
    out_image->image_view_create_info.subresourceRange.baseArrayLayer = 0;
    out_image->image_view_create_info.subresourceRange.layerCount = create_info->array_layers;
    STBVK__CHECK( vkCreateImageView(context->device, &out_image->image_view_create_info, context->allocation_callbacks, &out_image->image_view) );

    // If the requested initial layout was something besides PREINITALIZED or UNDEFINED, convert it here.
    if (create_info->initial_layout != VK_IMAGE_LAYOUT_PREINITIALIZED &&
        create_info->initial_layout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.pNext = nullptr;
        fence_create_info.flags = 0;
        VkFence fence = VK_NULL_HANDLE;
        STBVK__CHECK( vkCreateFence(context->device, &fence_create_info, context->allocation_callbacks, &fence) );

        VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
        command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.pNext = NULL;
        command_buffer_allocate_info.commandPool = context->command_pool;
        command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_allocate_info.commandBufferCount = 1;
        VkCommandBuffer cmd_buf = {};
        STBVK__CHECK( vkAllocateCommandBuffers(context->device, &command_buffer_allocate_info, &cmd_buf) );
        VkCommandBufferBeginInfo cmd_buf_begin_info = {};
        cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_begin_info.pNext = nullptr;
        cmd_buf_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT ;
        cmd_buf_begin_info.pInheritanceInfo = nullptr;
        STBVK__CHECK( vkBeginCommandBuffer(cmd_buf, &cmd_buf_begin_info) );

        VkImageSubresourceRange dst_image_subresource_range = {};
        dst_image_subresource_range.aspectMask = out_image->image_view_create_info.subresourceRange.aspectMask;
        dst_image_subresource_range.baseMipLevel = 0;
        dst_image_subresource_range.levelCount = VK_REMAINING_MIP_LEVELS;
        dst_image_subresource_range.baseArrayLayer = 0;
        dst_image_subresource_range.layerCount = VK_REMAINING_ARRAY_LAYERS;
        stbvk_set_image_layout(cmd_buf, out_image->image, dst_image_subresource_range,
            VK_IMAGE_LAYOUT_UNDEFINED, create_info->initial_layout, 0);

        STBVK__CHECK( vkEndCommandBuffer(cmd_buf) );
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = NULL;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = NULL;
        submit_info.pWaitDstStageMask = NULL;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd_buf;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = NULL;
        STBVK__CHECK( vkQueueSubmit(context->graphics_queue, 1, &submit_info, fence) );
        STBVK__CHECK( vkWaitForFences(context->device, 1, &fence, VK_TRUE, UINT64_MAX) );
        vkFreeCommandBuffers(context->device, context->command_pool, 1, &cmd_buf);
        vkDestroyFence(context->device, fence, context->allocation_callbacks);
    }

    return VK_SUCCESS;
}

static VkResult stbvk__create_staging_image(stbvk_context const *context, stbvk_image const *image,
    VkImageSubresource subresource, VkImage *out_staging_image)
{
    VkImageCreateInfo staging_image_create_info = image->image_create_info;
    staging_image_create_info.flags &= ~VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
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

STBVKDEF VkResult stbvk_image_get_subresource_source_layout(stbvk_context const *context, stbvk_image const *image, VkImageSubresource subresource,
    VkSubresourceLayout *out_layout)
{
    // TODO(cort): validate subresource against image bounds
    VkImage staging_image_temp = VK_NULL_HANDLE;
    STBVK__CHECK( stbvk__create_staging_image(context, image, subresource, &staging_image_temp) );
    vkGetImageSubresourceLayout(context->device, staging_image_temp, &subresource, out_layout);
    vkDestroyImage(context->device, staging_image_temp, context->allocation_callbacks);
    return VK_SUCCESS;
}

STBVKDEF VkResult stbvk_image_load_subresource(stbvk_context const *context, stbvk_image const *image,
    VkImageSubresource subresource, VkSubresourceLayout subresource_layout, VkImageLayout final_image_layout,
    void const *pixels)
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
    VkBool32 found_memory_type = stbvk_get_memory_type_from_properties(&context->physical_device_memory_properties,
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
    copy_region.dstSubresource.aspectMask = image->image_view_create_info.subresourceRange.aspectMask;
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
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, final_image_layout, 0);

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

STBVKDEF void stbvk_image_destroy(stbvk_context const *context, stbvk_image *image)
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

STBVKDEF VkResult stbvk_create_descriptor_pool(stbvk_context const *c, const VkDescriptorSetLayoutCreateInfo *layout_ci, uint32_t max_sets,
    VkDescriptorPoolCreateFlags flags, VkDescriptorPool *out_pool)
{
    // TODO(cort): should this function take an array of layout_cis and set its sizes based on the total descriptor counts
    // across all sets? That would allow one monolithic pool for each thread, instead of one per descriptor set.
    // max_sets would need to be an array as well most likely: the number of instances of each set.
	VkDescriptorPoolSize pool_sizes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
	for(int iType=0; iType<VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++iType)
	{
		pool_sizes[iType].descriptorCount = 0;
        pool_sizes[iType].type = (VkDescriptorType)iType;
	}
	for(uint32_t iBinding=0; iBinding<layout_ci->bindingCount; iBinding+=1)
    {
        STBVK_ASSERT(
            layout_ci->pBindings[iBinding].descriptorType >= VK_DESCRIPTOR_TYPE_BEGIN_RANGE &&
            layout_ci->pBindings[iBinding].descriptorType <= VK_DESCRIPTOR_TYPE_END_RANGE);
		pool_sizes[ layout_ci->pBindings[iBinding].descriptorType ].descriptorCount +=
            layout_ci->pBindings[iBinding].descriptorCount;
	}

	VkDescriptorPoolCreateInfo pool_ci = {};
    pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.pNext = NULL;
    pool_ci.flags = flags;
	pool_ci.maxSets = max_sets;
    pool_ci.poolSizeCount = VK_DESCRIPTOR_TYPE_RANGE_SIZE;
    pool_ci.pPoolSizes = pool_sizes;
    return vkCreateDescriptorPool(c->device, &pool_ci, c->allocation_callbacks, out_pool);
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

///////////////////////////////////////// DDS loader code //////////////////////////////

static const uint32_t kDdsPrefixMagic = 0x20534444;

typedef struct
{
    uint32_t structSize;
    uint32_t flags;
    uint32_t code4;
    uint32_t numBitsRGB;
    uint32_t maskR;
    uint32_t maskG;
    uint32_t maskB;
    uint32_t maskA;
}  DdsPixelFormat;

typedef enum
{
    PF_FLAGS_CODE4     = 0x00000004,  // DDPF_FOURCC
    PF_FLAGS_RGB       = 0x00000040,  // DDPF_RGB
    PF_FLAGS_RGBA      = 0x00000041,  // DDPF_RGB | DDPF_ALPHAPIXELS
    PF_FLAGS_LUMINANCE = 0x00020000,  // DDPF_LUMINANCE
    PF_FLAGS_ALPHA     = 0x00000002,  // DDPF_ALPHA
}  DdsPixelFormatFlags;

static stbvk_inline uint32_t stbvk__make_code4(char c0, char c1, char c2, char c3)
{
    return
        ((uint32_t)(uint8_t)(c0) <<  0) |
        ((uint32_t)(uint8_t)(c1) <<  8) |
        ((uint32_t)(uint8_t)(c2) << 16) |
        ((uint32_t)(uint8_t)(c3) << 24);
}

static const DdsPixelFormat PF_DXT1 =
{ sizeof(DdsPixelFormat), PF_FLAGS_CODE4, stbvk__make_code4('D','X','T','1'), 0, 0, 0, 0, 0 };
static const DdsPixelFormat PF_DXT2 =
{ sizeof(DdsPixelFormat), PF_FLAGS_CODE4, stbvk__make_code4('D','X','T','2'), 0, 0, 0, 0, 0 };
static const DdsPixelFormat PF_DXT3 =
{ sizeof(DdsPixelFormat), PF_FLAGS_CODE4, stbvk__make_code4('D','X','T','3'), 0, 0, 0, 0, 0 };
static const DdsPixelFormat PF_DXT4 =
{ sizeof(DdsPixelFormat), PF_FLAGS_CODE4, stbvk__make_code4('D','X','T','4'), 0, 0, 0, 0, 0 };
static const DdsPixelFormat PF_DXT5 =
{ sizeof(DdsPixelFormat), PF_FLAGS_CODE4, stbvk__make_code4('D','X','T','5'), 0, 0, 0, 0, 0 };

static const DdsPixelFormat PF_A8R8G8B8 =
{ sizeof(DdsPixelFormat), PF_FLAGS_RGBA, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };

static const DdsPixelFormat PF_A8B8G8R8 =
{ sizeof(DdsPixelFormat), PF_FLAGS_RGBA, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };

static const DdsPixelFormat PF_A1R5G5B5 =
{ sizeof(DdsPixelFormat), PF_FLAGS_RGBA, 0, 16, 0x00007c00, 0x000003e0, 0x0000001f, 0x00008000 };

static const DdsPixelFormat PF_A4R4G4B4 =
{ sizeof(DdsPixelFormat), PF_FLAGS_RGBA, 0, 16, 0x00000f00, 0x000000f0, 0x0000000f, 0x0000f000 };

static const DdsPixelFormat PF_R8G8B8 =
{ sizeof(DdsPixelFormat), PF_FLAGS_RGB, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000 };

static const DdsPixelFormat PF_R8G8B8A8 =
{ sizeof(DdsPixelFormat), PF_FLAGS_RGBA, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };

static const DdsPixelFormat PF_B8G8R8 =
{ sizeof(DdsPixelFormat), PF_FLAGS_RGB, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000 };

static const DdsPixelFormat PF_R5G6B5 =
{ sizeof(DdsPixelFormat), PF_FLAGS_RGB, 0, 16, 0x0000f800, 0x000007e0, 0x0000001f, 0x00000000 };

static const DdsPixelFormat PF_L16 =
{ sizeof(DdsPixelFormat), PF_FLAGS_LUMINANCE, 0, 16, 0x0000ffff, 0x00000000, 0x00000000, 0x00000000 };

static const DdsPixelFormat PF_L8 =
{ sizeof(DdsPixelFormat), PF_FLAGS_LUMINANCE, 0, 8, 0x000000ff, 0x00000000, 0x00000000, 0x00000000 };

static const DdsPixelFormat PF_R32F =
{ sizeof(DdsPixelFormat), PF_FLAGS_CODE4, 114, 0, 0, 0, 0, 0 };

static const DdsPixelFormat PF_R16FG16FB16FA16F =
{ sizeof(DdsPixelFormat), PF_FLAGS_CODE4, 36, 0, 0, 0, 0, 0 };

static const DdsPixelFormat PF_R32FG32FB32FA32F =
{ sizeof(DdsPixelFormat), PF_FLAGS_CODE4, 116, 0, 0, 0, 0, 0 };

// This indicates the DDS_HEADER_DXT10 extension is present (the format is in the extended header's dxFormat field)
static const DdsPixelFormat PF_DX10 =
{ sizeof(DdsPixelFormat), PF_FLAGS_CODE4, stbvk__make_code4('D','X','1','0'), 0, 0, 0, 0, 0 };

enum DdsHeaderFlag
{
    HEADER_FLAGS_CAPS        = 0x00000001,
    HEADER_FLAGS_HEIGHT      = 0x00000002,
    HEADER_FLAGS_WIDTH       = 0x00000004,
    HEADER_FLAGS_PITCH       = 0x00000008,
    HEADER_FLAGS_PIXELFORMAT = 0x00001000,
    HEADER_FLAGS_LINEARSIZE  = 0x00080000,
    HEADER_FLAGS_DEPTH       = 0x00800000,
    HEADER_FLAGS_TEXTURE     = 0x00001007,  // CAPS | HEIGHT | WIDTH | PIXELFORMAT
    HEADER_FLAGS_MIPMAP      = 0x00020000,
};

enum DdsSurfaceFlags
{
    SURFACE_FLAGS_TEXTURE = 0x00001000, // HEADER_FLAGS_TEXTURE
    SURFACE_FLAGS_MIPMAP  = 0x00400008, // COMPLEX | MIPMAP
    SURFACE_FLAGS_COMPLEX = 0x00000008, // COMPLEX
};

enum DdsCubemapFlags
{
    CUBEMAP_FLAG_ISCUBEMAP = 0x00000200, // CUBEMAP
    CUBEMAP_FLAG_POSITIVEX = 0x00000600, // CUBEMAP | POSITIVEX
    CUBEMAP_FLAG_NEGATIVEX = 0x00000a00, // CUBEMAP | NEGATIVEX
    CUBEMAP_FLAG_POSITIVEY = 0x00001200, // CUBEMAP | POSITIVEY
    CUBEMAP_FLAG_NEGATIVEY = 0x00002200, // CUBEMAP | NEGATIVEY
    CUBEMAP_FLAG_POSITIVEZ = 0x00004200, // CUBEMAP | POSITIVEZ
    CUBEMAP_FLAG_NEGATIVEZ = 0x00008200, // CUBEMAP | NEGATIVEZ
    CUBEMAP_FLAG_VOLUME    = 0x00200000, // VOLUME
};
const uint32_t kCubemapFlagAllFaces =
    CUBEMAP_FLAG_ISCUBEMAP |
    CUBEMAP_FLAG_POSITIVEX | CUBEMAP_FLAG_NEGATIVEX |
    CUBEMAP_FLAG_POSITIVEY | CUBEMAP_FLAG_NEGATIVEY |
    CUBEMAP_FLAG_POSITIVEZ | CUBEMAP_FLAG_NEGATIVEZ;

typedef enum
{
    DIMENSIONS_UNKNOWN   = 0,
    DIMENSIONS_BUFFER    = 1,
    DIMENSIONS_TEXTURE1D = 2,
    DIMENSIONS_TEXTURE2D = 3,
    DIMENSIONS_TEXTURE3D = 4,
} DdsDimensions;

typedef enum
{
    DX_FORMAT_UNKNOWN                     = 0,
    DX_FORMAT_R32G32B32A32_TYPELESS       = 1,
    DX_FORMAT_R32G32B32A32_FLOAT          = 2,
    DX_FORMAT_R32G32B32A32_UINT           = 3,
    DX_FORMAT_R32G32B32A32_SINT           = 4,
    DX_FORMAT_R32G32B32_TYPELESS          = 5,
    DX_FORMAT_R32G32B32_FLOAT             = 6,
    DX_FORMAT_R32G32B32_UINT              = 7,
    DX_FORMAT_R32G32B32_SINT              = 8,
    DX_FORMAT_R16G16B16A16_TYPELESS       = 9,
    DX_FORMAT_R16G16B16A16_FLOAT          = 10,
    DX_FORMAT_R16G16B16A16_UNORM          = 11,
    DX_FORMAT_R16G16B16A16_UINT           = 12,
    DX_FORMAT_R16G16B16A16_SNORM          = 13,
    DX_FORMAT_R16G16B16A16_SINT           = 14,
    DX_FORMAT_R32G32_TYPELESS             = 15,
    DX_FORMAT_R32G32_FLOAT                = 16,
    DX_FORMAT_R32G32_UINT                 = 17,
    DX_FORMAT_R32G32_SINT                 = 18,
    DX_FORMAT_R32G8X24_TYPELESS           = 19,
    DX_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
    DX_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
    DX_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
    DX_FORMAT_R10G10B10A2_TYPELESS        = 23,
    DX_FORMAT_R10G10B10A2_UNORM           = 24,
    DX_FORMAT_R10G10B10A2_UINT            = 25,
    DX_FORMAT_R11G11B10_FLOAT             = 26,
    DX_FORMAT_R8G8B8A8_TYPELESS           = 27,
    DX_FORMAT_R8G8B8A8_UNORM              = 28,
    DX_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
    DX_FORMAT_R8G8B8A8_UINT               = 30,
    DX_FORMAT_R8G8B8A8_SNORM              = 31,
    DX_FORMAT_R8G8B8A8_SINT               = 32,
    DX_FORMAT_R16G16_TYPELESS             = 33,
    DX_FORMAT_R16G16_FLOAT                = 34,
    DX_FORMAT_R16G16_UNORM                = 35,
    DX_FORMAT_R16G16_UINT                 = 36,
    DX_FORMAT_R16G16_SNORM                = 37,
    DX_FORMAT_R16G16_SINT                 = 38,
    DX_FORMAT_R32_TYPELESS                = 39,
    DX_FORMAT_D32_FLOAT                   = 40,
    DX_FORMAT_R32_FLOAT                   = 41,
    DX_FORMAT_R32_UINT                    = 42,
    DX_FORMAT_R32_SINT                    = 43,
    DX_FORMAT_R24G8_TYPELESS              = 44,
    DX_FORMAT_D24_UNORM_S8_UINT           = 45,
    DX_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
    DX_FORMAT_X24_TYPELESS_G8_UINT        = 47,
    DX_FORMAT_R8G8_TYPELESS               = 48,
    DX_FORMAT_R8G8_UNORM                  = 49,
    DX_FORMAT_R8G8_UINT                   = 50,
    DX_FORMAT_R8G8_SNORM                  = 51,
    DX_FORMAT_R8G8_SINT                   = 52,
    DX_FORMAT_R16_TYPELESS                = 53,
    DX_FORMAT_R16_FLOAT                   = 54,
    DX_FORMAT_D16_UNORM                   = 55,
    DX_FORMAT_R16_UNORM                   = 56,
    DX_FORMAT_R16_UINT                    = 57,
    DX_FORMAT_R16_SNORM                   = 58,
    DX_FORMAT_R16_SINT                    = 59,
    DX_FORMAT_R8_TYPELESS                 = 60,
    DX_FORMAT_R8_UNORM                    = 61,
    DX_FORMAT_R8_UINT                     = 62,
    DX_FORMAT_R8_SNORM                    = 63,
    DX_FORMAT_R8_SINT                     = 64,
    DX_FORMAT_A8_UNORM                    = 65,
    DX_FORMAT_R1_UNORM                    = 66,
    DX_FORMAT_R9G9B9E5_SHAREDEXP          = 67,
    DX_FORMAT_R8G8_B8G8_UNORM             = 68,
    DX_FORMAT_G8R8_G8B8_UNORM             = 69,
    DX_FORMAT_BC1_TYPELESS                = 70,
    DX_FORMAT_BC1_UNORM                   = 71,
    DX_FORMAT_BC1_UNORM_SRGB              = 72,
    DX_FORMAT_BC2_TYPELESS                = 73,
    DX_FORMAT_BC2_UNORM                   = 74,
    DX_FORMAT_BC2_UNORM_SRGB              = 75,
    DX_FORMAT_BC3_TYPELESS                = 76,
    DX_FORMAT_BC3_UNORM                   = 77,
    DX_FORMAT_BC3_UNORM_SRGB              = 78,
    DX_FORMAT_BC4_TYPELESS                = 79,
    DX_FORMAT_BC4_UNORM                   = 80,
    DX_FORMAT_BC4_SNORM                   = 81,
    DX_FORMAT_BC5_TYPELESS                = 82,
    DX_FORMAT_BC5_UNORM                   = 83,
    DX_FORMAT_BC5_SNORM                   = 84,
    DX_FORMAT_B5G6R5_UNORM                = 85,
    DX_FORMAT_B5G5R5A1_UNORM              = 86,
    DX_FORMAT_B8G8R8A8_UNORM              = 87,
    DX_FORMAT_B8G8R8X8_UNORM              = 88,
    DX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
    DX_FORMAT_B8G8R8A8_TYPELESS           = 90,
    DX_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
    DX_FORMAT_B8G8R8X8_TYPELESS           = 92,
    DX_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,
    DX_FORMAT_BC6H_TYPELESS               = 94,
    DX_FORMAT_BC6H_UF16                   = 95,
    DX_FORMAT_BC6H_SF16                   = 96,
    DX_FORMAT_BC7_TYPELESS                = 97,
    DX_FORMAT_BC7_UNORM                   = 98,
    DX_FORMAT_BC7_UNORM_SRGB              = 99,
} DxFormat;

static stbvk_inline bool stbvk__is_pf_mask(const DdsPixelFormat &pf, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    return (pf.maskR == r && pf.maskG == g && pf.maskB == b && pf.maskA == a);
}
static bool DDSPFtoVKFormat( const DdsPixelFormat& pf, uint32_t *outBlockSize, VkFormat *outFormat )
{
    if( pf.flags & PF_FLAGS_RGBA )
    {
        switch (pf.numBitsRGB)
        {
        case 32:
            *outBlockSize = 4;
            if( stbvk__is_pf_mask(pf, 0x00ff0000,0x0000ff00,0x000000ff,0xff000000) ) // BGRA
            {
                *outFormat = VK_FORMAT_B8G8R8A8_UNORM; return true;
            }
            else if( stbvk__is_pf_mask(pf, 0x00ff0000,0x0000ff00,0x000000ff,0x00000000) ) // BGRX
            {
                *outFormat = VK_FORMAT_B8G8R8A8_UNORM; return true;
            }
            else if( stbvk__is_pf_mask(pf, 0x000000ff,0x0000ff00,0x00ff0000,0xff000000) )
            {
                *outFormat = VK_FORMAT_R8G8B8A8_UNORM; return true;
            }
            else if( stbvk__is_pf_mask(pf, 0x000000ff,0x0000ff00,0x00ff0000,0x00000000) )
            {
                *outFormat = VK_FORMAT_R8G8B8A8_UNORM; return true;
            }

            // Note that many common DDS reader/writers swap the
            // the RED/BLUE masks for 10:10:10:2 formats. We assume
            // below that the 'correct' header mask is being used
            else if( stbvk__is_pf_mask(pf, 0x3ff00000,0x000ffc00,0x000003ff,0xc0000000) )
            {
                *outFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32; return true;
            }
            else if( stbvk__is_pf_mask(pf, 0x000003ff,0x000ffc00,0x3ff00000,0xc0000000) )
            {
                *outFormat = VK_FORMAT_A2R10G10B10_UNORM_PACK32; return true;
            }

            else if( stbvk__is_pf_mask(pf, 0xffffffff,0x00000000,0x00000000,0x00000000) )
            {
                *outFormat = VK_FORMAT_R32_UINT; return true;
            }
            else if( stbvk__is_pf_mask(pf, 0x0000ffff,0xffff0000,0x00000000,0x00000000) )
            {
                *outFormat = VK_FORMAT_R16G16_UNORM; return true;
            }
            break;

        case 24:
            *outBlockSize = 3;
            if( stbvk__is_pf_mask(pf, 0x00ff0000,0x0000ff00,0x000000ff,0x00000000) )
            {
                *outFormat = VK_FORMAT_R8G8B8_UNORM; return true;
            }
            break;

        case 16:
            *outBlockSize = 2;
            if( stbvk__is_pf_mask(pf, 0x0000f800,0x000007e0,0x0000001f,0x00000000) )
            {
                *outFormat = VK_FORMAT_R5G6B5_UNORM_PACK16; return true;
            }
            else if( stbvk__is_pf_mask(pf, 0x00007c00,0x000003e0,0x0000001f,0x00000000) )
            {
                *outFormat = VK_FORMAT_R5G5B5A1_UNORM_PACK16; return true;
            }
            else if( stbvk__is_pf_mask(pf, 0x00007c00,0x000003e0,0x0000001f,0x00008000) )
            {
                *outFormat = VK_FORMAT_B5G5R5A1_UNORM_PACK16; return true;
            }
            else if( stbvk__is_pf_mask(pf, 0x00000f00,0x000000f0,0x0000000f,0x0000f000) )
            {
                *outFormat = VK_FORMAT_R4G4B4A4_UNORM_PACK16; return true;
            }
            else if( stbvk__is_pf_mask(pf, 0x00000f00,0x000000f0,0x0000000f,0x00000000) )
            {
                *outFormat = VK_FORMAT_B4G4R4A4_UNORM_PACK16; return true;
            }
            break;

        case 8:
            *outBlockSize = 1;
            if( stbvk__is_pf_mask(pf, 0x000000ff,0x00000000,0x00000000,0x00000000) )
            {
                *outFormat = VK_FORMAT_R8_UNORM; return true;
            }
            break;
        }
    }
    else if( pf.flags & PF_FLAGS_LUMINANCE )
    {
        if( 8 == pf.numBitsRGB )
        {
            if( stbvk__is_pf_mask(pf, 0x000000ff,0x00000000,0x00000000,0x00000000) ) // L8
            {
                *outFormat = VK_FORMAT_R8_UNORM; *outBlockSize = 1; return true;
            }
        }
        else if( 16 == pf.numBitsRGB )
        {
            if( stbvk__is_pf_mask(pf, 0x0000ffff,0x00000000,0x00000000,0x00000000) ) // L16
            {
                *outFormat = VK_FORMAT_R16_UNORM; *outBlockSize = 2; return true;
            }
        }
    }
    else if( pf.flags & PF_FLAGS_ALPHA )
    {
    }
    else if( pf.flags & PF_FLAGS_CODE4 )
    {
        if(      stbvk__make_code4( 'D', 'X', 'T', '1' ) == pf.code4 )
        {
            *outFormat = VK_FORMAT_BC1_RGB_UNORM_BLOCK; *outBlockSize = 8; return true;
        }
        else if( stbvk__make_code4( 'D', 'X', 'T', '2' ) == pf.code4 )
        {
            *outFormat = VK_FORMAT_BC2_UNORM_BLOCK; *outBlockSize = 16; return true;
        }
        else if( stbvk__make_code4( 'D', 'X', 'T', '3' ) == pf.code4 )
        {
            *outFormat = VK_FORMAT_BC2_UNORM_BLOCK; *outBlockSize = 16; return true;
        }
        else if( stbvk__make_code4( 'D', 'X', 'T', '4' ) == pf.code4 )
        {
            *outFormat = VK_FORMAT_BC3_UNORM_BLOCK; *outBlockSize = 16; return true;
        }
        else if( stbvk__make_code4( 'D', 'X', 'T', '5' ) == pf.code4 )
        {
            *outFormat = VK_FORMAT_BC3_UNORM_BLOCK; *outBlockSize = 16; return true;
        }
        else if( stbvk__make_code4( 'B', 'C', '4', 'U' ) == pf.code4 )
        {
            *outFormat = VK_FORMAT_BC4_UNORM_BLOCK; *outBlockSize = 8; return true;
        }
        else if( stbvk__make_code4( 'B', 'C', '4', 'S' ) == pf.code4 )
        {
            *outFormat = VK_FORMAT_BC4_SNORM_BLOCK; *outBlockSize = 8; return true;
        }
        else if( stbvk__make_code4( 'B', 'C', '5', 'U' ) == pf.code4 )
        {
            *outFormat = VK_FORMAT_BC5_UNORM_BLOCK; *outBlockSize = 16; return true;
        }
        else if( stbvk__make_code4( 'B', 'C', '5', 'S' ) == pf.code4 )
        {
            *outFormat = VK_FORMAT_BC5_SNORM_BLOCK; *outBlockSize = 16; return true;
        }

        // Certain values are hard-coded into the FourCC field for specific formats
        else if ( 110 == pf.code4 )
        {
            *outFormat = VK_FORMAT_R16G16B16A16_SNORM; *outBlockSize = 8; return true;
        }
        else if ( 111 == pf.code4 )
        {
            *outFormat = VK_FORMAT_R16_SFLOAT; *outBlockSize = 2; return true;
        }
        else if ( 112 == pf.code4 )
        {
            *outFormat = VK_FORMAT_R16G16_SFLOAT; *outBlockSize = 4; return true;
        }
        else if ( 113 == pf.code4 )
        {
            *outFormat = VK_FORMAT_R16G16B16A16_SFLOAT; *outBlockSize = 8; return true;
        }
        else if ( 114 == pf.code4 )
        {
            *outFormat = VK_FORMAT_R32_SFLOAT; *outBlockSize = 4; return true;
        }
        if ( 115 == pf.code4 )
        {
            *outFormat = VK_FORMAT_R32G32_SFLOAT; *outBlockSize = 8; return true;
        }
        else if ( 116 == pf.code4 )
        {
            *outFormat = VK_FORMAT_R32G32B32A32_SFLOAT; *outBlockSize = 16; return true;
        }
        else if ( 36 == pf.code4 )
        {
            *outFormat = VK_FORMAT_R16G16B16A16_UINT; *outBlockSize = 8; return true;
        }
    }

    *outFormat = VK_FORMAT_UNDEFINED;
    *outBlockSize = 0;
    return false; // unknown/unsupported DDSPF format
}

static bool DXtoVKFormat(DxFormat dxFmt, uint32_t *outBlockSize, VkFormat *outFormat)
{
    switch(dxFmt)
    {
    case DX_FORMAT_UNKNOWN:
    case DX_FORMAT_R32G32B32A32_TYPELESS:
    case DX_FORMAT_R32G32B32_TYPELESS:
    case DX_FORMAT_R16G16B16A16_TYPELESS:
    case DX_FORMAT_R32G32_TYPELESS:
    case DX_FORMAT_R32G8X24_TYPELESS:
    case DX_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DX_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DX_FORMAT_R10G10B10A2_TYPELESS:
    case DX_FORMAT_R8G8B8A8_TYPELESS:
    case DX_FORMAT_R16G16_TYPELESS:
    case DX_FORMAT_R32_TYPELESS:
    case DX_FORMAT_R24G8_TYPELESS:
    case DX_FORMAT_R24_UNORM_X8_TYPELESS:
    case DX_FORMAT_X24_TYPELESS_G8_UINT:
    case DX_FORMAT_R8G8_TYPELESS:
    case DX_FORMAT_R16_TYPELESS:
    case DX_FORMAT_R8_TYPELESS:
    case DX_FORMAT_D32_FLOAT_S8X24_UINT:
    case DX_FORMAT_D24_UNORM_S8_UINT:
    case DX_FORMAT_R9G9B9E5_SHAREDEXP:
    case DX_FORMAT_R8G8_B8G8_UNORM:
    case DX_FORMAT_G8R8_G8B8_UNORM:
    case DX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DX_FORMAT_B8G8R8A8_TYPELESS:
    case DX_FORMAT_B8G8R8X8_TYPELESS:
    case DX_FORMAT_R1_UNORM:
    case DX_FORMAT_A8_UNORM:
        break;
    case DX_FORMAT_R32G32B32A32_FLOAT:
        *outFormat = VK_FORMAT_R32G32B32A32_SFLOAT; *outBlockSize = 16; return true;
    case DX_FORMAT_R32G32B32A32_UINT:
        *outFormat = VK_FORMAT_R32G32B32A32_UINT; *outBlockSize = 16; return true;
    case DX_FORMAT_R32G32B32A32_SINT:
        *outFormat = VK_FORMAT_R32G32B32A32_SINT; *outBlockSize = 16; return true;
    case DX_FORMAT_R32G32B32_FLOAT:
        *outFormat = VK_FORMAT_R32G32B32_SFLOAT; *outBlockSize = 12; return true;
    case DX_FORMAT_R32G32B32_UINT:
        *outFormat = VK_FORMAT_R32G32B32_UINT; *outBlockSize = 12; return true;
    case DX_FORMAT_R32G32B32_SINT:
        *outFormat = VK_FORMAT_R32G32B32_SINT; *outBlockSize = 16; return true;
    case DX_FORMAT_R16G16B16A16_FLOAT:
        *outFormat = VK_FORMAT_R16G16B16A16_SFLOAT; *outBlockSize = 8; return true;
    case DX_FORMAT_R16G16B16A16_UNORM:
        *outFormat = VK_FORMAT_R16G16B16A16_UNORM; *outBlockSize = 8; return true;
    case DX_FORMAT_R16G16B16A16_UINT:
        *outFormat = VK_FORMAT_R16G16B16A16_UINT; *outBlockSize = 8; return true;
    case DX_FORMAT_R16G16B16A16_SNORM:
        *outFormat = VK_FORMAT_R16G16B16A16_SNORM; *outBlockSize = 8; return true;
    case DX_FORMAT_R16G16B16A16_SINT:
        *outFormat = VK_FORMAT_R16G16B16A16_SINT; *outBlockSize = 8; return true;
    case DX_FORMAT_R32G32_FLOAT:
        *outFormat = VK_FORMAT_R32G32_SFLOAT; *outBlockSize = 8; return true;
    case DX_FORMAT_R32G32_UINT:
        *outFormat = VK_FORMAT_R32G32_UINT; *outBlockSize = 8; return true;
    case DX_FORMAT_R32G32_SINT:
        *outFormat = VK_FORMAT_R32G32_SINT; *outBlockSize = 8; return true;
    case DX_FORMAT_R10G10B10A2_UNORM:
        *outFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32; *outBlockSize = 4; return true;
    case DX_FORMAT_R10G10B10A2_UINT:
        *outFormat = VK_FORMAT_A2B10G10R10_UINT_PACK32; *outBlockSize = 4; return true;
    case DX_FORMAT_R11G11B10_FLOAT:
        *outFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32; *outBlockSize = 4; return true;
    case DX_FORMAT_R8G8B8A8_UNORM:
        *outFormat = VK_FORMAT_R8G8B8A8_UNORM; *outBlockSize = 4; return true;
    case DX_FORMAT_R8G8B8A8_UNORM_SRGB:
        *outFormat = VK_FORMAT_R8G8B8A8_SRGB; *outBlockSize = 4; return true;
    case DX_FORMAT_R8G8B8A8_UINT:
        *outFormat = VK_FORMAT_R8G8B8A8_UINT; *outBlockSize = 4; return true;
    case DX_FORMAT_R8G8B8A8_SNORM:
        *outFormat = VK_FORMAT_R8G8B8A8_SNORM; *outBlockSize = 4; return true;
    case DX_FORMAT_R8G8B8A8_SINT:
        *outFormat = VK_FORMAT_R8G8B8A8_SINT; *outBlockSize = 4; return true;
    case DX_FORMAT_R16G16_FLOAT:
        *outFormat = VK_FORMAT_R16G16_SFLOAT; *outBlockSize = 4; return true;
    case DX_FORMAT_R16G16_UNORM:
        *outFormat = VK_FORMAT_R16G16_UNORM; *outBlockSize = 4; return true;
    case DX_FORMAT_R16G16_UINT:
        *outFormat = VK_FORMAT_R16G16_UINT; *outBlockSize = 4; return true;
    case DX_FORMAT_R16G16_SNORM:
        *outFormat = VK_FORMAT_R16G16_SNORM; *outBlockSize = 4; return true;
    case DX_FORMAT_R16G16_SINT:
        *outFormat = VK_FORMAT_R16G16_SINT; *outBlockSize = 4; return true;
    case DX_FORMAT_D32_FLOAT:
        *outFormat = VK_FORMAT_D32_SFLOAT; *outBlockSize = 4; return true;
    case DX_FORMAT_R32_FLOAT:
        *outFormat = VK_FORMAT_R32_SFLOAT; *outBlockSize = 4; return true;
    case DX_FORMAT_R32_UINT:
        *outFormat = VK_FORMAT_R32_UINT; *outBlockSize = 4; return true;
    case DX_FORMAT_R32_SINT:
        *outFormat = VK_FORMAT_R32_SINT; *outBlockSize = 4; return true;
    case DX_FORMAT_R8G8_UNORM:
        *outFormat = VK_FORMAT_R8G8_UNORM; *outBlockSize = 2; return true;
    case DX_FORMAT_R8G8_UINT:
        *outFormat = VK_FORMAT_R8G8_UINT; *outBlockSize = 2; return true;
    case DX_FORMAT_R8G8_SNORM:
        *outFormat = VK_FORMAT_R8G8_SNORM; *outBlockSize = 2; return true;
    case DX_FORMAT_R8G8_SINT:
        *outFormat = VK_FORMAT_R8G8_SINT; *outBlockSize = 2; return true;
    case DX_FORMAT_R16_FLOAT:
        *outFormat = VK_FORMAT_R16_SFLOAT; *outBlockSize = 2; return true;
    case DX_FORMAT_D16_UNORM:
        *outFormat = VK_FORMAT_D16_UNORM; *outBlockSize = 2; return true;
    case DX_FORMAT_R16_UNORM:
        *outFormat = VK_FORMAT_R16_UNORM; *outBlockSize = 2; return true;
    case DX_FORMAT_R16_UINT:
        *outFormat = VK_FORMAT_R16_UINT; *outBlockSize = 2; return true;
    case DX_FORMAT_R16_SNORM:
        *outFormat = VK_FORMAT_R16_SNORM; *outBlockSize = 2; return true;
    case DX_FORMAT_R16_SINT:
        *outFormat = VK_FORMAT_R16_SINT; *outBlockSize = 2; return true;
    case DX_FORMAT_R8_UNORM:
        *outFormat = VK_FORMAT_R8_UNORM; *outBlockSize = 1; return true;
    case DX_FORMAT_R8_UINT:
        *outFormat = VK_FORMAT_R8_UINT; *outBlockSize = 1; return true;
    case DX_FORMAT_R8_SNORM:
        *outFormat = VK_FORMAT_R8_SNORM; *outBlockSize = 1; return true;
    case DX_FORMAT_R8_SINT:
        *outFormat = VK_FORMAT_R8_SINT; *outBlockSize = 1; return true;
    case DX_FORMAT_BC1_TYPELESS:
    case DX_FORMAT_BC1_UNORM:
        *outFormat = VK_FORMAT_BC1_RGBA_UNORM_BLOCK; *outBlockSize = 8; return true;
    case DX_FORMAT_BC1_UNORM_SRGB:
        *outFormat = VK_FORMAT_BC1_RGBA_SRGB_BLOCK; *outBlockSize = 8; return true;
    case DX_FORMAT_BC2_TYPELESS:
    case DX_FORMAT_BC2_UNORM:
        *outFormat = VK_FORMAT_BC2_UNORM_BLOCK; *outBlockSize = 16; return true;
    case DX_FORMAT_BC2_UNORM_SRGB:
        *outFormat = VK_FORMAT_BC2_SRGB_BLOCK; *outBlockSize = 16; return true;
    case DX_FORMAT_BC3_TYPELESS:
    case DX_FORMAT_BC3_UNORM:
        *outFormat = VK_FORMAT_BC3_UNORM_BLOCK; *outBlockSize = 16; return true;
    case DX_FORMAT_BC3_UNORM_SRGB:
        *outFormat = VK_FORMAT_BC3_SRGB_BLOCK; *outBlockSize = 16; return true;
    case DX_FORMAT_BC4_TYPELESS:
    case DX_FORMAT_BC4_UNORM:
        *outFormat = VK_FORMAT_BC4_UNORM_BLOCK; *outBlockSize = 8; return true;
    case DX_FORMAT_BC4_SNORM:
        *outFormat = VK_FORMAT_BC4_SNORM_BLOCK; *outBlockSize = 8; return true;
    case DX_FORMAT_BC5_TYPELESS:
    case DX_FORMAT_BC5_UNORM:
        *outFormat = VK_FORMAT_BC5_UNORM_BLOCK; *outBlockSize = 16; return true;
    case DX_FORMAT_BC5_SNORM:
        *outFormat = VK_FORMAT_BC5_SNORM_BLOCK; *outBlockSize = 16; return true;
    case DX_FORMAT_BC6H_UF16:
        *outFormat = VK_FORMAT_BC6H_UFLOAT_BLOCK; *outBlockSize = 16; return true;
    case DX_FORMAT_BC6H_SF16:
        *outFormat = VK_FORMAT_BC6H_SFLOAT_BLOCK; *outBlockSize = 16; return true;
    case DX_FORMAT_BC7_UNORM:
        *outFormat = VK_FORMAT_BC7_UNORM_BLOCK; *outBlockSize = 16; return true;
    case DX_FORMAT_BC7_UNORM_SRGB:
        *outFormat = VK_FORMAT_BC7_SRGB_BLOCK; *outBlockSize = 16; return true;
    case DX_FORMAT_B5G6R5_UNORM:
        *outFormat = VK_FORMAT_R5G6B5_UNORM_PACK16; *outBlockSize = 2; return true;
    case DX_FORMAT_B5G5R5A1_UNORM:
        *outFormat = VK_FORMAT_B5G5R5A1_UNORM_PACK16; *outBlockSize = 2; return true;
    case DX_FORMAT_B8G8R8A8_UNORM:
    case DX_FORMAT_B8G8R8X8_UNORM:
        *outFormat = VK_FORMAT_B8G8R8A8_UNORM; *outBlockSize = 4; return true;
    case DX_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DX_FORMAT_B8G8R8X8_UNORM_SRGB:
        *outFormat = VK_FORMAT_B8G8R8A8_SRGB; *outBlockSize = 4; return true;
    default:
        break;
    }

    *outFormat = VK_FORMAT_UNDEFINED;
    *outBlockSize = 4;
    return false; // unknown/unsupported format
}

struct DdsHeader
{
    uint32_t structSize;
    DdsHeaderFlag flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth; // only if HEADER_FLAGS_VOLUME is set in flags
    uint32_t mipCount;
    uint32_t unused1[11];
    DdsPixelFormat pixelFormat;
    uint32_t caps;
    uint32_t caps2;
    uint32_t unused2[3];
};

struct DdsHeader10
{
    DxFormat dxgiFormat;
    DdsDimensions resourceDimension;
    uint32_t flag;
    uint32_t arraySize;
    uint32_t unused;
};

static bool ContainsCompressedTexture(const DdsHeader *header, const DdsHeader10 *header10)
{
    if (header10 != NULL)
    {
        switch(header10->dxgiFormat)
        {
        case DX_FORMAT_BC1_TYPELESS:
        case DX_FORMAT_BC1_UNORM:
        case DX_FORMAT_BC1_UNORM_SRGB:
        case DX_FORMAT_BC2_TYPELESS:
        case DX_FORMAT_BC2_UNORM:
        case DX_FORMAT_BC2_UNORM_SRGB:
        case DX_FORMAT_BC3_TYPELESS:
        case DX_FORMAT_BC3_UNORM:
        case DX_FORMAT_BC3_UNORM_SRGB:
        case DX_FORMAT_BC4_TYPELESS:
        case DX_FORMAT_BC4_UNORM:
        case DX_FORMAT_BC4_SNORM:
        case DX_FORMAT_BC5_TYPELESS:
        case DX_FORMAT_BC5_UNORM:
        case DX_FORMAT_BC5_SNORM:
        case DX_FORMAT_BC6H_UF16:
        case DX_FORMAT_BC6H_SF16:
        case DX_FORMAT_BC7_UNORM:
        case DX_FORMAT_BC7_UNORM_SRGB:
            return true;
        default:
            return false;
        }
    }
    else if( header->pixelFormat.flags & PF_FLAGS_CODE4 )
    {
        return
            stbvk__make_code4( 'D', 'X', 'T', '1' ) == header->pixelFormat.code4 ||
            stbvk__make_code4( 'D', 'X', 'T', '2' ) == header->pixelFormat.code4 ||
            stbvk__make_code4( 'D', 'X', 'T', '3' ) == header->pixelFormat.code4 ||
            stbvk__make_code4( 'D', 'X', 'T', '4' ) == header->pixelFormat.code4 ||
            stbvk__make_code4( 'D', 'X', 'T', '5' ) == header->pixelFormat.code4 ||
            stbvk__make_code4( 'B', 'C', '4', 'U' ) == header->pixelFormat.code4 ||
            stbvk__make_code4( 'B', 'C', '4', 'S' ) == header->pixelFormat.code4 ||
            stbvk__make_code4( 'B', 'C', '5', 'U' ) == header->pixelFormat.code4 ||
            stbvk__make_code4( 'B', 'C', '5', 'S' ) == header->pixelFormat.code4 ||
            stbvk__make_code4( 'A', 'T', 'I', '1' ) == header->pixelFormat.code4 ||
            stbvk__make_code4( 'A', 'T', 'I', '2' ) == header->pixelFormat.code4;
    }
    return false;
}

STBVKDEF int stbvk_image_load_from_dds_file(stbvk_context const *context, char const *dds_file_path, stbvk_image *out_image)
{
    FILE *dds_file = stbvk__fopen(dds_file_path, "rb");
    if (dds_file == NULL)
        return -1; // File load error
    fseek(dds_file, 0, SEEK_END);
    size_t dds_file_size = ftell(dds_file);
    fseek(dds_file, 0, SEEK_SET);
    if (dds_file_size < sizeof(DdsHeader)+sizeof(uint32_t))
    {
        fclose(dds_file);
        return -2; // File too small to contain a valid DDS
    }
    uint8_t *dds_file_data = (uint8_t*)STBVK_MALLOC(dds_file_size);
    if (fread(dds_file_data, dds_file_size, 1, dds_file) != 1)
    {
        fclose(dds_file);
        free(dds_file_data);
        return -3; // fread size mismatch
    }
    fclose(dds_file);

    int retval = stbvk_image_load_from_dds_buffer(context, dds_file_data, dds_file_size, out_image);
    free(dds_file_data);
    return retval;
}

STBVKDEF int stbvk_image_load_from_dds_buffer(stbvk_context const *context, void const *dds_file_data, size_t dds_file_size, stbvk_image *out_image)
{
    const uint8_t *dds_bytes = (const uint8_t*)dds_file_data;

    // Check magic number and header validity
    const uint32_t *magic = (const uint32_t*)dds_bytes;
    if (*magic != kDdsPrefixMagic)
    {
        return -4; // Incorrect magic number
    }
    const DdsHeader *header = (const DdsHeader*)(dds_bytes + sizeof(uint32_t));
    if (header->structSize != sizeof(DdsHeader) || header->pixelFormat.structSize != sizeof(DdsPixelFormat))
    {
        return -5; // Incorrect header size
    }
    if ((header->flags & (HEADER_FLAGS_WIDTH | HEADER_FLAGS_HEIGHT)) != (HEADER_FLAGS_WIDTH | HEADER_FLAGS_HEIGHT))
    {
        // technically DDSD_CAPS and DDSD_PIXELFORMAT are required as well, but their absence is so widespread that they can't be relied upon.
        return -6; // Required flag is missing from header
    }

    // Note according to msdn:  when you read a .dds file, you should not rely on the DDSCAPS_TEXTURE
    //	and DDSCAPS_COMPLEX flags being set because some writers of such a file might not set these flags.
    //if ((header->caps & SURFACE_FLAGS_TEXTURE) == 0)
    //{
    //	free(ddsFileData);
    //	return -7; // Required flag is missing from header
    //}
    uint32_t pixel_offset = sizeof(uint32_t) + sizeof(DdsHeader);

    // Check for DX10 header
    const DdsHeader10 *header10 = NULL;
    if ( (header->pixelFormat.flags & PF_FLAGS_CODE4) && (stbvk__make_code4( 'D', 'X', '1', '0' ) == header->pixelFormat.code4) )
    {
        // Must be long enough for both headers and magic value
        if( dds_file_size < (sizeof(DdsHeader)+sizeof(uint32_t)+sizeof(DdsHeader10)) )
        {
            return -8; // File too small to contain a valid DX10 DDS
        }
        header10 = (const DdsHeader10*)(dds_bytes + sizeof(uint32_t) + sizeof(DdsHeader));
        pixel_offset += sizeof(DdsHeader10);
    }

    // Check if the contents are a cubemap.  If so, all six faces must be present.
    bool is_cube_map = false;
    if ((header->caps & SURFACE_FLAGS_COMPLEX) && (header->caps2 & CUBEMAP_FLAG_ISCUBEMAP))
    {
        if ((header->caps2 & kCubemapFlagAllFaces) != kCubemapFlagAllFaces)
        {
            return -9; // The cubemap is missing one or more faces.
        }
        is_cube_map = true;
    }

    // Check if the contents are a volume texture.
    bool is_volume_texture = false;
    if ((header->flags & HEADER_FLAGS_DEPTH) && (header->caps2 & CUBEMAP_FLAG_VOLUME)) // (header->dwCaps & SURFACE_FLAGS_COMPLEX) -- doesn't always seem to be set?
    {
        if (header->depth == 0)
        {
            return -10; // The file is marked as a volume texture, but depth is <1
        }
        is_volume_texture = true;
    }

    bool is_compressed = ContainsCompressedTexture(header, header10);

    uint32_t mipMapCount = 1;
    if ((header->flags & HEADER_FLAGS_MIPMAP) == HEADER_FLAGS_MIPMAP)
    {
        mipMapCount = header->mipCount;
    }

    // Begin VK-specific code!
    uint32_t block_size = 0;
    VkFormat vk_format = VK_FORMAT_UNDEFINED;
    if (header10 != NULL)
    {
        DXtoVKFormat(header10->dxgiFormat, &block_size, &vk_format);
    }
    else
    {
        DDSPFtoVKFormat(header->pixelFormat, &block_size, &vk_format);
    }
    if (vk_format == VK_FORMAT_UNDEFINED)
    {
        return -11; // It is either unknown or unsupported format
    }
    stbvk_image_create_info create_info = {};
    //create_info.image_type = 0;
    create_info.format = vk_format;
    create_info.extent.width  = stbvk__max(1U, header->width);
    create_info.extent.height = stbvk__max(1U, header->height);
    create_info.extent.depth  = stbvk__max(1U, header->depth);
    create_info.mip_levels = header->mipCount;
    create_info.array_layers = header10 ? header10->arraySize : 1;
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT; // TODO(cort): generalize
    create_info.initial_layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    create_info.memory_properties_mask = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    //create_info.view_type = 0;
    const uint8_t *next_src_surface = dds_bytes + pixel_offset;
if (is_volume_texture)
    {
#if 0
        glTarget = GL_TEXTURE_3D;
        // TODO
#endif
    }
    else
    {
        if (is_cube_map)
        {
            create_info.image_type = VK_IMAGE_TYPE_2D;
            create_info.view_type = (create_info.array_layers > 1) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
            create_info.array_layers *= 6;
        }
        else if (create_info.extent.height == 1)
        {
            create_info.image_type = VK_IMAGE_TYPE_1D;
            create_info.view_type = (create_info.array_layers > 1) ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        }
        else
        {
            create_info.image_type = VK_IMAGE_TYPE_2D;
            create_info.view_type = (create_info.array_layers > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        }
        stbvk_image_create(context, &create_info, out_image);
        for(uint32_t iLayer=0; iLayer<create_info.array_layers; ++iLayer)
        {
            for(uint32_t iMip=0; iMip<create_info.mip_levels; ++iMip)
            {
                uint32_t mip_width  = stbvk__max(header->width >> iMip, 1U);
                uint32_t mip_height = stbvk__max(header->height >> iMip, 1U);
                uint32_t mip_pitch  = is_compressed ? ((mip_width+3)/4)*block_size : mip_width*block_size;
                uint32_t num_rows = is_compressed ? ((mip_height+3)/4) : mip_height;
                uint32_t surface_size = mip_pitch*num_rows;
                STBVK_ASSERT(next_src_surface + surface_size <= dds_bytes + dds_file_size);
                VkImageSubresource subresource = {};
                subresource.arrayLayer = iLayer;
                subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                subresource.mipLevel = iMip;
                VkSubresourceLayout subresource_layout = {};
                STBVK__CHECK( stbvk_image_get_subresource_source_layout(context, out_image, subresource, &subresource_layout) );
                uint32_t *padded_pixels = (uint32_t*)STBVK_MALLOC(subresource_layout.size);
                STBVK_ASSERT(mip_pitch <= subresource_layout.rowPitch);
                for(uint32_t iY=0; iY<num_rows; iY+=1)
                {
                    const void *src_row = (void*)( intptr_t(next_src_surface) + iY*mip_pitch );
                    void *dst_row = (void*)( (intptr_t)padded_pixels + iY*subresource_layout.rowPitch );
                    memcpy(dst_row, src_row, mip_pitch);
                }
                STBVK__CHECK( stbvk_image_load_subresource(context, out_image, subresource, subresource_layout,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, padded_pixels) );
                STBVK_FREE(padded_pixels);
                next_src_surface += surface_size;
            }
        }
    }
    return 0;
}

STBVKDEF int stbvk_prepare_graphics_pipeline_create_info_vsps(
    stbvk_graphics_pipeline_settings_vsps const *settings,
    stbvk_graphics_pipeline_create_info *out_create_info)
{
    *out_create_info = {};

    out_create_info->shader_stage_create_infos[0] = {};
    out_create_info->shader_stage_create_infos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    out_create_info->shader_stage_create_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    out_create_info->shader_stage_create_infos[0].module = settings->vertex_shader;
    out_create_info->shader_stage_create_infos[0].pName = "main";
    out_create_info->shader_stage_create_infos[1] = {};
    out_create_info->shader_stage_create_infos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    out_create_info->shader_stage_create_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    out_create_info->shader_stage_create_infos[1].module = settings->fragment_shader;
    out_create_info->shader_stage_create_infos[1].pName = "main";

    out_create_info->vertex_input_state_create_info = {};
    out_create_info->vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    out_create_info->vertex_input_state_create_info.pNext = NULL;
    out_create_info->vertex_input_state_create_info.flags = 0;
    out_create_info->vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
    out_create_info->vertex_input_state_create_info.pVertexBindingDescriptions = out_create_info->vertex_input_binding_descriptions;
    STBVK_ASSERT(settings->vertex_buffer_layout.attribute_count <=
        sizeof(out_create_info->vertex_input_attribute_descriptions) / sizeof(out_create_info->vertex_input_attribute_descriptions[0]));
    out_create_info->vertex_input_state_create_info.vertexAttributeDescriptionCount = settings->vertex_buffer_layout.attribute_count;
    out_create_info->vertex_input_state_create_info.pVertexAttributeDescriptions = out_create_info->vertex_input_attribute_descriptions;
    out_create_info->vertex_input_binding_descriptions[0] = {};
    out_create_info->vertex_input_binding_descriptions[0].binding = 0;
    out_create_info->vertex_input_binding_descriptions[0].stride = settings->vertex_buffer_layout.stride;
    out_create_info->vertex_input_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    for(uint32_t iAttr=0; iAttr<settings->vertex_buffer_layout.attribute_count; ++iAttr)
    {
        out_create_info->vertex_input_attribute_descriptions[iAttr] = settings->vertex_buffer_layout.attributes[iAttr];
        out_create_info->vertex_input_attribute_descriptions[iAttr].binding = out_create_info->vertex_input_binding_descriptions[0].binding;
    }

    out_create_info->input_assembly_state_create_info = {};
    out_create_info->input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    out_create_info->input_assembly_state_create_info.pNext = NULL;
    out_create_info->input_assembly_state_create_info.flags = 0;
    out_create_info->input_assembly_state_create_info.topology = settings->primitive_topology;

    out_create_info->tessellation_state_create_info = {};
    out_create_info->tessellation_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    out_create_info->tessellation_state_create_info.pNext = NULL;
    out_create_info->tessellation_state_create_info.flags = 0;

    out_create_info->viewport_state_create_info = {};
    out_create_info->viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    out_create_info->viewport_state_create_info.pNext = NULL;
    out_create_info->viewport_state_create_info.flags = 0;
    out_create_info->viewport_state_create_info.viewportCount = 1;
    out_create_info->viewport_state_create_info.pViewports = out_create_info->viewports;
    out_create_info->viewport_state_create_info.scissorCount = 1;
    out_create_info->viewport_state_create_info.pScissors = out_create_info->scissor_rects;
    out_create_info->viewports[0] = settings->viewport;
    out_create_info->scissor_rects[0] = settings->scissor_rect;

    out_create_info->rasterization_state_create_info = {};
    out_create_info->rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    out_create_info->rasterization_state_create_info.pNext = NULL;
    out_create_info->rasterization_state_create_info.flags = 0;
    out_create_info->rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    out_create_info->rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
    out_create_info->rasterization_state_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    out_create_info->rasterization_state_create_info.depthClampEnable = VK_FALSE;
    out_create_info->rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
    out_create_info->rasterization_state_create_info.depthBiasEnable = VK_FALSE;
    out_create_info->rasterization_state_create_info.lineWidth = 1.0f;

    out_create_info->multisample_state_create_info = {};
    out_create_info->multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    out_create_info->multisample_state_create_info.pNext = NULL;
    out_create_info->multisample_state_create_info.flags = 0;
    out_create_info->multisample_state_create_info.pSampleMask = NULL;
    out_create_info->multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    out_create_info->multisample_state_create_info.sampleShadingEnable = VK_FALSE;

    out_create_info->depth_stencil_state_create_info = {};
    out_create_info->depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    out_create_info->depth_stencil_state_create_info.pNext = NULL;
    out_create_info->depth_stencil_state_create_info.flags = 0;
    out_create_info->depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
    out_create_info->depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
    out_create_info->depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    out_create_info->depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
    out_create_info->depth_stencil_state_create_info.back = {};
    out_create_info->depth_stencil_state_create_info.back.failOp = VK_STENCIL_OP_KEEP;
    out_create_info->depth_stencil_state_create_info.back.passOp = VK_STENCIL_OP_KEEP;
    out_create_info->depth_stencil_state_create_info.back.compareOp = VK_COMPARE_OP_ALWAYS;
    out_create_info->depth_stencil_state_create_info.front = {};
    out_create_info->depth_stencil_state_create_info.front.failOp = VK_STENCIL_OP_KEEP;
    out_create_info->depth_stencil_state_create_info.front.passOp = VK_STENCIL_OP_KEEP;
    out_create_info->depth_stencil_state_create_info.front.compareOp = VK_COMPARE_OP_ALWAYS;
    out_create_info->depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;

    out_create_info->color_blend_state_create_info = {};
    out_create_info->color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    out_create_info->color_blend_state_create_info.pNext = NULL;
    out_create_info->color_blend_state_create_info.flags = 0;
    out_create_info->color_blend_state_create_info.attachmentCount = settings->subpass_color_attachment_count;
    out_create_info->color_blend_state_create_info.pAttachments = out_create_info->color_blend_attachment_states;
    for(uint32_t iCA=0; iCA<settings->subpass_color_attachment_count; ++iCA)
    {
        out_create_info->color_blend_attachment_states[iCA] = {};
        out_create_info->color_blend_attachment_states[iCA].colorWriteMask = 0xF;
        out_create_info->color_blend_attachment_states[iCA].blendEnable = VK_FALSE;
        //out_create_info->color_blend_attachment_states[iCA].colorBlendOp = VK_BLEND_OP_ADD;
    }

    out_create_info->dynamic_state_create_info = {};
    out_create_info->dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    out_create_info->dynamic_state_create_info.pNext = NULL;
    out_create_info->dynamic_state_create_info.flags = 0;
    out_create_info->dynamic_state_create_info.dynamicStateCount = 0;
    out_create_info->dynamic_state_create_info.pDynamicStates = out_create_info->dynamic_states;
    for(int iDS=VK_DYNAMIC_STATE_BEGIN_RANGE; iDS<=VK_DYNAMIC_STATE_END_RANGE; ++iDS)
    {
        if (settings->dynamic_state_mask & (1<<iDS))
        {
            out_create_info->dynamic_states[out_create_info->dynamic_state_create_info.dynamicStateCount++] = (VkDynamicState)iDS;
        }
    }
    STBVK_ASSERT(out_create_info->dynamic_state_create_info.dynamicStateCount <= VK_DYNAMIC_STATE_RANGE_SIZE);

    out_create_info->graphics_pipeline_create_info = {};
    out_create_info->graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    out_create_info->graphics_pipeline_create_info.pNext = NULL;
    out_create_info->graphics_pipeline_create_info.flags = 0;
    out_create_info->graphics_pipeline_create_info.layout = settings->pipeline_layout;
    out_create_info->graphics_pipeline_create_info.stageCount = 2;
    out_create_info->graphics_pipeline_create_info.pStages = out_create_info->shader_stage_create_infos;
    out_create_info->graphics_pipeline_create_info.pVertexInputState = &out_create_info->vertex_input_state_create_info;
    out_create_info->graphics_pipeline_create_info.pInputAssemblyState = &out_create_info->input_assembly_state_create_info;
    out_create_info->graphics_pipeline_create_info.pRasterizationState = &out_create_info->rasterization_state_create_info;
    out_create_info->graphics_pipeline_create_info.pColorBlendState = &out_create_info->color_blend_state_create_info;
    out_create_info->graphics_pipeline_create_info.pMultisampleState = &out_create_info->multisample_state_create_info;
    out_create_info->graphics_pipeline_create_info.pViewportState = &out_create_info->viewport_state_create_info;
    out_create_info->graphics_pipeline_create_info.pDepthStencilState = &out_create_info->depth_stencil_state_create_info;
    out_create_info->graphics_pipeline_create_info.renderPass = settings->render_pass;
    out_create_info->graphics_pipeline_create_info.subpass = settings->subpass;
    out_create_info->graphics_pipeline_create_info.pDynamicState = &out_create_info->dynamic_state_create_info;
    out_create_info->graphics_pipeline_create_info.pTessellationState = NULL;

    return 0;
}



#endif // STB_VULKAN_IMPLEMENTATION
