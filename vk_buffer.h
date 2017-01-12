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

  VkResult create(const DeviceContext& device_context, uint32_t depth, const VkBufferCreateInfo& buffer_ci,
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE);
  VkResult load(const DeviceContext& device_context, uint32_t pframe, const void *src_data, size_t data_size,
    size_t src_offset = 0, VkDeviceSize dst_offset = 0) const;
  // View creation is optional; it's only necessary for texel buffers.
  VkResult create_views(const DeviceContext& device_context, VkFormat format);
  void destroy(const DeviceContext& device_context);

  VkBuffer handle(uint32_t pframe) const { return handles_[pframe]; }
  VkBufferView view(uint32_t pframe) const { return views_[pframe]; }
  // mapped() returns the base address of the specified pframe's data.
  void *mapped(uint32_t pframe) const {
    return (pframe < depth_ && memory_.mapped()) ?
      (void*)( intptr_t(memory_.mapped()) + pframe * bytes_per_pframe_ ) :
      nullptr;
  }
  uint32_t depth() const { return depth_; }
  // TODO(cort): this is dangerous, and should be revisited.
  // - No indication whether the allocation is for one buffer or N.
  // - No way to flush/invalidate only a specific pframe's range.
  const DeviceMemoryAllocation& memory() const { return memory_; }

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

  VkResult create(const DeviceContext& device_context, const VkBufferCreateInfo& buffer_ci,
      VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE) {
    return PipelinedBuffer::create(device_context, 1, buffer_ci, memory_properties, allocation_scope);
  }
  VkResult load(const DeviceContext& device_context, const void *src_data, size_t data_size, size_t src_offset = 0,
      VkDeviceSize dst_offset = 0) const {
    return PipelinedBuffer::load(device_context, 0, src_data, data_size, src_offset, dst_offset);
  }
  // View creation is optional; it's only necessary for texel buffers.
  VkResult create_view(const DeviceContext& device_context, VkFormat format) {
    return create_views(device_context, format);
  }

  VkBuffer handle() const { return handles_[0]; }
  VkBufferView view() const { return views_[0]; }
  void *mapped() const { return memory_.mapped(); }
};

}  // namespace spokk

#endif  // !defined(VK_BUFFER_H)