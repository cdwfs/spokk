/* cds_vulkan - v0.01 - public domain Vulkan helper
                                     no warranty implied; use at your own risk

   Do this:
      #define CDS_VULKAN_IMPLEMENTATION
   before you include this file in *one* C++ file to create the implementation.

   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define CDS_VULKAN_IMPLEMENTATION
   #include "cds_vulkan.hpp"

   You can #define CDSVK_ASSERT(x) before the #include to avoid using assert.h.

LICENSE

This software is in the public domain. Where that dedication is not
recognized, you are granted a perpetual, irrevocable license to copy,
distribute, and modify this file as you see fit.

*/

#ifndef CDSVK_INCLUDE_CDS_VULKAN_HPP
#define CDSVK_INCLUDE_CDS_VULKAN_HPP

#include <vulkan/vulkan.hpp>
#include <vector>

#ifndef CDSVK_NO_STDIO
#   include <stdio.h>
#endif // CDSVK_NO_STDIO

#define CDSVK_VERSION 1

typedef unsigned char cdsvk_uc;

#ifdef CDS_VULKAN_STATIC
#   define CDSVKDEF static
#else
#   define CDSVKDEF extern
#endif

//////////////////////////////////////////////////////////////////////////////
//
// PUBLIC API
//
namespace cdsvk {
    typedef VkSurfaceKHR FN_GetVkSurface(VkInstance instance, const VkAllocationCallbacks *allocation_callbacks, void *userdata);

    struct ContextCreateInfo {
        vk::AllocationCallbacks *allocation_callbacks;

        std::vector<const std::string> required_instance_layer_names;
        std::vector<const std::string> required_instance_extension_names;
        std::vector<const std::string> required_device_extension_names;
        std::vector<const std::string> optional_instance_layer_names;
        std::vector<const std::string> optional_instance_extension_names;
        std::vector<const std::string> optional_device_extension_names;

        // If non-NULL, this function will be called after VkInstance initialization to retrieve (and possibly create)
        // a VkSurfaceKHR to present to.
        // If NULL, initialization of presentation-related features (swapchain, present queue, etc.) will be skipped.
        // This would be appropriate for headless/compute-only applications, or in cases where some other library is
        // managing the swapchain/presentation functionality.
        FN_GetVkSurface *pfn_get_vk_surface;
        void *get_vk_surface_userdata;

        const vk::ApplicationInfo *application_info; // Used to initialize VkInstance. Optional; set to NULL for default values.
        PFN_vkDebugReportCallbackEXT debug_report_callback; // Optional; set to NULL to disable debug reports.
        vk::DebugReportFlagsEXT debug_report_flags; // Optional; ignored if debug_report_callback is NULL.
        void *debug_report_callback_user_data; // Optional; passed to debug_report_callback, if enabled.
    };

    class Context {
    public:
        explicit Context(const ContextCreateInfo &context_ci);
        ~Context();

        vk::Instance instance() const { return instance_; }
        vk::PhysicalDevice physical_device() const { return physical_device_; }
        vk::Device device() const { return device_; }

        bool is_instance_layer_enabled(const std::string& layer_name) const;
        bool is_instance_extension_enabled(const std::string& ext_name) const;
        bool is_device_extension_enabled(const std::string& ext_name) const;
    private:
        const vk::AllocationCallbacks *allocation_callbacks_;

        vk::Instance instance_;
        vk::DebugReportCallbackEXT debug_report_callback_;

        vk::PhysicalDevice physical_device_;
        vk::PhysicalDeviceProperties physical_device_properties_;
        vk::PhysicalDeviceMemoryProperties physical_device_memory_properties_;
        vk::PhysicalDeviceFeatures physical_device_features_;
        vk::Device device_;

        uint32_t graphics_queue_family_index_;
        vk::QueueFamilyProperties graphics_queue_family_properties_;

        vk::Queue graphics_queue_;

        vk::PipelineCache pipeline_cache_;

        // These members are only used if a present surface is passed at init time.
        vk::SurfaceKHR present_surface_;
        vk::Queue present_queue_;
        uint32_t present_queue_family_index_;
        vk::QueueFamilyProperties present_queue_family_properties_;
        vk::SwapchainKHR swapchain_;
        vk::SurfaceFormatKHR swapchain_surface_format_;
        std::vector<vk::Image> swapchain_images_;
        std::vector<vk::ImageView> swapchain_image_views_;

        std::vector<vk::LayerProperties> enabled_instance_layers_;
        std::vector<vk::ExtensionProperties> enabled_instance_extensions_;
        std::vector<vk::ExtensionProperties> enabled_device_extensions_;
    };
}  // namespace cdsvk

//
//
////   end header file   /////////////////////////////////////////////////////
#endif // CDSVK_INCLUDE_CDS_VULKAN_HPP

#if defined(CDS_VULKAN_IMPLEMENTATION)

#ifndef CDSVK_NO_STDIO
#   include <stdio.h>
#endif

#ifndef CDSVK_ASSERT
#   include <assert.h>
#   define CDSVK_ASSERT(x) assert(x)
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1700)
typedef UINT16 cdsvk__uint16;
typedef  INT16 cdsvk__int16;
typedef UINT32 cdsvk__uint32;
typedef  INT32 cdsvk__int32;
#else
#include <stdint.h>
typedef uint16_t cdsvk__uint16;
typedef int16_t  cdsvk__int16;
typedef uint32_t cdsvk__uint32;
typedef int32_t  cdsvk__int32;
#endif
// should produce compiler error if size is wrong
static_assert(sizeof(cdsvk__uint16) == 2, "sizeof(cdsvk__uint16) != 2");
static_assert(sizeof(cdsvk__int16)  == 2, "sizeof(cdsvk__int16) != 2");
static_assert(sizeof(cdsvk__uint32) == 4, "sizeof(cdsvk__uint32) != 4");
static_assert(sizeof(cdsvk__int32)  == 4, "sizeof(cdsvk__int32) != 4");

#ifdef _MSC_VER
#   define CDSVK_NOTUSED(v)  (void)(v)
#else
#   define CDSVK_NOTUSED(v)  (void)sizeof(v)
#endif

#if !defined(CDSVK_LOG)
# if !defined(CDSVK_NO_STDIO)
#   include <stdarg.h>
static void cdsvk__log_default(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}
#   define CDSVK_LOG(...) cdsvk__log_default(__VA_ARGS__)
# else
#   define CDSVK_LOG(...) (void)(__VA_ARGS)__)
# endif
#endif

#ifndef CDSVK_REALLOC_SIZED
#   define CDSVK_REALLOC_SIZED(p,oldsz,newsz) CDSVK_REALLOC(p,newsz)
#endif

// x86/x64 detection
#if defined(__x86_64__) || defined(_M_X64)
#   define CDSVK__X64_TARGET
#elif defined(__i386) || defined(_M_IX86)
#   define CDSVK__X86_TARGET
#endif

