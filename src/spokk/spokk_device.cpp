#include "spokk_device.h"
#include "spokk_platform.h"
#include "spokk_utilities.h"

#include <assert.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include <malloc.h>
#endif

namespace spokk {

Device::~Device() {
  ZOMBO_ASSERT(logical_device_ == VK_NULL_HANDLE, "Call Device::Destroy()! Don't count on the destructor!");
}

void Device::Create(VkDevice logical_device, VkPhysicalDevice physical_device, VkPipelineCache pipeline_cache,
    const DeviceQueue *queues, uint32_t queue_count, const VkPhysicalDeviceFeatures &enabled_device_features,
    const std::vector<VkLayerProperties> &enabled_instance_layers,
    const std::vector<VkExtensionProperties> &enabled_instance_extensions,
    const std::vector<VkExtensionProperties> &enabled_device_extensions, const VkAllocationCallbacks *host_allocator,
    const DeviceAllocationCallbacks *device_allocator) {
  physical_device_ = physical_device;
  logical_device_ = logical_device;
  pipeline_cache_ = pipeline_cache;
  host_allocator_ = host_allocator;
  device_allocator_ = device_allocator;
  device_features_ = enabled_device_features;
  vkGetPhysicalDeviceProperties(physical_device_, &device_properties_);
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties_);
  instance_layers_ = enabled_instance_layers;
  instance_extensions_ = enabled_instance_extensions;
  device_extensions_ = enabled_device_extensions;
  queues_.insert(queues_.begin(), queues + 0, queues + queue_count);

#if defined(VK_EXT_debug_marker)
  // These calls will return NULL if VK_EXT_debug_marker is not enabled, and the Device wrappers below will be no-ops.
  pfnVkCmdDebugMarkerBeginEXT_ = SPOKK_VK_GET_DEVICE_PROC_ADDR(logical_device_, vkCmdDebugMarkerBeginEXT);
  pfnVkCmdDebugMarkerEndEXT_ = SPOKK_VK_GET_DEVICE_PROC_ADDR(logical_device_, vkCmdDebugMarkerEndEXT);
  pfnVkCmdDebugMarkerBeginEXT_ = SPOKK_VK_GET_DEVICE_PROC_ADDR(logical_device_, vkCmdDebugMarkerInsertEXT);
  pfnVkDebugMarkerSetObjectNameEXT_ = SPOKK_VK_GET_DEVICE_PROC_ADDR(logical_device_, vkDebugMarkerSetObjectNameEXT);
  pfnVkDebugMarkerSetObjectTagEXT_ = SPOKK_VK_GET_DEVICE_PROC_ADDR(logical_device_, vkDebugMarkerSetObjectTagEXT);
#endif
}

void Device::Destroy() {
  if (pipeline_cache_ != VK_NULL_HANDLE) {
    vkDestroyPipelineCache(logical_device_, pipeline_cache_, host_allocator_);
    pipeline_cache_ = VK_NULL_HANDLE;
  }
  queues_.clear();
  if (logical_device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(logical_device_, host_allocator_);
    logical_device_ = VK_NULL_HANDLE;
  }
  host_allocator_ = nullptr;
  device_allocator_ = nullptr;
}

const DeviceQueue *Device::FindQueue(VkQueueFlags queue_flags, VkSurfaceKHR present_surface) const {
  // Search for an exact match first
  for (auto &queue : queues_) {
    if (queue.flags == queue_flags) {
      // Make sure presentation requirement is met, if necessary.
      if ((queue_flags | VK_QUEUE_GRAPHICS_BIT) != 0 && present_surface != VK_NULL_HANDLE) {
        if (queue.present_surface != present_surface) {
          continue;
        }
      }
      return &queue;
    }
  }
  // Next pass looks for anything with the right flags set
  for (auto &queue : queues_) {
    if ((queue.flags & queue_flags) == queue_flags) {
      // Make sure presentation requirement is met, if necessary.
      if ((queue_flags | VK_QUEUE_GRAPHICS_BIT) != 0 && present_surface != VK_NULL_HANDLE) {
        if (queue.present_surface != present_surface) {
          continue;
        }
      }
      return &queue;
    }
  }
  // No match for you!
  return nullptr;
}

