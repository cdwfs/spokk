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
        vk::VertexInputRate input_rate;
        std::vector<vk::VertexInputAttributeDescription> attributes;
    } cdsvk_vertex_buffer_layout;
    struct GraphicsPipelineSettingsVsPs {
        VertexBufferLayout vertex_buffer_layout; // assumed to be bound at slot 0
        uint32_t dynamic_state_mask;
        vk::PrimitiveTopology primitive_topology;
        vk::Viewport viewport;   // ignored if dynamic_state_mask & (1<<VK_DYNAMIC_STATE_VIEWPORT)
        vk::Rect2D scissor_rect; // ignored if dynamic_state_mask & (1<<VK_DYNAMIC_STATE_SCISSOR)
        vk::PipelineLayout pipeline_layout;
        vk::RenderPass render_pass;
        uint32_t subpass;
        uint32_t subpass_color_attachment_count;
        vk::ShaderModule vertex_shader;
        vk::ShaderModule fragment_shader;
    };
    class GraphicsPipelineCreateInfo {
    public:
        GraphicsPipelineCreateInfo();
        explicit GraphicsPipelineCreateInfo(const GraphicsPipelineSettingsVsPs &settings);
        operator const vk::GraphicsPipelineCreateInfo&() const {
            return graphics_pipeline_ci;
        }
        operator vk::GraphicsPipelineCreateInfo&() {
            return graphics_pipeline_ci;
        }

	    vk::GraphicsPipelineCreateInfo graphics_pipeline_ci;
        // Various structures referred to by graphics_pipeline_ci:
	    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_cis;
        std::vector<vk::VertexInputBindingDescription> vertex_input_binding_descriptions;
        std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute_descriptions;
        vk::PipelineVertexInputStateCreateInfo vertex_input_state_ci;
	    vk::PipelineInputAssemblyStateCreateInfo input_assembly_state_ci;
        vk::PipelineTessellationStateCreateInfo tessellation_state_ci;
        std::vector<vk::Viewport> viewports;
        std::vector<vk::Rect2D> scissor_rects;
	    vk::PipelineViewportStateCreateInfo viewport_state_ci;
	    vk::PipelineRasterizationStateCreateInfo rasterization_state_ci;
	    vk::PipelineMultisampleStateCreateInfo multisample_state_ci;
	    vk::PipelineDepthStencilStateCreateInfo depth_stencil_state_ci;
	    std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachment_states;
	    vk::PipelineColorBlendStateCreateInfo color_blend_state_ci;
        std::vector<vk::DynamicState> dynamic_states;
	    vk::PipelineDynamicStateCreateInfo dynamic_state_ci;
    };

    // Interface for device memory allocators.
    class DeviceMemoryAllocator {
    public:
        virtual vk::Result allocate(const vk::MemoryAllocateInfo &alloc_info, vk::DeviceSize alignment,
                vk::DeviceMemory *out_mem, vk::DeviceSize *out_offset) = 0;
        virtual void free(vk::DeviceMemory mem, vk::DeviceSize offset) = 0;
    };
    // Default device memory allocator (used internally if NULL is passed for a device allocator).
    // Just forwards allocations to the default vkAllocateMemory()/vkFreeMemory().
    class DefaultDeviceMemoryAllocator : public DeviceMemoryAllocator {
    public:
        explicit DefaultDeviceMemoryAllocator(vk::Device device, const vk::AllocationCallbacks *allocation_callbacks = nullptr) :
               device_(device),
               allocation_callbacks_(allocation_callbacks) {
        }
        virtual vk::Result allocate(const vk::MemoryAllocateInfo &alloc_info, vk::DeviceSize alignment,
               vk::DeviceMemory *out_mem, vk::DeviceSize *out_offset) {
            (void)alignment;
            *out_mem = device_.allocateMemory(alloc_info, allocation_callbacks_);
            *out_offset = 0;
            return vk::Result::eSuccess;
        }
        virtual void free(vk::DeviceMemory mem, vk::DeviceSize offset) {
            (void)offset;
            device_.freeMemory(mem, allocation_callbacks_);
        }
    private:
        vk::Device device_;
        const vk::AllocationCallbacks *allocation_callbacks_;
    };

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
        uint32_t graphics_queue_family_index() const { return graphics_queue_family_index_; }
        vk::SwapchainKHR swapchain() const { return swapchain_; }
        vk::Queue graphics_queue() const { return graphics_queue_; }
        vk::Queue present_queue() const { return present_queue_; }
        vk::Format swapchain_format() const { return swapchain_surface_format_.format; }
        const std::vector<vk::ImageView>& swapchain_image_views() const { return swapchain_image_views_; }

        // Active layer/extension queries
        bool is_instance_layer_enabled(const std::string& layer_name) const;
        bool is_instance_extension_enabled(const std::string& ext_name) const;
        bool is_device_extension_enabled(const std::string& ext_name) const;

        // Load/destroy shader modules
        vk::ShaderModule load_shader_from_memory(const void *buf, int len, const std::string &name = "Anonymous") const;
        vk::ShaderModule load_shader_from_file(FILE *f, int len, const std::string &name = "Anonymous") const;
        vk::ShaderModule load_shader(const std::string &filename, const std::string &name = "Anonymous") const;
        void destroy_shader(vk::ShaderModule shader) const;

        // Create/destroy helpers for various Vulkan objects.
        // Mostly just a way to avoid passing device/allocation_callbacks everywhere, but in a few cases
        // more significant shortcuts are provided.
        vk::CommandPool create_command_pool(const vk::CommandPoolCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_command_pool(vk::CommandPool cpool) const;

        vk::Semaphore create_semaphore(const vk::SemaphoreCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_semaphore(vk::Semaphore semaphore) const;

        vk::Fence create_fence(const vk::FenceCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_fence(vk::Fence fence) const;

        vk::Event create_event(const vk::EventCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_event(vk::Event event) const;

        vk::QueryPool create_query_pool(const VkQueryPoolCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_query_pool(vk::QueryPool pool) const;

        vk::PipelineCache create_pipeline_cache(const vk::PipelineCacheCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_pipeline_cache(vk::PipelineCache cache) const;

        vk::PipelineLayout create_pipeline_layout(const vk::PipelineLayoutCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_pipeline_layout(vk::PipelineLayout layout) const;

        vk::RenderPass create_render_pass(const vk::RenderPassCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_render_pass(vk::RenderPass render_pass) const;

        vk::Pipeline create_graphics_pipeline(const vk::GraphicsPipelineCreateInfo &ci, const std::string &name = "Anonymous") const;
        vk::Pipeline create_compute_pipeline(const vk::ComputePipelineCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_pipeline(vk::Pipeline pipeline) const;

        vk::DescriptorSetLayout create_descriptor_set_layout(const vk::DescriptorSetLayoutCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_descriptor_set_layout(vk::DescriptorSetLayout layout) const;

        vk::Sampler create_sampler(const vk::SamplerCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_sampler(vk::Sampler sampler) const;

        vk::Framebuffer create_framebuffer(const vk::FramebufferCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_framebuffer(vk::Framebuffer framebuffer) const;

        vk::Buffer create_buffer(const vk::BufferCreateInfo &ci, const std::string &name = "Anonymous") const;
        void destroy_buffer(vk::Buffer buffer) const;

        vk::BufferView create_buffer_view(const vk::BufferViewCreateInfo &ci, const std::string &name = "Anonymous") const;
        vk::BufferView create_buffer_view(vk::Buffer buffer, vk::Format format, const std::string &name = "Anonymous") const;
        void destroy_buffer_view(vk::BufferView view) const;

        vk::Image create_image(const vk::ImageCreateInfo &ci, vk::ImageLayout final_layout,
            vk::AccessFlags final_access_flags, const std::string &name = "Anonymous") const;
        void destroy_image(vk::Image image) const;

        vk::ImageView create_image_view(const vk::ImageViewCreateInfo &ci, const std::string &name = "Anonymous") const;
        vk::ImageView create_image_view(vk::Image image, const vk::ImageCreateInfo &image_ci, const std::string &name = "Anonymous") const;
        void destroy_image_view(vk::ImageView view) const;

        vk::DescriptorPool create_descriptor_pool(const vk::DescriptorPoolCreateInfo &ci, const std::string &name = "Anonymous") const;
        vk::DescriptorPool create_descriptor_pool(const vk::DescriptorSetLayoutCreateInfo &layout_ci, uint32_t max_sets,
            vk::DescriptorPoolCreateFlags flags, const std::string &name = "Anonymous") const;
        void destroy_descriptor_pool(vk::DescriptorPool pool) const;

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
        VkResult set_debug_name(VkInstance name_me, const std::string &name) const { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eInstance, name); }
        VkResult set_debug_name(VkPhysicalDevice name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::ePhysicalDevice, name); }
        VkResult set_debug_name(VkDevice name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eDevice, name); }
        VkResult set_debug_name(VkQueue name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eQueue, name); }
        VkResult set_debug_name(VkSemaphore name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eSemaphore, name); }
        VkResult set_debug_name(VkCommandBuffer name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eCommandBuffer, name); }
        VkResult set_debug_name(VkFence name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eFence, name); }
        VkResult set_debug_name(VkDeviceMemory name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eDeviceMemory, name); }
        VkResult set_debug_name(VkBuffer name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eBuffer, name); }
        VkResult set_debug_name(VkImage name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eImage, name); }
        VkResult set_debug_name(VkEvent name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eEvent, name); }
        VkResult set_debug_name(VkQueryPool name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eQueryPool, name); }
        VkResult set_debug_name(VkBufferView name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eBufferView, name); }
        VkResult set_debug_name(VkImageView name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eImageView, name); }
        VkResult set_debug_name(VkShaderModule name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eShaderModule, name); }
        VkResult set_debug_name(VkPipelineCache name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::ePipelineCache, name); }
        VkResult set_debug_name(VkPipelineLayout name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::ePipelineLayout, name); }
        VkResult set_debug_name(VkRenderPass name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eRenderPass, name); }
        VkResult set_debug_name(VkPipeline name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::ePipeline, name); }
        VkResult set_debug_name(VkDescriptorSetLayout name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eDescriptorSetLayout, name); }
        VkResult set_debug_name(VkSampler name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eSampler, name); }
        VkResult set_debug_name(VkDescriptorPool name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eDescriptorPool, name); }
        VkResult set_debug_name(VkDescriptorSet name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eDescriptorSet, name); }
        VkResult set_debug_name(VkFramebuffer name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eFramebuffer, name); }
        VkResult set_debug_name(VkCommandPool name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eCommandPool, name); }
        VkResult set_debug_name(VkSurfaceKHR name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eSurfaceKhr, name); }
        VkResult set_debug_name(VkSwapchainKHR name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eSwapchainKhr, name); }
        VkResult set_debug_name(VkDebugReportCallbackEXT name_me, const std::string &name) const  { return set_debug_name_impl((uint64_t)name_me, vk::DebugReportObjectTypeEXT::eDebugReport, name); }