// TODO(cort): proper return-value test
#if defined(_MSC_VER)
#   define CDSVK__RETVAL_CHECK(expected, expr) \
    do {  \
        int err = (expr);                             \
        if (err != (expected)) {                                            \
            CDSVK_LOG("%s(%d): error in %s() -- %s returned %d", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
            __debugbreak();                                                   \
        }                                                                   \
        assert(err == (expected));                                          \
        __pragma(warning(push))                                             \
        __pragma(warning(disable:4127))                                 \
        } while(0)                                                      \
    __pragma(warning(pop))
#elif defined(__ANDROID__)
#   define CDSVK__RETVAL_CHECK(expected, expr) \
    do {  \
        int err = (expr);                                                   \
        if (err != (expected)) {                                            \
            CDSVK_LOG("%s(%d): error in %s() -- %s returned %d", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
            /*__asm__("int $3"); */                 \
        }                                                                   \
        assert(err == (expected));                                          \
    } while(0)
#else
#   define CDSVK__RETVAL_CHECK(expected, expr) \
    do {  \
        int err = (expr);                                                   \
        if (err != (expected)) {                                            \
            CDSVK_LOG("%s(%d): error in %s() -- %s returned %d", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
            /*__asm__("int $3"); */                 \
        }                                                                   \
        assert(err == (expected));                                          \
    } while(0)
#endif
#define CDSVK__CHECK(expr) CDSVK__RETVAL_CHECK(VK_SUCCESS, expr)

#define CDSVK__CLAMP(x, xmin, xmax) ( ((x)<(xmin)) ? (xmin) : ( ((x)>(xmax)) ? (xmax) : (x) ) )

template<typename T>
static const T& cdsvk__min(const T& a, const T& b) { return (a<b) ? a : b; }
template<typename T>
static const T& cdsvk__max(const T& a, const T& b) { return (a>b) ? a : b; }

#ifndef CDSVK_NO_STDIO
static FILE *cdsvk__fopen(char const *filename, char const *mode)
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

using namespace cdsvk;

static void *cdsvk__host_alloc(size_t size, size_t alignment, VkSystemAllocationScope scope, const VkAllocationCallbacks *pAllocator)
{
    if (pAllocator)
    {
        return pAllocator->pfnAllocation(pAllocator->pUserData, size, alignment, scope);
    }
    else
    {
#if defined(_MSC_VER)
        return _mm_malloc(size, alignment);
#else
        return malloc(size); // TODO(cort): ignores alignment :(
#endif
    }
}
static void cdsvk__host_free(void *ptr, const VkAllocationCallbacks *pAllocator)
{
    if (pAllocator)
    {
        return pAllocator->pfnFree(pAllocator->pUserData, ptr);
    }
    else
    {
#if defined(_MSC_VER)
        return _mm_free(ptr);
#else
        return free(ptr);
#endif
    }
}

static vk::ImageAspectFlags cdsvk__image_aspect_from_format(vk::Format format)
{
    switch(format)
    {
    case vk::Format::eD16Unorm:
    case vk::Format::eD32Sfloat:
    case vk::Format::eX8D24UnormPack32:
        return vk::ImageAspectFlagBits::eDepth;
    case vk::Format::eD16UnormS8Uint:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32SfloatS8Uint:
        return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    case vk::Format::eUndefined:
        return static_cast<vk::ImageAspectFlagBits>(0);
    default:
        return vk::ImageAspectFlagBits::eColor;
    }
}

Context::Context(const ContextCreateInfo &context_ci) {
    allocation_callbacks_ = context_ci.allocation_callbacks;

    // Build a list of instance layers to enable
    {
        auto all_instance_layers = vk::enumerateInstanceLayerProperties();
        enabled_instance_layers_.reserve(all_instance_layers.size());
        // Check optional layers first, removing duplicates (some loaders don't like duplicates).
        for(const auto &layer_name : context_ci.optional_instance_layer_names) {
            for(auto& layer : all_instance_layers) {
                if (layer_name == layer.layerName) {
                    if (layer.specVersion != 0xDEADC0DE) {
                        enabled_instance_layers_.push_back(layer);
                        layer.specVersion = 0xDEADC0DE;
                    }
                    break;
                }
            }
        }
        for(const auto &layer_name : context_ci.required_instance_layer_names) {
            bool found = false;
            for(auto& layer : all_instance_layers) {
                if (layer_name == layer.layerName) {
                    if (layer.specVersion != 0xDEADC0DE) {
                        enabled_instance_layers_.push_back(layer);
                        layer.specVersion = 0xDEADC0DE;
                    }
                    found = true;
                    break;
                }
            }
            CDSVK_ASSERT(found);
        }
    }
    // Build a list of instance extensions to enable
    {
        std::vector<vk::ExtensionProperties> all_instance_extensions;
        // Build list of unique instance extensions across all enabled instance layers
        for(int32_t iLayer = -1; iLayer < (int32_t)enabled_instance_layers_.size(); ++iLayer) {
            auto instance_layer_extensions = (iLayer == -1) ?
                vk::enumerateInstanceExtensionProperties() :
                vk::enumerateInstanceExtensionProperties(std::string(enabled_instance_layers_[iLayer].layerName));
            for(const auto &layer_extension : instance_layer_extensions) {
                bool found = false;
                const std::string extension_name_str(layer_extension.extensionName);
                for(const auto &extension : all_instance_extensions) {
                    if (extension_name_str == extension.extensionName) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    all_instance_extensions.push_back(layer_extension);
                }
            }
        }
        // Check optional extensions first, removing duplicates (some loaders don't like duplicates).
        for(const auto &extension_name : context_ci.optional_instance_extension_names) {
            for(auto& extension : all_instance_extensions) {
                if (extension_name == extension.extensionName) {
                    if (extension.specVersion != 0xDEADC0DE) {
                        enabled_instance_extensions_.push_back(extension);
                        extension.specVersion = 0xDEADC0DE;
                    }
                    break;
                }
            }
        }
        for(const auto &extension_name : context_ci.required_instance_extension_names) {
            bool found = false;
            for(auto& extension : all_instance_extensions) {
                if (extension_name == extension.extensionName) {
                    if (extension.specVersion != 0xDEADC0DE) {
                        enabled_instance_extensions_.push_back(extension);
                        extension.specVersion = 0xDEADC0DE;
                    }
                    found = true;
                    break;
                }
            }
            CDSVK_ASSERT(found);
        }
    }

    // Create Instance
    {
        std::vector<const char*> enabled_instance_layer_names;
        enabled_instance_layer_names.reserve(enabled_instance_layers_.size());
        for(const auto &layer : enabled_instance_layers_) {
            enabled_instance_layer_names.push_back(layer.layerName);
        }
        std::vector<const char*> enabled_instance_extension_names;
        enabled_instance_extension_names.reserve(enabled_instance_extensions_.size());
        for(const auto &extension : enabled_instance_extensions_) {
            enabled_instance_extension_names.push_back(extension.extensionName);
        }

        vk::ApplicationInfo application_info_default("Default Application Name", 0x1000,
            "Default Engine Name", 0x1000, VK_MAKE_VERSION(1,0,0));
        vk::InstanceCreateInfo instance_ci(vk::InstanceCreateFlagBits(0),
            context_ci.application_info ? context_ci.application_info : &application_info_default,
            (uint32_t)enabled_instance_layer_names.size(), enabled_instance_layer_names.data(),
            (uint32_t)enabled_instance_extension_names.size(), enabled_instance_extension_names.data());
        instance_ = vk::createInstance(instance_ci, allocation_callbacks_);
    }

    // Set up debug report callback
    if (context_ci.debug_report_callback && is_instance_extension_enabled(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) {
        CDSVK_ASSERT(context_ci.debug_report_flags); // enabling a callback with zero flags is pointless!
        VkDebugReportCallbackCreateInfoEXT debug_report_callback_ci = {};
        debug_report_callback_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debug_report_callback_ci.pNext = nullptr;
        debug_report_callback_ci.flags = (VkDebugReportFlagsEXT)context_ci.debug_report_flags;
        debug_report_callback_ci.pfnCallback = context_ci.debug_report_callback;
        debug_report_callback_ci.pUserData = context_ci.debug_report_callback_user_data;
        PFN_vkCreateDebugReportCallbackEXT my_vkCreateDebugReportCallback = 
            (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugReportCallbackEXT");
        VkDebugReportCallbackEXT callback = VK_NULL_HANDLE;
        CDSVK__CHECK(my_vkCreateDebugReportCallback(instance_, &debug_report_callback_ci,
            (const VkAllocationCallbacks*)allocation_callbacks_, &callback));
        debug_report_callback_ = callback;
    }

    // Invoke callback to get a present surface
    if (context_ci.pfn_get_vk_surface) {
        present_surface_ = context_ci.pfn_get_vk_surface(instance_,
            (const VkAllocationCallbacks*)allocation_callbacks_, context_ci.get_vk_surface_userdata);
    }

    // Select a physical device, after locating suitable queue families.
    {
        auto all_physical_devices = instance_.enumeratePhysicalDevices();
        bool found_graphics_queue_family = false;
        bool found_present_queue_family = false;
        physical_device_ = VK_NULL_HANDLE;
        graphics_queue_family_index_ = VK_QUEUE_FAMILY_IGNORED;
        present_queue_family_index_ = VK_QUEUE_FAMILY_IGNORED;
        for(auto physical_device : all_physical_devices) {
            found_graphics_queue_family = false;
            found_present_queue_family = false;
            auto all_queue_family_properties = physical_device.getQueueFamilyProperties();
            for(uint32_t iQF=0; (size_t)iQF < all_queue_family_properties.size(); ++iQF) {
                VkBool32 supports_graphics = (all_queue_family_properties[iQF].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlagBits(0);
                VkBool32 supports_present = present_surface_ && physical_device.getSurfaceSupportKHR(iQF, present_surface_);
                if (supports_graphics && supports_present) {
                    found_graphics_queue_family = true;
                    found_present_queue_family = true;
                    graphics_queue_family_index_ = iQF;
                    present_queue_family_index_ = iQF;
                } else if (!found_present_queue_family && supports_present) {
                    found_present_queue_family = true;
                    present_queue_family_index_ = iQF;
                } else if (!found_graphics_queue_family && supports_graphics) {
                    found_graphics_queue_family = true;
                    graphics_queue_family_index_ = iQF;
                }
                if (found_graphics_queue_family &&
                    (found_present_queue_family && present_surface_)) {
                    physical_device_ = physical_device;
                    break;
                }
            }
            if (physical_device_) {
                if (found_graphics_queue_family) {
                    graphics_queue_family_properties_ = all_queue_family_properties[graphics_queue_family_index_];
                }
                if (found_present_queue_family) {
                    present_queue_family_properties_ = all_queue_family_properties[present_queue_family_index_];
                }
                break;
            }
        }
        CDSVK_ASSERT(physical_device_);
        CDSVK_ASSERT(found_graphics_queue_family);
        CDSVK_ASSERT(!present_surface_ || found_present_queue_family);
        physical_device_properties_ = physical_device_.getProperties();
        physical_device_memory_properties_ = physical_device_.getMemoryProperties();
        physical_device_features_ = physical_device_.getFeatures();
    }

    // Build a list of device extensions to enable
    {
        std::vector<vk::ExtensionProperties> all_device_extensions;
        // Build list of unique device extensions across all enabled instance layers
        for(int32_t iExt = -1; iExt < (int32_t)enabled_instance_layers_.size(); ++iExt) {
            auto device_layer_extensions = (iExt == -1) ?
                physical_device_.enumerateDeviceExtensionProperties() :
                physical_device_.enumerateDeviceExtensionProperties(std::string(enabled_instance_layers_[iExt].layerName));
            for(const auto &layer_extension : device_layer_extensions) {
                bool found = false;
                const std::string extension_name_str(layer_extension.extensionName);
                for(const auto &extension : all_device_extensions) {
                    if (extension_name_str == extension.extensionName) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    all_device_extensions.push_back(layer_extension);
                }
            }
        }
        // Check optional extensions first, removing duplicates (some loaders don't like duplicates).
        for(const auto &extension_name : context_ci.optional_device_extension_names) {
            for(auto& extension : all_device_extensions) {
                if (extension_name == extension.extensionName) {
                    if (extension.specVersion != 0xDEADC0DE) {
                        enabled_device_extensions_.push_back(extension);
                        extension.specVersion = 0xDEADC0DE;
                    }
                    break;
                }
            }
        }
        for(const auto &extension_name : context_ci.required_device_extension_names) {
            bool found = false;
            for(auto& extension : all_device_extensions) {
                if (extension_name == extension.extensionName) {
                    if (extension.specVersion != 0xDEADC0DE) {
                        enabled_device_extensions_.push_back(extension);
                        extension.specVersion = 0xDEADC0DE;
                    }
                    found = true;
                    break;
                }
            }
            CDSVK_ASSERT(found);
        }
    }

    // Create the logical device
    {
        // Set up queue priorities
        std::vector<vk::DeviceQueueCreateInfo> device_queue_cis;
        device_queue_cis.reserve(2); // graphics + present
        std::vector<float> graphics_queue_priorities(graphics_queue_family_properties_.queueCount);
        for(auto &priority : graphics_queue_priorities) {
            priority = 0.5f;
        }
        device_queue_cis.push_back(vk::DeviceQueueCreateInfo(
            vk::DeviceQueueCreateFlags(), graphics_queue_family_index_,
            graphics_queue_family_properties_.queueCount, graphics_queue_priorities.data()));
        if (present_surface_ && present_queue_family_index_ != graphics_queue_family_index_) {
            std::vector<float> present_queue_priorities(present_queue_family_properties_.queueCount);
            for(auto &priority : present_queue_priorities) {
                priority = 0.5f;
            }
            device_queue_cis.push_back(vk::DeviceQueueCreateInfo(
                vk::DeviceQueueCreateFlags(), present_queue_family_index_,
                present_queue_family_properties_.queueCount, present_queue_priorities.data()));
        }

        // Build list of enabled extension names
        std::vector<const char*> enabled_device_extension_names;
        enabled_device_extension_names.reserve(enabled_device_extensions_.size());
        for(const auto &extension : enabled_device_extensions_) {
            enabled_device_extension_names.push_back(extension.extensionName);
        }

        vk::DeviceCreateInfo device_ci(vk::DeviceCreateFlags(),
            (uint32_t)device_queue_cis.size(), device_queue_cis.data(), 0, nullptr,
            (uint32_t)enabled_device_extension_names.size(), enabled_device_extension_names.data(),
            &physical_device_features_);
        device_ = physical_device_.createDevice(device_ci, allocation_callbacks_);

        graphics_queue_ = device_.getQueue(graphics_queue_family_index_, 0);
        if (present_surface_) {
            present_queue_ = device_.getQueue(present_queue_family_index_, 0);
        }
    }

    // TODO(cort): we can now assign debug names to things.

    // Create a pipeline cache. TODO(cort): optionally load cache contents from disk.
    {
        vk::PipelineCacheCreateInfo pipeline_cache_ci = {};
        pipeline_cache_ = device_.createPipelineCache(pipeline_cache_ci, allocation_callbacks_);
    }

}
Context::~Context() {
    if (device_) {
        device_.destroy(allocation_callbacks_);
    }
    if (present_surface_)
        instance_.destroySurfaceKHR(present_surface_, allocation_callbacks_);
    if (debug_report_callback_) {
        PFN_vkDestroyDebugReportCallbackEXT my_vkDestroyDebugReportCallback = 
            (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugReportCallbackEXT");
        my_vkDestroyDebugReportCallback(instance_, debug_report_callback_, (const VkAllocationCallbacks*)allocation_callbacks_);
    }
    instance_.destroy(allocation_callbacks_);
}

bool Context::is_instance_layer_enabled(const std::string &layer_name) const {
    for(const auto &layer : enabled_instance_layers_)
    {
        if (layer_name == layer.layerName)
        {
            return true;
        }
    }
    return false;
}
bool Context::is_instance_extension_enabled(const std::string &extension_name) const {
    for(const auto &extension : enabled_instance_extensions_)
    {
        if (extension_name == extension.extensionName)
        {
            return true;
        }
    }
    return false;
}
bool Context::is_device_extension_enabled(const std::string &extension_name) const {
    for(const auto &extension : enabled_device_extensions_)
    {
        if (extension_name == extension.extensionName)
        {
            return true;
        }
    }
    return false;
}

#if 0
CDSVKDEF VkResult cdsvk_init_device(cdsvk_context_create_info const * create_info, VkSurfaceKHR present_surface, cdsvk_context *context)
{
    VkResult result = VK_SUCCESS;

    uint32_t physical_device_count = 0;
    CDSVK__CHECK( vkEnumeratePhysicalDevices(context->instance, &physical_device_count, NULL) );
    CDSVK_ASSERT(physical_device_count > 0);
    VkPhysicalDevice *all_physical_devices = (VkPhysicalDevice*)cdsvk__host_alloc(physical_device_count * sizeof(VkPhysicalDevice),
        sizeof(VkPhysicalDevice), VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE, context->allocation_callbacks);
    CDSVK__CHECK( vkEnumeratePhysicalDevices(context->instance, &physical_device_count, all_physical_devices) );

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
        VkQueueFamilyProperties *queue_family_properties_all = (VkQueueFamilyProperties*)cdsvk__host_alloc(
            queue_family_count * sizeof(VkQueueFamilyProperties), sizeof(VkQueueFamilyProperties*),
            VK_SYSTEM_ALLOCATION_SCOPE_DEVICE, context->allocation_callbacks);
        vkGetPhysicalDeviceQueueFamilyProperties(all_physical_devices[iPD], &queue_family_count, queue_family_properties_all);

        for(uint32_t iQF=0; iQF<queue_family_count; ++iQF)
        {
            bool queue_supports_graphics = (queue_family_properties_all[iQF].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            VkBool32 queue_supports_present = VK_FALSE;
            CDSVK__CHECK( vkGetPhysicalDeviceSurfaceSupportKHR(all_physical_devices[iPD], iQF,
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
        cdsvk__host_free(queue_family_properties_all, context->allocation_callbacks);
        queue_family_properties_all = NULL;

        if (found_present_queue_family && found_graphics_queue_family)
        {
            context->physical_device = all_physical_devices[iPD];
            context->graphics_queue_family_index = graphics_queue_family_index;
            context->present_queue_family_index  = present_queue_family_index;

            uint32_t graphics_queue_count = context->graphics_queue_family_properties.queueCount;
            float *graphics_queue_priorities = (float*)cdsvk__host_alloc(graphics_queue_count * sizeof(float),
                sizeof(float), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT, context->allocation_callbacks);
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
                float *present_queue_priorities = (float*)cdsvk__host_alloc(present_queue_count * sizeof(float),
                    sizeof(float), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT, context->allocation_callbacks);
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
    CDSVK_ASSERT(found_graphics_queue_family && found_present_queue_family);
    cdsvk__host_free(all_physical_devices, context->allocation_callbacks);
    context->present_surface = present_surface;

    vkGetPhysicalDeviceProperties(context->physical_device, &context->physical_device_properties);
#if 0
    CDSVK_LOG("Physical device #%u: '%s', API version %u.%u.%u",
        0,
        context->physical_device_properties.deviceName,
        VK_VERSION_MAJOR(context->physical_device_properties.apiVersion),
        VK_VERSION_MINOR(context->physical_device_properties.apiVersion),
        VK_VERSION_PATCH(context->physical_device_properties.apiVersion));
#endif

    vkGetPhysicalDeviceMemoryProperties(context->physical_device, &context->physical_device_memory_properties);

    vkGetPhysicalDeviceFeatures(context->physical_device, &context->physical_device_features);

    // Tally up total extension count for all enabled layers (including NULL, which queries
    // core extensions exposed by the driver).
    uint32_t all_extension_count = 0;
    for(int32_t iLayer = -1; iLayer < (int32_t)context->enabled_instance_layer_count; ++iLayer)
    {
        const char *layer_name = (iLayer == -1) ? NULL : context->enabled_instance_layers[iLayer].layerName;
        uint32_t layer_extension_count = 0;
        vkEnumerateDeviceExtensionProperties(context->physical_device, layer_name, &layer_extension_count, NULL);
        all_extension_count += layer_extension_count;
    }
    // Construct a list of unique extensions provided by all layers.
    VkExtensionProperties *all_extensions =
        (VkExtensionProperties*)cdsvk__host_alloc(all_extension_count * sizeof(VkExtensionProperties),
        sizeof(void*), VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, context->allocation_callbacks);
    uint32_t unique_extension_count = 0;
    for(int32_t iLayer = -1; iLayer < (int32_t)context->enabled_instance_layer_count; ++iLayer)
    {
        const char *layer_name = (iLayer == -1) ? NULL : context->enabled_instance_layers[iLayer].layerName;
        uint32_t layer_extension_count = 0;
        vkEnumerateDeviceExtensionProperties(context->physical_device, layer_name, &layer_extension_count, NULL);
        CDSVK_ASSERT(unique_extension_count + layer_extension_count <= all_extension_count);
        VkExtensionProperties *layer_extensions = all_extensions + unique_extension_count;
        vkEnumerateDeviceExtensionProperties(context->physical_device, layer_name, &layer_extension_count,
            layer_extensions);
        for(uint32_t iExt = 0; iExt < layer_extension_count; ++iExt)
        {
            const char *ext_name = layer_extensions[iExt].extensionName;
            int found = 0;
            for(uint32_t jExt = 0; jExt < unique_extension_count; ++jExt)
            {
                if (strcmp(all_extensions[jExt].extensionName, ext_name) == 0)
                {
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                all_extensions[unique_extension_count++] = layer_extensions[iExt];
            }
        }
    }
    // Filter supported extensions into enabled extensions based on provided optional/required lists.
    // Remove duplicates as we go; some loaders don't support them.
    VkExtensionProperties *enabled_device_extensions =
        (VkExtensionProperties*)cdsvk__host_alloc(unique_extension_count * sizeof(VkExtensionProperties),
            sizeof(void*), VK_SYSTEM_ALLOCATION_SCOPE_DEVICE, context->allocation_callbacks);
    uint32_t enabled_device_extension_count = 0;
    for(uint32_t iExt = 0; iExt < create_info->optional_device_extension_count; ++iExt)
    {
        const char *ext_name = create_info->optional_device_extension_names[iExt];
        for(uint32_t jExtension = 0; jExtension < unique_extension_count; ++jExtension)
        {
            if (strcmp(ext_name, all_extensions[jExtension].extensionName) == 0)
            {
                if (all_extensions[jExtension].specVersion != 0xDEADC0DE)
                {
                    enabled_device_extensions[enabled_device_extension_count++] = all_extensions[jExtension];
                    all_extensions[jExtension].specVersion = 0xDEADC0DE;
                }
                break;
            }
        }
    }
    for(uint32_t iExt = 0; iExt < create_info->required_device_extension_count; ++iExt)
    {
        const char *ext_name = create_info->required_device_extension_names[iExt];
        int found = 0;
        for(uint32_t jExtension = 0; jExtension < unique_extension_count; ++jExtension)
        {
            if (strcmp(ext_name, all_extensions[jExtension].extensionName) == 0)
            {
                if (all_extensions[jExtension].specVersion != 0xDEADC0DE)
                {
                    enabled_device_extensions[enabled_device_extension_count++] = all_extensions[jExtension];
                    all_extensions[jExtension].specVersion = 0xDEADC0DE;
                }
                found = 1;
                break;
            }
        }
        if (!found)
        {
            result = VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
    cdsvk__host_free(all_extensions, context->allocation_callbacks);
    if (result != VK_SUCCESS)
    {
        return result;
    }
    const char **enabled_device_extension_names =
        (const char**)cdsvk__host_alloc(enabled_device_extension_count * sizeof(const char*),
            sizeof(const char*), VK_SYSTEM_ALLOCATION_SCOPE_COMMAND, context->allocation_callbacks);
    for(uint32_t iExt = 0; iExt < enabled_device_extension_count; ++iExt)
    {
        enabled_device_extension_names[iExt] = enabled_device_extensions[iExt].extensionName;
    }

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = NULL;
    device_create_info.flags = 0;
    device_create_info.queueCreateInfoCount = device_queue_create_info_count;
    device_create_info.pQueueCreateInfos = device_queue_create_infos;
    device_create_info.enabledLayerCount = 0;
    device_create_info.ppEnabledLayerNames = NULL;
    device_create_info.enabledExtensionCount = enabled_device_extension_count;
    device_create_info.ppEnabledExtensionNames = enabled_device_extension_names;
    device_create_info.pEnabledFeatures = &context->physical_device_features;
    CDSVK__CHECK( vkCreateDevice(context->physical_device, &device_create_info, context->allocation_callbacks, &context->device) );

    cdsvk__host_free(enabled_device_extension_names, context->allocation_callbacks);
    cdsvk__host_free((void*)device_create_info.pQueueCreateInfos[0].pQueuePriorities, context->allocation_callbacks);
    cdsvk__host_free((void*)device_create_info.pQueueCreateInfos[1].pQueuePriorities, context->allocation_callbacks);

    context->enabled_device_extension_count = enabled_device_extension_count;
    context->enabled_device_extensions = enabled_device_extensions;

#if VK_EXT_debug_marker
    bool debug_marker_extension_loaded = false;
    for(uint32_t iExt=0; iExt<context->enabled_device_extension_count; ++iExt)
    {
        if (strcmp(context->enabled_device_extensions[iExt].extensionName, "VK_EXT_debug_marker") == 0)
        {
            debug_marker_extension_loaded = true;
            break;
        }
    }
    if (debug_marker_extension_loaded)
    {
        cdsvk__DebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(context->device,
            "vkDebugMarkerSetObjectNameEXT");
    }
    // Name the things we've already created
    CDSVK__CHECK(cdsvk_name_instance(context->device, context->instance, "cdsvk_context instance"));
    CDSVK__CHECK(cdsvk_name_physical_device(context->device, context->physical_device, "cdsvk_context physical device"));
    CDSVK__CHECK(cdsvk_name_device(context->device, context->device, "cdsvk_context device"));
    CDSVK__CHECK(cdsvk_name_surface(context->device, present_surface, "cdsvk_context present surface"));
    CDSVK__CHECK(cdsvk_name_debug_report_callback(context->device, context->debug_report_callback, "cdsvk_context debug report callback"));
#endif

    CDSVK_ASSERT(context->present_queue_family_properties.queueCount > 0);
    vkGetDeviceQueue(context->device, context->present_queue_family_index, 0, &context->present_queue);
    CDSVK_ASSERT(context->graphics_queue_family_properties.queueCount > 0);
    vkGetDeviceQueue(context->device, context->graphics_queue_family_index, 0, &context->graphics_queue);

    VkPipelineCacheCreateInfo pipeline_cache_create_info = {};
    pipeline_cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipeline_cache_create_info.pNext = NULL;
    pipeline_cache_create_info.flags = 0;
    pipeline_cache_create_info.initialDataSize = 0;
    pipeline_cache_create_info.pInitialData = NULL;
    context->pipeline_cache = cdsvk_create_pipeline_cache(context, &pipeline_cache_create_info, "pipeline cache");
    return VK_SUCCESS;
}

CDSVKDEF VkResult cdsvk_init_swapchain(cdsvk_context_create_info const * /*create_info*/, cdsvk_context *context, VkSwapchainKHR old_swapchain)
{
    VkSurfaceCapabilitiesKHR surface_capabilities;
    CDSVK__CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physical_device, context->present_surface, &surface_capabilities) );
    VkExtent2D swapchain_extent;
    if ( (int32_t)surface_capabilities.currentExtent.width == -1 )
    {
        CDSVK_ASSERT( (int32_t)surface_capabilities.currentExtent.height == -1 );
        // TODO(cort): better defaults here, when we can't detect the present surface extent?
        swapchain_extent.width  = CDSVK__CLAMP(1280, surface_capabilities.minImageExtent.width,  surface_capabilities.maxImageExtent.width);
        swapchain_extent.height = CDSVK__CLAMP( 720, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    }
    else
    {
        swapchain_extent = surface_capabilities.currentExtent;
    }
    uint32_t device_surface_format_count = 0;
    CDSVK__CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->present_surface, &device_surface_format_count, NULL) );
    VkSurfaceFormatKHR *device_surface_formats = (VkSurfaceFormatKHR*)cdsvk__host_alloc(device_surface_format_count * sizeof(VkSurfaceFormatKHR),
        sizeof(VkSurfaceFormatKHR), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT, context->allocation_callbacks);
    CDSVK__CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->present_surface, &device_surface_format_count, device_surface_formats) );
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
    cdsvk__host_free(device_surface_formats, context->allocation_callbacks);

    uint32_t device_present_mode_count = 0;
    CDSVK__CHECK( vkGetPhysicalDeviceSurfacePresentModesKHR(context->physical_device, context->present_surface, &device_present_mode_count, NULL) );
    VkPresentModeKHR *device_present_modes = (VkPresentModeKHR*)cdsvk__host_alloc(device_present_mode_count * sizeof(VkPresentModeKHR),
        sizeof(VkPresentModeKHR), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT, context->allocation_callbacks);
    CDSVK__CHECK( vkGetPhysicalDeviceSurfacePresentModesKHR(context->physical_device, context->present_surface, &device_present_mode_count, device_present_modes) );
    VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
    bool found_mailbox_mode = false;
    for(uint32_t iMode=0; iMode<device_present_mode_count; ++iMode) {
      if (device_present_modes[iMode] == VK_PRESENT_MODE_MAILBOX_KHR) {
        found_mailbox_mode = true;
        break;
      }
    }
    if (!found_mailbox_mode)
      swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    cdsvk__host_free(device_present_modes, context->allocation_callbacks);

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
    CDSVK_ASSERT( (swapchain_image_usage & surface_capabilities.supportedUsageFlags) == swapchain_image_usage );

    CDSVK_ASSERT(surface_capabilities.supportedCompositeAlpha != 0);
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
    CDSVK__CHECK( vkCreateSwapchainKHR(context->device, &swapchain_create_info, context->allocation_callbacks, &context->swapchain) );
    if (old_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(context->device, old_swapchain, context->allocation_callbacks);
    }

    CDSVK__CHECK( vkGetSwapchainImagesKHR(context->device, context->swapchain, &context->swapchain_image_count, NULL) );
    context->swapchain_images = (VkImage*)cdsvk__host_alloc(context->swapchain_image_count * sizeof(VkImage),
        sizeof(VkImage), VK_SYSTEM_ALLOCATION_SCOPE_DEVICE, context->allocation_callbacks);
    CDSVK__CHECK( vkGetSwapchainImagesKHR(context->device, context->swapchain, &context->swapchain_image_count, context->swapchain_images) );

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
    context->swapchain_image_views = (VkImageView*)cdsvk__host_alloc(context->swapchain_image_count * sizeof(VkImageView),
        sizeof(VkImageView), VK_SYSTEM_ALLOCATION_SCOPE_DEVICE, context->allocation_callbacks);
    for(uint32_t iSCI=0; iSCI<context->swapchain_image_count; iSCI+=1)
    {
        image_view_create_info.image = context->swapchain_images[iSCI];
        context->swapchain_image_views[iSCI] = cdsvk_create_image_view(context, &image_view_create_info, "swapchain image view");
    }

    return VK_SUCCESS;
}

CDSVKDEF void cdsvk_destroy_context(cdsvk_context *context)
{
    vkDeviceWaitIdle(context->device);

    for(uint32_t iSCI=0; iSCI<context->swapchain_image_count; ++iSCI)
    {
        vkDestroyImageView(context->device, context->swapchain_image_views[iSCI], context->allocation_callbacks);
    }
    cdsvk__host_free(context->swapchain_image_views, context->allocation_callbacks);
    context->swapchain_image_views = NULL;
    cdsvk__host_free(context->swapchain_images, context->allocation_callbacks);
    context->swapchain_images = NULL;
    vkDestroySwapchainKHR(context->device, context->swapchain, context->allocation_callbacks);

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

    cdsvk__host_free((VkLayerProperties*)(context->enabled_instance_layers), context->allocation_callbacks);
    cdsvk__host_free((VkExtensionProperties*)(context->enabled_instance_extensions), context->allocation_callbacks);
    cdsvk__host_free((VkExtensionProperties*)(context->enabled_device_extensions), context->allocation_callbacks);

    context->allocation_callbacks = NULL;
    *context = {};
}

CDSVKDEF int cdsvk_is_instance_layer_enabled(cdsvk_context const *context, const char *layer_name)
{
    for(uint32_t iLayer = 0; iLayer < context->enabled_instance_layer_count; ++iLayer)
    {
        if (strcmp(layer_name, context->enabled_instance_layers[iLayer].layerName) == 0)
        {
            return 1;
        }
    }
    return 0;
}
CDSVKDEF int cdsvk_is_instance_extension_enabled(cdsvk_context const *context, const char *extension_name)
{
    for(uint32_t iExt = 0; iExt < context->enabled_instance_extension_count; ++iExt)
    {
        if (strcmp(extension_name, context->enabled_instance_extensions[iExt].extensionName) == 0)
        {
            return 1;
        }
    }
    return 0;
}
CDSVKDEF int cdsvk_is_device_extension_enabled(cdsvk_context const *context, const char *extension_name)
{
    for(uint32_t iExt = 0; iExt < context->enabled_device_extension_count; ++iExt)
    {
        if (strcmp(extension_name, context->enabled_device_extensions[iExt].extensionName) == 0)
        {
            return 1;
        }
    }
    return 0;
}


//////////////////////// Device memory allocation

CDSVKDEF VkResult cdsvk_allocate_device_memory(cdsvk_context const *context, VkMemoryRequirements const *mem_reqs,
    cdsvk_device_memory_arena const *arena, VkMemoryPropertyFlags memory_properties_mask, const char *name,
    VkDeviceMemory *out_mem, VkDeviceSize *out_offset)
{
    VkMemoryAllocateInfo memory_allocate_info = {};
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = mem_reqs->size;
    memory_allocate_info.memoryTypeIndex = cdsvk_find_memory_type_index(&context->physical_device_memory_properties,
        mem_reqs, memory_properties_mask);
    CDSVK_ASSERT(memory_allocate_info.memoryTypeIndex < VK_MAX_MEMORY_TYPES);

    return cdsvk__device_alloc(context, &memory_allocate_info, mem_reqs->alignment, arena, name,
        out_mem, out_offset);
}

CDSVKDEF void cdsvk_free_device_memory(cdsvk_context const *context, cdsvk_device_memory_arena const *arena,
    VkDeviceMemory mem, VkDeviceSize offset)
{
    return cdsvk__device_free(context, arena, mem, offset);
}

CDSVKDEF VkResult cdsvk_allocate_and_bind_image_memory(cdsvk_context const *context, VkImage image,
    cdsvk_device_memory_arena const *arena, VkMemoryPropertyFlags memory_properties_mask, const char *name,
    VkDeviceMemory *out_mem, VkDeviceSize *out_offset)
{
    VkMemoryRequirements mem_reqs = {};
    vkGetImageMemoryRequirements(context->device, image, &mem_reqs);
    VkResult result = cdsvk_allocate_device_memory(context, &mem_reqs, arena, memory_properties_mask,
        name, out_mem, out_offset);
    if (result != VK_SUCCESS)
        return result;
    return vkBindImageMemory(context->device, image, *out_mem, *out_offset);
}
CDSVKDEF VkResult cdsvk_allocate_and_bind_buffer_memory(cdsvk_context const *context, VkBuffer buffer,
    cdsvk_device_memory_arena const *arena, VkMemoryPropertyFlags memory_properties_mask, const char *name,
    VkDeviceMemory *out_mem, VkDeviceSize *out_offset)
{
    VkMemoryRequirements mem_reqs = {};
    vkGetBufferMemoryRequirements(context->device, buffer, &mem_reqs);
    VkResult result = cdsvk_allocate_device_memory(context, &mem_reqs, arena, memory_properties_mask,
        name, out_mem, out_offset);
    if (result != VK_SUCCESS)
        return result;
    return vkBindBufferMemory(context->device, buffer, *out_mem, *out_offset);
}

CDSVKDEF uint32_t cdsvk_find_memory_type_index(VkPhysicalDeviceMemoryProperties const *device_memory_properties,
    VkMemoryRequirements const *memory_reqs, VkMemoryPropertyFlags memory_properties_mask)
{
    for(uint32_t iMemType=0; iMemType<VK_MAX_MEMORY_TYPES; iMemType+=1)
    {
        if (	(memory_reqs->memoryTypeBits & (1<<iMemType)) != 0
            &&	(device_memory_properties->memoryTypes[iMemType].propertyFlags & memory_properties_mask) == memory_properties_mask)
        {
            return iMemType;
        }
    }
    return VK_MAX_MEMORY_TYPES; /* invalid index */
}


///////////////////////////// Object creation/deletion helpers

CDSVKDEF VkCommandPool cdsvk_create_command_pool(cdsvk_context const *context, VkCommandPoolCreateInfo const *ci, const char *name)
{
    VkCommandPool cpool = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateCommandPool(context->device, ci, context->allocation_callbacks, &cpool));
    CDSVK__CHECK(cdsvk_name_command_pool(context->device, cpool, name));
    return cpool;
}
CDSVKDEF void cdsvk_destroy_command_pool(cdsvk_context const *context, VkCommandPool cpool)
{
    vkDestroyCommandPool(context->device, cpool, context->allocation_callbacks);
}

CDSVKDEF VkSemaphore cdsvk_create_semaphore(cdsvk_context const *context, VkSemaphoreCreateInfo const *ci, const char *name)
{
    VkSemaphore semaphore = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateSemaphore(context->device, ci, context->allocation_callbacks, &semaphore));
    CDSVK__CHECK(cdsvk_name_semaphore(context->device, semaphore, name));
    return semaphore;
}
CDSVKDEF void cdsvk_destroy_semaphore(cdsvk_context const *context, VkSemaphore semaphore)
{
    vkDestroySemaphore(context->device, semaphore, context->allocation_callbacks);
}

CDSVKDEF VkFence cdsvk_create_fence(cdsvk_context const *context, VkFenceCreateInfo const *ci, const char *name)
{
    VkFence fence = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateFence(context->device, ci, context->allocation_callbacks, &fence));
    CDSVK__CHECK(cdsvk_name_fence(context->device, fence, name));
    return fence;
}
CDSVKDEF void cdsvk_destroy_fence(cdsvk_context const *context, VkFence fence)
{
    vkDestroyFence(context->device, fence, context->allocation_callbacks);
}

CDSVKDEF VkEvent cdsvk_create_event(cdsvk_context const *context, VkEventCreateInfo const *ci, const char *name)
{
    VkEvent event = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateEvent(context->device, ci, context->allocation_callbacks, &event));
    CDSVK__CHECK(cdsvk_name_event(context->device, event, name));
    return event;
}
CDSVKDEF void cdsvk_destroy_event(cdsvk_context const *context, VkEvent event)
{
    vkDestroyEvent(context->device, event, context->allocation_callbacks);
}

CDSVKDEF VkQueryPool cdsvk_create_query_pool(cdsvk_context const *context, VkQueryPoolCreateInfo const *ci, const char *name)
{
    VkQueryPool pool = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateQueryPool(context->device, ci, context->allocation_callbacks, &pool));
    CDSVK__CHECK(cdsvk_name_query_pool(context->device, pool, name));
    return pool;
}
CDSVKDEF void cdsvk_destroy_query_pool(cdsvk_context const *context, VkQueryPool pool)
{
    vkDestroyQueryPool(context->device, pool, context->allocation_callbacks);
}

CDSVKDEF VkPipelineCache cdsvk_create_pipeline_cache(cdsvk_context const *context, VkPipelineCacheCreateInfo const *ci, const char *name)
{
    VkPipelineCache cache = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreatePipelineCache(context->device, ci, context->allocation_callbacks, &cache));
    CDSVK__CHECK(cdsvk_name_pipeline_cache(context->device, cache, name));
    return cache;
}
CDSVKDEF void cdsvk_destroy_pipeline_cache(cdsvk_context const *context, VkPipelineCache cache)
{
    vkDestroyPipelineCache(context->device, cache, context->allocation_callbacks);
}

CDSVKDEF VkPipelineLayout cdsvk_create_pipeline_layout(cdsvk_context const *context, VkPipelineLayoutCreateInfo const *ci, const char *name)
{
    VkPipelineLayout layout = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreatePipelineLayout(context->device, ci, context->allocation_callbacks, &layout));
    CDSVK__CHECK(cdsvk_name_pipeline_layout(context->device, layout, name));
    return layout;
}
CDSVKDEF void cdsvk_destroy_pipeline_layout(cdsvk_context const *context, VkPipelineLayout layout)
{
    vkDestroyPipelineLayout(context->device, layout, context->allocation_callbacks);
}

CDSVKDEF VkRenderPass cdsvk_create_render_pass(cdsvk_context const *context, VkRenderPassCreateInfo const *ci, const char *name)
{
    VkRenderPass render_pass = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateRenderPass(context->device, ci, context->allocation_callbacks, &render_pass));
    CDSVK__CHECK(cdsvk_name_render_pass(context->device, render_pass, name));
    return render_pass;
}
CDSVKDEF void cdsvk_destroy_render_pass(cdsvk_context const *context, VkRenderPass render_pass)
{
    vkDestroyRenderPass(context->device, render_pass, context->allocation_callbacks);
}

CDSVKDEF VkPipeline cdsvk_create_graphics_pipeline(cdsvk_context const *context, VkGraphicsPipelineCreateInfo const *ci, const char *name)
{
    VkPipeline pipeline = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateGraphicsPipelines(context->device, context->pipeline_cache, 1, ci, context->allocation_callbacks, &pipeline));
    CDSVK__CHECK(cdsvk_name_pipeline(context->device, pipeline, name));
    return pipeline;
}
CDSVKDEF VkPipeline cdsvk_create_compute_pipeline(cdsvk_context const *context, VkComputePipelineCreateInfo const *ci, const char *name)
{
    VkPipeline pipeline = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateComputePipelines(context->device, context->pipeline_cache, 1, ci, context->allocation_callbacks, &pipeline));
    CDSVK__CHECK(cdsvk_name_pipeline(context->device, pipeline, name));
    return pipeline;
}
CDSVKDEF void cdsvk_destroy_pipeline(cdsvk_context const *context, VkPipeline pipeline)
{
    vkDestroyPipeline(context->device, pipeline, context->allocation_callbacks);
}

