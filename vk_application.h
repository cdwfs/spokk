#pragma once

#ifdef _MSC_VER
#include <windows.h>
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

namespace cdsvk {

class InputState {
public:
  InputState()
      : window_{}
      , current_{}
      , prev_{} {

  }
  explicit InputState(const std::shared_ptr<GLFWwindow>& window)
      : window_(window)
      , current_{}
      , prev_{} {
  }
  ~InputState() = default;

  void set_window(const std::shared_ptr<GLFWwindow>& window) {
    window_ = window;
  }

  enum Digital {
    DIGITAL_LPAD_UP    =  0,
    DIGITAL_LPAD_LEFT  =  1,
    DIGITAL_LPAD_RIGHT =  2,
    DIGITAL_LPAD_DOWN  =  3,
    DIGITAL_RPAD_UP    =  4,
    DIGITAL_RPAD_LEFT  =  5,
    DIGITAL_RPAD_RIGHT =  6,
    DIGITAL_RPAD_DOWN  =  7,

    DIGITAL_COUNT
  };
  enum Analog {
    ANALOG_L_X     = 0,
    ANALOG_L_Y     = 1,
    ANALOG_R_X     = 2,
    ANALOG_R_Y     = 3,
    ANALOG_MOUSE_X = 4,
    ANALOG_MOUSE_Y = 5,