#endif

        // Helpers for one-shot command buffers.
        vk::CommandBuffer begin_one_shot_command_buffer(void) const;
        vk::Result end_and_submit_one_shot_command_buffer(vk::CommandBuffer cb) const;

        vk::Result allocate_device_memory(const vk::MemoryRequirements &mem_reqs, vk::MemoryPropertyFlags memory_properties_mask,
            vk::DeviceMemory *out_mem, vk::DeviceSize *out_offset, DeviceMemoryAllocator *device_allocator = nullptr) const;
        void free_device_memory(vk::DeviceMemory mem, vk::DeviceSize offset, DeviceMemoryAllocator *device_allocator = nullptr) const;
        // Shortcuts for the most common types of allocations
        vk::Result allocate_and_bind_image_memory(vk::Image image, vk::MemoryPropertyFlags memory_properties_mask,
            vk::DeviceMemory *out_mem, vk::DeviceSize *out_offset, DeviceMemoryAllocator *device_allocator = nullptr) const;
        vk::Result allocate_and_bind_buffer_memory(vk::Buffer buffer, vk::MemoryPropertyFlags memory_properties_mask,
            vk::DeviceMemory *out_mem, vk::DeviceSize *out_offset, DeviceMemoryAllocator *device_allocator = nullptr) const;
        // Helper to locate the optimal memory type for a given allocation.
        uint32_t find_memory_type_index(const vk::MemoryRequirements &memory_reqs,
            vk::MemoryPropertyFlags memory_properties_mask) const;

        // Load buffer contents
        vk::Result Context::load_buffer_contents(vk::Buffer dst_buffer,
            const vk::BufferCreateInfo &dst_buffer_ci, vk::DeviceSize dst_offset,
            const void *src_data, vk::DeviceSize src_size, vk::AccessFlags final_access_flags) const;

    private:
        const vk::AllocationCallbacks *allocation_callbacks_;
        std::unique_ptr<DefaultDeviceMemoryAllocator> default_device_allocator;
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

        mutable std::mutex one_shot_cpool_mutex_;
        vk::CommandPool one_shot_cpool_;

        // These members are only used if a present surface is passed at init time.
        vk::SurfaceKHR present_surface_;
        vk::Queue present_queue_;
        uint32_t present_queue_family_index_;
        vk::QueueFamilyProperties present_queue_family_properties_;
        vk::SwapchainKHR swapchain_;
        vk::SurfaceFormatKHR swapchain_surface_format_;
        std::vector<vk::Image> swapchain_images_;
        std::vector<vk::ImageView> swapchain_image_views_;

        // Allocate/free host memory using the provided allocation callbacks (or the default allocator
        // if no callbacks were provided)
        void *Context::host_alloc(size_t size, size_t alignment, vk::SystemAllocationScope scope) const;
        void Context::host_free(void *ptr) const;