uint32_t Device::FindMemoryTypeIndex(
    const VkMemoryRequirements &memory_reqs, VkMemoryPropertyFlags memory_properties_mask) const {
  for (uint32_t iMemType = 0; iMemType < VK_MAX_MEMORY_TYPES; ++iMemType) {
    if ((memory_reqs.memoryTypeBits & (1 << iMemType)) != 0 &&
        (memory_properties_.memoryTypes[iMemType].propertyFlags & memory_properties_mask) == memory_properties_mask) {
      return iMemType;
    }
  }
  return VK_MAX_MEMORY_TYPES;  // invalid index
}
VkMemoryPropertyFlags Device::MemoryTypeProperties(uint32_t memory_type_index) const {
  if (memory_type_index >= memory_properties_.memoryTypeCount) {
    return (VkMemoryPropertyFlags)0;
  }
  return memory_properties_.memoryTypes[memory_type_index].propertyFlags;
}

DeviceMemoryAllocation Device::DeviceAlloc(const VkMemoryRequirements &mem_reqs,
    VkMemoryPropertyFlags memory_properties_mask, DeviceAllocationScope scope) const {
  if (device_allocator_ != nullptr) {
    return device_allocator_->pfnAllocation(
        device_allocator_->pUserData, *this, mem_reqs, memory_properties_mask, scope);
  } else {
    DeviceMemoryAllocation allocation = {};
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = FindMemoryTypeIndex(mem_reqs, memory_properties_mask);
    if (alloc_info.memoryTypeIndex != VK_MAX_MEMORY_TYPES) {
      DeviceMemoryBlock *block = new DeviceMemoryBlock;
      VkResult result = block->Allocate(*this, alloc_info);
      if (result == VK_SUCCESS) {
        allocation.block = block;
        allocation.offset = 0;
        allocation.size = alloc_info.allocationSize;
      } else {
        delete block;
      }
    }
    return allocation;
  }
}
void Device::DeviceFree(DeviceMemoryAllocation allocation) const {
  if (allocation.block != nullptr) {
    if (device_allocator_ != nullptr) {
      return device_allocator_->pfnFree(device_allocator_->pUserData, *this, allocation);
    } else {
      ZOMBO_ASSERT(allocation.offset == 0, "with no custom allocator, allocations must have offset=0");
      ZOMBO_ASSERT(allocation.size == allocation.block->Info().allocationSize,
          "with no custom allocator, allocation size must match block size");
      allocation.block->Free(*this);
    }
  }
}
DeviceMemoryAllocation Device::DeviceAllocAndBindToImage(
    VkImage image, VkMemoryPropertyFlags memory_properties_mask, DeviceAllocationScope scope) const {
  VkMemoryRequirements mem_reqs = {};
  vkGetImageMemoryRequirements(logical_device_, image, &mem_reqs);
  DeviceMemoryAllocation allocation = DeviceAlloc(mem_reqs, memory_properties_mask, scope);
  if (allocation.block != nullptr) {
    VkResult result = vkBindImageMemory(logical_device_, image, allocation.block->Handle(), allocation.offset);
    if (result != VK_SUCCESS) {
      DeviceFree(allocation);
      allocation.block = nullptr;
    }
  }
  return allocation;
}
DeviceMemoryAllocation Device::DeviceAllocAndBindToBuffer(
    VkBuffer buffer, VkMemoryPropertyFlags memory_properties_mask, DeviceAllocationScope scope) const {
  VkMemoryRequirements mem_reqs = {};
  vkGetBufferMemoryRequirements(logical_device_, buffer, &mem_reqs);
  DeviceMemoryAllocation allocation = DeviceAlloc(mem_reqs, memory_properties_mask, scope);
  if (allocation.block != nullptr) {
    VkResult result = vkBindBufferMemory(logical_device_, buffer, allocation.block->Handle(), allocation.offset);
    if (result != VK_SUCCESS) {
      DeviceFree(allocation);
      allocation.block = nullptr;
    }
  }
  return allocation;
}

