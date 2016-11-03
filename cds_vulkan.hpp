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

#include <vulkan/vulkan.h>
#include <memory>
#include <mutex>
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
    // Shortcut to populate a VkGraphicsPipelineCreateInfo for graphics, using reasonable default values wherever
    // possible. The final VkGraphicsPipelineCreateInfo can still be customized before the pipeline is created.
    struct VertexBufferLayout {
        uint32_t stride;
        VkVertexInputRate input_rate;
        std::vector<VkVertexInputAttributeDescription> attributes;
    };
    struct GraphicsPipelineSettingsVsPs {
        VertexBufferLayout vertex_buffer_layout; // assumed to be bound at slot 0
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
    };
    class GraphicsPipelineCreateInfo {
    public:
        GraphicsPipelineCreateInfo();
        explicit GraphicsPipelineCreateInfo(const GraphicsPipelineSettingsVsPs &settings);
        // Cast to standard Vulkan object
        operator const VkGraphicsPipelineCreateInfo&() const {
            return graphics_pipeline_ci;
        }
        operator VkGraphicsPipelineCreateInfo&() {
            return graphics_pipeline_ci;
        }

	    VkGraphicsPipelineCreateInfo graphics_pipeline_ci;
        // Various structures referred to by graphics_pipeline_ci:
	    std::vector<VkPipelineShaderStageCreateInfo> shader_stage_cis;
        std::vector<VkVertexInputBindingDescription> vertex_input_binding_descriptions;
        std::vector<VkVertexInputAttributeDescription> vertex_input_attribute_descriptions;
        VkPipelineVertexInputStateCreateInfo vertex_input_state_ci;
	    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_ci;
        VkPipelineTessellationStateCreateInfo tessellation_state_ci;
        std::vector<VkViewport> viewports;
        std::vector<VkRect2D> scissor_rects;
	    VkPipelineViewportStateCreateInfo viewport_state_ci;
	    VkPipelineRasterizationStateCreateInfo rasterization_state_ci;
	    VkPipelineMultisampleStateCreateInfo multisample_state_ci;
	    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_ci;
	    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachment_states;
	    VkPipelineColorBlendStateCreateInfo color_blend_state_ci;
        std::vector<VkDynamicState> dynamic_states;
	    VkPipelineDynamicStateCreateInfo dynamic_state_ci;
    };

    // Interface for device memory allocators.
    class DeviceMemoryAllocator {
    public:
        virtual VkResult allocate(const VkMemoryAllocateInfo &alloc_info, VkDeviceSize alignment,
                VkDeviceMemory *out_mem, VkDeviceSize *out_offset) = 0;
        virtual void free(VkDeviceMemory mem, VkDeviceSize offset) = 0;
    };
    // Default device memory allocator (used internally if NULL is passed for a device allocator).
    // Just forwards allocations to the default vkAllocateMemory()/vkFreeMemory().
    class DefaultDeviceMemoryAllocator : public DeviceMemoryAllocator {
    public:
        explicit DefaultDeviceMemoryAllocator(VkDevice device, const VkAllocationCallbacks *allocation_callbacks = nullptr) :
               device_(device),
               allocation_callbacks_(allocation_callbacks) {
        }
        virtual VkResult allocate(const VkMemoryAllocateInfo &alloc_info, VkDeviceSize alignment,
               VkDeviceMemory *out_mem, VkDeviceSize *out_offset) {
            (void)alignment;
            *out_offset = 0;
            return vkAllocateMemory(device_, &alloc_info, allocation_callbacks_, out_mem);
        }
        virtual void free(VkDeviceMemory mem, VkDeviceSize offset) {
            (void)offset;
            vkFreeMemory(device_, mem, allocation_callbacks_);
        }
    private:
        VkDevice device_;
        const VkAllocationCallbacks *allocation_callbacks_;
    };

    typedef VkSurfaceKHR FN_GetVkSurface(VkInstance instance, const VkAllocationCallbacks *allocation_callbacks, void *userdata);

    struct ContextCreateInfo {
        VkAllocationCallbacks *allocation_callbacks;

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

        const VkApplicationInfo *application_info; // Used to initialize VkInstance. Optional; set to NULL for default values.
        PFN_vkDebugReportCallbackEXT debug_report_callback; // Optional; set to NULL to disable debug reports.
        VkDebugReportFlagsEXT debug_report_flags; // Optional; ignored if debug_report_callback is NULL.
        void *debug_report_callback_user_data; // Optional; passed to debug_report_callback, if enabled.
    };

    class Context {
    public:
        explicit Context(const ContextCreateInfo &context_ci);
        ~Context();

        VkInstance instance() const { return instance_; }
        VkPhysicalDevice physical_device() const { return physical_device_; }
        VkDevice device() const { return device_; }
        uint32_t graphics_queue_family_index() const { return graphics_queue_family_index_; }
        VkSwapchainKHR swapchain() const { return swapchain_; }
        VkQueue graphics_queue() const { return graphics_queue_; }
        VkQueue present_queue() const { return present_queue_; }
        VkFormat swapchain_format() const { return swapchain_surface_format_.format; }
        const std::vector<VkImageView>& swapchain_image_views() const { return swapchain_image_views_; }

        // Active layer/extension queries
        bool is_instance_layer_enabled(const std::string& layer_name) const;
        bool is_instance_extension_enabled(const std::string& ext_name) const;
        bool is_device_extension_enabled(const std::string& ext_name) const;

        // Load/destroy shader modules
        VkShaderModule load_shader_from_memory(const void *buf, int len, const std::string &name = "Anonymous") const;
        VkShaderModule load_shader_from_file(FILE *f, int len, const std::string &name = "Anonymous") const;
        VkShaderModule load_shader(const std::string &filename, const std::string &name = "Anonymous") const;
        void destroy_shader(VkShaderModule shader) const;

        // Create/destroy helpers for various Vulkan objects.
        // Mostly just a way to avoid passing device/allocation_callbacks everywhere, but in a few cases
        // more significant shortcuts are provided.
        VkCommandPool create_command_pool(const VkCommandPoolCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_command_pool(VkCommandPool cpool) const;

        VkSemaphore create_semaphore(const VkSemaphoreCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_semaphore(VkSemaphore semaphore) const;

        VkFence create_fence(const VkFenceCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_fence(VkFence fence) const;

        VkEvent create_event(const VkEventCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_event(VkEvent event) const;

        VkQueryPool create_query_pool(const VkQueryPoolCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_query_pool(VkQueryPool pool) const;

        VkPipelineCache create_pipeline_cache(const VkPipelineCacheCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_pipeline_cache(VkPipelineCache cache) const;

        VkPipelineLayout create_pipeline_layout(const VkPipelineLayoutCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_pipeline_layout(VkPipelineLayout layout) const;

        VkRenderPass create_render_pass(const VkRenderPassCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_render_pass(VkRenderPass render_pass) const;

        VkPipeline create_graphics_pipeline(const VkGraphicsPipelineCreateInfo &ci, const std::string &name = "Anonymous") const;
        VkPipeline create_compute_pipeline(const VkComputePipelineCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_pipeline(VkPipeline pipeline) const;

        VkDescriptorSetLayout create_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_descriptor_set_layout(VkDescriptorSetLayout layout) const;

        VkSampler create_sampler(const VkSamplerCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_sampler(VkSampler sampler) const;

        VkFramebuffer create_framebuffer(const VkFramebufferCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_framebuffer(VkFramebuffer framebuffer) const;

        VkBuffer create_buffer(const VkBufferCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_buffer(VkBuffer buffer) const;

        VkBufferView create_buffer_view(const VkBufferViewCreateInfo &ci, const std::string &name = "Anonymous") const;
        VkBufferView create_buffer_view(VkBuffer buffer, VkFormat format, const std::string &name = "Anonymous") const;
        void destroy_buffer_view(VkBufferView view) const;

        VkImage create_image(const VkImageCreateInfo &ci, VkImageLayout final_layout,
            VkAccessFlags final_access_flags, const std::string &name = "Anonymous") const;
        void destroy_image(VkImage image) const;

        VkImageView create_image_view(const VkImageViewCreateInfo &ci, const std::string &name = "Anonymous") const;
        VkImageView create_image_view(VkImage image, const VkImageCreateInfo &image_ci, const std::string &name = "Anonymous") const;
        void destroy_image_view(VkImageView view) const;

        VkDescriptorPool create_descriptor_pool(const VkDescriptorPoolCreateInfo &ci, const std::string &name = "Anonymous") const;
        VkDescriptorPool create_descriptor_pool(const VkDescriptorSetLayoutCreateInfo &layout_ci, uint32_t max_sets,
            VkDescriptorPoolCreateFlags flags = 0, const std::string &name = "Anonymous") const;
        void destroy_descriptor_pool(VkDescriptorPool pool) const;

        // Object naming (using VK_EXT_debug_marker, if present)
#ifndef VK_EXT_debug_marker
        // TODO(cort): If we had a VkT -> VK_DEBUG_REPORT_OBJECT_TYPE_* mapping, we could use
        // a template for both cases...
        template<typename VkT>
        VkResult set_debug_name(VkT handle, const std::string &name) const {
            (void)handle;
            (void)name;
            return VK_SUCCESS;
        }
#else
        VkResult set_debug_name(VkInstance name_me, const std::string &name) const { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT, name); }
        VkResult set_debug_name(VkPhysicalDevice name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT, name); }
        VkResult set_debug_name(VkDevice name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT, name); }
        VkResult set_debug_name(VkQueue name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT, name); }
        VkResult set_debug_name(VkSemaphore name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, name); }
        VkResult set_debug_name(VkCommandBuffer name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, name); }
        VkResult set_debug_name(VkFence name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, name); }
        VkResult set_debug_name(VkDeviceMemory name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, name); }
        VkResult set_debug_name(VkBuffer name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name); }
        VkResult set_debug_name(VkImage name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, name); }
        VkResult set_debug_name(VkEvent name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT, name); }
        VkResult set_debug_name(VkQueryPool name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT, name); }
        VkResult set_debug_name(VkBufferView name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT, name); }
        VkResult set_debug_name(VkImageView name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, name); }
        VkResult set_debug_name(VkShaderModule name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, name); }
        VkResult set_debug_name(VkPipelineCache name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT, name); }
        VkResult set_debug_name(VkPipelineLayout name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, name); }
        VkResult set_debug_name(VkRenderPass name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, name); }
        VkResult set_debug_name(VkPipeline name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, name); }
        VkResult set_debug_name(VkDescriptorSetLayout name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, name); }
        VkResult set_debug_name(VkSampler name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, name); }
        VkResult set_debug_name(VkDescriptorPool name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT, name); }
        VkResult set_debug_name(VkDescriptorSet name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, name); }
        VkResult set_debug_name(VkFramebuffer name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, name); }
        VkResult set_debug_name(VkCommandPool name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT, name); }
        VkResult set_debug_name(VkSurfaceKHR name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT, name); }
        VkResult set_debug_name(VkSwapchainKHR name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT, name); }
        VkResult set_debug_name(VkDebugReportCallbackEXT name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_EXT, name); }
