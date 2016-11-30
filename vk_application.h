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

// Effective Modern C++, Item 21: make_unique() is C++14 only, but easy to implement in C++11.
template <typename T, typename... Ts>
std::unique_ptr<T> my_make_unique(Ts&&... params) {
  return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

//
// Device memory allocation
//
enum DeviceAllocationScope {
  DEVICE_ALLOCATION_SCOPE_FRAME  = 1,
  DEVICE_ALLOCATION_SCOPE_DEVICE = 2,
};

typedef VkResult (VKAPI_PTR *PFN_deviceAllocationFunction)(
  VkDeviceMemory*                             out_memory,
  VkDeviceSize*                               out_offset,
  void*                                       pUserData,
  const VkMemoryAllocateInfo*                 alloc_info,
  DeviceAllocationScope                       allocationScope);

typedef void (VKAPI_PTR *PFN_deviceFreeFunction)(
  void*                                       pUserData,
  VkDeviceMemory                              mem,
  VkDeviceSize                                offset);

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
};

//
// Bundle of Vulkan device context for the application to pass into other parts of the framework.
//
class DeviceContext {
public:
  DeviceContext(VkDevice device, VkPhysicalDevice physical_device, const DeviceQueueContext *queue_contexts, uint32_t queue_context_count,
      const VkAllocationCallbacks *host_allocator = nullptr, const DeviceAllocationCallbacks *device_allocator = nullptr);
  ~DeviceContext();

  VkDevice device() const { return device_; }
  VkPhysicalDevice physical_device() const { return physical_device_; }
  const VkAllocationCallbacks* host_allocator() const { return host_allocator_; }
  const DeviceAllocationCallbacks *device_allocator() const { return device_allocator_; }

  const DeviceQueueContext* find_device_queue(VkQueueFlags queue_flags) const;

  uint32_t find_memory_type_index(const VkMemoryRequirements &memory_reqs,
    VkMemoryPropertyFlags memory_properties_mask) const;

  VkResult device_alloc(const VkMemoryRequirements &mem_reqs, VkMemoryPropertyFlags memory_properties_mask,
    DeviceAllocationScope scope, VkDeviceMemory *out_mem, VkDeviceSize *out_offset) const;
  VkResult device_alloc(const VkMemoryAllocateInfo& alloc_info,
    DeviceAllocationScope scope, VkDeviceMemory *out_mem, VkDeviceSize *out_offset) const;
  // Additional shortcuts for the most common device memory allocations
  VkResult device_alloc_and_bind_to_image(VkImage image, VkMemoryPropertyFlags memory_properties_mask,
    DeviceAllocationScope scope, VkDeviceMemory *out_mem, VkDeviceSize *out_offset) const;
  VkResult device_alloc_and_bind_to_buffer(VkBuffer buffer, VkMemoryPropertyFlags memory_properties_mask,
    DeviceAllocationScope scope, VkDeviceMemory *out_mem, VkDeviceSize *out_offset) const;
  void device_free(VkDeviceMemory mem, VkDeviceSize offset) const;

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

//
// Application base class
//
class Application {
public:
  struct CreateInfo {
    std::string app_name = "Spokk Application";
    uint32_t window_width = 1920, window_height = 1080;
    bool enable_fullscreen = false;
    bool enable_validation = true;
    bool enable_vsync = true;
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
  std::vector<VkImage> swapchain_images_ = {};
  std::vector<VkImageView> swapchain_image_views_ = {};
  VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;
    
  std::shared_ptr<GLFWwindow> window_ = nullptr;

private:
  bool init_successful = false;

};

}  // namespace cdsvk