void *Device::HostAlloc(size_t size, size_t alignment, VkSystemAllocationScope scope) const {
  if (host_allocator_) {
    return host_allocator_->pfnAllocation(host_allocator_->pUserData, size, alignment, scope);
  } else {
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    void *ptr = nullptr;
    int ret = posix_memalign(&ptr, alignment, size);
    return (ret == 0) ? ptr : nullptr;
#endif
  }
}
void Device::HostFree(void *ptr) const {
  if (host_allocator_) {
    return host_allocator_->pfnFree(host_allocator_->pUserData, ptr);
  } else {
#if defined(_MSC_VER)
    return _aligned_free(ptr);
#else
    return free(ptr);
#endif
  }
}

bool Device::IsInstanceLayerEnabled(const std::string &layer_name) const {
  for (const auto &layer : instance_layers_) {
    if (layer_name == layer.layerName) {
      return true;
    }
  }
  return false;
}
bool Device::IsInstanceExtensionEnabled(const std::string &extension_name) const {
  for (const auto &extension : instance_extensions_) {
    if (extension_name == extension.extensionName) {
      return true;
    }
  }
  return false;
}
bool Device::IsDeviceExtensionEnabled(const std::string &extension_name) const {
  for (const auto &extension : device_extensions_) {
    if (extension_name == extension.extensionName) {
      return true;
    }
  }
  return false;
}

#if !defined(VK_EXT_debug_marker)
// stub no-op implementations if the extension isn't even defined in vulkan.h
void Device::DebugMarkerBegin(VkCommandBuffer cb, const char *marker_name, const float marker_color[4]) const {}
void Device::DebugMarkerEnd(VkCommandBuffer cb) const {}
void Device::DebugMarkerInsert(VkCommandBuffer cb, const char *marker_name, const float marker_color[4]) const {}
template <typename VK_HANDLE_T>
VkResult Device::SetObjectTag(VK_HANDLE_T handle, uint64_t tag_name, size_t tag_size, const void *tag) const {
  return VK_SUCCESS;
}
template <typename VK_HANDLE_T>
VkResult Device::SetObjectName(VK_HANDLE_T handle, const char *object_name) const {
  return VK_SUCCESS;
}
#else
void Device::DebugMarkerBegin(VkCommandBuffer cb, const char *marker_name, const float marker_color[4]) const {
  if (pfnVkCmdDebugMarkerBeginEXT_ != nullptr) {
    VkDebugMarkerMarkerInfoEXT marker_info = {VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT};
    marker_info.pMarkerName = marker_name;
    if (marker_color != nullptr) {
      marker_info.color[0] = marker_color[0];
      marker_info.color[1] = marker_color[1];
      marker_info.color[2] = marker_color[2];
      marker_info.color[3] = marker_color[3];
    }
    // pfnVkCmdDebugMarkerBeginEXT_(cb, &marker_info);
    static bool first_call = true;
    if (first_call) {
      (void)cb;
      fprintf(stderr,
          "WARNING: vkCmdDebugMarkerBeginEXT is disabled until "
          "https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/2314 is fixed\n");
      first_call = false;
    }
  }
}
void Device::DebugMarkerEnd(VkCommandBuffer cb) const {
  if (pfnVkCmdDebugMarkerEndEXT_ != nullptr) {
    // pfnVkCmdDebugMarkerEndEXT_(cb);
    static bool first_call = true;
    if (first_call) {
      (void)cb;
      fprintf(stderr,
          "WARNING: vkCmdDebugMarkerEndEXT is disabled until "
          "https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/2314 is fixed\n");
      first_call = false;
    }
  }
}
void Device::DebugMarkerInsert(VkCommandBuffer cb, const char *marker_name, const float marker_color[4]) const {
  if (pfnVkCmdDebugMarkerInsertEXT_ != nullptr) {
    VkDebugMarkerMarkerInfoEXT marker_info = {VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT};
    marker_info.pMarkerName = marker_name;
    if (marker_color != nullptr) {
      marker_info.color[0] = marker_color[0];
      marker_info.color[1] = marker_color[1];
      marker_info.color[2] = marker_color[2];
      marker_info.color[3] = marker_color[3];
    }
    // pfnVkCmdDebugMarkerInsertEXT_(cb, &marker_info);
    static bool first_call = true;
    if (first_call) {
      (void)cb;
      fprintf(stderr,
          "WARNING: vkCmdDebugMarkerInsertEXT is disabled until "
          "https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/2314 is fixed\n");
      first_call = false;
    }
  }
}