#endif

        // Helpers for one-shot command buffers.
        // Begin: allocates a one-time-submit command buffer from an internal, mutex-protected command pool,
        //        and puts it into a reccordable state.
        // End: ends recording, submits the command buffer on the graphics queue, waits on a fence for it to complete,
        //      frees the command buffer, and sets it to VK_NULL_HANDLE.
        VkCommandBuffer begin_one_shot_command_buffer(void) const;
        VkResult end_and_submit_one_shot_command_buffer(VkCommandBuffer *cb) const;

        VkResult allocate_device_memory(const VkMemoryRequirements &mem_reqs, VkMemoryPropertyFlags memory_properties_mask,
            VkDeviceMemory *out_mem, VkDeviceSize *out_offset, DeviceMemoryAllocator *device_allocator = nullptr) const;
        void free_device_memory(VkDeviceMemory mem, VkDeviceSize offset, DeviceMemoryAllocator *device_allocator = nullptr) const;
        // Shortcuts for the most common types of allocations
        VkResult allocate_and_bind_image_memory(VkImage image, VkMemoryPropertyFlags memory_properties_mask,
            VkDeviceMemory *out_mem, VkDeviceSize *out_offset, DeviceMemoryAllocator *device_allocator = nullptr) const;
        VkResult allocate_and_bind_buffer_memory(VkBuffer buffer, VkMemoryPropertyFlags memory_properties_mask,
            VkDeviceMemory *out_mem, VkDeviceSize *out_offset, DeviceMemoryAllocator *device_allocator = nullptr) const;
        // Helper to locate the optimal memory type for a given allocation.
        uint32_t find_memory_type_index(const VkMemoryRequirements &memory_reqs,
            VkMemoryPropertyFlags memory_properties_mask) const;

        // Load buffer contents
        VkResult Context::load_buffer_contents(VkBuffer dst_buffer,
            const VkBufferCreateInfo &dst_buffer_ci, VkDeviceSize dst_offset,
            const void *src_data, VkDeviceSize src_size, VkAccessFlags final_access_flags) const;

    private:
        const VkAllocationCallbacks *allocation_callbacks_;
        std::unique_ptr<DefaultDeviceMemoryAllocator> default_device_allocator_;
        VkInstance instance_;
        VkDebugReportCallbackEXT debug_report_callback_;
        
        VkPhysicalDevice physical_device_;
        VkPhysicalDeviceProperties physical_device_properties_;
        VkPhysicalDeviceMemoryProperties physical_device_memory_properties_;
        VkPhysicalDeviceFeatures physical_device_features_;
        VkDevice device_;

        uint32_t graphics_queue_family_index_;
        VkQueueFamilyProperties graphics_queue_family_properties_;

        VkQueue graphics_queue_;

        VkPipelineCache pipeline_cache_;

        mutable std::mutex one_shot_cpool_mutex_;
        VkCommandPool one_shot_cpool_;

        // These members are only used if a present surface is passed at init time.
        VkSurfaceKHR present_surface_;
        VkQueue present_queue_;
        uint32_t present_queue_family_index_;
        VkQueueFamilyProperties present_queue_family_properties_;
        VkSwapchainKHR swapchain_;
        VkSurfaceFormatKHR swapchain_surface_format_;
        std::vector<VkImage> swapchain_images_;
        std::vector<VkImageView> swapchain_image_views_;

        // Allocate/free host memory using the provided allocation callbacks (or the default allocator
        // if no callbacks were provided)
        void *Context::host_alloc(size_t size, size_t alignment, VkSystemAllocationScope scope) const;
        void Context::host_free(void *ptr) const;

#if defined(VK_EXT_debug_marker)
        PFN_vkDebugMarkerSetObjectNameEXT pfn_vkDebugMarkerSetObjectName;
        PFN_vkDebugMarkerSetObjectTagEXT pfn_vkDebugMarkerSetObjectTag;
        VkResult set_debug_name_impl(uint64_t object_as_u64, VkDebugReportObjectTypeEXT object_type, const std::string &name) const;
#endif

        std::vector<VkLayerProperties> enabled_instance_layers_;
        std::vector<VkExtensionProperties> enabled_instance_extensions_;
        std::vector<VkExtensionProperties> enabled_device_extensions_;
    };
}  // namespace cdsvk

//
//
////   end header file   /////////////////////////////////////////////////////
#endif // CDSVK_INCLUDE_CDS_VULKAN_HPP

#if defined(CDS_VULKAN_IMPLEMENTATION)

