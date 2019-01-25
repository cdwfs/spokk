#pragma once

#include "spokk_barrier.h"
#include "spokk_memory.h"

#include <vector>

namespace spokk {

class Device;

class PipelinedBuffer {
public:
  PipelinedBuffer();
  ~PipelinedBuffer();

  VkResult Create(const Device& device, uint32_t depth, const VkBufferCreateInfo& buffer_ci,
      VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE);
  VkResult Load(const Device& device, uint32_t pframe, ThsvsAccessType src_access, ThsvsAccessType dst_access,
      const void* src_data, size_t data_size, size_t src_offset = 0, VkDeviceSize dst_offset = 0) const;
  // View creation is optional; it's only necessary for texel buffers.
  VkResult CreateViews(const Device& device, VkFormat format);
  void Destroy(const Device& device);

  VkBuffer Handle(uint32_t pframe) const { return handles_.empty() ? VK_NULL_HANDLE : handles_[pframe]; }
  VkBufferView View(uint32_t pframe) const { return views_.empty() ? VK_NULL_HANDLE : views_[pframe]; }
  // Mapped() returns the base address of the specified pframe's data.
  void* Mapped(uint32_t pframe) const {
    return (pframe < depth_) ? allocations_[pframe].Mapped() : nullptr;
  }
  uint32_t Depth() const { return depth_; }
  VkDeviceSize BytesPerPframe() const { return bytes_per_pframe_; }
  // Invalidate the specified pframe's data in the host's caches, to
  // ensure GPU writes to its range are visible by the host.
  // If this allocation is not mapped, this function has no effect.
  VkResult InvalidatePframeHostCache(
      const Device& device, uint32_t pframe, VkDeviceSize offset, VkDeviceSize nbytes) const;
  VkResult InvalidatePframeHostCache(const Device& device, uint32_t pframe) const {
    return InvalidatePframeHostCache(device, pframe, 0, allocations_[pframe].size);
  }
  // Flush the specified pframe's data from the host's caches, to
  // ensure host writes to its range are visible by the GPU.
  // If this allocation is not mapped, this function has no effect.
  VkResult FlushPframeHostCache(const Device& device, uint32_t pframe, VkDeviceSize offset, VkDeviceSize nbytes) const;
  VkResult FlushPframeHostCache(const Device& device, uint32_t pframe) const {
    return FlushPframeHostCache(device, pframe, 0, allocations_[pframe].size);
  }

protected:
  std::vector<VkBuffer> handles_;
  std::vector<VkBufferView> views_;
  std::vector<DeviceMemoryAllocation> allocations_;
  uint32_t depth_;
  VkDeviceSize bytes_per_pframe_;
};

class Buffer : public PipelinedBuffer {
public:
  Buffer() : PipelinedBuffer() {}
  ~Buffer() {}

  VkResult Create(const Device& device, const VkBufferCreateInfo& buffer_ci,
      VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE) {
    return PipelinedBuffer::Create(device, 1, buffer_ci, memory_properties, allocation_scope);
  }
  VkResult Load(const Device& device, ThsvsAccessType src_access, ThsvsAccessType dst_access, const void* src_data,
      size_t data_size, size_t src_offset = 0, VkDeviceSize dst_offset = 0) const {
    return PipelinedBuffer::Load(device, 0, src_access, dst_access, src_data, data_size, src_offset, dst_offset);
  }
  // View creation is optional; it's only necessary for texel buffers.
  VkResult CreateView(const Device& device, VkFormat format) { return CreateViews(device, format); }

  VkBuffer Handle() const { return handles_.empty() ? VK_NULL_HANDLE : handles_[0]; }
  VkBufferView View() const { return views_.empty() ? VK_NULL_HANDLE : views_[0]; }
  void* Mapped() const { return allocations_[0].Mapped(); }
  VkResult InvalidateHostCache(const Device& device) const { return InvalidatePframeHostCache(device, 0); }
  VkResult FlushHostCache(const Device& device) const { return FlushPframeHostCache(device, 0); }
};

}  // namespace spokk