#if defined(VK_EXT_debug_marker)
        PFN_vkDebugMarkerSetObjectNameEXT pfn_vkDebugMarkerSetObjectName;
        PFN_vkDebugMarkerSetObjectTagEXT pfn_vkDebugMarkerSetObjectTag;
        VkResult set_debug_name_impl(uint64_t object_as_u64, vk::DebugReportObjectTypeEXT object_type, const std::string &name) const;
#endif

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
    vk::ImageAspectFlags vk_format_to_image_aspect(vk::Format format) {
        switch(format) {
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
    shader_stage_cis.reserve(2);
    shader_stage_cis.push_back(vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eVertex, settings.vertex_shader, "main", nullptr));
    shader_stage_cis.push_back(vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eFragment, settings.fragment_shader, "main", nullptr));

    vertex_input_binding_descriptions.reserve(1); // TODO(cort): multiple vertex streams?
    vertex_input_binding_descriptions.push_back(
        vk::VertexInputBindingDescription(0, settings.vertex_buffer_layout.stride, settings.vertex_buffer_layout.input_rate));
    vertex_input_attribute_descriptions = settings.vertex_buffer_layout.attributes;
    for(auto &attr : vertex_input_attribute_descriptions) {
        attr.binding = vertex_input_binding_descriptions[0].binding;
    }
    vertex_input_state_ci = vk::PipelineVertexInputStateCreateInfo(vk::PipelineVertexInputStateCreateFlags(),
        (uint32_t)vertex_input_binding_descriptions.size(), vertex_input_binding_descriptions.data(),
        (uint32_t)vertex_input_attribute_descriptions.size(), vertex_input_attribute_descriptions.data());

    input_assembly_state_ci = vk::PipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(),
        settings.primitive_topology, VK_FALSE);

    tessellation_state_ci = vk::PipelineTessellationStateCreateInfo();

    viewports.resize(1);
    viewports[0] = settings.viewport;
    scissor_rects.resize(1);
    scissor_rects[0] = settings.scissor_rect;
    viewport_state_ci = vk::PipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(),
        (uint32_t)viewports.size(), viewports.data(), (uint32_t)scissor_rects.size(), scissor_rects.data());

    rasterization_state_ci = vk::PipelineRasterizationStateCreateInfo()
        .setPolygonMode(vk::PolygonMode::eFill)
        .setCullMode(vk::CullModeFlagBits::eBack)
        .setFrontFace(vk::FrontFace::eCounterClockwise)
        .setLineWidth(1.0f);

    multisample_state_ci = vk::PipelineMultisampleStateCreateInfo()
        .setRasterizationSamples(vk::SampleCountFlagBits::e1);

    depth_stencil_state_ci = vk::PipelineDepthStencilStateCreateInfo()
        .setDepthTestEnable(VK_TRUE)
        .setDepthWriteEnable(VK_TRUE)
        .setDepthCompareOp(vk::CompareOp::eLessOrEqual)
        .setStencilTestEnable(VK_FALSE);

    color_blend_attachment_states.resize(settings.subpass_color_attachment_count);
    for(auto &attachment : color_blend_attachment_states) {
        attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        attachment.blendEnable = VK_FALSE;
    }
    color_blend_state_ci = vk::PipelineColorBlendStateCreateInfo()
        .setAttachmentCount((uint32_t)color_blend_attachment_states.size())
        .setPAttachments(color_blend_attachment_states.data());

    dynamic_states.reserve(VK_DYNAMIC_STATE_END_RANGE);
    for(int iDS=VK_DYNAMIC_STATE_BEGIN_RANGE; iDS<=VK_DYNAMIC_STATE_END_RANGE; ++iDS) {
        if (settings.dynamic_state_mask & (1<<iDS)) {
            dynamic_states.push_back((vk::DynamicState)iDS);
        }
    }
    dynamic_state_ci = vk::PipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(),
        (uint32_t)dynamic_states.size(), dynamic_states.data());

    graphics_pipeline_ci = vk::GraphicsPipelineCreateInfo(vk::PipelineCreateFlags(),
        (uint32_t)shader_stage_cis.size(), shader_stage_cis.data(), &vertex_input_state_ci,
        &input_assembly_state_ci, &tessellation_state_ci, &viewport_state_ci,
        &rasterization_state_ci, &multisample_state_ci, &depth_stencil_state_ci,
        &color_blend_state_ci, &dynamic_state_ci, settings.pipeline_layout,
        settings.render_pass, settings.subpass, VK_NULL_HANDLE, 0);
}