CDSVKDEF VkDescriptorSetLayout cdsvk_create_descriptor_set_layout(cdsvk_context const *context, VkDescriptorSetLayoutCreateInfo const *ci, const char *name)
{
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateDescriptorSetLayout(context->device, ci, context->allocation_callbacks, &layout));
    CDSVK__CHECK(cdsvk_name_descriptor_set_layout(context->device, layout, name));
    return layout;
}
CDSVKDEF void cdsvk_destroy_descriptor_set_layout(cdsvk_context const *context, VkDescriptorSetLayout layout)
{
    vkDestroyDescriptorSetLayout(context->device, layout, context->allocation_callbacks);
}

CDSVKDEF VkSampler cdsvk_create_sampler(cdsvk_context const *context, VkSamplerCreateInfo const *ci, const char *name)
{
    VkSampler sampler = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateSampler(context->device, ci, context->allocation_callbacks, &sampler));
    CDSVK__CHECK(cdsvk_name_sampler(context->device, sampler, name));
    return sampler;
}
CDSVKDEF void cdsvk_destroy_sampler(cdsvk_context const *context, VkSampler sampler)
{
    vkDestroySampler(context->device, sampler, context->allocation_callbacks);
}

CDSVKDEF VkFramebuffer cdsvk_create_framebuffer(cdsvk_context const *context, VkFramebufferCreateInfo const *ci, const char *name)
{
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateFramebuffer(context->device, ci, context->allocation_callbacks, &framebuffer));
    CDSVK__CHECK(cdsvk_name_framebuffer(context->device, framebuffer, name));
    return framebuffer;
}
CDSVKDEF void cdsvk_destroy_framebuffer(cdsvk_context const *context, VkFramebuffer framebuffer)
{
    vkDestroyFramebuffer(context->device, framebuffer, context->allocation_callbacks);
}

