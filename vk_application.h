#pragma once

#ifdef _MSC_VER
#include <windows.h>
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>
#include <assert.h>
#include <mutex>

namespace cdsvk {

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
      const VkAllocationCallbacks *host_allocator = nullptr, const DeviceAllocationCallbacks *device_allocator = nullptr) :
      physical_device_(physical_device),
      device_(device),
      host_allocator_(host_allocator),
      device_allocator_(device_allocator) {
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties_);
    queue_contexts_.insert(queue_contexts_.begin(), queue_contexts+0, queue_contexts+queue_context_count);
  }
  ~DeviceContext() {
  }

  VkDevice device() const { return device_; }

  VkPhysicalDevice physical_device() const { return physical_device_; }

  const DeviceQueueContext* find_device_queue(VkQueueFlags queue_flags) const {
    // Search for an exact match first
    for(auto& queue : queue_contexts_) {
      if (queue.queueFlags == queue_flags) {
        return &queue;
      }
    }
    // Next pass looks for anything with the right flags set
    for(auto& queue : queue_contexts_) {
      if ((queue.queueFlags & queue_flags) == queue_flags) {
        return &queue;
      }
    }
    // No match for you!
    return nullptr;
  }

  uint32_t find_memory_type_index(const VkMemoryRequirements &memory_reqs,
    VkMemoryPropertyFlags memory_properties_mask) const {
    for(uint32_t iMemType=0; iMemType<VK_MAX_MEMORY_TYPES; ++iMemType) {
      if ((memory_reqs.memoryTypeBits & (1<<iMemType)) != 0
          && (memory_properties_.memoryTypes[iMemType].propertyFlags & memory_properties_mask) == memory_properties_mask) {
        return iMemType;
      }
    }
    return VK_MAX_MEMORY_TYPES; // invalid index
  }

  VkResult device_alloc(const VkMemoryRequirements &mem_reqs, VkMemoryPropertyFlags memory_properties_mask,
    DeviceAllocationScope scope, VkDeviceMemory *out_mem, VkDeviceSize *out_offset) const {
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type_index(mem_reqs, memory_properties_mask);
    if (device_allocator_ != nullptr) {
      return device_allocator_->pfnAllocation(out_mem, out_offset, device_allocator_->pUserData, &alloc_info, scope);
    } else {
      *out_offset = 0;
      return vkAllocateMemory(device_, &alloc_info, host_allocator_, out_mem);
    }
  }
  void device_free(VkDeviceMemory mem, VkDeviceSize offset) const {
    if (device_allocator_ != nullptr) {
      return device_allocator_->pfnFree(host_allocator_->pUserData, mem, offset);
    } else {
      vkFreeMemory(device_, mem, host_allocator_);
    }
  }

  void *host_alloc(size_t size, size_t alignment, VkSystemAllocationScope scope) const {
    if (host_allocator_) {
      return host_allocator_->pfnAllocation(host_allocator_->pUserData,
        size, alignment, scope);
    } else {
#if defined(_MSC_VER)
      return _mm_malloc(size, alignment);
#else
      return malloc(size); // TODO(cort): ignores alignment :(
#endif
    }
  }
  void host_free(void *ptr) const {
    if (host_allocator_) {
      return host_allocator_->pfnFree(host_allocator_->pUserData, ptr);
    } else {
#if defined(_MSC_VER)
      return _mm_free(ptr);
#else
      return free(ptr);
#endif
    }
  }

private:
  // cached Vulkan handles; do not destroy!
  VkPhysicalDevice physical_device_;
  VkDevice device_;
  const VkAllocationCallbacks* host_allocator_;
  const DeviceAllocationCallbacks *device_allocator_;

  VkPhysicalDeviceMemoryProperties memory_properties_;
  std::vector<DeviceQueueContext> queue_contexts_;
};

class OneShotCommandPool {
public:
  OneShotCommandPool(VkDevice device, VkQueue transfer_queue, uint32_t transfer_queue_family,
    const VkAllocationCallbacks *allocator = nullptr) :
    device_(device),
    transfer_queue_(transfer_queue),
    transfer_queue_family_(transfer_queue_family),
    allocator_(allocator) {
    VkCommandPoolCreateInfo cpool_ci = {};
    cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    cpool_ci.queueFamilyIndex = transfer_queue_family_;
    VkResult result = vkCreateCommandPool(device_, &cpool_ci, allocator, &pool_);
    assert(result == VK_SUCCESS);
  }
  ~OneShotCommandPool() {
    if (pool_ != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device_, pool_, allocator_);
      pool_ = VK_NULL_HANDLE;
    }
  }

  VkCommandBuffer allocate_and_begin(void) const {
    VkCommandBuffer cb = VK_NULL_HANDLE;
    {
      std::lock_guard<std::mutex> lock(pool_mutex_);
      VkCommandBufferAllocateInfo cb_allocate_info = {};
      cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      cb_allocate_info.commandPool = pool_;
      cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cb_allocate_info.commandBufferCount = 1;
      if (VK_SUCCESS != vkAllocateCommandBuffers(device_, &cb_allocate_info, &cb)) {
        return VK_NULL_HANDLE;
      }
    }
    VkCommandBufferBeginInfo cb_begin_info = {};
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult result = vkBeginCommandBuffer(cb, &cb_begin_info);
    if (VK_SUCCESS != result) {
      vkFreeCommandBuffers(device_, pool_, 1, &cb);
      return VK_NULL_HANDLE;
    }
    return cb;
  }

  VkResult end_submit_and_free(VkCommandBuffer *cb) const {
    VkResult result = vkEndCommandBuffer(*cb);
    if (result == VK_SUCCESS) {
      VkFenceCreateInfo fence_ci = {};
      fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      VkFence fence = VK_NULL_HANDLE;
      result = vkCreateFence(device_, &fence_ci, allocator_, &fence);
      if (result == VK_SUCCESS) {
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = cb;
        result = vkQueueSubmit(transfer_queue_, 1, &submit_info, fence);
        if (result == VK_SUCCESS) {
          result = vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX);
        }
      }
      vkDestroyFence(device_, fence, allocator_);
    }
    {
      std::lock_guard<std::mutex> lock(pool_mutex_);
      vkFreeCommandBuffers(device_, pool_, 1, cb);
    }
    *cb = VK_NULL_HANDLE;
    return result;
  }

private:
  VkCommandPool pool_ = VK_NULL_HANDLE;
  mutable std::mutex pool_mutex_ = {};

  // Cached handled -- do not delete!
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue transfer_queue_ = VK_NULL_HANDLE;
  uint32_t transfer_queue_family_ = VK_QUEUE_FAMILY_IGNORED;
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