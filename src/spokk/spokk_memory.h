#pragma once

#include <vulkan/vulkan.h>

namespace spokk {

class Device;
class DeviceMemoryBlock {
public:
  DeviceMemoryBlock() : handle_(VK_NULL_HANDLE), info_{}, mapped_(nullptr) {}
  ~DeviceMemoryBlock();

  VkResult Allocate(const Device& device, const VkMemoryAllocateInfo& alloc_info);
  void Free(const Device& device);

  VkDeviceMemory Handle() const { return handle_; }
  const VkMemoryAllocateInfo& Info() const { return info_; }
  void* Mapped() const { return mapped_; }

  // Invalidate a range of this block in the host's caches, to ensure GPU writes to that range are visible by the host.
  // If this block was not allocated with the HOST_VISIBLE flag, this function has no effect.
  void InvalidateHostCache(const VkMappedMemoryRange& range) const;
  // Flush a range of this block from the host's caches, to ensure host writes to that range are visible by the GPU.
  // If this block was not allocated with the HOST_VISIBLE flag, this function has no effect.
  void FlushHostCache(const VkMappedMemoryRange& range) const;

private:
  VkDevice device_;  // Cached, to allow invalidate/flush
  VkDeviceMemory handle_;
  VkMemoryAllocateInfo info_;
  void* mapped_;  // NULL if allocation is not mapped.
};

struct DeviceMemoryAllocation {
  DeviceMemoryAllocation() : block(nullptr), offset(0), size(0) {}

  void* Mapped() const {
    if (block == nullptr || block->Mapped() == nullptr) {
      return nullptr;
    }
    return (void*)(uintptr_t(block->Mapped()) + offset);
  }

  // Invalidate this allocation in the host's caches, to ensure GPU writes to its range are visible by the host.
  // If this allocation is not mapped, this function has no effect.
  void InvalidateHostCache() const;
  // Flush this allocation from the host's caches, to ensure host writes to its range are visible by the GPU.
  // If this allocation is not mapped, this function has no effect.
  void FlushHostCache() const;

  DeviceMemoryBlock* block;  // May or may not be exclusively owned; depends on the device allocator.
  // May be NULL for invalid allocations.
  VkDeviceSize offset;
  VkDeviceSize size;
};

enum DeviceAllocationScope {
  DEVICE_ALLOCATION_SCOPE_FRAME = 1,
  DEVICE_ALLOCATION_SCOPE_DEVICE = 2,
};

typedef DeviceMemoryAllocation(VKAPI_PTR* PFN_deviceAllocationFunction)(void* pUserData, const Device& device,
    const VkMemoryRequirements& memory_reqs, VkMemoryPropertyFlags memory_property_flags,
    DeviceAllocationScope allocationScope);

typedef void(VKAPI_PTR* PFN_deviceFreeFunction)(
    void* pUserData, const Device& device, DeviceMemoryAllocation& allocation);

typedef struct DeviceAllocationCallbacks {
  void* pUserData;
  PFN_deviceAllocationFunction pfnAllocation;
  PFN_deviceFreeFunction pfnFree;
} DeviceAllocationCallbacks;

}  // namespace spokk