#include <array>
#include <stdio.h>

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
        int err = (int)(expr);                             \
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
        int err = (int)(expr);                                                   \
        if (err != (expected)) {                                            \
            CDSVK_LOG("%s(%d): error in %s() -- %s returned %d", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
            /*__asm__("int $3"); */                 \
        }                                                                   \
        assert(err == (expected));                                          \
    } while(0)
#else
#   define CDSVK__RETVAL_CHECK(expected, expr) \
    do {  \
        int err = (int)(expr);                                                   \
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

namespace {
    VkImageAspectFlags vk_format_to_image_aspect(VkFormat format) {
        switch(format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        case VK_FORMAT_UNDEFINED:
            return static_cast<VkImageAspectFlagBits>(0);
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }
}  // namespace

cdsvk::GraphicsPipelineCreateInfo::GraphicsPipelineCreateInfo() : 
        graphics_pipeline_ci(),
        shader_stage_cis(),
        vertex_input_binding_descriptions(),
        vertex_input_attribute_descriptions(),
        vertex_input_state_ci(),
        input_assembly_state_ci(),
        tessellation_state_ci(),
        viewports{},
        scissor_rects{},
        viewport_state_ci(),
        rasterization_state_ci(),
        multisample_state_ci(),
        depth_stencil_state_ci(),
        color_blend_attachment_states(),
        color_blend_state_ci(),
        dynamic_states(),
        dynamic_state_ci() {
}
cdsvk::GraphicsPipelineCreateInfo::GraphicsPipelineCreateInfo(const GraphicsPipelineSettingsVsPs &settings) {
    shader_stage_cis.resize(2);
    shader_stage_cis[0] = {};
    shader_stage_cis[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_cis[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stage_cis[0].module = settings.vertex_shader;
    shader_stage_cis[0].pName = "main";
    shader_stage_cis[0].pSpecializationInfo = nullptr;
    shader_stage_cis[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_cis[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stage_cis[1].module = settings.fragment_shader;
    shader_stage_cis[1].pName = "main";
    shader_stage_cis[1].pSpecializationInfo = nullptr;

    vertex_input_binding_descriptions.resize(1); // TODO(cort): multiple vertex streams?
    vertex_input_binding_descriptions[0] = {};
    vertex_input_binding_descriptions[0].binding = 0;
    vertex_input_binding_descriptions[0].stride = settings.vertex_buffer_layout.stride;
    vertex_input_binding_descriptions[0].inputRate = settings.vertex_buffer_layout.input_rate;
    vertex_input_attribute_descriptions = settings.vertex_buffer_layout.attributes;
    for(auto &attr : vertex_input_attribute_descriptions) {
        attr.binding = vertex_input_binding_descriptions[0].binding;
    }
    vertex_input_state_ci = {};
    vertex_input_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state_ci.vertexBindingDescriptionCount = (uint32_t)vertex_input_binding_descriptions.size();
    vertex_input_state_ci.pVertexBindingDescriptions = vertex_input_binding_descriptions.data();
    vertex_input_state_ci.vertexAttributeDescriptionCount = (uint32_t)vertex_input_attribute_descriptions.size();
    vertex_input_state_ci.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data();

    input_assembly_state_ci = {};
    input_assembly_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_ci.topology = settings.primitive_topology;
    input_assembly_state_ci.primitiveRestartEnable = VK_FALSE;

    tessellation_state_ci = {};
    tessellation_state_ci.sType =VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

    viewports.resize(1);
    viewports[0] = settings.viewport;
    scissor_rects.resize(1);
    scissor_rects[0] = settings.scissor_rect;
    viewport_state_ci = {};
    viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_ci.viewportCount = (uint32_t)viewports.size();
    viewport_state_ci.pViewports = viewports.data();
    viewport_state_ci.scissorCount = (uint32_t)scissor_rects.size();
    viewport_state_ci.pScissors = scissor_rects.data();

    rasterization_state_ci = {};
    rasterization_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_ci.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_ci.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state_ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state_ci.depthClampEnable = VK_FALSE;
    rasterization_state_ci.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state_ci.depthBiasEnable = VK_FALSE;
    rasterization_state_ci.lineWidth = 1.0f;

    multisample_state_ci = {};
    multisample_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state_ci.pSampleMask = nullptr;
    multisample_state_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_state_ci.sampleShadingEnable = VK_FALSE;

    depth_stencil_state_ci = {};
    depth_stencil_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state_ci.depthTestEnable = VK_TRUE;
    depth_stencil_state_ci.depthWriteEnable = VK_TRUE;
    depth_stencil_state_ci.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depth_stencil_state_ci.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_state_ci.back = {};
    depth_stencil_state_ci.back.failOp = VK_STENCIL_OP_KEEP;
    depth_stencil_state_ci.back.passOp = VK_STENCIL_OP_KEEP;
    depth_stencil_state_ci.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depth_stencil_state_ci.front = {};
    depth_stencil_state_ci.front.failOp = VK_STENCIL_OP_KEEP;
    depth_stencil_state_ci.front.passOp = VK_STENCIL_OP_KEEP;
    depth_stencil_state_ci.front.compareOp = VK_COMPARE_OP_ALWAYS;
    depth_stencil_state_ci.stencilTestEnable = VK_FALSE;

    color_blend_attachment_states.resize(settings.subpass_color_attachment_count);
    for(auto &attachment : color_blend_attachment_states) {
        attachment = {};
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        attachment.blendEnable = VK_FALSE;
        //attachment.colorBlendOp = VK_BLEND_OP_ADD;
    }
    color_blend_state_ci = {};
    color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_ci.attachmentCount = (uint32_t)color_blend_attachment_states.size();
    color_blend_state_ci.pAttachments = color_blend_attachment_states.data();

    dynamic_states.reserve(VK_DYNAMIC_STATE_END_RANGE);
    for(int iDS=VK_DYNAMIC_STATE_BEGIN_RANGE; iDS<=VK_DYNAMIC_STATE_END_RANGE; ++iDS) {
        if (settings.dynamic_state_mask & (1<<iDS)) {
            dynamic_states.push_back((VkDynamicState)iDS);
        }
    }
    dynamic_state_ci = {};
    dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_ci.dynamicStateCount = (uint32_t)dynamic_states.size();
    dynamic_state_ci.pDynamicStates = dynamic_states.data();

    graphics_pipeline_ci = {};
    graphics_pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphics_pipeline_ci.stageCount = (uint32_t)shader_stage_cis.size();
    graphics_pipeline_ci.pStages = shader_stage_cis.data();
    graphics_pipeline_ci.pVertexInputState = &vertex_input_state_ci;
    graphics_pipeline_ci.pInputAssemblyState = &input_assembly_state_ci;
    graphics_pipeline_ci.pTessellationState = &tessellation_state_ci;
    graphics_pipeline_ci.pViewportState = &viewport_state_ci;
    graphics_pipeline_ci.pRasterizationState = &rasterization_state_ci;
    graphics_pipeline_ci.pMultisampleState = &multisample_state_ci;
    graphics_pipeline_ci.pDepthStencilState = &depth_stencil_state_ci;
    graphics_pipeline_ci.pColorBlendState = &color_blend_state_ci;
    graphics_pipeline_ci.pDynamicState = &dynamic_state_ci;
    graphics_pipeline_ci.layout = settings.pipeline_layout;
    graphics_pipeline_ci.renderPass = settings.render_pass;
    graphics_pipeline_ci.subpass = settings.subpass;
    graphics_pipeline_ci.basePipelineHandle = VK_NULL_HANDLE;
    graphics_pipeline_ci.basePipelineIndex = 0;
}


Context::Context(const ContextCreateInfo &context_ci) :
        allocation_callbacks_(context_ci.allocation_callbacks),
        default_device_allocator_(),
        instance_(VK_NULL_HANDLE),
        debug_report_callback_(VK_NULL_HANDLE),
        physical_device_(VK_NULL_HANDLE),
        physical_device_properties_(),
        physical_device_memory_properties_(),
        physical_device_features_(),
        device_(VK_NULL_HANDLE),
        graphics_queue_family_index_(VK_QUEUE_FAMILY_IGNORED),
        graphics_queue_family_properties_(),
        graphics_queue_(VK_NULL_HANDLE),
        pipeline_cache_(VK_NULL_HANDLE),
        one_shot_cpool_mutex_(),
        one_shot_cpool_(VK_NULL_HANDLE),
        present_surface_(VK_NULL_HANDLE),
        present_queue_(VK_NULL_HANDLE),
        present_queue_family_index_(VK_QUEUE_FAMILY_IGNORED),
        present_queue_family_properties_(),
        swapchain_(VK_NULL_HANDLE),
        swapchain_surface_format_({VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}),
        swapchain_images_(),
        swapchain_image_views_(),
#if defined(VK_EXT_debug_marker)
        pfn_vkDebugMarkerSetObjectName(nullptr),
        pfn_vkDebugMarkerSetObjectTag(nullptr),
#endif
        enabled_instance_layers_(),
        enabled_instance_extensions_(),
        enabled_device_extensions_() {
    // Build a list of instance layers to enable
    {
        uint32_t all_instance_layer_count = 0;
        std::vector<VkLayerProperties> all_instance_layers;
        VkResult result = VK_INCOMPLETE;
        do {
            result = vkEnumerateInstanceLayerProperties(&all_instance_layer_count, nullptr);
            if (result == VK_SUCCESS && all_instance_layer_count > 0) {
                all_instance_layers.resize(all_instance_layer_count);
                result = vkEnumerateInstanceLayerProperties(&all_instance_layer_count, all_instance_layers.data());
            }
        } while (result == VK_INCOMPLETE);
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
        std::vector<VkExtensionProperties> all_instance_extensions;
        // Build list of unique instance extensions across all enabled instance layers
        for(int32_t iLayer = -1; iLayer < (int32_t)enabled_instance_layers_.size(); ++iLayer) {
            const char *layer_name = (iLayer == -1) ? nullptr : enabled_instance_layers_[iLayer].layerName;
            uint32_t layer_instance_extension_count = 0;
            std::vector<VkExtensionProperties> layer_instance_extensions;
            VkResult result = VK_INCOMPLETE;
            do {
                result = vkEnumerateInstanceExtensionProperties(layer_name, &layer_instance_extension_count, nullptr);
                if (result == VK_SUCCESS && layer_instance_extension_count > 0) {
                    layer_instance_extensions.resize(layer_instance_extension_count);
                    result = vkEnumerateInstanceExtensionProperties(layer_name, &layer_instance_extension_count,
                        layer_instance_extensions.data());
                }
            } while (result == VK_INCOMPLETE);
            for(const auto &layer_extension : layer_instance_extensions) {
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

        VkApplicationInfo application_info_default = {};
        application_info_default.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info_default.pApplicationName = "Default Application Name";
        application_info_default.applicationVersion = 0x1000;
        application_info_default.pEngineName = "Default Engine Name";
        application_info_default.engineVersion = 0x1000;
        application_info_default.apiVersion = VK_MAKE_VERSION(1,0,0);

        VkInstanceCreateInfo instance_ci = {};
        instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_ci.pApplicationInfo = context_ci.application_info ? context_ci.application_info : &application_info_default;
        instance_ci.enabledLayerCount       = (uint32_t)enabled_instance_layer_names.size();
        instance_ci.ppEnabledLayerNames     = enabled_instance_layer_names.data();
        instance_ci.enabledExtensionCount   = (uint32_t)enabled_instance_extension_names.size();
        instance_ci.ppEnabledExtensionNames = enabled_instance_extension_names.data();
        CDSVK__CHECK(vkCreateInstance(&instance_ci, allocation_callbacks_, &instance_));
    }

    // Set up debug report callback
    if (context_ci.debug_report_callback && is_instance_extension_enabled(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) {
        CDSVK_ASSERT(context_ci.debug_report_flags); // enabling a callback with zero flags is pointless!
        VkDebugReportCallbackCreateInfoEXT debug_report_callback_ci = {};
        debug_report_callback_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debug_report_callback_ci.flags = (VkDebugReportFlagsEXT)context_ci.debug_report_flags;
        debug_report_callback_ci.pfnCallback = context_ci.debug_report_callback;
        debug_report_callback_ci.pUserData = context_ci.debug_report_callback_user_data;
        PFN_vkCreateDebugReportCallbackEXT my_vkCreateDebugReportCallback = 
            (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugReportCallbackEXT");
        VkDebugReportCallbackEXT callback = VK_NULL_HANDLE;
        CDSVK__CHECK(my_vkCreateDebugReportCallback(instance_, &debug_report_callback_ci, allocation_callbacks_, &callback));
        debug_report_callback_ = callback;
    }

    // Invoke callback to get a present surface
    if (context_ci.pfn_get_vk_surface) {
        present_surface_ = context_ci.pfn_get_vk_surface(instance_, allocation_callbacks_, context_ci.get_vk_surface_userdata);
    }

    // Select a physical device, after locating suitable queue families.
    {
        uint32_t physical_device_count = 0;
        std::vector<VkPhysicalDevice> all_physical_devices;
        VkResult result = VK_INCOMPLETE;
        do {
            result = vkEnumeratePhysicalDevices(instance_, &physical_device_count, nullptr);
            if (result == VK_SUCCESS && physical_device_count > 0) {
                all_physical_devices.resize(physical_device_count);
                result = vkEnumeratePhysicalDevices(instance_, &physical_device_count, all_physical_devices.data());
            }
        } while (result == VK_INCOMPLETE);
        bool found_graphics_queue_family = false;
        bool found_present_queue_family = false;
        physical_device_ = VK_NULL_HANDLE;
        graphics_queue_family_index_ = VK_QUEUE_FAMILY_IGNORED;
        present_queue_family_index_ = VK_QUEUE_FAMILY_IGNORED;
        for(auto physical_device : all_physical_devices) {
            found_graphics_queue_family = false;
            found_present_queue_family = false;
            uint32_t queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
            std::vector<VkQueueFamilyProperties> all_queue_family_properties(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, all_queue_family_properties.data());
            for(uint32_t iQF=0; (size_t)iQF < all_queue_family_properties.size(); ++iQF) {
                VkBool32 supports_graphics = (all_queue_family_properties[iQF].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
                VkBool32 supports_present = VK_FALSE;
                if (present_surface_) {
                    CDSVK__CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, iQF, present_surface_, &supports_present));
                }
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
        vkGetPhysicalDeviceProperties(physical_device_, &physical_device_properties_);
        vkGetPhysicalDeviceMemoryProperties(physical_device_, &physical_device_memory_properties_);
        vkGetPhysicalDeviceFeatures(physical_device_, &physical_device_features_);
    }

    // Build a list of device extensions to enable
    {
        std::vector<VkExtensionProperties> all_device_extensions;
        // Build list of unique device extensions across all enabled instance layers
        for(int32_t iLayer = -1; iLayer < (int32_t)enabled_instance_layers_.size(); ++iLayer) {
            const char *layer_name = (iLayer == -1) ? nullptr : enabled_instance_layers_[iLayer].layerName;
            uint32_t layer_device_extension_count = 0;
            std::vector<VkExtensionProperties> layer_device_extensions;
            VkResult result = VK_INCOMPLETE;
            do {
                result = vkEnumerateDeviceExtensionProperties(physical_device_, layer_name, &layer_device_extension_count, nullptr);
                if (result == VK_SUCCESS && layer_device_extension_count > 0) {
                    layer_device_extensions.resize(layer_device_extension_count);
                    result = vkEnumerateDeviceExtensionProperties(physical_device_, layer_name, &layer_device_extension_count,
                        layer_device_extensions.data());
                }
            } while (result == VK_INCOMPLETE);
            for(const auto &layer_extension : layer_device_extensions) {
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
        std::vector<float> graphics_queue_priorities(graphics_queue_family_properties_.queueCount);
        for(auto &priority : graphics_queue_priorities) {
            priority = 0.5f;
        }
        std::vector<VkDeviceQueueCreateInfo> device_queue_cis;
        device_queue_cis.reserve(2); // graphics + present
        VkDeviceQueueCreateInfo graphics_queue_ci = {};
        graphics_queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        graphics_queue_ci.queueFamilyIndex = graphics_queue_family_index_;
        graphics_queue_ci.queueCount = graphics_queue_family_properties_.queueCount;
        graphics_queue_ci.pQueuePriorities = graphics_queue_priorities.data();
        device_queue_cis.push_back(graphics_queue_ci);
        if (present_surface_ && present_queue_family_index_ != graphics_queue_family_index_) {
            std::vector<float> present_queue_priorities(present_queue_family_properties_.queueCount);
            for(auto &priority : present_queue_priorities) {
                priority = 0.5f;
            }
            VkDeviceQueueCreateInfo present_queue_ci = {};
            present_queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            present_queue_ci.queueFamilyIndex = present_queue_family_index_;
            present_queue_ci.queueCount = present_queue_family_properties_.queueCount;
            present_queue_ci.pQueuePriorities = present_queue_priorities.data();
            device_queue_cis.push_back(present_queue_ci);
        }

        // Build list of enabled extension names
        std::vector<const char*> enabled_device_extension_names;
        enabled_device_extension_names.reserve(enabled_device_extensions_.size());
        for(const auto &extension : enabled_device_extensions_) {
            enabled_device_extension_names.push_back(extension.extensionName);
        }

        VkDeviceCreateInfo device_ci = {};
        device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_ci.queueCreateInfoCount = (uint32_t)device_queue_cis.size();
        device_ci.pQueueCreateInfos = device_queue_cis.data();
        device_ci.enabledExtensionCount = (uint32_t)enabled_device_extension_names.size();
        device_ci.ppEnabledExtensionNames = enabled_device_extension_names.data();
        device_ci.pEnabledFeatures = &physical_device_features_;
        CDSVK__CHECK(vkCreateDevice(physical_device_, &device_ci, allocation_callbacks_, &device_));
        vkGetDeviceQueue(device_, graphics_queue_family_index_, 0, &graphics_queue_);
        if (present_surface_) {
            vkGetDeviceQueue(device_, present_queue_family_index_, 0, &present_queue_);
        }

        default_device_allocator_.reset(new DefaultDeviceMemoryAllocator(device_, allocation_callbacks_));
    }

    // TODO(cort): we can now assign debug names to things.
    {
#if defined(VK_EXT_debug_marker)
        if (is_device_extension_enabled(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
        {
            pfn_vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(device_, "vkDebugMarkerSetObjectNameEXT");
            pfn_vkDebugMarkerSetObjectTag  = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr(device_, "vkDebugMarkerSetObjectTagEXT");
        } else {
            pfn_vkDebugMarkerSetObjectName = nullptr;
            pfn_vkDebugMarkerSetObjectTag  = nullptr;
        }
#endif
        set_debug_name(instance_, "Context instance");
        //set_debug_name(physical_device_, "Context physical device"); // Currently broken; see https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1120
        set_debug_name(device_, "Context logical device");
    }

    // Create a pipeline cache. TODO(cort): optionally load cache contents from disk.
    {
        VkPipelineCacheCreateInfo pipeline_cache_ci = {};
        pipeline_cache_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        pipeline_cache_ = create_pipeline_cache(pipeline_cache_ci, "Context pipeline cache");
    }

    // Create a command pool for one-shot command buffers
    {
        VkCommandPoolCreateInfo cpool_ci = {};
        cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        cpool_ci.queueFamilyIndex = graphics_queue_family_index_;
        one_shot_cpool_ = create_command_pool(cpool_ci, "Context one-shot command pool");
    }

    // Create swapchain. TODO(cort): Should this be moved outside the context? It seems common enough for this
    // to be handled by a separate library.
    if (present_surface_) {
        VkSurfaceCapabilitiesKHR surface_caps = {};
        CDSVK__CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, present_surface_, &surface_caps));
        VkExtent2D swapchain_extent = surface_caps.currentExtent;
        if ((int32_t)swapchain_extent.width == -1) {
            CDSVK_ASSERT( (int32_t)swapchain_extent.height == -1 );
            // TODO(cort): better defaults here, when we can't detect the present surface extent?
            swapchain_extent.width =
                CDSVK__CLAMP(1280, surface_caps.minImageExtent.width, surface_caps.maxImageExtent.width);
            swapchain_extent.height =
                CDSVK__CLAMP( 720, surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height);
        }

        uint32_t device_surface_format_count = 0;
        std::vector<VkSurfaceFormatKHR> device_surface_formats;
        VkResult result = VK_INCOMPLETE;
        do {
            result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, present_surface_, &device_surface_format_count, nullptr);
            if (result == VK_SUCCESS && device_surface_format_count > 0) {
                device_surface_formats.resize(device_surface_format_count);
                result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, present_surface_, &device_surface_format_count,
                    device_surface_formats.data());
            }
        } while (result == VK_INCOMPLETE);
        if (device_surface_formats.size() == 1 && device_surface_formats[0].format == VK_FORMAT_UNDEFINED) {
            // No preferred format.
            swapchain_surface_format_.format = VK_FORMAT_B8G8R8A8_UNORM;
            swapchain_surface_format_.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        } else {
            CDSVK_ASSERT(device_surface_formats.size() >= 1);
            swapchain_surface_format_ = device_surface_formats[0];
        }

        uint32_t device_present_mode_count = 0;
        std::vector<VkPresentModeKHR> device_present_modes;
        do {
            result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, present_surface_, &device_present_mode_count, nullptr);
            if (result == VK_SUCCESS && device_present_mode_count > 0) {
                device_present_modes.resize(device_present_mode_count);
                result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, present_surface_, &device_present_mode_count,
                    device_present_modes.data());
            }
        } while (result == VK_INCOMPLETE);
        bool found_mailbox_mode = false;
        for(auto mode : device_present_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                found_mailbox_mode = true;
                break;
            }
        }
        VkPresentModeKHR present_mode = found_mailbox_mode ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;

        uint32_t desired_swapchain_image_count = surface_caps.minImageCount+1;
        if (surface_caps.maxImageCount > 0 && desired_swapchain_image_count > surface_caps.maxImageCount) {
            desired_swapchain_image_count = surface_caps.maxImageCount;
        }

        VkSurfaceTransformFlagBitsKHR surface_transform = surface_caps.currentTransform;

        VkImageUsageFlags swapchain_image_usage = 0
            | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
            ;
        CDSVK_ASSERT( (surface_caps.supportedUsageFlags & swapchain_image_usage) == swapchain_image_usage );

        CDSVK_ASSERT(surface_caps.supportedCompositeAlpha); // at least one mode must be supported
        CDSVK_ASSERT(surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
        VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        VkSwapchainKHR old_swapchain = VK_NULL_HANDLE;
        VkSwapchainCreateInfoKHR swapchain_ci = {};
        swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.surface = present_surface_;
        swapchain_ci.minImageCount = desired_swapchain_image_count;
        swapchain_ci.imageFormat = swapchain_surface_format_.format;
        swapchain_ci.imageColorSpace = swapchain_surface_format_.colorSpace;
        swapchain_ci.imageExtent.width = swapchain_extent.width;
        swapchain_ci.imageExtent.height = swapchain_extent.height;
        swapchain_ci.imageArrayLayers = 1;
        swapchain_ci.imageUsage = swapchain_image_usage;
        swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_ci.queueFamilyIndexCount = 0;
        swapchain_ci.pQueueFamilyIndices = NULL;
        swapchain_ci.preTransform = surface_transform;
        swapchain_ci.compositeAlpha = composite_alpha;
        swapchain_ci.presentMode = present_mode;
        swapchain_ci.clipped = VK_TRUE;
        swapchain_ci.oldSwapchain = old_swapchain;
        CDSVK__CHECK(vkCreateSwapchainKHR(device_, &swapchain_ci, allocation_callbacks_, &swapchain_));
        set_debug_name(swapchain_, "Context swapchain");
        if (old_swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, old_swapchain, allocation_callbacks_);
        }

        uint32_t swapchain_image_count = 0;
        do {
            result = vkGetSwapchainImagesKHR(device_, swapchain_, &swapchain_image_count, nullptr);
            if (result == VK_SUCCESS && swapchain_image_count > 0) {
                swapchain_images_.resize(swapchain_image_count);
                result = vkGetSwapchainImagesKHR(device_, swapchain_, &swapchain_image_count, swapchain_images_.data());
            }
        } while (result == VK_INCOMPLETE);
        VkImageViewCreateInfo image_view_ci = {};
        image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_ci.image = VK_NULL_HANDLE; // filled in below
        image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_ci.format = swapchain_surface_format_.format;
        image_view_ci.components = {};
        image_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_ci.subresourceRange = {};
        image_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_ci.subresourceRange.baseMipLevel = 0;
        image_view_ci.subresourceRange.levelCount = 1;
        image_view_ci.subresourceRange.baseArrayLayer = 0;
        image_view_ci.subresourceRange.layerCount = 1;
        swapchain_image_views_.reserve(swapchain_images_.size());
        for(auto image : swapchain_images_) {
            image_view_ci.image = image;
            swapchain_image_views_.push_back(create_image_view(image_view_ci, "Swapchain image view"));
        }
    }
}
Context::~Context() {
    if (device_) {
        vkDeviceWaitIdle(device_);

        default_device_allocator_.reset();
        destroy_command_pool(one_shot_cpool_);
        for(auto view : swapchain_image_views_) {
            destroy_image_view(view);
        }
        vkDestroySwapchainKHR(device_, swapchain_, allocation_callbacks_);
        vkDestroyPipelineCache(device_, pipeline_cache_, allocation_callbacks_);
        vkDestroyDevice(device_, allocation_callbacks_);
    }
    if (present_surface_) {
        vkDestroySurfaceKHR(instance_, present_surface_, allocation_callbacks_);
    }
    if (debug_report_callback_) {
        PFN_vkDestroyDebugReportCallbackEXT my_vkDestroyDebugReportCallback = 
            (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugReportCallbackEXT");
        my_vkDestroyDebugReportCallback(instance_, debug_report_callback_, (const VkAllocationCallbacks*)allocation_callbacks_);
    }
    vkDestroyInstance(instance_, allocation_callbacks_);
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

////////////////////// Shader module loading

VkShaderModule Context::load_shader_from_memory(const void *buffer, int len, const std::string &name) const {
    CDSVK_ASSERT( (len % sizeof(uint32_t)) == 0);
    VkShaderModuleCreateInfo shader_ci = {};
    shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_ci.codeSize = len;
    shader_ci.pCode = static_cast<const uint32_t*>(buffer);
    VkShaderModule shader = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateShaderModule(device_, &shader_ci, allocation_callbacks_, &shader));
    CDSVK__CHECK(set_debug_name(shader, name));
    return shader;
}
VkShaderModule Context::load_shader_from_file(FILE *f, int len, const std::string &name) const {
    std::vector<uint8_t> shader_bin(len);
    size_t bytes_read = fread(shader_bin.data(), 1, len, f);
    if ( (int)bytes_read != len) {
        return VK_NULL_HANDLE;
    }
    return load_shader_from_memory(shader_bin.data(), len, name);
}
VkShaderModule Context::load_shader(const std::string &filename, const std::string &name) const {
    FILE *spv_file = cdsvk__fopen(filename.c_str(), "rb");
    if (!spv_file) {
        return VK_NULL_HANDLE;
    }
    fseek(spv_file, 0, SEEK_END);
    long spv_file_size = ftell(spv_file);
    fseek(spv_file, 0, SEEK_SET);
    VkShaderModule shader = load_shader_from_file(spv_file, spv_file_size, name);
    fclose(spv_file);
    return shader;
}
void Context::destroy_shader(VkShaderModule shader) const {
    vkDestroyShaderModule(device_, shader, allocation_callbacks_);
}

VkCommandPool Context::create_command_pool(const VkCommandPoolCreateInfo &ci, const std::string &name) const {
    VkCommandPool object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateCommandPool(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_command_pool(VkCommandPool cpool) const {
    vkDestroyCommandPool(device_, cpool, allocation_callbacks_);
}

VkSemaphore Context::create_semaphore(const VkSemaphoreCreateInfo &ci, const std::string &name) const {
    VkSemaphore object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateSemaphore(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_semaphore(VkSemaphore semaphore) const {
    vkDestroySemaphore(device_, semaphore, allocation_callbacks_);
}

VkFence Context::create_fence(const VkFenceCreateInfo &ci, const std::string &name) const {
    VkFence object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateFence(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_fence(VkFence fence) const {
    vkDestroyFence(device_, fence, allocation_callbacks_);
}

VkEvent Context::create_event(const VkEventCreateInfo &ci, const std::string &name) const {
    VkEvent object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateEvent(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_event(VkEvent event) const {
    vkDestroyEvent(device_, event, allocation_callbacks_);
}

VkQueryPool Context::create_query_pool(const VkQueryPoolCreateInfo &ci, const std::string &name) const {
    VkQueryPool object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateQueryPool(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_query_pool(VkQueryPool pool) const {
    vkDestroyQueryPool(device_, pool, allocation_callbacks_);
}

VkPipelineCache Context::create_pipeline_cache(const VkPipelineCacheCreateInfo &ci, const std::string &name) const {
    VkPipelineCache object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreatePipelineCache(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_pipeline_cache(VkPipelineCache cache) const {
    vkDestroyPipelineCache(device_, cache, allocation_callbacks_);
}

VkPipelineLayout Context::create_pipeline_layout(const VkPipelineLayoutCreateInfo &ci, const std::string &name) const {
    VkPipelineLayout object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreatePipelineLayout(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_pipeline_layout(VkPipelineLayout layout) const {
    vkDestroyPipelineLayout(device_, layout, allocation_callbacks_);
}

VkRenderPass Context::create_render_pass(const VkRenderPassCreateInfo &ci, const std::string &name) const {
    VkRenderPass object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateRenderPass(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_render_pass(VkRenderPass render_pass) const {
    vkDestroyRenderPass(device_, render_pass, allocation_callbacks_);
}

VkPipeline Context::create_graphics_pipeline(const VkGraphicsPipelineCreateInfo &ci, const std::string &name) const {
    VkPipeline object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateGraphicsPipelines(device_, pipeline_cache_, 1, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
VkPipeline Context::create_compute_pipeline(const VkComputePipelineCreateInfo &ci, const std::string &name) const {
    VkPipeline object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateComputePipelines(device_, pipeline_cache_, 1, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_pipeline(VkPipeline pipeline) const {
    vkDestroyPipeline(device_, pipeline, allocation_callbacks_);
}

VkDescriptorSetLayout Context::create_descriptor_set_layout(const VkDescriptorSetLayoutCreateInfo &ci, const std::string &name) const {
    VkDescriptorSetLayout object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateDescriptorSetLayout(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_descriptor_set_layout(VkDescriptorSetLayout layout) const {
    vkDestroyDescriptorSetLayout(device_, layout, allocation_callbacks_);
}

VkSampler Context::create_sampler(const VkSamplerCreateInfo &ci, const std::string &name) const {
    VkSampler object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateSampler(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_sampler(VkSampler sampler) const {
    vkDestroySampler(device_, sampler, allocation_callbacks_);
}

VkFramebuffer Context::create_framebuffer(const VkFramebufferCreateInfo &ci, const std::string &name) const {
    VkFramebuffer object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateFramebuffer(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_framebuffer(VkFramebuffer framebuffer) const {
    vkDestroyFramebuffer(device_, framebuffer, allocation_callbacks_);
}

VkBuffer Context::create_buffer(const VkBufferCreateInfo &ci, const std::string &name) const {
    VkBuffer object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateBuffer(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_buffer(VkBuffer buffer) const {
    vkDestroyBuffer(device_, buffer, allocation_callbacks_);
}

VkBufferView Context::create_buffer_view(const VkBufferViewCreateInfo &ci, const std::string &name) const {
    VkBufferView object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateBufferView(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
VkBufferView Context::create_buffer_view(VkBuffer buffer, VkFormat format, const std::string &name) const {
    VkBufferViewCreateInfo view_ci = {};
    view_ci.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    view_ci.buffer = buffer;
    view_ci.format = format;
    view_ci.offset = 0;
    view_ci.range = VK_WHOLE_SIZE;
    return create_buffer_view(view_ci, name);
}
void Context::destroy_buffer_view(VkBufferView view) const {
    vkDestroyBufferView(device_, view, allocation_callbacks_);
}

VkImage Context::create_image(const VkImageCreateInfo &ci, VkImageLayout final_layout,
        VkAccessFlags final_access_flags, const std::string &name) const {
    VkImage object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateImage(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    if (ci.initialLayout != final_layout) {
        // Transition to final layout
        VkImageSubresourceRange sub_range = {};
        sub_range.aspectMask = vk_format_to_image_aspect(ci.format);
        sub_range.baseMipLevel = 0;
        sub_range.levelCount = VK_REMAINING_MIP_LEVELS;
        sub_range.baseArrayLayer = 0;
        sub_range.layerCount = VK_REMAINING_ARRAY_LAYERS;
        VkImageMemoryBarrier img_barrier = {};
        img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        img_barrier.srcAccessMask = 0;
        img_barrier.dstAccessMask = final_access_flags;
        img_barrier.oldLayout = ci.initialLayout;
        img_barrier.newLayout = final_layout;
        img_barrier.srcQueueFamilyIndex = graphics_queue_family_index_;
        img_barrier.dstQueueFamilyIndex = graphics_queue_family_index_;
        img_barrier.image = object;
        img_barrier.subresourceRange = sub_range;
        VkCommandBuffer cb = begin_one_shot_command_buffer();
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_DEPENDENCY_BY_REGION_BIT, 0,nullptr, 0,nullptr, 1,&img_barrier);
        CDSVK__CHECK(end_and_submit_one_shot_command_buffer(&cb));
    }
    return object;
}
void Context::destroy_image(VkImage image) const {
    vkDestroyImage(device_, image, allocation_callbacks_);
}

VkImageView Context::create_image_view(const VkImageViewCreateInfo &ci, const std::string &name) const {
    VkImageView object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateImageView(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
VkImageView Context::create_image_view(VkImage image, const VkImageCreateInfo &image_ci, const std::string &name) const {
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    if (image_ci.imageType == VK_IMAGE_TYPE_1D) {
        view_type = (image_ci.arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    } else if (image_ci.imageType == VK_IMAGE_TYPE_2D) {
        if (image_ci.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) {
            CDSVK_ASSERT((image_ci.arrayLayers) % 6 == 0);
            view_type = (image_ci.arrayLayers == 6) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        } else {
            view_type = (image_ci.arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
    } else if (image_ci.imageType == VK_IMAGE_TYPE_3D) {
        view_type = VK_IMAGE_VIEW_TYPE_3D;
    }
    VkImageViewCreateInfo view_ci = {};
    view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image = image;
    view_ci.viewType = view_type;
    view_ci.format = image_ci.format;
    view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_ci.subresourceRange.aspectMask = vk_format_to_image_aspect(view_ci.format);
    view_ci.subresourceRange.baseMipLevel = 0;
    view_ci.subresourceRange.levelCount = image_ci.mipLevels;
    view_ci.subresourceRange.baseArrayLayer = 0;
    view_ci.subresourceRange.layerCount = image_ci.arrayLayers;
    return create_image_view(view_ci, name);
}
void Context::destroy_image_view(VkImageView view) const {
    vkDestroyImageView(device_, view, allocation_callbacks_);
}

VkDescriptorPool Context::create_descriptor_pool(const VkDescriptorPoolCreateInfo &ci, const std::string &name) const {
    VkDescriptorPool object = VK_NULL_HANDLE;
    CDSVK__CHECK(vkCreateDescriptorPool(device_, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
VkDescriptorPool Context::create_descriptor_pool(const VkDescriptorSetLayoutCreateInfo &layout_ci, uint32_t max_sets,
        VkDescriptorPoolCreateFlags flags, const std::string &name) const {
    // TODO(cort): should this function take an array of layout_cis and set its sizes based on the total descriptor counts
    // across all sets? That would allow one monolithic pool for each thread, instead of one per descriptor set.
    // max_sets would need to be an array as well most likely: the number of instances of each set.
	std::array<VkDescriptorPoolSize, VK_DESCRIPTOR_TYPE_RANGE_SIZE> pool_sizes;
	for(int iType=0; iType<VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++iType) {
		pool_sizes[iType].descriptorCount = 0;
        pool_sizes[iType].type = (VkDescriptorType)iType;
	}
	for(uint32_t iBinding=0; iBinding<layout_ci.bindingCount; ++iBinding) {
        CDSVK_ASSERT(
            (int)layout_ci.pBindings[iBinding].descriptorType >= VK_DESCRIPTOR_TYPE_BEGIN_RANGE &&
            (int)layout_ci.pBindings[iBinding].descriptorType <= VK_DESCRIPTOR_TYPE_END_RANGE);
		pool_sizes[ (int)layout_ci.pBindings[iBinding].descriptorType ].descriptorCount +=
            layout_ci.pBindings[iBinding].descriptorCount;
	}
    VkDescriptorPoolCreateInfo pool_ci = {};
    pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.flags = flags;
	pool_ci.maxSets = max_sets;
    pool_ci.poolSizeCount = (uint32_t)pool_sizes.size();
    pool_ci.pPoolSizes = pool_sizes.data();
    return create_descriptor_pool(pool_ci, name);
}
void Context::destroy_descriptor_pool(VkDescriptorPool pool) const {
    vkDestroyDescriptorPool(device_, pool, allocation_callbacks_);
}

VkCommandBuffer Context::begin_one_shot_command_buffer(void) const {
    VkCommandBuffer cb = VK_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(one_shot_cpool_mutex_);
        VkCommandBufferAllocateInfo cb_allocate_info = {};
        cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cb_allocate_info.commandPool = one_shot_cpool_;
        cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cb_allocate_info.commandBufferCount = 1;
        CDSVK__CHECK(vkAllocateCommandBuffers(device_, &cb_allocate_info, &cb));
    }
    CDSVK__CHECK(set_debug_name(cb, "one-shot command buffer"));
    VkCommandBufferBeginInfo cb_begin_info = {};
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CDSVK__CHECK(vkBeginCommandBuffer(cb, &cb_begin_info));
    return cb;
}
VkResult Context::end_and_submit_one_shot_command_buffer(VkCommandBuffer *cb) const {
    CDSVK__CHECK(vkEndCommandBuffer(*cb));
    
    VkFenceCreateInfo fence_ci = {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = create_fence(fence_ci, "one-shot command buffer fence");
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = cb;
    CDSVK__CHECK(vkQueueSubmit(graphics_queue_, 1, &submit_info, fence));
    CDSVK__CHECK(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX));
    destroy_fence(fence);
    {
        std::lock_guard<std::mutex> lock(one_shot_cpool_mutex_);
        vkFreeCommandBuffers(device_, one_shot_cpool_, 1, cb);
    }
    *cb = VK_NULL_HANDLE;
    return VK_SUCCESS;
}

// Shortcuts for the most common types of allocations
VkResult Context::allocate_device_memory(const VkMemoryRequirements &mem_reqs,
        VkMemoryPropertyFlags memory_properties_mask,
        VkDeviceMemory *out_mem, VkDeviceSize *out_offset, DeviceMemoryAllocator *device_allocator) const {
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type_index(mem_reqs, memory_properties_mask);
    if (!device_allocator)
        device_allocator = default_device_allocator_.get();
    return device_allocator->allocate(alloc_info, mem_reqs.alignment, out_mem, out_offset);
}
void Context::free_device_memory(VkDeviceMemory mem, VkDeviceSize offset, DeviceMemoryAllocator *device_allocator) const {
    if (!device_allocator)
        device_allocator = default_device_allocator_.get();
    device_allocator->free(mem, offset);    
}
VkResult Context::allocate_and_bind_image_memory(VkImage image, VkMemoryPropertyFlags memory_properties_mask,
        VkDeviceMemory *out_mem, VkDeviceSize *out_offset, DeviceMemoryAllocator *device_allocator) const {
    VkMemoryRequirements mem_reqs = {};
    vkGetImageMemoryRequirements(device_, image, &mem_reqs);
    VkResult result = allocate_device_memory(mem_reqs, memory_properties_mask, out_mem, out_offset, device_allocator);
    if (result == VK_SUCCESS) {
        result = vkBindImageMemory(device_, image, *out_mem, *out_offset);
    }
    return result;
}
VkResult Context::allocate_and_bind_buffer_memory(VkBuffer buffer, VkMemoryPropertyFlags memory_properties_mask,
        VkDeviceMemory *out_mem, VkDeviceSize *out_offset, DeviceMemoryAllocator *device_allocator) const {
    VkMemoryRequirements mem_reqs = {};
    vkGetBufferMemoryRequirements(device_, buffer, &mem_reqs);
    VkResult result = allocate_device_memory(mem_reqs, memory_properties_mask, out_mem, out_offset, device_allocator);
    if (result == VK_SUCCESS) {
        result = vkBindBufferMemory(device_, buffer, *out_mem, *out_offset);
    }
    return result;
}
uint32_t Context::find_memory_type_index(const VkMemoryRequirements &memory_reqs,
       VkMemoryPropertyFlags memory_properties_mask) const {
    for(uint32_t iMemType=0; iMemType<VK_MAX_MEMORY_TYPES; ++iMemType) {
        if ((memory_reqs.memoryTypeBits & (1<<iMemType)) != 0
                && (physical_device_memory_properties_.memoryTypes[iMemType].propertyFlags & memory_properties_mask) == memory_properties_mask) {
            return iMemType;
        }
    }
    return VK_MAX_MEMORY_TYPES; // invalid index
}

VkResult Context::load_buffer_contents(VkBuffer dst_buffer,
        const VkBufferCreateInfo &dst_buffer_ci, VkDeviceSize dst_offset,
        const void *src_data, VkDeviceSize src_size, VkAccessFlags final_access_flags) const {
    // TODO(cort): Make sure I'm clear that dst_offset is relative to buffer start, not relative
    // to the backing VkDeviceMemory objects!
    CDSVK_ASSERT(src_size <= dst_offset + dst_buffer_ci.size);
    CDSVK_ASSERT(dst_buffer_ci.usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    (void)dst_buffer_ci; // only needed for asserts

    VkBufferCreateInfo staging_buffer_ci = {};
    staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_ci.size = src_size;
    staging_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer staging_buffer = create_buffer(staging_buffer_ci, "load_buffer_contents() staging buffer");

    VkDeviceMemory staging_buffer_mem = VK_NULL_HANDLE;
    VkDeviceSize staging_buffer_mem_offset = 0;
    // TODO(cort): pass a device allocator, somehow? Maybe give the Context one at init time for staging resources?
    DeviceMemoryAllocator *device_allocator = default_device_allocator_.get();
    allocate_and_bind_buffer_memory(staging_buffer,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buffer_mem, &staging_buffer_mem_offset, device_allocator);

    void *mapped_data = nullptr;
    CDSVK__CHECK(vkMapMemory(device_, staging_buffer_mem, staging_buffer_mem_offset, src_size,
        VkMemoryMapFlags(0), &mapped_data));
    memcpy(mapped_data, src_data, src_size);
    vkUnmapMemory(device_, staging_buffer_mem);

    VkCommandBuffer cb = begin_one_shot_command_buffer();
    std::array<VkBufferMemoryBarrier,2> buf_barriers = {};
    buf_barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buf_barriers[0].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    buf_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    buf_barriers[0].srcQueueFamilyIndex = graphics_queue_family_index_;
    buf_barriers[0].dstQueueFamilyIndex = graphics_queue_family_index_;
    buf_barriers[0].buffer = staging_buffer;
    buf_barriers[0].offset = 0;
    buf_barriers[0].size = src_size;
    buf_barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buf_barriers[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    buf_barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    buf_barriers[1].srcQueueFamilyIndex = graphics_queue_family_index_;
    buf_barriers[1].dstQueueFamilyIndex = graphics_queue_family_index_;
    buf_barriers[1].buffer = dst_buffer;
    buf_barriers[1].offset = dst_offset;
    buf_barriers[1].size = src_size;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_DEPENDENCY_BY_REGION_BIT, 0,nullptr, (uint32_t)buf_barriers.size(), buf_barriers.data(), 0,nullptr);
    VkBufferCopy buffer_copy_region = {};
    buffer_copy_region.srcOffset = 0;
    buffer_copy_region.dstOffset = dst_offset;
    buffer_copy_region.size = src_size;
    vkCmdCopyBuffer(cb, staging_buffer, dst_buffer, 1, &buffer_copy_region);

    buf_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    buf_barriers[1].dstAccessMask = final_access_flags;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_DEPENDENCY_BY_REGION_BIT, 0,nullptr, 1, &buf_barriers[1], 0,nullptr);

    end_and_submit_one_shot_command_buffer(&cb);

    device_allocator->free(staging_buffer_mem, staging_buffer_mem_offset);
    destroy_buffer(staging_buffer);

    return VK_SUCCESS;
}


void *Context::host_alloc(size_t size, size_t alignment, VkSystemAllocationScope scope) const {
    if (allocation_callbacks_) {
        return allocation_callbacks_->pfnAllocation(allocation_callbacks_->pUserData,
            size, alignment, (VkSystemAllocationScope)scope);
    } else {
#if defined(_MSC_VER)
        return _mm_malloc(size, alignment);
#else
        return malloc(size); // TODO(cort): ignores alignment :(
#endif
    }
}
void Context::host_free(void *ptr) const {
    if (allocation_callbacks_) {
        return allocation_callbacks_->pfnFree(allocation_callbacks_->pUserData, ptr);
    } else {
#if defined(_MSC_VER)
        return _mm_free(ptr);
#else
        return free(ptr);
#endif
    }
}

#if defined(VK_EXT_debug_marker)
VkResult Context::set_debug_name_impl(uint64_t object_as_u64, VkDebugReportObjectTypeEXT object_type, const std::string &name) const {
    if (pfn_vkDebugMarkerSetObjectName) {
        VkDebugMarkerObjectNameInfoEXT debug_name_info = {};
        debug_name_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
        debug_name_info.pNext = nullptr;
        debug_name_info.objectType = (VkDebugReportObjectTypeEXT)object_type;
        debug_name_info.object = object_as_u64;
        debug_name_info.pObjectName = name.c_str();
        return pfn_vkDebugMarkerSetObjectName(device_, &debug_name_info);
    }
    return VK_SUCCESS;
}
#endif // defined(VK_EXT_debug_marker)

#endif // CDS_VULKAN_IMPLEMENTATION
