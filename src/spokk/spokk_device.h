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
};

//
// Bundle of Vulkan device state for the application to pass into other parts of the framework.
//
class Device {
public:
  Device()
    : physical_device_(VK_NULL_HANDLE),
      logical_device_(VK_NULL_HANDLE),
      pipeline_cache_(VK_NULL_HANDLE),
      host_allocator_(nullptr),
      device_allocator_(nullptr),
      device_properties_{},
      memory_properties_{},
      queues_{} {}
  ~Device();

  void Create(VkDevice logical_device, VkPhysicalDevice physical_device, VkPipelineCache pipeline_cache,
      const DeviceQueue *queues, uint32_t queue_count, const VkPhysicalDeviceFeatures &enabled_device_features,
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

  DeviceMemoryAllocation DeviceAlloc(const VkMemoryRequirements &mem_reqs, VkMemoryPropertyFlags memory_properties_mask,
      DeviceAllocationScope scope) const;
  void DeviceFree(DeviceMemoryAllocation allocation) const;
  // Additional shortcuts for the most common device memory allocations
  DeviceMemoryAllocation DeviceAllocAndBindToImage(
      VkImage image, VkMemoryPropertyFlags memory_properties_mask, DeviceAllocationScope scope) const;
  DeviceMemoryAllocation DeviceAllocAndBindToBuffer(
      VkBuffer buffer, VkMemoryPropertyFlags memory_properties_mask, DeviceAllocationScope scope) const;

  void *HostAlloc(size_t size, size_t alignment, VkSystemAllocationScope scope) const;
  void HostFree(void *ptr) const;

private:
  VkPhysicalDevice physical_device_;
  VkDevice logical_device_;
  VkPipelineCache pipeline_cache_;
  const VkAllocationCallbacks *host_allocator_;
  const DeviceAllocationCallbacks *device_allocator_;

  VkPhysicalDeviceFeatures device_features_;  // Features enabled at device creation time.
  VkPhysicalDeviceProperties device_properties_;
  VkPhysicalDeviceMemoryProperties memory_properties_;
  std::vector<DeviceQueue> queues_;
};

}  // namespace spokk