// Generate explicit template specializations for SetObjectName and SetObjectTag for all recognized
// handle types. No default implementation is provided; attempting either function with an unsupported handle
// type will cause a compilation error.
#define SPECIALIZE_SET_OBJECT_NAME_AND_TAG(HANDLE_TYPE, OBJECT_TYPE_SUFFIX)                                           \
  template <>                                                                                                         \
  VkResult Device::SetObjectName<HANDLE_TYPE>(HANDLE_TYPE handle, const char *object_name) const {                    \
    if (pfnVkDebugMarkerSetObjectNameEXT_ == nullptr) {                                                               \
      return VK_SUCCESS;                                                                                              \
    }                                                                                                                 \
    VkDebugMarkerObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT};                 \
    name_info.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_##OBJECT_TYPE_SUFFIX##_EXT;                                    \
    name_info.object = reinterpret_cast<uint64_t>(handle);                                                            \
    name_info.pObjectName = object_name;                                                                              \
    printf("Assigning name %s to %s (%p)\n", object_name, #HANDLE_TYPE, (void *)handle);                              \
    return pfnVkDebugMarkerSetObjectNameEXT_(logical_device_, &name_info);                                            \
  }                                                                                                                   \
  template <>                                                                                                         \
  VkResult Device::SetObjectTag<HANDLE_TYPE>(HANDLE_TYPE handle, uint64_t tag_name, size_t tag_size, const void *tag) \
      const {                                                                                                         \
    if (pfnVkDebugMarkerSetObjectTagEXT_ == nullptr) {                                                                \
      return VK_SUCCESS;                                                                                              \
    }                                                                                                                 \
    VkDebugMarkerObjectTagInfoEXT tag_info = {VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT};                    \
    tag_info.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_##OBJECT_TYPE_SUFFIX##_EXT;                                     \
    tag_info.object = reinterpret_cast<uint64_t>(handle);                                                             \
    tag_info.tagName = tag_name;                                                                                      \
    tag_info.tagSize = tag_size;                                                                                      \
    tag_info.pTag = tag;                                                                                              \
    return pfnVkDebugMarkerSetObjectTagEXT_(logical_device_, &tag_info);                                              \
  }

SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkInstance, INSTANCE)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkPhysicalDevice, PHYSICAL_DEVICE)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkDevice, DEVICE)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkQueue, QUEUE)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkSemaphore, SEMAPHORE)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkCommandBuffer, COMMAND_BUFFER)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkFence, FENCE)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkDeviceMemory, DEVICE_MEMORY)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkBuffer, BUFFER)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkImage, IMAGE)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkEvent, EVENT)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkQueryPool, QUERY_POOL)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkBufferView, BUFFER_VIEW)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkImageView, IMAGE_VIEW)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkShaderModule, SHADER_MODULE)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkPipelineCache, PIPELINE_CACHE)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkPipelineLayout, PIPELINE_LAYOUT)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkRenderPass, RENDER_PASS)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkPipeline, PIPELINE)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkSampler, SAMPLER)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkDescriptorPool, DESCRIPTOR_POOL)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkFramebuffer, FRAMEBUFFER)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkCommandPool, COMMAND_POOL)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkSurfaceKHR, SURFACE_KHR)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkSwapchainKHR, SWAPCHAIN_KHR)
SPECIALIZE_SET_OBJECT_NAME_AND_TAG(VkDebugReportCallbackEXT, DEBUG_REPORT_CALLBACK_EXT)
#undef SPECIALIZE_SET_OBJECT_NAME_AND_TAG
#endif

}  // namespace spokk
