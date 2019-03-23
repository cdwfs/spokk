#pragma once

#include "spokk_barrier.h"
#include "spokk_memory.h"

#include <vector>

namespace spokk {

class Device;

class Buffer {
public:
  Buffer();
  ~Buffer();

  VkResult Create(const Device& device, const VkBufferCreateInfo& buffer_ci,
      VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE);
  VkResult Load(const Device& device, ThsvsAccessType src_access, ThsvsAccessType dst_access, const void* src_data,
      size_t data_size, size_t src_offset = 0, VkDeviceSize dst_offset = 0) const;
  // View creation is optional; it's only necessary for texel buffers.
  VkResult CreateView(const Device& device, VkFormat format);
  void Destroy(const Device& device);

  VkDeviceSize Size() const { return nbytes_; }
  VkBuffer Handle() const { return handle_; }
  VkBufferView View() const { return view_; }
  void* Mapped() const { return memory_.Mapped(); }
  // Invalidate the specified pframe's data in the host's caches, to
  // ensure GPU writes to its range are visible by the host.
  // If this allocation is not mapped, this function has no effect.
  VkResult InvalidateHostCache(const Device& device, VkDeviceSize offset, VkDeviceSize nbytes) const;
  VkResult InvalidateHostCache(const Device& device) const { return InvalidateHostCache(device, 0, Size()); }
  // Flush a range of the buffer's data from the host's caches, to
  // ensure host writes to its range are visible by the GPU.
  // If this allocation is not mapped, this function has no effect.
  VkResult FlushHostCache(const Device& device, VkDeviceSize offset, VkDeviceSize nbytes) const;
  VkResult FlushHostCache(const Device& device) const { return FlushHostCache(device, 0, Size()); }

protected:
  VkBuffer handle_;
  VkBufferView view_;
  DeviceMemoryAllocation memory_;
  VkDeviceSize nbytes_;
};

}  // namespace spokk