Context::Context(const ContextCreateInfo &context_ci) :
        allocation_callbacks_(context_ci.allocation_callbacks),
        default_device_allocator(),
        instance_(),
        debug_report_callback_(),
        physical_device_(),
        physical_device_properties_(),
        physical_device_memory_properties_(),
        physical_device_features_(),
        device_(),
        graphics_queue_family_index_(VK_QUEUE_FAMILY_IGNORED),
        graphics_queue_family_properties_(),
        graphics_queue_(),
        pipeline_cache_(),
        one_shot_cpool_mutex_(),
        one_shot_cpool_(),
        present_surface_(),
        present_queue_(),
        present_queue_family_index_(VK_QUEUE_FAMILY_IGNORED),
        present_queue_family_properties_(),
        swapchain_(),
        swapchain_surface_format_({vk::Format::eUndefined, vk::ColorSpaceKHR::eSrgbNonlinear}),
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

        default_device_allocator.reset(new DefaultDeviceMemoryAllocator(device_, allocation_callbacks_));
    }

    // TODO(cort): we can now assign debug names to things.
    {
#if defined(VK_EXT_debug_marker)
        if (is_device_extension_enabled(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
        {
            pfn_vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)device_.getProcAddr("vkDebugMarkerSetObjectNameEXT");
            pfn_vkDebugMarkerSetObjectTag  = (PFN_vkDebugMarkerSetObjectTagEXT)device_.getProcAddr("vkDebugMarkerSetObjectTagEXT");
        } else {
            pfn_vkDebugMarkerSetObjectName = nullptr;
            pfn_vkDebugMarkerSetObjectTag  = nullptr;
        }
#endif
        set_debug_name(instance_, "Context instance");
        //set_debug_name(physical_device_, "Context physical device");
        set_debug_name(device_, "Context logical device");
    }

    // Create a pipeline cache. TODO(cort): optionally load cache contents from disk.
    {
        vk::PipelineCacheCreateInfo pipeline_cache_ci = {};
        pipeline_cache_ = create_pipeline_cache(pipeline_cache_ci, "Context pipeline cache");
    }

    // Create a command pool for one-shot command buffers
    {
        vk::CommandPoolCreateInfo cpool_ci(vk::CommandPoolCreateFlagBits::eTransient, graphics_queue_family_index_);
        one_shot_cpool_ = create_command_pool(cpool_ci, "Context one-shot command pool");
    }

    // Create swapchain. TODO(cort): Should this be moved outside the context? It seems common enough for this
    // to be handled by a separate library.
    if (present_surface_) {
        vk::SurfaceCapabilitiesKHR surface_caps = physical_device_.getSurfaceCapabilitiesKHR(present_surface_);
        vk::Extent2D swapchain_extent = surface_caps.currentExtent;
        if ((int32_t)swapchain_extent.width == -1) {
            CDSVK_ASSERT( (int32_t)swapchain_extent.height == -1 );
            // TODO(cort): better defaults here, when we can't detect the present surface extent?
            swapchain_extent.width =
                CDSVK__CLAMP(1280, surface_caps.minImageExtent.width, surface_caps.maxImageExtent.width);
            swapchain_extent.height =
                CDSVK__CLAMP( 720, surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height);
        }

        auto device_surface_formats = physical_device_.getSurfaceFormatsKHR(present_surface_);
        if (device_surface_formats.size() == 1 && device_surface_formats[0].format == vk::Format::eUndefined) {
            // No preferred format.
            swapchain_surface_format_.format = vk::Format::eB8G8R8A8Unorm;
            swapchain_surface_format_.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        } else {
            CDSVK_ASSERT(device_surface_formats.size() >= 1);
            swapchain_surface_format_ = device_surface_formats[0];
        }

        auto device_present_modes = physical_device_.getSurfacePresentModesKHR(present_surface_);
        bool found_mailbox_mode = false;
        for(auto mode : device_present_modes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                found_mailbox_mode = true;
                break;
            }
        }
        vk::PresentModeKHR present_mode = found_mailbox_mode
            ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;

        uint32_t desired_swapchain_image_count = surface_caps.minImageCount+1;
        if (surface_caps.maxImageCount > 0 && desired_swapchain_image_count > surface_caps.maxImageCount) {
            desired_swapchain_image_count = surface_caps.maxImageCount;
        }

        vk::SurfaceTransformFlagBitsKHR surface_transform = surface_caps.currentTransform;

        vk::ImageUsageFlags swapchain_image_usage = vk::ImageUsageFlagBits(0)
            | vk::ImageUsageFlagBits::eColorAttachment
            | vk::ImageUsageFlagBits::eTransferDst
            ;
        CDSVK_ASSERT( (surface_caps.supportedUsageFlags & swapchain_image_usage) == swapchain_image_usage );

        CDSVK_ASSERT(surface_caps.supportedCompositeAlpha); // at least one mode must be supported
        CDSVK_ASSERT(surface_caps.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eOpaque);
        vk::CompositeAlphaFlagBitsKHR composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

        vk::SwapchainKHR old_swapchain = {};
        vk::SwapchainCreateInfoKHR swapchain_ci(vk::SwapchainCreateFlagsKHR(), present_surface_,
            desired_swapchain_image_count, swapchain_surface_format_.format, swapchain_surface_format_.colorSpace,
            swapchain_extent, 1, swapchain_image_usage, vk::SharingMode::eExclusive, 0, nullptr,
            surface_transform, composite_alpha, present_mode, VK_TRUE, old_swapchain);
        swapchain_ = device_.createSwapchainKHR(swapchain_ci, allocation_callbacks_);
        set_debug_name(swapchain_, "Context swapchain");
        if (old_swapchain) {
            device_.destroySwapchainKHR(old_swapchain, allocation_callbacks_);
        }

        swapchain_images_ = device_.getSwapchainImagesKHR(swapchain_);
        vk::ImageViewCreateInfo image_view_ci(vk::ImageViewCreateFlags(),
            VK_NULL_HANDLE, vk::ImageViewType::e2D,
            swapchain_surface_format_.format, vk::ComponentMapping(),
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
        swapchain_image_views_.reserve(swapchain_images_.size());
        for(auto image : swapchain_images_) {
            image_view_ci.image = image;
            swapchain_image_views_.push_back(create_image_view(image_view_ci, "Swapchain image view"));
        }
    }
}
Context::~Context() {
    if (device_) {
        device_.waitIdle();

        default_device_allocator.reset();
        destroy_command_pool(one_shot_cpool_);
        for(auto view : swapchain_image_views_) {
            device_.destroyImageView(view, allocation_callbacks_);
        }
        device_.destroySwapchainKHR(swapchain_);
        device_.destroyPipelineCache(pipeline_cache_, allocation_callbacks_);
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

////////////////////// Shader module loading

vk::ShaderModule Context::load_shader_from_memory(const void *buffer, int len, const std::string &name) const {
    CDSVK_ASSERT( (len % sizeof(uint32_t)) == 0);
    vk::ShaderModuleCreateInfo shader_ci(vk::ShaderModuleCreateFlags(), len,
        static_cast<const uint32_t*>(buffer));
    vk::ShaderModule shader = device_.createShaderModule(shader_ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(shader, name));
    return shader;
}
vk::ShaderModule Context::load_shader_from_file(FILE *f, int len, const std::string &name) const {
    std::vector<uint8_t> shader_bin(len);
    size_t bytes_read = fread(shader_bin.data(), 1, len, f);
    if ( (int)bytes_read != len) {
        return vk::ShaderModule();
    }
    return load_shader_from_memory(shader_bin.data(), len, name);
}
vk::ShaderModule Context::load_shader(const std::string &filename, const std::string &name) const {
    FILE *spv_file = cdsvk__fopen(filename.c_str(), "rb");
    if (!spv_file) {
        return VK_NULL_HANDLE;
    }
    fseek(spv_file, 0, SEEK_END);
    long spv_file_size = ftell(spv_file);
    fseek(spv_file, 0, SEEK_SET);
    vk::ShaderModule shader = load_shader_from_file(spv_file, spv_file_size, name);
    fclose(spv_file);
    return shader;
}
void Context::destroy_shader(vk::ShaderModule shader) const {
    device_.destroyShaderModule(shader, allocation_callbacks_);
}

vk::CommandPool Context::create_command_pool(const vk::CommandPoolCreateInfo &ci, const std::string &name) const {
    auto object = device_.createCommandPool(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_command_pool(vk::CommandPool cpool) const {
    device_.destroyCommandPool(cpool, allocation_callbacks_);
}

vk::Semaphore Context::create_semaphore(const vk::SemaphoreCreateInfo &ci, const std::string &name) const {
    auto object = device_.createSemaphore(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_semaphore(vk::Semaphore semaphore) const {
    device_.destroySemaphore(semaphore, allocation_callbacks_);
}

vk::Fence Context::create_fence(const vk::FenceCreateInfo &ci, const std::string &name) const {
    auto object = device_.createFence(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_fence(vk::Fence fence) const {
    device_.destroyFence(fence, allocation_callbacks_);
}

vk::Event Context::create_event(const vk::EventCreateInfo &ci, const std::string &name) const {
    auto object = device_.createEvent(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_event(vk::Event event) const {
    device_.destroyEvent(event, allocation_callbacks_);
}

vk::QueryPool Context::create_query_pool(const VkQueryPoolCreateInfo &ci, const std::string &name) const {
    auto object = device_.createQueryPool(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_query_pool(vk::QueryPool pool) const {
    device_.destroyQueryPool(pool, allocation_callbacks_);
}

vk::PipelineCache Context::create_pipeline_cache(const vk::PipelineCacheCreateInfo &ci, const std::string &name) const {
    auto object = device_.createPipelineCache(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_pipeline_cache(vk::PipelineCache cache) const {
    device_.destroyPipelineCache(cache, allocation_callbacks_);
}

vk::PipelineLayout Context::create_pipeline_layout(const vk::PipelineLayoutCreateInfo &ci, const std::string &name) const {
    auto object = device_.createPipelineLayout(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_pipeline_layout(vk::PipelineLayout layout) const {
    device_.destroyPipelineLayout(layout, allocation_callbacks_);
}

vk::RenderPass Context::create_render_pass(const vk::RenderPassCreateInfo &ci, const std::string &name) const {
    auto object = device_.createRenderPass(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_render_pass(vk::RenderPass render_pass) const {
    device_.destroyRenderPass(render_pass, allocation_callbacks_);
}

vk::Pipeline Context::create_graphics_pipeline(const vk::GraphicsPipelineCreateInfo &ci, const std::string &name) const {
    vk::Pipeline object;
    CDSVK__CHECK(device_.createGraphicsPipelines(pipeline_cache_, 1, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
vk::Pipeline Context::create_compute_pipeline(const vk::ComputePipelineCreateInfo &ci, const std::string &name) const {
    vk::Pipeline object;
    CDSVK__CHECK(device_.createComputePipelines(pipeline_cache_, 1, &ci, allocation_callbacks_, &object));
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_pipeline(vk::Pipeline pipeline) const {
    device_.destroyPipeline(pipeline, allocation_callbacks_);
}

vk::DescriptorSetLayout Context::create_descriptor_set_layout(const vk::DescriptorSetLayoutCreateInfo &ci, const std::string &name) const {
    auto object = device_.createDescriptorSetLayout(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_descriptor_set_layout(vk::DescriptorSetLayout layout) const {
    device_.destroyDescriptorSetLayout(layout, allocation_callbacks_);
}

vk::Sampler Context::create_sampler(const vk::SamplerCreateInfo &ci, const std::string &name) const {
    auto object = device_.createSampler(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_sampler(vk::Sampler sampler) const {
    device_.destroySampler(sampler, allocation_callbacks_);
}

vk::Framebuffer Context::create_framebuffer(const vk::FramebufferCreateInfo &ci, const std::string &name) const {
    auto object = device_.createFramebuffer(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_framebuffer(vk::Framebuffer framebuffer) const {
    device_.destroyFramebuffer(framebuffer, allocation_callbacks_);
}

vk::Buffer Context::create_buffer(const vk::BufferCreateInfo &ci, const std::string &name) const {
    auto object = device_.createBuffer(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
void Context::destroy_buffer(vk::Buffer buffer) const {
    device_.destroyBuffer(buffer, allocation_callbacks_);
}

vk::BufferView Context::create_buffer_view(const vk::BufferViewCreateInfo &ci, const std::string &name) const {
    auto object = device_.createBufferView(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
vk::BufferView Context::create_buffer_view(vk::Buffer buffer, vk::Format format, const std::string &name) const {
    vk::BufferViewCreateInfo view_ci(vk::BufferViewCreateFlags(), buffer, format, 0, VK_WHOLE_SIZE);
    return create_buffer_view(view_ci, name);
}
void Context::destroy_buffer_view(vk::BufferView view) const {
    device_.destroyBufferView(view, allocation_callbacks_);
}

vk::Image Context::create_image(const vk::ImageCreateInfo &ci, vk::ImageLayout final_layout,
        vk::AccessFlags final_access_flags, const std::string &name) const {
    auto object = device_.createImage(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    if (ci.initialLayout != final_layout) {
        // Transition to final layout
        vk::ImageSubresourceRange sub_range(vk_format_to_image_aspect(ci.format),
            0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS);
        vk::ImageMemoryBarrier img_barrier(vk::AccessFlags(), final_access_flags, ci.initialLayout, final_layout,
            graphics_queue_family_index_, graphics_queue_family_index_, object, sub_range);
        vk::CommandBuffer cb = begin_one_shot_command_buffer();
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
            vk::DependencyFlags(), nullptr, nullptr, img_barrier);
        end_and_submit_one_shot_command_buffer(cb);
    }
    return object;
}
void Context::destroy_image(vk::Image image) const {
    device_.destroyImage(image, allocation_callbacks_);
}

vk::ImageView Context::create_image_view(const vk::ImageViewCreateInfo &ci, const std::string &name) const {
    auto object = device_.createImageView(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
vk::ImageView Context::create_image_view(vk::Image image, const vk::ImageCreateInfo &image_ci, const std::string &name) const {
    vk::ImageViewType view_type = vk::ImageViewType::e2D;
    if (image_ci.imageType == vk::ImageType::e1D) {
        view_type = (image_ci.arrayLayers == 1) ? vk::ImageViewType::e1D : vk::ImageViewType::e1DArray;
    } else if (image_ci.imageType == vk::ImageType::e2D) {
        if (image_ci.flags & vk::ImageCreateFlagBits::eCubeCompatible) {
            CDSVK_ASSERT((image_ci.arrayLayers) % 6 == 0);
            view_type = (image_ci.arrayLayers == 6) ? vk::ImageViewType::eCube : vk::ImageViewType::eCubeArray;
        } else {
            view_type = (image_ci.arrayLayers == 1) ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray;
        }
    } else if (image_ci.imageType == vk::ImageType::e3D) {
        view_type = vk::ImageViewType::e3D;
    }
    vk::ImageViewCreateInfo view_ci(vk::ImageViewCreateFlags(), image, view_type, image_ci.format, vk::ComponentMapping(),
        vk::ImageSubresourceRange(vk_format_to_image_aspect(image_ci.format), 0, image_ci.mipLevels, 0, image_ci.arrayLayers));
    return create_image_view(view_ci, name);
}
void Context::destroy_image_view(vk::ImageView view) const {
    device_.destroyImageView(view, allocation_callbacks_);
}

vk::DescriptorPool Context::create_descriptor_pool(const vk::DescriptorPoolCreateInfo &ci, const std::string &name) const {
    auto object = device_.createDescriptorPool(ci, allocation_callbacks_);
    CDSVK__CHECK(set_debug_name(object, name));
    return object;
}
vk::DescriptorPool Context::create_descriptor_pool(const vk::DescriptorSetLayoutCreateInfo &layout_ci, uint32_t max_sets,
        vk::DescriptorPoolCreateFlags flags, const std::string &name) const {
    // TODO(cort): should this function take an array of layout_cis and set its sizes based on the total descriptor counts
    // across all sets? That would allow one monolithic pool for each thread, instead of one per descriptor set.
    // max_sets would need to be an array as well most likely: the number of instances of each set.
	std::array<vk::DescriptorPoolSize, VK_DESCRIPTOR_TYPE_RANGE_SIZE> pool_sizes;
	for(int iType=0; iType<VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++iType) {
		pool_sizes[iType].descriptorCount = 0;
        pool_sizes[iType].type = (vk::DescriptorType)iType;
	}
	for(uint32_t iBinding=0; iBinding<layout_ci.bindingCount; ++iBinding) {
        CDSVK_ASSERT(
            (int)layout_ci.pBindings[iBinding].descriptorType >= VK_DESCRIPTOR_TYPE_BEGIN_RANGE &&
            (int)layout_ci.pBindings[iBinding].descriptorType <= VK_DESCRIPTOR_TYPE_END_RANGE);
		pool_sizes[ (int)layout_ci.pBindings[iBinding].descriptorType ].descriptorCount +=
            layout_ci.pBindings[iBinding].descriptorCount;
	}
	vk::DescriptorPoolCreateInfo pool_ci(flags, max_sets, (uint32_t)pool_sizes.size(), pool_sizes.data());
    return create_descriptor_pool(pool_ci, name);
}
void Context::destroy_descriptor_pool(vk::DescriptorPool pool) const {
    device_.destroyDescriptorPool(pool, allocation_callbacks_);
}

vk::CommandBuffer Context::begin_one_shot_command_buffer(void) const {
    vk::CommandBuffer cb = VK_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(one_shot_cpool_mutex_);
        vk::CommandBufferAllocateInfo cb_allocate_info(one_shot_cpool_, vk::CommandBufferLevel::ePrimary, 1);
        CDSVK__CHECK(device_.allocateCommandBuffers(&cb_allocate_info, &cb));
    }
    CDSVK__CHECK(set_debug_name(cb, "one-shot command buffer"));
    vk::CommandBufferBeginInfo cb_begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    CDSVK__CHECK(cb.begin(&cb_begin_info));
    return cb;
}
vk::Result Context::end_and_submit_one_shot_command_buffer(vk::CommandBuffer cb) const {
    CDSVK__CHECK(vkEndCommandBuffer(cb)); // TODO(cort): cb.end() is ambiguous. Bug reported.
    
    vk::Fence fence = create_fence(vk::FenceCreateInfo(), "one-shot command buffer fence");
    vk::SubmitInfo submit_info(0,nullptr,nullptr, 1,&cb, 0,nullptr);
    graphics_queue_.submit(submit_info, fence);
    device_.waitForFences(fence, VK_TRUE, UINT64_MAX);
    destroy_fence(fence);
    {
        std::lock_guard<std::mutex> lock(one_shot_cpool_mutex_);
        device_.freeCommandBuffers(one_shot_cpool_, cb);
    }
    return vk::Result::eSuccess;
}

// Shortcuts for the most common types of allocations
vk::Result Context::allocate_device_memory(const vk::MemoryRequirements &mem_reqs,
        vk::MemoryPropertyFlags memory_properties_mask,
        vk::DeviceMemory *out_mem, vk::DeviceSize *out_offset, DeviceMemoryAllocator *device_allocator) const {
    vk::MemoryAllocateInfo alloc_info(mem_reqs.size, find_memory_type_index(mem_reqs, memory_properties_mask));
    if (!device_allocator)
        device_allocator = default_device_allocator.get();
    return device_allocator->allocate(alloc_info, mem_reqs.alignment, out_mem, out_offset);
}
void Context::free_device_memory(vk::DeviceMemory mem, vk::DeviceSize offset, DeviceMemoryAllocator *device_allocator) const {
    if (!device_allocator)
        device_allocator = default_device_allocator.get();
    device_allocator->free(mem, offset);    
}
vk::Result Context::allocate_and_bind_image_memory(vk::Image image, vk::MemoryPropertyFlags memory_properties_mask,
        vk::DeviceMemory *out_mem, vk::DeviceSize *out_offset, DeviceMemoryAllocator *device_allocator) const {
    vk::MemoryRequirements mem_reqs = device_.getImageMemoryRequirements(image);
    vk::Result result = allocate_device_memory(mem_reqs, memory_properties_mask, out_mem, out_offset, device_allocator);
    if (result == vk::Result::eSuccess) {
        device_.bindImageMemory(image, *out_mem, *out_offset);
    }
    return result;
}
vk::Result Context::allocate_and_bind_buffer_memory(vk::Buffer buffer, vk::MemoryPropertyFlags memory_properties_mask,
        vk::DeviceMemory *out_mem, vk::DeviceSize *out_offset, DeviceMemoryAllocator *device_allocator) const {
    vk::MemoryRequirements mem_reqs = device_.getBufferMemoryRequirements(buffer);
    vk::Result result = allocate_device_memory(mem_reqs, memory_properties_mask, out_mem, out_offset, device_allocator);
    if (result == vk::Result::eSuccess) {
        device_.bindBufferMemory(buffer, *out_mem, *out_offset);
    }
    return result;
}
uint32_t Context::find_memory_type_index(const vk::MemoryRequirements &memory_reqs,
       vk::MemoryPropertyFlags memory_properties_mask) const {
    for(uint32_t iMemType=0; iMemType<VK_MAX_MEMORY_TYPES; ++iMemType) {
        if ((memory_reqs.memoryTypeBits & (1<<iMemType)) != 0
                && (physical_device_memory_properties_.memoryTypes[iMemType].propertyFlags & memory_properties_mask) == memory_properties_mask) {
            return iMemType;
        }
    }
    return VK_MAX_MEMORY_TYPES; // invalid index
}

vk::Result Context::load_buffer_contents(vk::Buffer dst_buffer,
        const vk::BufferCreateInfo &dst_buffer_ci, vk::DeviceSize dst_offset,
        const void *src_data, vk::DeviceSize src_size, vk::AccessFlags final_access_flags) const {
    // TODO(cort): Make sure I'm clear that dst_offset is relative to buffer start, not relative
    // to the backing VkDeviceMemory objects!
    CDSVK_ASSERT(src_size <= dst_offset + dst_buffer_ci.size);
    CDSVK_ASSERT(dst_buffer_ci.usage & vk::BufferUsageFlagBits::eTransferDst);
    (void)dst_buffer_ci; // only needed for asserts

    vk::BufferCreateInfo staging_buffer_ci(vk::BufferCreateFlags(), src_size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive, 0,nullptr);
    vk::Buffer staging_buffer = create_buffer(staging_buffer_ci, "load_buffer_contents() staging buffer");

    vk::DeviceMemory staging_buffer_mem = VK_NULL_HANDLE;
    vk::DeviceSize staging_buffer_mem_offset = 0;
    // TODO(cort): pass a device allocator, somehow? Maybe give the Context one at init time for staging resources?
    DeviceMemoryAllocator *device_allocator = default_device_allocator.get();
    allocate_and_bind_buffer_memory(staging_buffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        &staging_buffer_mem, &staging_buffer_mem_offset, device_allocator);

    void *mapped_data = device_.mapMemory(staging_buffer_mem, staging_buffer_mem_offset, src_size);
    memcpy(mapped_data, src_data, src_size);
    device_.unmapMemory(staging_buffer_mem);

    vk::CommandBuffer cb = begin_one_shot_command_buffer();
    std::array<vk::BufferMemoryBarrier,2> buf_barriers = {
        vk::BufferMemoryBarrier(vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eTransferRead,
            graphics_queue_family_index_, graphics_queue_family_index_, staging_buffer, 0, src_size),
        vk::BufferMemoryBarrier(vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
            vk::AccessFlagBits::eTransferWrite, graphics_queue_family_index_, graphics_queue_family_index_,
            dst_buffer, dst_offset, src_size),
    };
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
        vk::DependencyFlagBits::eByRegion, nullptr, buf_barriers, nullptr);

    vk::BufferCopy buffer_copy_region(0, dst_offset, src_size);
    cb.copyBuffer(staging_buffer, dst_buffer, buffer_copy_region);

    buf_barriers[1].srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    buf_barriers[1].dstAccessMask = final_access_flags;
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
        vk::DependencyFlagBits::eByRegion, 0,nullptr, 1,&buf_barriers[1], 0,nullptr);

    end_and_submit_one_shot_command_buffer(cb);

    device_allocator->free(staging_buffer_mem, staging_buffer_mem_offset);
    destroy_buffer(staging_buffer);

    return vk::Result::eSuccess;
}


void *Context::host_alloc(size_t size, size_t alignment, vk::SystemAllocationScope scope) const {
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
VkResult Context::set_debug_name_impl(uint64_t object_as_u64, vk::DebugReportObjectTypeEXT object_type, const std::string &name) const {
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
