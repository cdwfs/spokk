#pragma once

#include <vulkan/vulkan.h>

namespace spokk {

class Device;

struct DeviceMemoryAllocation {
  DeviceMemoryAllocation()
    : device_memory(VK_NULL_HANDLE), offset(0), size(0), mapped(nullptr), allocator_data(nullptr) {}

  void* Mapped() const { return mapped; }

  // Invalidate this allocation in the host's caches, to ensure GPU writes to its range are visible by the host.
  // If this allocation is not mapped, this function has no effect.
  VkResult InvalidateHostCache(VkDevice device, VkDeviceSize offset, VkDeviceSize size) const;
  VkResult InvalidateHostCache(VkDevice device) const {
    return InvalidateHostCache(device, offset, size);
  }
  // Flush this allocation from the host's caches, to ensure host writes to its range are visible by the GPU.
  // If this allocation is not mapped, this function has no effect.
  VkResult FlushHostCache(VkDevice device, VkDeviceSize offset, VkDeviceSize size) const;
  VkResult FlushHostCache(VkDevice device) const {
    FlushHostCache(device, offset, size);
  }

  // This handle may be shared among multiple allocations, and should not be free'd at this level.
  // For failed/invalid allocations, this handle will be VK_NULL_HANDLE.
  VkDeviceMemory device_memory;
  VkDeviceSize offset;
  VkDeviceSize size;

  // If the underlying memory is host-visible, this will be the host-visible address at device_memory+offset.
  // Otherwise, it will be NULL.
  void* mapped;

  // Allocator-specific user data.
  void* allocator_data;
};

enum DeviceAllocationScope {
  DEVICE_ALLOCATION_SCOPE_FRAME = 1,
  DEVICE_ALLOCATION_SCOPE_DEVICE = 2,
};

typedef VkResult (*PFN_deviceAllocationFunction)(void* user_data, const Device& device,
    const VkMemoryRequirements& memory_reqs, VkMemoryPropertyFlags memory_property_flags,
    DeviceAllocationScope allocation_scope, DeviceMemoryAllocation* out_allocation);

typedef void (*PFN_deviceFreeFunction)(void* user_data, const Device& device, DeviceMemoryAllocation& allocation);

typedef struct DeviceAllocationCallbacks {
  void* pUserData;
  PFN_deviceAllocationFunction pfnAllocation;
  PFN_deviceFreeFunction pfnFree;
} DeviceAllocationCallbacks;

}  // namespace spokk
