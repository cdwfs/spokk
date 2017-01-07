#if !defined(VK_BUFFER_H)
#define VK_BUFFER_H

#include "vk_context.h"
#include "vk_memory.h"

namespace spokk {

struct Buffer {
  Buffer() : handle(VK_NULL_HANDLE), view(VK_NULL_HANDLE), memory{} {}
  VkResult create(const DeviceContext& device_context, const VkBufferCreateInfo buffer_ci,
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE);
  VkResult load(const DeviceContext& device_context, const void *src_data, size_t data_size, size_t src_offset = 0,
    VkDeviceSize dst_offset = 0) const;
  // View creation is optional; it's only necessary for texel buffers.
  VkResult create_view(const DeviceContext& device_context, VkFormat format);
  void destroy(const DeviceContext& device_context);
  VkBuffer handle;
  VkBufferView view;
  DeviceMemoryAllocation memory;
};

}  // namespace spokk

#endif  // !defined(VK_BUFFER_H)