    ANALOG_COUNT
  };
  void Update();
  bool IsPressed(Digital id) const  { return  current_.digital[id] && !prev_.digital[id]; }
  bool IsReleased(Digital id) const { return !current_.digital[id] &&  prev_.digital[id]; }
  bool GetDigital(Digital id) const { return  current_.digital[id]; }
  float GetAnalog(Analog id) const  { return  current_.analog[id]; }

private:
  struct {
    std::array<bool, DIGITAL_COUNT> digital;
    std::array<float, ANALOG_COUNT> analog;
  } current_, prev_;
  std::weak_ptr<GLFWwindow> window_;
};

// How many frames can be in flight simultaneously? The higher the count, the more independent copies
// of various resources (anything changing per-frame) must be created and maintained in memory.
// 1 = CPU and GPU run synchronously, each idling while the other works. Safe, but slow.
// 2 = GPU renders from N while CPU builds commands for frame N+1. Usually a safe choice.
//     If the CPU finishes early, it will block until the GPU is finished.
// 3 = GPU renders from N, while CPU builds commands for frame N+1. This mode is best when using the
//     MAILBOX present mode; it prevents the CPU from ever blocking on the GPU. If the CPU finishes
//     early, it can queue it for presentation and get started on frame N+2; if it finishes *that*
//     before the GPU finishes frame N, then frame N+1 is discarded and frame N+2 is queued for
//     presentation instead.
const uint32_t VFRAME_COUNT = 2;

// Effective Modern C++, Item 21: make_unique() is C++14 only, but easy to implement in C++11.
template <typename T, typename... Ts>
std::unique_ptr<T> my_make_unique(Ts&&... params) {
  return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

//
// Device memory allocation
//
class DeviceContext;
class DeviceMemoryBlock {
public:
  DeviceMemoryBlock() : handle_(VK_NULL_HANDLE), info_{}, mapped_(nullptr) {}
  ~DeviceMemoryBlock() {
    assert(handle_ == VK_NULL_HANDLE);  // call free() before deleting!
  }
  VkResult allocate(const DeviceContext& device_context, const VkMemoryAllocateInfo &alloc_info);
  void free(const DeviceContext& device_context);

  VkDeviceMemory handle() const { return handle_; }
  const VkMemoryAllocateInfo& info() const { return info_; }
  void* mapped() const { return mapped_; }
private:
  VkDeviceMemory handle_;
  VkMemoryAllocateInfo info_;
  void *mapped_;  // NULL if allocation is not mapped.
};

struct DeviceMemoryAllocation {
  DeviceMemoryAllocation() : block(nullptr), offset(0), size(0) {}
  void *mapped() const {
    if (block == nullptr || block->mapped() == nullptr) {
      return nullptr;
    }
    return (void*)( uintptr_t(block->mapped()) + offset );
  }
  // TODO(cort): cache a VkDevice with the memory block?
  void invalidate(VkDevice device) const;  // invalidate host caches, to make sure GPU writes are visible on the host.
  void flush(VkDevice device) const;  // flush host caches, to make sure host writes are visible by the GPU.
  DeviceMemoryBlock *block;  // May or may not be exclusively owned; depends on the device allocator.
                             // May be NULL for invalid allocations.
  VkDeviceSize offset;
  VkDeviceSize size;
};

enum DeviceAllocationScope {
  DEVICE_ALLOCATION_SCOPE_FRAME  = 1,
  DEVICE_ALLOCATION_SCOPE_DEVICE = 2,
};

typedef DeviceMemoryAllocation (VKAPI_PTR *PFN_deviceAllocationFunction)(
  void*                                       pUserData,
  const DeviceContext&                        device_context,
  const VkMemoryRequirements&                 memory_reqs,
  VkMemoryPropertyFlags                       memory_property_flags,
  DeviceAllocationScope                       allocationScope);

typedef void (VKAPI_PTR *PFN_deviceFreeFunction)(
  void*                                       pUserData,
  const DeviceContext&                        device_context,
  DeviceMemoryAllocation&                     allocation);

typedef struct DeviceAllocationCallbacks {
  void*                                   pUserData;
  PFN_deviceAllocationFunction            pfnAllocation;
  PFN_deviceFreeFunction                  pfnFree;
} DeviceAllocationCallbacks;

//
// Device queue + metadata
//
struct DeviceQueueContext {
  VkQueue queue;
  uint32_t queue_family;
  float priority;
  // copied from VkQueueFamilyProperties
  VkQueueFlags queueFlags;
  uint32_t timestampValidBits;
  VkExtent3D minImageTransferGranularity;
  // For graphics queues that support presentation, this is the surface the queue can present to.
  VkSurfaceKHR present_surface;
};

//
// Bundle of Vulkan device context for the application to pass into other parts of the framework.
//
class DeviceContext {
public:
  DeviceContext() : device_(VK_NULL_HANDLE), physical_device_(VK_NULL_HANDLE), pipeline_cache_(VK_NULL_HANDLE),
      host_allocator_(nullptr), device_allocator_(nullptr), device_properties_{},
      memory_properties_{}, queue_contexts_{} {
  }
  DeviceContext(VkDevice device, VkPhysicalDevice physical_device, VkPipelineCache pipeline_cache, const DeviceQueueContext *queue_contexts, uint32_t queue_context_count,
      const VkAllocationCallbacks *host_allocator = nullptr, const DeviceAllocationCallbacks *device_allocator = nullptr);
  ~DeviceContext();

  VkDevice device() const { return device_; }
  VkPhysicalDevice physical_device() const { return physical_device_; }
  VkPipelineCache pipeline_cache() const { return pipeline_cache_; }
  const VkAllocationCallbacks* host_allocator() const { return host_allocator_; }
  const DeviceAllocationCallbacks *device_allocator() const { return device_allocator_; }

  const VkPhysicalDeviceProperties& device_properties() const { return device_properties_; }

  const DeviceQueueContext* find_queue_context(VkQueueFlags queue_flags, VkSurfaceKHR present_surface = VK_NULL_HANDLE) const;

  uint32_t find_memory_type_index(const VkMemoryRequirements &memory_reqs,
    VkMemoryPropertyFlags memory_properties_mask) const;
  VkMemoryPropertyFlags memory_type_properties(uint32_t memory_type_index) const;

  DeviceMemoryAllocation device_alloc(const VkMemoryRequirements &mem_reqs, VkMemoryPropertyFlags memory_properties_mask,
    DeviceAllocationScope scope) const;
  void device_free(DeviceMemoryAllocation allocation) const;
  // Additional shortcuts for the most common device memory allocations
  DeviceMemoryAllocation device_alloc_and_bind_to_image(VkImage image, VkMemoryPropertyFlags memory_properties_mask,
    DeviceAllocationScope scope) const;
  DeviceMemoryAllocation device_alloc_and_bind_to_buffer(VkBuffer buffer, VkMemoryPropertyFlags memory_properties_mask,
    DeviceAllocationScope scope) const;

  void *host_alloc(size_t size, size_t alignment, VkSystemAllocationScope scope) const;
  void host_free(void *ptr) const;

private:
  // cached Vulkan handles; do not destroy!
  VkPhysicalDevice physical_device_;
  VkDevice device_;
  VkPipelineCache pipeline_cache_;
  const VkAllocationCallbacks* host_allocator_;
  const DeviceAllocationCallbacks *device_allocator_;

  VkPhysicalDeviceProperties device_properties_;
  VkPhysicalDeviceMemoryProperties memory_properties_;
  std::vector<DeviceQueueContext> queue_contexts_;
};

//
// Simplifies quick, synchronous, single-shot command buffers.
//
class OneShotCommandPool {
public:
  OneShotCommandPool(VkDevice device, VkQueue queue, uint32_t queue_family,
    const VkAllocationCallbacks *allocator = nullptr);
  ~OneShotCommandPool();

  // Allocates a new single shot command buffer and puts it into the recording state.
  // Commands can be written immediately.
  VkCommandBuffer allocate_and_begin(void) const;
  // Ends recording on the command buffer, submits it, waits for it to complete, and returns
  // the command buffer to the pool.
  VkResult end_submit_and_free(VkCommandBuffer *cb) const;

private:
  VkCommandPool pool_ = VK_NULL_HANDLE;
  mutable std::mutex pool_mutex_ = {};

  // Cached handled -- do not delete!
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  uint32_t queue_family_ = VK_QUEUE_FAMILY_IGNORED;
  const VkAllocationCallbacks *allocator_ = nullptr;
};

struct Buffer {
  Buffer() : handle(VK_NULL_HANDLE), view(VK_NULL_HANDLE), memory{} {}
  VkResult create(const DeviceContext& device_context, const VkBufferCreateInfo buffer_ci,
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE);
  VkResult load(const DeviceContext& device_context, const void *src_data, size_t data_size, size_t src_offset = 0,
    VkDeviceSize dst_offset = 0) const;
  // View creation is optional; it's only necessary for texel buffers.
  VkResult create_view(const DeviceContext& device_context, VkFormat format);
  void destroy(const DeviceContext& device_context);
  VkBuffer handle;
  VkBufferView view;
  DeviceMemoryAllocation memory;
};

class TextureLoader;
struct Image {
  Image() : handle(VK_NULL_HANDLE), view(VK_NULL_HANDLE), memory{} {}
  VkResult create(const DeviceContext& device_context, const VkImageCreateInfo image_ci,
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE);
  VkResult create_and_load(const DeviceContext& device_context, const TextureLoader& loader,
    const std::string& filename, VkBool32 generate_mipmaps = VK_TRUE,
    VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VkAccessFlags final_access_flags = VK_ACCESS_SHADER_READ_BIT);
  void destroy(const DeviceContext& device_context);
  VkImage handle;
  VkImageView view;
  DeviceMemoryAllocation memory;
};

struct MeshFormat {
  std::vector<VkVertexInputBindingDescription> vertex_buffer_bindings;
  std::vector<VkVertexInputAttributeDescription> vertex_attributes;
  static const MeshFormat* get_empty(VkPrimitiveTopology topology, VkBool32 enable_primitive_restart = VK_FALSE);
  // Call me after filling in attributes and bindings.
  void finalize(VkPrimitiveTopology topology, VkBool32 enable_primitive_restart = VK_FALSE);
  VkPipelineVertexInputStateCreateInfo vertex_input_state_ci;  // used for graphics pipeline creation
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_ci;  // used for graphics pipeline creation
};

// TODO(cort): better abstraction. create/destroy functions?
struct Mesh {
  std::vector<Buffer> vertex_buffers;
  const MeshFormat* mesh_format;
  Buffer index_buffer;
  VkIndexType index_type;
  uint32_t index_count;
};

struct DescriptorSetLayoutBindingInfo {
  // The name of each binding in a given shader stage. Purely for debugging.
  std::vector< std::tuple<VkShaderStageFlagBits, std::string> > stage_names;
};
struct DescriptorSetLayoutInfo {
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::vector<DescriptorSetLayoutBindingInfo> binding_infos;
};

struct Shader {
  Shader() : handle(VK_NULL_HANDLE), spirv{}, stage((VkShaderStageFlagBits)0), dset_layout_infos{}, push_constant_range{} {}
  VkResult create_and_load(const DeviceContext& device_context, const std::string& filename);
  VkResult create_and_load_from_file(const DeviceContext& device_context, FILE *fp, int len);
  void unload_spirv(void) {
    spirv.clear();
  }
  void destroy(const DeviceContext& device_context);

  VkShaderModule handle;
  std::vector<uint32_t> spirv;
  VkShaderStageFlagBits stage;
  // Resources used by this shader:
  std::vector<DescriptorSetLayoutInfo> dset_layout_infos;
  VkPushConstantRange push_constant_range;  // range.size = 0 means this stage doesn't use push constants.
};

struct ShaderPipeline {
  ShaderPipeline() : dset_layout_cis{}, dset_layout_infos{}, push_constant_ranges{}, shader_stage_cis{},
      entry_point_names{}, pipeline_layout(VK_NULL_HANDLE), dset_layouts{}, active_stages(0) {
  }
  VkResult add_shader(const Shader *shader, const char *entry_point = "main");
  static VkResult force_compatible_layouts_and_finalize(const DeviceContext& device_context,
    const std::vector<ShaderPipeline*> pipelines);
  VkResult finalize(const DeviceContext& device_context);
  void destroy(const DeviceContext& device_context);

  std::vector<VkDescriptorSetLayoutCreateInfo> dset_layout_cis; // one per dset
  std::vector<DescriptorSetLayoutInfo> dset_layout_infos; // one per dset
  std::vector<VkPushConstantRange> push_constant_ranges;  // one per active stage that uses push constants.

  std::vector<VkPipelineShaderStageCreateInfo> shader_stage_cis;  // one per active stage. used to create graphics pipelines
  std::vector<std::string> entry_point_names;  // one per active stage.

  VkPipelineLayout pipeline_layout;
  std::vector<VkDescriptorSetLayout> dset_layouts;  // one per dset

  VkShaderStageFlags active_stages;
};

struct SubpassAttachments {
  std::vector<VkAttachmentReference> input_refs;
  std::vector<VkAttachmentReference> color_refs;
  std::vector<VkAttachmentReference> resolve_refs;
  std::vector<VkAttachmentReference> depth_stencil_refs;  // max length of 1
  std::vector<uint32_t> preserve_indices;
};

struct RenderPass {
  VkRenderPass handle;
  std::vector<VkAttachmentDescription> attachment_descs;
  std::vector<SubpassAttachments> subpass_attachments;
  std::vector<VkSubpassDependency> subpass_dependencies;
  // Call this after populating the previous vectors.
  void finalize_subpasses(VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS,
    VkSubpassDescriptionFlags flags = 0);
  std::vector<VkSubpassDescription> subpass_descs;
  std::vector<VkPipelineMultisampleStateCreateInfo> subpass_multisample_state_cis;
};

struct ComputePipeline {
  ComputePipeline();
  VkResult create(const DeviceContext& device_context, const ShaderPipeline *shader_pipeline, bool defer_pipeline_creation = false);
  void destroy(const DeviceContext& device_context);
  VkPipeline handle;

  const ShaderPipeline *shader_pipeline;

  VkComputePipelineCreateInfo ci;
};

struct GraphicsPipeline {
  GraphicsPipeline();
  VkResult create(const DeviceContext& device_context, const MeshFormat *mesh_format, const ShaderPipeline *shader_pipeline, const RenderPass *render_pass, uint32_t subpass,
    const std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT},
    const VkViewport viewport = {}, const VkRect2D scissor_rect = {}, bool defer_pipeline_creation = false);
  void destroy(const DeviceContext& device_context);
  VkPipeline handle;

  const MeshFormat *mesh_format;
  const ShaderPipeline *shader_pipeline;
  const RenderPass *render_pass;
  uint32_t subpass;
  std::vector<VkDynamicState> dynamic_states;

  VkGraphicsPipelineCreateInfo ci;
  VkPipelineTessellationStateCreateInfo tessellation_state_ci;
  VkPipelineViewportStateCreateInfo viewport_state_ci;
  std::vector<VkViewport> viewports;
  std::vector<VkRect2D> scissor_rects;
  VkPipelineRasterizationStateCreateInfo rasterization_state_ci;
  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_ci;
  VkPipelineColorBlendStateCreateInfo color_blend_state_ci;
  std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachment_states;
  VkPipelineDynamicStateCreateInfo dynamic_state_ci;
};

struct DescriptorPool {
  DescriptorPool();

