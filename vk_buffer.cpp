#include "vk_buffer.h"
#include "vk_utilities.h"

#include <cassert>
#include <memory>

namespace spokk {

//
// Buffer
//
VkResult Buffer::create(const DeviceContext& device_context, const VkBufferCreateInfo buffer_ci,
  VkMemoryPropertyFlags memory_properties, DeviceAllocationScope allocation_scope) {
  VkResult result = vkCreateBuffer(device_context.device(), &buffer_ci, device_context.host_allocator(), &handle);
  if (result == VK_SUCCESS) {
    memory = device_context.device_alloc_and_bind_to_buffer(handle, memory_properties, allocation_scope);
    if (memory.block == nullptr) {
      vkDestroyBuffer(device_context.device(), handle, device_context.host_allocator());
      handle = VK_NULL_HANDLE;
      result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
  }
  return result;
}
VkResult Buffer::load(const DeviceContext& device_context, const void *src_data, size_t data_size, size_t src_offset,
  VkDeviceSize dst_offset) const {
  if (handle == VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED; // Call create() first!
  }
  VkResult result = VK_SUCCESS;
  if (memory.mapped()) {
    memory.invalidate(device_context.device());
    memcpy(memory.mapped(), reinterpret_cast<const uint8_t*>(src_data) + src_offset, data_size);
    memory.flush(device_context.device());
  } else {
    // TODO(cort): Maybe it's time for a BufferLoader class?
    const DeviceQueue* transfer_queue = device_context.find_queue(VK_QUEUE_TRANSFER_BIT);
    assert(transfer_queue != nullptr);
    std::unique_ptr<OneShotCommandPool> one_shot_cpool = my_make_unique<OneShotCommandPool>(device_context.device(),
      transfer_queue->handle, transfer_queue->family, device_context.host_allocator());
    VkCommandBuffer cb = one_shot_cpool->allocate_and_begin();
    if (data_size <= 65536) {
      vkCmdUpdateBuffer(cb, handle, dst_offset, data_size, reinterpret_cast<const uint8_t*>(src_data) + src_offset);
    } else {
      assert(0); // TODO(cort): staging buffer? Multiple vkCmdUpdateBuffers? Ignore for now, buffers are small.
    }
    result = one_shot_cpool->end_submit_and_free(&cb);
  }
  return result;
}
VkResult Buffer::create_view(const DeviceContext& device_context, VkFormat format) {
  if (handle == VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED; // Call create() first!
  }
  VkBufferViewCreateInfo view_ci = {};
  view_ci.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
  view_ci.buffer = handle;
  view_ci.format = format;
  view_ci.offset = 0;
  view_ci.range = VK_WHOLE_SIZE;
  VkResult result = vkCreateBufferView(device_context.device(), &view_ci, device_context.host_allocator(), &view);
  return result;
}
void Buffer::destroy(const DeviceContext& device_context) {
  device_context.device_free(memory);
  memory.block = nullptr;
  if (view != VK_NULL_HANDLE) {
    vkDestroyBufferView(device_context.device(), view, device_context.host_allocator());
    view = VK_NULL_HANDLE;
  }
  vkDestroyBuffer(device_context.device(), handle, device_context.host_allocator());
  handle = VK_NULL_HANDLE;
}

}  // namespace spokk