CDSVKDEF VkBuffer cdsvk_create_buffer(cdsvk_context const *context, VkBufferCreateInfo const *ci,
    const char *name)
{
    VkBuffer buffer = VK_NULL_HANDLE;
    CDSVK__CHECK( vkCreateBuffer(context->device, ci, context->allocation_callbacks, &buffer) );
    CDSVK__CHECK(cdsvk_name_buffer(context->device, buffer, name));
    return buffer;
}

CDSVKDEF void cdsvk_destroy_buffer(cdsvk_context const *context, VkBuffer buffer)
{
    vkDestroyBuffer(context->device, buffer, context->allocation_callbacks);
}

CDSVKDEF VkBufferView cdsvk_create_buffer_view(cdsvk_context const *context, VkBufferViewCreateInfo const *ci,
    const char *name)
{
    VkBufferView view = VK_NULL_HANDLE;
    CDSVK__CHECK( vkCreateBufferView(context->device, ci, context->allocation_callbacks, &view) );
    CDSVK__CHECK(cdsvk_name_buffer_view(context->device, view, name));
    return view;
}

CDSVKDEF VkBufferView cdsvk_create_buffer_view_from_buffer(cdsvk_context const *context, VkBuffer buffer,
    VkFormat format, const char *name)
{
    VkBufferViewCreateInfo view_ci = {};
    view_ci.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    view_ci.buffer = buffer;
    view_ci.format = format;
    view_ci.offset = 0;
    view_ci.range = VK_WHOLE_SIZE;
    return cdsvk_create_buffer_view(context, &view_ci, name);
}