  // Adds a number of instances of each type of dset in the array. This would be pretty easy to call on a ShaderPipeline.
  // if dsets_per_layout is nullptr, assume one of each layout.
  void add(uint32_t layout_count, const VkDescriptorSetLayoutCreateInfo* dset_layout_cis, const uint32_t* dsets_per_layout = nullptr);
  // Shortcut to add a single dset layout
  void add(const VkDescriptorSetLayoutCreateInfo& dset_layout, uint32_t dset_count = 1);

  VkResult finalize(const DeviceContext& device_context, VkDescriptorPoolCreateFlags flags = 0);
  void destroy(const DeviceContext& device_context);

  VkResult allocate_sets(const DeviceContext& device_context, uint32_t dset_count, const VkDescriptorSetLayout *dset_layouts, VkDescriptorSet *out_dsets) const;
  VkDescriptorSet allocate_set(const DeviceContext& device_context, VkDescriptorSetLayout dset_layout) const;
  // Only if VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT is set at creation time
  void free_sets(DeviceContext& device_context, uint32_t set_count, const VkDescriptorSet* sets) const;
  void free_set(DeviceContext& device_context, VkDescriptorSet set) const;

  VkDescriptorPool handle;
  VkDescriptorPoolCreateInfo ci;
  std::array<VkDescriptorPoolSize, VK_DESCRIPTOR_TYPE_RANGE_SIZE> pool_sizes;
};

struct DescriptorSetWriter {
  explicit DescriptorSetWriter(const VkDescriptorSetLayoutCreateInfo &layout_ci);

