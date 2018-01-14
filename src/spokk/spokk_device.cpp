#include "spokk_device.h"
#include "spokk_platform.h"
#include "spokk_utilities.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <malloc.h>
#endif

namespace {

// Used by the default device memory allocator to represent a single VkDeviceMemory allocation
// and associated metadata.
class DeviceMemoryBlock {
public:
  DeviceMemoryBlock() : handle_(VK_NULL_HANDLE), info_{}, mapped_(nullptr) {}
  ~DeviceMemoryBlock();

  VkResult Allocate(const spokk::Device& device, const VkMemoryAllocateInfo& alloc_info);
  void Free(const spokk::Device& device);

  VkDeviceMemory Handle() const { return handle_; }
  const VkMemoryAllocateInfo& Info() const { return info_; }
  void* Mapped() const { return mapped_; }

private:
  VkDeviceMemory handle_;
  VkMemoryAllocateInfo info_;
  void* mapped_;  // NULL if allocation is not mapped.
};

//
// DeviceMemoryBlock
//
DeviceMemoryBlock::~DeviceMemoryBlock() {
  assert(handle_ == VK_NULL_HANDLE);  // call free() before deleting!
}
VkResult DeviceMemoryBlock::Allocate(const spokk::Device& device, const VkMemoryAllocateInfo& alloc_info) {
  assert(handle_ == VK_NULL_HANDLE);
  VkResult result = vkAllocateMemory(device, &alloc_info, device.HostAllocator(), &handle_);
  if (result == VK_SUCCESS) {
    info_ = alloc_info;
    VkMemoryPropertyFlags properties = device.MemoryTypeProperties(alloc_info.memoryTypeIndex);
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      result = vkMapMemory(device, handle_, 0, VK_WHOLE_SIZE, 0, &mapped_);
    } else {
      mapped_ = nullptr;
    }
  }
  return result;
}
void DeviceMemoryBlock::Free(const spokk::Device& device) {
  if (handle_ != VK_NULL_HANDLE) {
    vkFreeMemory(device, handle_, device.HostAllocator());
    handle_ = VK_NULL_HANDLE;
    mapped_ = nullptr;
  }
}

}  // namespace

