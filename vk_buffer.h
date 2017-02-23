#if !defined(VK_BUFFER_H)
#define VK_BUFFER_H

#include "vk_context.h"
#include "vk_memory.h"
#include <vector>

namespace spokk {

class PipelinedBuffer {
public:
  PipelinedBuffer();
  ~PipelinedBuffer();

  VkResult Create(const DeviceContext& device_context, uint32_t depth, const VkBufferCreateInfo& buffer_ci,
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE);
  VkResult Load(const DeviceContext& device_context, uint32_t pframe, const void *src_data, size_t data_size,
    size_t src_offset = 0, VkDeviceSize dst_offset = 0) const;
  // View creation is optional; it's only necessary for texel buffers.
  VkResult CreateViews(const DeviceContext& device_context, VkFormat format);
  void Destroy(const DeviceContext& device_context);

  VkBuffer Handle(uint32_t pframe) const { return handles_[pframe]; }
  VkBufferView View(uint32_t pframe) const { return views_[pframe]; }
  // Mapped() returns the base address of the specified pframe's data.
  void *Mapped(uint32_t pframe) const {
    return (pframe < depth_ && memory_.Mapped()) ?
      (void*)( intptr_t(memory_.Mapped()) + pframe * bytes_per_pframe_ ) :
      nullptr;
  }
  uint32_t Depth() const { return depth_; }
  VkDeviceSize BytesPerPframe() const { return bytes_per_pframe_; }
  // TODO(cort): this is dangerous, and should be revisited.
  // - No indication whether the allocation is for one buffer or N.
  const DeviceMemoryAllocation& Memory() const { return memory_; }
  // Invalidate the specified pframe's data in the host's caches, to ensure GPU writes to its range are visible by the host.
  // If this allocation is not mapped, this function has no effect.
  void InvalidatePframeHostCache(uint32_t pframe) const {
    return InvalidatePframeHostCache(pframe, 0, bytes_per_pframe_);
  }
  void InvalidatePframeHostCache(uint32_t pframe, VkDeviceSize offset, VkDeviceSize nbytes) const;
  // Flush the specified pframe's data from the host's caches, to ensure host writes to its range are visible by the GPU.
  // If this allocation is not mapped, this function has no effect.
  void FlushPframeHostCache(uint32_t pframe) const {
    return FlushPframeHostCache(pframe, 0, bytes_per_pframe_);
  }
  void FlushPframeHostCache(uint32_t pframe, VkDeviceSize offset, VkDeviceSize nbytes) const;

protected:
  std::vector<VkBuffer> handles_;
  std::vector<VkBufferView> views_;
  DeviceMemoryAllocation memory_;
  uint32_t depth_;
  VkDeviceSize bytes_per_pframe_;
};

class Buffer : public PipelinedBuffer {
public:
  Buffer() : PipelinedBuffer() {}
  ~Buffer() {}

  VkResult Create(const DeviceContext& device_context, const VkBufferCreateInfo& buffer_ci,
      VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE) {
    return PipelinedBuffer::Create(device_context, 1, buffer_ci, memory_properties, allocation_scope);
  }
  VkResult Load(const DeviceContext& device_context, const void *src_data, size_t data_size, size_t src_offset = 0,
      VkDeviceSize dst_offset = 0) const {
    return PipelinedBuffer::Load(device_context, 0, src_data, data_size, src_offset, dst_offset);
  }
  // View creation is optional; it's only necessary for texel buffers.
  VkResult CreateView(const DeviceContext& device_context, VkFormat format) {
    return CreateViews(device_context, format);
  }

  VkBuffer Handle() const { return handles_[0]; }
  VkBufferView View() const { return views_[0]; }
  void *Mapped() const { return memory_.Mapped(); }
  void InvalidateHostCache() const { return InvalidatePframeHostCache(0); }
  void FlushHostCache() const { return FlushPframeHostCache(0); }
};

}  // namespace spokk

#endif  // !defined(VK_BUFFER_H)