  void bind_image(VkImageView view, VkImageLayout layout, VkSampler sampler, uint32_t binding, uint32_t array_element = 0);
  void bind_buffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding, uint32_t array_element = 0);
  void bind_texel_buffer(VkBufferView view, uint32_t binding, uint32_t array_element = 0);

  void write_all_to_dset(const DeviceContext& device_context, VkDescriptorSet dset);
  void write_one_to_dset(const DeviceContext& device_context, VkDescriptorSet dset, uint32_t binding, uint32_t array_element = 0);

  // Walk through the layout and build the following lists:
  std::vector<VkDescriptorImageInfo> image_infos;
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  std::vector<VkBufferView> texel_buffer_views;
  std::vector<VkWriteDescriptorSet> binding_writes; // one per binding. Sparse dsets are valid, but discouraged.
};


//
// Application base class
//
class Application {
public:
  struct QueueFamilyRequest {
    VkQueueFlags flags;  // Mask of features which must be supported by this queue family.
    bool support_present;  // If flags & VK_QUEUE_GRAPHICS_BIT, support_present=true means the queue must support presentation to the application's VkSurfaceKHR.
    uint32_t queue_count;
    float priority;
  };

  struct CreateInfo {
    std::string app_name = "Spokk Application";
    uint32_t window_width = 1280, window_height = 720;
    bool enable_graphics = true;  // TODO(cort): implement me! Skip window creation, swapchain, etc.
    bool enable_fullscreen = false;
    bool enable_validation = true;
    bool enable_vsync = true;
    std::vector<QueueFamilyRequest> queue_family_requests;
  };