CDSVKDEF void cdsvk_destroy_buffer_view(cdsvk_context const *context, VkBufferView view)
{
    vkDestroyBufferView(context->device, view, context->allocation_callbacks);
}

CDSVKDEF VkImage cdsvk_create_image(cdsvk_context const *context, VkImageCreateInfo const *ci,
    VkImageLayout final_layout, VkAccessFlags final_access_flags, const char *name)
{
    VkImage image = VK_NULL_HANDLE;
    CDSVK__CHECK( vkCreateImage(context->device, ci, context->allocation_callbacks, &image) );
    CDSVK__CHECK(cdsvk_name_image(context->device, image, name));
    if (final_layout != ci->initialLayout)
    {
        VkCommandPoolCreateInfo cpool_ci;
        cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpool_ci.pNext = NULL;
        cpool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        cpool_ci.queueFamilyIndex = context->graphics_queue_family_index;
        VkCommandPool cpool = cdsvk_create_command_pool(context, &cpool_ci, "cdsvk_create_image temp cpool");

        VkFenceCreateInfo fence_ci;
        fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_ci.pNext = NULL;
        fence_ci.flags = 0;
        VkFence fence = cdsvk_create_fence(context, &fence_ci, "cdsvk_create_image temp fence");

        VkCommandBufferAllocateInfo cb_allocate_info = {};
        cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cb_allocate_info.pNext = NULL;
        cb_allocate_info.commandPool = cpool;
        cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cb_allocate_info.commandBufferCount = 1;
        VkCommandBuffer cb = VK_NULL_HANDLE;
        CDSVK__CHECK( vkAllocateCommandBuffers(context->device, &cb_allocate_info, &cb) );
        VkCommandBufferBeginInfo cb_begin_info = {};
        cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cb_begin_info.pNext = NULL;
        cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cb_begin_info.pInheritanceInfo = NULL;
        CDSVK__CHECK( vkBeginCommandBuffer(cb, &cb_begin_info) );

        VkImageSubresourceRange sub_range;
        sub_range.aspectMask = cdsvk__image_aspect_from_format(ci->format);
        sub_range.baseMipLevel = 0;
        sub_range.baseArrayLayer = 0;
        sub_range.levelCount = VK_REMAINING_MIP_LEVELS;
        sub_range.layerCount = VK_REMAINING_ARRAY_LAYERS;
        VkImageMemoryBarrier img_barrier = {};
        img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        img_barrier.pNext = NULL;
        img_barrier.srcAccessMask = 0;
        img_barrier.dstAccessMask = final_access_flags;
        img_barrier.oldLayout = ci->initialLayout;
        img_barrier.newLayout = final_layout;
        img_barrier.image = image;
        img_barrier.subresourceRange = sub_range;
        vkCmdPipelineBarrier(cb,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
            0,NULL, 0,NULL, 1,&img_barrier);

        CDSVK__CHECK( vkEndCommandBuffer(cb) );
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = NULL;
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = NULL;
        submit_info.pWaitDstStageMask = NULL;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cb;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = NULL;
        CDSVK__CHECK( vkQueueSubmit(context->graphics_queue, 1, &submit_info, fence) );
        CDSVK__CHECK( vkWaitForFences(context->device, 1, &fence, VK_TRUE, UINT64_MAX) );
        cdsvk_destroy_fence(context, fence);
        cdsvk_destroy_command_pool(context, cpool);
    }
    return image;
}
CDSVKDEF void cdsvk_destroy_image(cdsvk_context const *context, VkImage image)
{
    vkDestroyImage(context->device, image, context->allocation_callbacks);
}

