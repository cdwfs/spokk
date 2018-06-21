#pragma once

#include "spokk_memory.h"

#include <cstdint>
#include <vector>

namespace spokk {

//
// Device queue + metadata
//
struct DeviceQueue {
  VkQueue handle;
  uint32_t family;
  float priority;
  // copied from VkQueueFamilyProperties
  VkQueueFlags flags;
  uint32_t timestamp_valid_bits;
  VkExtent3D min_image_transfer_granularity;
  // For graphics queues that support presentation, this is the surface the queue can present to.
  VkSurfaceKHR present_surface;

  // Allow implicit conversion to a VkQueue, which is mostly what you want.
  operator VkQueue(void) const { return handle; }
};

//
// Bundle of Vulkan device state for the application to pass into other parts of the framework.
//
class Device {
public:
  Device() {}
  ~Device();

  void Create(VkDevice logical_device, VkPhysicalDevice physical_device, VkPipelineCache pipeline_cache,
      const DeviceQueue *queues, uint32_t queue_count, const VkPhysicalDeviceFeatures &enabled_device_features,
      const std::vector<VkLayerProperties> &enabled_instance_layers,
      const std::vector<VkExtensionProperties> &enabled_instance_extensions,
      const std::vector<VkExtensionProperties> &enabled_device_extensions,
      const VkAllocationCallbacks *host_allocator = nullptr,
      const DeviceAllocationCallbacks *device_allocator = nullptr);
  void Destroy();

  Device(const Device &) = delete;
  Device &operator=(const Device &) = delete;

  // Allow implicit conversion to VkDevice, which 99% of the time is what you want from this object.
  operator VkDevice() const { return logical_device_; }

  VkDevice Logical() const { return logical_device_; }
  VkPhysicalDevice Physical() const { return physical_device_; }
  VkPipelineCache PipelineCache() const { return pipeline_cache_; }
  const VkAllocationCallbacks *HostAllocator() const { return host_allocator_; }
  const DeviceAllocationCallbacks *DeviceAllocator() const { return device_allocator_; }

  const VkPhysicalDeviceProperties &Properties() const { return device_properties_; }
  const VkPhysicalDeviceFeatures &Features() const { return device_features_; }

  const DeviceQueue *FindQueue(VkQueueFlags queue_flags, VkSurfaceKHR present_surface = VK_NULL_HANDLE) const;

  uint32_t FindMemoryTypeIndex(
      const VkMemoryRequirements &memory_reqs, VkMemoryPropertyFlags memory_properties_mask) const;
  VkMemoryPropertyFlags MemoryTypeProperties(uint32_t memory_type_index) const;
  VkMemoryPropertyFlags MemoryFlagsForAccessPattern(DeviceMemoryAccessPattern access_pattern) const;

  VkResult DeviceAlloc(const VkMemoryRequirements &mem_reqs, VkMemoryPropertyFlags memory_properties_mask,
      DeviceAllocationScope scope, DeviceMemoryAllocation *out_allocation) const;
  void DeviceFree(DeviceMemoryAllocation &allocation) const;
  // Additional shortcuts for the most common device memory allocations
  VkResult DeviceAllocAndBindToImage(VkImage image, VkMemoryPropertyFlags memory_properties_mask,
      DeviceAllocationScope scope, DeviceMemoryAllocation *out_allocation) const;
  VkResult DeviceAllocAndBindToBuffer(VkBuffer buffer, VkMemoryPropertyFlags memory_properties_mask,
      DeviceAllocationScope scope, DeviceMemoryAllocation *out_allocation) const;

  void *HostAlloc(size_t size, size_t alignment, VkSystemAllocationScope scope) const;
  void HostFree(void *ptr) const;

  // active layer/extension queries
  bool IsInstanceLayerEnabled(const char *layer_name) const;
  bool IsInstanceExtensionEnabled(const char *layer_name) const;
  bool IsDeviceExtensionEnabled(const char *layer_name) const;

  // VK_EXT_debug_utils wrappers
  // If the extension is unavailable, these calls will be no-ops.
  void DebugLabelBegin(VkCommandBuffer cb, const std::string& label_name, const float label_color[4] = nullptr) const;
  void DebugLabelBegin(VkQueue queue, const std::string& label_name, const float label_color[4] = nullptr) const;
  void DebugLabelEnd(VkCommandBuffer cb) const;
  void DebugLabelEnd(VkQueue queue) const;
  void DebugLabelInsert(VkCommandBuffer cb, const std::string& label_name, const float label_color[4] = nullptr) const;
  void DebugLabelInsert(VkQueue queue, const std::string& label_name, const float label_color[4] = nullptr) const;
  // Specializations provided for all relevant Vulkan handle types.
  template <typename VK_HANDLE_T>
  VkResult SetObjectTag(VK_HANDLE_T handle, uint64_t tag_name, size_t tag_size, const void *tag) const;
  template <typename VK_HANDLE_T>
  VkResult SetObjectName(VK_HANDLE_T handle, const std::string& label_name) const;

private:
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice logical_device_ = VK_NULL_HANDLE;
  VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;
  const VkAllocationCallbacks *host_allocator_ = nullptr;
  const DeviceAllocationCallbacks *device_allocator_ = nullptr;

  VkPhysicalDeviceFeatures device_features_ = {};  // Features enabled at device creation time.
  VkPhysicalDeviceProperties device_properties_ = {};
  VkPhysicalDeviceMemoryProperties memory_properties_ = {};
  std::vector<DeviceQueue> queues_ = {};

  std::vector<VkLayerProperties> instance_layers_ = {};
  std::vector<VkExtensionProperties> instance_extensions_ = {};
  std::vector<VkExtensionProperties> device_extensions_ = {};

#if defined(VK_EXT_debug_utils)
  PFN_vkCmdBeginDebugUtilsLabelEXT pfnVkCmdBeginDebugUtilsLabelEXT_ = nullptr;
  PFN_vkCmdEndDebugUtilsLabelEXT pfnVkCmdEndDebugUtilsLabelEXT_ = nullptr;
  PFN_vkCmdInsertDebugUtilsLabelEXT pfnVkCmdInsertDebugUtilsLabelEXT_ = nullptr;
  PFN_vkQueueBeginDebugUtilsLabelEXT pfnVkQueueBeginDebugUtilsLabelEXT_ = nullptr;
  PFN_vkQueueEndDebugUtilsLabelEXT pfnVkQueueEndDebugUtilsLabelEXT_ = nullptr;
  PFN_vkQueueInsertDebugUtilsLabelEXT pfnVkQueueInsertDebugUtilsLabelEXT_ = nullptr;
  PFN_vkSetDebugUtilsObjectNameEXT pfnVkSetDebugUtilsObjectNameEXT_ = nullptr;
  PFN_vkSetDebugUtilsObjectTagEXT pfnVkSetDebugUtilsObjectTagEXT_ = nullptr;
#endif
};

}  // namespace spokk