  explicit Application(const CreateInfo &ci);
  virtual ~Application();

  Application(const Application&) = delete;
  const Application& operator=(const Application&) = delete;

  int run();

  virtual void update(double dt);
  virtual void render();

protected:
  bool is_instance_layer_enabled(const std::string& layer_name) const;
  bool is_instance_extension_enabled(const std::string& layer_name) const;
  bool is_device_extension_enabled(const std::string& layer_name) const;

  const VkAllocationCallbacks *allocation_callbacks_ = nullptr;
  VkInstance instance_ = VK_NULL_HANDLE;
  std::vector<VkLayerProperties> instance_layers_ = {};
  std::vector<VkExtensionProperties> instance_extensions_ = {};
  VkDebugReportCallbackEXT debug_report_callback_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkPhysicalDeviceFeatures physical_device_features_ = {};
  VkDevice device_ = VK_NULL_HANDLE;
  std::vector<VkExtensionProperties> device_extensions_ = {};
  std::vector<DeviceQueueContext> queue_contexts_;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkSurfaceFormatKHR swapchain_surface_format_ = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  VkExtent2D swapchain_extent_;
  std::vector<VkImage> swapchain_images_ = {};
  std::vector<VkImageView> swapchain_image_views_ = {};
  VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;
    
  std::shared_ptr<GLFWwindow> window_ = nullptr;

  InputState input_state_;

  // handles refer to this application's device_, queue_contexts_, etc.
  DeviceContext device_context_;

  uint32_t frame_index_;  // Frame number since launch
  uint32_t vframe_index_;  // current vframe index; cycles from 0 to VFRAME_COUNT.

  bool force_exit_ = false;  // Application can set this to true to exit at the next available chance.

private:
  VkResult find_physical_device(const std::vector<QueueFamilyRequest>& qf_reqs, VkInstance instance,
    VkSurfaceKHR present_surface, VkPhysicalDevice *out_physical_device, std::vector<uint32_t>* out_queue_families);

  bool init_successful = false;

};

}  // namespace cdsvk