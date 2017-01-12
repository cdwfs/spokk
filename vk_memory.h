#if !defined(VK_MEMORY_H)
#define VK_MEMORY_H

#include <vulkan/vulkan.h>

namespace spokk {

class DeviceContext;
class DeviceMemoryBlock {
public:
  DeviceMemoryBlock() : handle_(VK_NULL_HANDLE), info_{}, mapped_(nullptr) {}
  ~DeviceMemoryBlock();
  VkResult allocate(const DeviceContext& device_context, const VkMemoryAllocateInfo &alloc_info);
  void free(const DeviceContext& device_context);

  VkDeviceMemory handle() const { return handle_; }
  const VkMemoryAllocateInfo& info() const { return info_; }
  void* mapped() const { return mapped_; }
  
  // Invalidate a range of this block in the host's caches, to ensure GPU writes to that range are visible by the host.
  // If this block was not allocated with the HOST_VISIBLE flag, this function has no effect.
  void invalidate_host_cache(const VkMappedMemoryRange& range) const;  
  // Flush a range of this block from the host's caches, to ensure host writes to that range are visible by the GPU.
  // If this block was not allocated with the HOST_VISIBLE flag, this function has no effect.
  void flush_host_cache(const VkMappedMemoryRange& range) const;

private:
  VkDevice device_;  // Cached, to allow invalidate/flush
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

  // Invalidate this allocation in the host's caches, to ensure GPU writes to its range are visible by the host.
  // If this allocation is not mapped, this function has no effect.
  void invalidate_host_caches() const;
  // Flush this allocation from the host's caches, to ensure host writes to its range are visible by the GPU.
  // If this allocation is not mapped, this function has no effect.
  void flush_host_caches() const;

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

}  // namespace spokk

#endif // !defined(VK_MEMORY_H)