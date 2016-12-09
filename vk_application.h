#pragma once

#ifdef _MSC_VER
#include <windows.h>
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

namespace cdsvk {

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
  void invalidate(VkDevice device) const;
  void flush(VkDevice device) const;
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
  DeviceContext() : device_(VK_NULL_HANDLE), physical_device_(VK_NULL_HANDLE),
      host_allocator_(nullptr), device_allocator_(nullptr), queue_contexts_{} {
  }
  DeviceContext(VkDevice device, VkPhysicalDevice physical_device, const DeviceQueueContext *queue_contexts, uint32_t queue_context_count,
      const VkAllocationCallbacks *host_allocator = nullptr, const DeviceAllocationCallbacks *device_allocator = nullptr);
  ~DeviceContext();

  VkDevice device() const { return device_; }
  VkPhysicalDevice physical_device() const { return physical_device_; }
  const VkAllocationCallbacks* host_allocator() const { return host_allocator_; }
  const DeviceAllocationCallbacks *device_allocator() const { return device_allocator_; }

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
  const VkAllocationCallbacks* host_allocator_;
  const DeviceAllocationCallbacks *device_allocator_;

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

struct SubpassAttachments {
  std::vector<VkAttachmentReference> input_refs;
  std::vector<VkAttachmentReference> color_refs;
  std::vector<VkAttachmentReference> resolve_refs;
  VkAttachmentReference depth_stencil_ref;
  std::vector<uint32_t> preserve_indices;
};

struct RenderPass {
  VkRenderPass handle;
  std::vector<VkAttachmentDescription> attachment_descs;
  std::vector<VkSubpassDescription> subpass_descs;
  std::vector<SubpassAttachments> subpass_attachments;
  std::vector<VkSubpassDependency> subpass_dependencies;
  void update_subpass_descriptions(VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS, VkSubpassDescriptionFlags flags = 0) {
    subpass_descs.resize(subpass_attachments.size());
    for(const auto& dep : subpass_dependencies) {
      // This is probably unnecessary; a mismatch would be caught by the validation layers at creation time.
      (void)dep;
      assert(dep.srcSubpass == VK_SUBPASS_EXTERNAL || dep.srcSubpass < subpass_descs.size());
      assert(dep.dstSubpass == VK_SUBPASS_EXTERNAL || dep.dstSubpass < subpass_descs.size());
    }
    for(size_t i=0; i<subpass_attachments.size(); ++i) {
      subpass_descs[i].flags = flags;
      subpass_descs[i].pipelineBindPoint = bind_point;
      subpass_descs[i].inputAttachmentCount = (uint32_t)subpass_attachments[i].input_refs.size();
      subpass_descs[i].pInputAttachments = subpass_attachments[i].input_refs.data();
      subpass_descs[i].colorAttachmentCount = (uint32_t)subpass_attachments[i].color_refs.size();
      subpass_descs[i].pColorAttachments = subpass_attachments[i].color_refs.data();
      assert(subpass_attachments[i].resolve_refs.empty() ||
        subpass_attachments[i].resolve_refs.size() == subpass_attachments[i].color_refs.size());
      subpass_descs[i].pResolveAttachments = subpass_attachments[i].resolve_refs.data();
      subpass_descs[i].pDepthStencilAttachment = &subpass_attachments[i].depth_stencil_ref;
      subpass_descs[i].preserveAttachmentCount = (uint32_t)subpass_attachments[i].preserve_indices.size();
      subpass_descs[i].pPreserveAttachments = subpass_attachments[i].preserve_indices.data();
    }
  }
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
    uint32_t window_width = 1920, window_height = 1080;
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

  // handles refer to this application's device_, queue_contexts_, etc.
  DeviceContext device_context_;

  uint32_t frame_index_;  // Frame number since launch
  uint32_t vframe_index_;  // current vframe index; cycles from 0 to VFRAME_COUNT.

private:
  VkResult find_physical_device(const std::vector<QueueFamilyRequest>& qf_reqs, VkInstance instance,
    VkSurfaceKHR present_surface, VkPhysicalDevice *out_physical_device, std::vector<uint32_t>* out_queue_families);

  bool init_successful = false;

};

}  // namespace cdsvk