namespace spokk {

Device::~Device() {
  ZOMBO_ASSERT(logical_device_ == VK_NULL_HANDLE, "Call Device::Destroy()! Don't count on the destructor!");
}

void Device::Create(VkDevice logical_device, VkPhysicalDevice physical_device, VkPipelineCache pipeline_cache,
    const DeviceQueue* queues, uint32_t queue_count, const VkPhysicalDeviceFeatures& enabled_device_features,
    const std::vector<VkLayerProperties>& enabled_instance_layers,
    const std::vector<VkExtensionProperties>& enabled_instance_extensions,
    const std::vector<VkExtensionProperties>& enabled_device_extensions, const VkAllocationCallbacks* host_allocator,
    const DeviceAllocationCallbacks* device_allocator) {
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

const DeviceQueue* Device::FindQueue(VkQueueFlags queue_flags, VkSurfaceKHR present_surface) const {
  // Search for an exact match first
  for (auto& queue : queues_) {
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
  for (auto& queue : queues_) {
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
    const VkMemoryRequirements& memory_reqs, VkMemoryPropertyFlags memory_properties_mask) const {
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

VkMemoryPropertyFlags Device::MemoryFlagsForAccessPattern(DeviceMemoryAccessPattern access_pattern) const {
  std::vector<VkMemoryPropertyFlags> valid_flags;
  switch (access_pattern) {
  case DEVICE_MEMORY_ACCESS_PATTERN_GPU_ONLY:
    valid_flags.push_back(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    break;
  case DEVICE_MEMORY_ACCESS_PATTERN_CPU_TO_GPU_IMMUTABLE:
    valid_flags.push_back(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    valid_flags.push_back(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    valid_flags.push_back(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    break;
  case DEVICE_MEMORY_ACCESS_PATTERN_CPU_TO_GPU_STREAMING:
    valid_flags.push_back(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    valid_flags.push_back(VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    break;
  case DEVICE_MEMORY_ACCESS_PATTERN_CPU_TO_GPU_DYNAMIC:
    valid_flags.push_back(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    valid_flags.push_back(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    break;
  case DEVICE_MEMORY_ACCESS_PATTERN_GPU_TO_CPU_STREAMING:
    valid_flags.push_back(VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    valid_flags.push_back(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    break;
  case DEVICE_MEMORY_ACCESS_PATTERN_GPU_TO_CPU_DYNAMIC:
    valid_flags.push_back(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    valid_flags.push_back(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    break;
  default:
    ZOMBO_ERROR_RETURN(0, "Unhandled usage: %d\n", access_pattern);
  }

  VkMemoryRequirements fake_mem_reqs = {};
  fake_mem_reqs.memoryTypeBits = UINT32_MAX;
  for (auto flags : valid_flags) {
    uint32_t memory_type_index = FindMemoryTypeIndex(fake_mem_reqs, flags);
    if (memory_type_index < VK_MAX_MEMORY_TYPES) {
      // The device contains a memory type suitable for this access pattern. Return the flags
      // necessary to find it again.
      // TODO(cort): there is a potential problem here. It's conceivable that while a memory type may exist
      // for a given set of flags, a particular resource may not list that memory type as valid in its
      // VkMemoryRequirements. Thus, this function could "succeed" while causing a false negative at allocation
      // time, but only on certain devices/drivers.
      return flags;
    }
  }
  return 0;
}

VkResult Device::DeviceAlloc(const VkMemoryRequirements& mem_reqs, VkMemoryPropertyFlags memory_properties_mask,
    DeviceAllocationScope scope, DeviceMemoryAllocation* out_allocation) const {
  if (device_allocator_ != nullptr) {
    return device_allocator_->pfnAllocation(
        device_allocator_->pUserData, *this, mem_reqs, memory_properties_mask, scope, out_allocation);
  } else {
    // Default device allocator
    *out_allocation = {};
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = FindMemoryTypeIndex(mem_reqs, memory_properties_mask);
    if (alloc_info.memoryTypeIndex >= VK_MAX_MEMORY_TYPES) {
      return VK_ERROR_INITIALIZATION_FAILED;
    }
    DeviceMemoryBlock* block = new DeviceMemoryBlock;
    VkResult result = block->Allocate(*this, alloc_info);
    if (result != VK_SUCCESS) {
      delete block;
      return result;
    }
    out_allocation->device_memory = block->Handle();
    out_allocation->offset = 0;
    out_allocation->size = alloc_info.allocationSize;
    if (block->Mapped()) {
      out_allocation->mapped = (void*)(uintptr_t(block->Mapped()) + out_allocation->offset);
    }
    out_allocation->allocator_data = block;
    return VK_SUCCESS;
  }
}
void Device::DeviceFree(DeviceMemoryAllocation& allocation) const {
  if (allocation.device_memory != VK_NULL_HANDLE) {
    if (device_allocator_ != nullptr) {
      return device_allocator_->pfnFree(device_allocator_->pUserData, *this, allocation);
    } else {
      ZOMBO_ASSERT(allocation.offset == 0, "with no custom allocator, allocations must have offset=0");
      DeviceMemoryBlock* block = (DeviceMemoryBlock*)allocation.allocator_data;
      ZOMBO_ASSERT(allocation.size == block->Info().allocationSize,
          "with no custom allocator, allocation size must match block size");
      block->Free(*this);
      delete block;
      allocation = {};
    }
  }
}
VkResult Device::DeviceAllocAndBindToImage(VkImage image, VkMemoryPropertyFlags memory_properties_mask,
    DeviceAllocationScope scope, DeviceMemoryAllocation* out_allocation) const {
  VkMemoryRequirements mem_reqs = {};
  vkGetImageMemoryRequirements(logical_device_, image, &mem_reqs);
  VkResult result = DeviceAlloc(mem_reqs, memory_properties_mask, scope, out_allocation);
  if (result == VK_SUCCESS) {
    result = vkBindImageMemory(logical_device_, image, out_allocation->device_memory, out_allocation->offset);
    if (result != VK_SUCCESS) {
      DeviceFree(*out_allocation);
      *out_allocation = {};
    }
  }
  return result;
}
VkResult Device::DeviceAllocAndBindToBuffer(VkBuffer buffer, VkMemoryPropertyFlags memory_properties_mask,
    DeviceAllocationScope scope, DeviceMemoryAllocation* out_allocation) const {
  VkMemoryRequirements mem_reqs = {};
  vkGetBufferMemoryRequirements(logical_device_, buffer, &mem_reqs);
  VkResult result = DeviceAlloc(mem_reqs, memory_properties_mask, scope, out_allocation);
  if (result == VK_SUCCESS) {
    result = vkBindBufferMemory(logical_device_, buffer, out_allocation->device_memory, out_allocation->offset);
    if (result != VK_SUCCESS) {
      DeviceFree(*out_allocation);
      *out_allocation = {};
    }
  }
  return result;
}

void* Device::HostAlloc(size_t size, size_t alignment, VkSystemAllocationScope scope) const {
  if (host_allocator_) {
    return host_allocator_->pfnAllocation(host_allocator_->pUserData, size, alignment, scope);
  } else {
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    int ret = posix_memalign(&ptr, alignment, size);
    return (ret == 0) ? ptr : nullptr;
#endif
  }
}
void Device::HostFree(void* ptr) const {
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

bool Device::IsInstanceLayerEnabled(const char* layer_name) const {
  for (const auto& layer : instance_layers_) {
    if (strcmp(layer_name, layer.layerName) == 0) {
      return true;
    }
  }
  return false;
}
bool Device::IsInstanceExtensionEnabled(const char* extension_name) const {
  for (const auto& extension : instance_extensions_) {
    if (strcmp(extension_name, extension.extensionName) == 0) {
      return true;
    }
  }
  return false;
}
bool Device::IsDeviceExtensionEnabled(const char* extension_name) const {
  for (const auto& extension : device_extensions_) {
    if (strcmp(extension_name, extension.extensionName) == 0) {
      return true;
    }
  }
  return false;
}

#if !defined(VK_EXT_debug_marker)
// stub no-op implementations if the extension isn't even defined in vulkan.h
void Device::DebugMarkerBegin(VkCommandBuffer cb, const char* marker_name, const float marker_color[4]) const {}
void Device::DebugMarkerEnd(VkCommandBuffer cb) const {}
void Device::DebugMarkerInsert(VkCommandBuffer cb, const char* marker_name, const float marker_color[4]) const {}
template <typename VK_HANDLE_T>
VkResult Device::SetObjectTag(VK_HANDLE_T handle, uint64_t tag_name, size_t tag_size, const void* tag) const {
  return VK_SUCCESS;
}
template <typename VK_HANDLE_T>
VkResult Device::SetObjectName(VK_HANDLE_T handle, const char* object_name) const {
  return VK_SUCCESS;
}
#else
void Device::DebugMarkerBegin(VkCommandBuffer cb, const char* marker_name, const float marker_color[4]) const {
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
    (void)marker_info;
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
void Device::DebugMarkerInsert(VkCommandBuffer cb, const char* marker_name, const float marker_color[4]) const {
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
    (void)marker_info;
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
  VkResult Device::SetObjectName<HANDLE_TYPE>(HANDLE_TYPE handle, const char* object_name) const {                    \
    if (pfnVkDebugMarkerSetObjectNameEXT_ == nullptr) {                                                               \
      return VK_SUCCESS;                                                                                              \
    }                                                                                                                 \
    VkDebugMarkerObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT};                 \
    name_info.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_##OBJECT_TYPE_SUFFIX##_EXT;                                    \
    name_info.object = reinterpret_cast<uint64_t>(handle);                                                            \
    name_info.pObjectName = object_name;                                                                              \
    printf("Assigning name %s to %s (%p)\n", object_name, #HANDLE_TYPE, (void*)handle);                               \
    return pfnVkDebugMarkerSetObjectNameEXT_(logical_device_, &name_info);                                            \
  }                                                                                                                   \
  template <>                                                                                                         \
  VkResult Device::SetObjectTag<HANDLE_TYPE>(HANDLE_TYPE handle, uint64_t tag_name, size_t tag_size, const void* tag) \
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