CDSVKDEF VkImageView cdsvk_create_image_view(cdsvk_context const *context, VkImageViewCreateInfo const *ci, const char *name)
{
    VkImageView image_view = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateImageView(context->device, ci, context->allocation_callbacks, &image_view));
    CDSVK__CHECK(cdsvk_name_image_view(context->device, image_view, name));
    return image_view;
}
CDSVKDEF VkImageView cdsvk_create_image_view_from_image(cdsvk_context const *context, VkImage image,
    VkImageCreateInfo const *image_ci, const char *name)
{
    VkImageViewCreateInfo view_ci = {};
    view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image = image;
    if (image_ci->imageType == VK_IMAGE_TYPE_1D)
    {
        view_ci.viewType = (image_ci->arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    }
    else if (image_ci->imageType == VK_IMAGE_TYPE_2D)
    {
        if (image_ci->flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
        {
            assert((image_ci->arrayLayers) % 6 == 0);
            view_ci.viewType = (image_ci->arrayLayers == 6) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        }
        else
        {
            view_ci.viewType = (image_ci->arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
    }
    else if (image_ci->imageType == VK_IMAGE_TYPE_3D)
    {
        view_ci.viewType = VK_IMAGE_VIEW_TYPE_3D;
    }
    view_ci.format = image_ci->format;
    view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_ci.subresourceRange.aspectMask = cdsvk__image_aspect_from_format(view_ci.format);
    view_ci.subresourceRange.baseMipLevel = 0;
    view_ci.subresourceRange.levelCount = image_ci->mipLevels;
    view_ci.subresourceRange.baseArrayLayer = 0;
    view_ci.subresourceRange.layerCount = image_ci->arrayLayers;
    return cdsvk_create_image_view(context, &view_ci, name);
}
CDSVKDEF void cdsvk_destroy_image_view(cdsvk_context const *context, VkImageView imageView)
{
    vkDestroyImageView(context->device, imageView, context->allocation_callbacks);
}

CDSVKDEF VkDescriptorPool cdsvk_create_descriptor_pool(cdsvk_context const *context,
    const VkDescriptorPoolCreateInfo *ci, const char *name)
{
    VkDescriptorPool dpool = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateDescriptorPool(context->device, ci, context->allocation_callbacks, &dpool));
    CDSVK__CHECK(cdsvk_name_descriptor_pool(context->device, dpool, name));
    return dpool;
}

CDSVKDEF VkDescriptorPool cdsvk_create_descriptor_pool_from_layout(cdsvk_context const *c, const VkDescriptorSetLayoutCreateInfo *layout_ci, uint32_t max_sets,
    VkDescriptorPoolCreateFlags flags, const char *name)
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
        CDSVK_ASSERT(
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
    return cdsvk_create_descriptor_pool(c, &pool_ci, name);
}

CDSVKDEF void cdsvk_destroy_descriptor_pool(cdsvk_context const *c, VkDescriptorPool pool)
{
    vkDestroyDescriptorPool(c->device, pool, c->allocation_callbacks);
}

CDSVKDEF VkResult cdsvk_buffer_load_contents(cdsvk_context const *context, VkBuffer dst_buffer,
    VkBufferCreateInfo const *dst_ci, VkDeviceSize dst_offset,
    const void *src_data, VkDeviceSize src_size, VkAccessFlagBits final_access_flags)
{
    // TODO(cort): Make sure I'm clear that dst_offset is relative to buffer start, not relative
    // to the backing VkDeviceMemory objects!
    CDSVK_ASSERT(src_size <= dst_offset + dst_ci->size);
    CDSVK_ASSERT(dst_ci->usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    (void)dst_ci; // only needed for asserts

    VkBufferCreateInfo staging_buffer_create_info = {};
    staging_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_create_info.pNext = NULL;
    staging_buffer_create_info.flags = 0;
    staging_buffer_create_info.size = src_size;
    staging_buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    staging_buffer_create_info.queueFamilyIndexCount = 0;
    staging_buffer_create_info.pQueueFamilyIndices = NULL;
    VkBuffer staging_buffer = cdsvk_create_buffer(context, &staging_buffer_create_info,
        "cdsvk_buffer_load_contents() staging");
    // TODO(cort): pass an arena to allocate from
    cdsvk_device_memory_arena *device_arena = NULL;
    VkDeviceMemory staging_buffer_mem = VK_NULL_HANDLE;
    VkDeviceSize staging_buffer_mem_offset = 0;
    CDSVK__CHECK(cdsvk_allocate_and_bind_buffer_memory(context, staging_buffer, device_arena,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "cdsvk_buffer_load_contents() staging buffer memory",
        &staging_buffer_mem, &staging_buffer_mem_offset));

    void *mapped_data = NULL;
    VkMemoryMapFlags map_flags = 0;
    CDSVK__CHECK( vkMapMemory(context->device, staging_buffer_mem, 0, src_size, map_flags, &mapped_data) );
    memcpy(mapped_data, src_data, src_size);
    vkUnmapMemory(context->device, staging_buffer_mem);

    VkCommandPoolCreateInfo cpool_ci;
    cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpool_ci.pNext = NULL;
    cpool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    cpool_ci.queueFamilyIndex = context->graphics_queue_family_index;
    VkCommandPool cpool = cdsvk_create_command_pool(context, &cpool_ci, "cdsvk_buffer_load_contents temp cpool");

    VkFenceCreateInfo fence_ci = {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.pNext = NULL;
    fence_ci.flags = 0;
    VkFence fence = cdsvk_create_fence(context, &fence_ci, "cdsvk_buffer_load_contents temp fence");

    VkCommandBufferAllocateInfo cb_allocate_info = {};
    cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_allocate_info.pNext = NULL;
    cb_allocate_info.commandPool = cpool;
    cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_allocate_info.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    CDSVK__CHECK( vkAllocateCommandBuffers(context->device, &cb_allocate_info, &cb) );
    CDSVK__CHECK(cdsvk_name_command_buffer(context->device, cb, "cdsvk_load_buffer_contents() cb"));
    VkCommandBufferBeginInfo cb_begin_info = {};
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cb_begin_info.pNext = NULL;
    cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT ;
    cb_begin_info.pInheritanceInfo = NULL;
    CDSVK__CHECK( vkBeginCommandBuffer(cb, &cb_begin_info) );

    VkBufferMemoryBarrier buf_barriers[2] = {};
    buf_barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buf_barriers[0].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    buf_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    buf_barriers[0].srcQueueFamilyIndex = context->graphics_queue_family_index;
    buf_barriers[0].dstQueueFamilyIndex = context->graphics_queue_family_index;
    buf_barriers[0].buffer = staging_buffer;
    buf_barriers[0].offset = 0;
    buf_barriers[0].size = src_size;
    buf_barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buf_barriers[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    buf_barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    buf_barriers[1].srcQueueFamilyIndex = context->graphics_queue_family_index;
    buf_barriers[1].dstQueueFamilyIndex = context->graphics_queue_family_index;
    buf_barriers[1].buffer = dst_buffer;
    buf_barriers[1].offset = dst_offset;
    buf_barriers[1].size = src_size;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
        0,NULL, 2,buf_barriers, 0,NULL);

    VkBufferCopy buffer_copy_region = {};
    buffer_copy_region.srcOffset = 0;
    buffer_copy_region.dstOffset = dst_offset;
    buffer_copy_region.size = src_size;
    vkCmdCopyBuffer(cb, staging_buffer, dst_buffer, 1, &buffer_copy_region);

    buf_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    buf_barriers[1].dstAccessMask = final_access_flags;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
        0,NULL, 1,&buf_barriers[1], 0,NULL);

    CDSVK__CHECK( vkEndCommandBuffer(cb) );
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cb;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = NULL;
    CDSVK__CHECK( vkQueueSubmit(context->graphics_queue, 1, &submit_info, fence) );
    CDSVK__CHECK( vkWaitForFences(context->device, 1, &fence, VK_TRUE, UINT64_MAX) );

    cdsvk_free_device_memory(context, device_arena, staging_buffer_mem, staging_buffer_mem_offset);
    cdsvk_destroy_buffer(context, staging_buffer);
    cdsvk_destroy_fence(context, fence);
    cdsvk_destroy_command_pool(context, cpool);

    return VK_SUCCESS;
}

static VkImage cdsvk__create_staging_image(cdsvk_context const *context, VkImageCreateInfo const *final_ci,
    VkImageSubresource subresource)
{
    VkImageCreateInfo staging_ci = *final_ci;
    staging_ci.flags &= ~VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    staging_ci.tiling = VK_IMAGE_TILING_LINEAR;
    staging_ci.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    staging_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    staging_ci.queueFamilyIndexCount = 0;
    staging_ci.pQueueFamilyIndices = NULL;
    staging_ci.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    staging_ci.arrayLayers = 1;
    staging_ci.mipLevels = 1;
    staging_ci.extent.width  = cdsvk__max(final_ci->extent.width  >> subresource.mipLevel, 1U);
    staging_ci.extent.height = cdsvk__max(final_ci->extent.height >> subresource.mipLevel, 1U);
    staging_ci.extent.depth  = cdsvk__max(final_ci->extent.depth  >> subresource.mipLevel, 1U);
    return cdsvk_create_image(context, &staging_ci, staging_ci.initialLayout, 0, "cdsvk staging image");
}

CDSVKDEF VkSubresourceLayout cdsvk_image_get_subresource_source_layout(cdsvk_context const *context,
    VkImageCreateInfo const *ci, VkImageSubresource subresource)
{
    // TODO(cort): validate subresource against image bounds
    VkImage staging_image_temp = cdsvk__create_staging_image(context, ci, subresource);
    VkSubresourceLayout sub_layout;
    vkGetImageSubresourceLayout(context->device, staging_image_temp, &subresource, &sub_layout);
    cdsvk_destroy_image(context, staging_image_temp);
    return sub_layout;
}

CDSVKDEF VkResult cdsvk_image_load_subresource(cdsvk_context const *context, VkImage dst_image,
    VkImageCreateInfo const *dst_ci, VkImageSubresource subresource, VkSubresourceLayout subresource_layout,
    VkImageLayout final_image_layout, VkAccessFlagBits final_access_flags, void const *pixels)
{
    CDSVK_ASSERT(dst_ci->usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    VkImage staging_image = cdsvk__create_staging_image(context, dst_ci, subresource);
    // TODO(cort): pass in a device_arena to allocate from.
    cdsvk_device_memory_arena *device_arena = NULL;
    VkDeviceMemory staging_device_memory = VK_NULL_HANDLE;
    VkDeviceSize staging_memory_offset = 0;
    CDSVK__CHECK(cdsvk_allocate_and_bind_image_memory(context, staging_image, device_arena,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "cdsvk_image_load_subresource() staging image memory",
        &staging_device_memory, &staging_memory_offset));

    VkSubresourceLayout layout_sanity_check = {};
    vkGetImageSubresourceLayout(context->device, staging_image, &subresource, &layout_sanity_check);
    CDSVK_ASSERT(
        layout_sanity_check.offset     == subresource_layout.offset &&
        layout_sanity_check.size       == subresource_layout.size &&
        layout_sanity_check.rowPitch   == subresource_layout.rowPitch &&
        layout_sanity_check.arrayPitch == subresource_layout.arrayPitch &&
        layout_sanity_check.depthPitch == subresource_layout.depthPitch);

    void *mapped_subresource_data = NULL;
    VkMemoryMapFlags memory_map_flags = 0;
    // TODO(cort): return memory reqs from allocate_and_bind functions
    VkMemoryRequirements staging_memory_reqs = {};
    vkGetImageMemoryRequirements(context->device, staging_image, &staging_memory_reqs);
    CDSVK__CHECK( vkMapMemory(context->device, staging_device_memory, staging_memory_offset,
        staging_memory_reqs.size, memory_map_flags, &mapped_subresource_data) );
    memcpy(mapped_subresource_data, pixels, subresource_layout.size);
    vkUnmapMemory(context->device, staging_device_memory);

    VkCommandPoolCreateInfo cpool_ci;
    cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpool_ci.pNext = NULL;
    cpool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    cpool_ci.queueFamilyIndex = context->graphics_queue_family_index;
    VkCommandPool cpool = cdsvk_create_command_pool(context, &cpool_ci, "cdsvk_image_load_subresource temp cpool");

    VkFenceCreateInfo fence_ci = {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.pNext = NULL;
    fence_ci.flags = 0;
    VkFence fence = cdsvk_create_fence(context, &fence_ci, "cdsvk_image_load_subresource temp fence");

    VkCommandBufferAllocateInfo cb_allocate_info = {};
    cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_allocate_info.pNext = NULL;
    cb_allocate_info.commandPool = cpool;
    cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_allocate_info.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    CDSVK__CHECK( vkAllocateCommandBuffers(context->device, &cb_allocate_info, &cb) );
    CDSVK__CHECK(cdsvk_name_command_buffer(context->device, cb, "cdsvk_image_load_subresource cb"));
    VkCommandBufferBeginInfo cb_begin_info = {};
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cb_begin_info.pNext = NULL;
    cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT ;
    cb_begin_info.pInheritanceInfo = NULL;
    CDSVK__CHECK( vkBeginCommandBuffer(cb, &cb_begin_info) );

    VkImageSubresourceRange src_sub_range = {};
    src_sub_range.aspectMask = cdsvk__image_aspect_from_format(dst_ci->format);
    src_sub_range.baseMipLevel = 0;
    src_sub_range.baseArrayLayer = 0;
    src_sub_range.levelCount = 1;
    src_sub_range.layerCount = 1;
    VkImageSubresourceRange dst_sub_range = {};
    dst_sub_range.aspectMask = src_sub_range.aspectMask;
    dst_sub_range.baseMipLevel = subresource.mipLevel;
    dst_sub_range.levelCount = 1;
    dst_sub_range.baseArrayLayer = subresource.arrayLayer;
    dst_sub_range.layerCount = 1;
    VkImageMemoryBarrier img_barriers[2] = {};
    img_barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_barriers[0].pNext = NULL;
    img_barriers[0].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    img_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    img_barriers[0].oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    img_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    img_barriers[0].image = staging_image;
    img_barriers[0].subresourceRange = src_sub_range;
    img_barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_barriers[1].pNext = NULL;
    img_barriers[1].srcAccessMask = 0;
    img_barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    img_barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    img_barriers[1].image = dst_image;
    img_barriers[1].subresourceRange = dst_sub_range;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
        0,NULL, 0,NULL, 2,img_barriers);

    VkImageCopy copy_region = {};
    copy_region.srcSubresource.aspectMask = src_sub_range.aspectMask;
    copy_region.srcSubresource.baseArrayLayer = src_sub_range.baseArrayLayer;
    copy_region.srcSubresource.layerCount = src_sub_range.layerCount;
    copy_region.srcSubresource.mipLevel = src_sub_range.baseMipLevel;
    copy_region.srcOffset.x = 0;
    copy_region.srcOffset.y = 0;
    copy_region.srcOffset.z = 0;
    copy_region.dstSubresource.aspectMask = dst_sub_range.aspectMask;
    copy_region.dstSubresource.baseArrayLayer = dst_sub_range.baseArrayLayer;
    copy_region.dstSubresource.layerCount = dst_sub_range.layerCount;
    copy_region.dstSubresource.mipLevel = dst_sub_range.baseMipLevel;
    copy_region.dstOffset.x = 0;
    copy_region.dstOffset.y = 0;
    copy_region.dstOffset.z = 0;
    copy_region.extent.width  = cdsvk__max(dst_ci->extent.width  >> subresource.mipLevel, 1U);
    copy_region.extent.height = cdsvk__max(dst_ci->extent.height >> subresource.mipLevel, 1U);
    copy_region.extent.depth  = cdsvk__max(dst_ci->extent.depth  >> subresource.mipLevel, 1U);
    vkCmdCopyImage(cb,
        staging_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    img_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_barriers[1].dstAccessMask = final_access_flags;
    img_barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    img_barriers[1].newLayout = final_image_layout;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
        0,NULL, 0,NULL, 1,&img_barriers[1]);

    CDSVK__CHECK( vkEndCommandBuffer(cb) );
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cb;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = NULL;
    CDSVK__CHECK( vkQueueSubmit(context->graphics_queue, 1, &submit_info, fence) );
    CDSVK__CHECK( vkWaitForFences(context->device, 1, &fence, VK_TRUE, UINT64_MAX) );

    cdsvk_free_device_memory(context, device_arena, staging_device_memory, staging_memory_offset);
    cdsvk_destroy_image(context, staging_image);
    cdsvk_destroy_fence(context, fence);
    cdsvk_destroy_command_pool(context, cpool);

    return VK_SUCCESS;
}

////////////////////// Shader module loading

#ifndef CDSVK_NO_STDIO
CDSVKDEF VkShaderModule cdsvk_load_shader_from_file(cdsvk_context const *c, FILE *f, int len, const char *name)
{
    void *shader_bin = cdsvk__host_alloc(len, sizeof(void*), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT, c->allocation_callbacks);
    size_t bytes_read = fread(shader_bin, 1, len, f);
    if ( (int)bytes_read != len)
    {
        cdsvk__host_free(shader_bin, c->allocation_callbacks);
        return VK_NULL_HANDLE;
    }
    VkShaderModule shader_module = cdsvk_load_shader_from_memory(c, (const cdsvk_uc*)shader_bin, len, name);
    cdsvk__host_free(shader_bin, c->allocation_callbacks);
    return shader_module;
}
CDSVKDEF VkShaderModule cdsvk_load_shader(cdsvk_context const *c, char const *filename)
{
    FILE *spv_file = cdsvk__fopen(filename, "rb");
    if (!spv_file)
    {
        return VK_NULL_HANDLE;
    }
    fseek(spv_file, 0, SEEK_END);
    long spv_file_size = ftell(spv_file);
    fseek(spv_file, 0, SEEK_SET);
    VkShaderModule shader_module = cdsvk_load_shader_from_file(c, spv_file, spv_file_size, filename);
    fclose(spv_file);
    return shader_module;
}
#endif

CDSVKDEF VkShaderModule cdsvk_load_shader_from_memory(cdsvk_context const *c, cdsvk_uc const *buffer, int len, const char *name)
{
    VkShaderModuleCreateInfo smci = {};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.pNext = NULL;
    smci.flags = 0;
    smci.codeSize = len;
    smci.pCode = (uint32_t*)buffer;
    VkShaderModule shader_module = VK_NULL_HANDLE;
    CDSVK__CHECK( vkCreateShaderModule(c->device, &smci, c->allocation_callbacks, &shader_module) );
    CDSVK__CHECK(cdsvk_name_shader_module(c->device, shader_module, name));
    return shader_module;
}
CDSVKDEF VkShaderModule cdsvk_load_shader_from_callbacks(cdsvk_context const * /*c*/, cdsvk_io_callbacks const * /*clbk*/, void * /*user*/,
    const char * /*name*/)
{
    return VK_NULL_HANDLE;
}

CDSVKDEF void cdsvk_destroy_shader(cdsvk_context const *c, VkShaderModule shader)
{
    vkDestroyShaderModule(c->device, shader, c->allocation_callbacks);
}


////////////////////////// VkGraphicsPipelineCreateInfo helpers

CDSVKDEF int cdsvk_prepare_graphics_pipeline_create_info_vsps(
    cdsvk_graphics_pipeline_settings_vsps const *settings,
    cdsvk_graphics_pipeline_create_info *out_create_info)
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
    CDSVK_ASSERT(settings->vertex_buffer_layout.attribute_count <=
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
    CDSVK_ASSERT(out_create_info->dynamic_state_create_info.dynamicStateCount <= VK_DYNAMIC_STATE_RANGE_SIZE);

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

#endif // CDS_VULKAN_IMPLEMENTATION

#endif