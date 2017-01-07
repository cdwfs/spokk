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
  void invalidate(VkDevice device) const;  // invalidate host caches, to make sure GPU writes are visible on the host.
  void flush(VkDevice device) const;  // flush host caches, to make sure host writes are visible by the GPU.
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