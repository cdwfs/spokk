#include "vk_context.h"
#include "vk_memory.h"

#include <cassert>

namespace spokk {

//
// DeviceMemoryBlock
//
DeviceMemoryBlock::~DeviceMemoryBlock() {
  assert(handle_ == VK_NULL_HANDLE);  // call free() before deleting!
}
VkResult DeviceMemoryBlock::allocate(const DeviceContext& device_context, const VkMemoryAllocateInfo &alloc_info) {
  assert(handle_ == VK_NULL_HANDLE);
  VkResult result = vkAllocateMemory(device_context.device(), &alloc_info, device_context.host_allocator(), &handle_);
  if (result == VK_SUCCESS) {
    info_ = alloc_info;
    VkMemoryPropertyFlags properties = device_context.memory_type_properties(alloc_info.memoryTypeIndex);
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      result = vkMapMemory(device_context.device(), handle_, 0, VK_WHOLE_SIZE, 0, &mapped_);
    } else {
      mapped_ = nullptr;
    }
  }
  return result;
}
void DeviceMemoryBlock::free(const DeviceContext& device_context) {
  if (handle_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_context.device(), handle_, device_context.host_allocator());
    handle_ = VK_NULL_HANDLE;
    mapped_ = nullptr;
  }
}

//
// DeviceMemoryAllocation
//
void DeviceMemoryAllocation::invalidate(VkDevice device) const {
  if (mapped()) {
    VkMappedMemoryRange range = {};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = block->handle();
    range.offset = offset;
    range.size = size;
    vkInvalidateMappedMemoryRanges(device, 1, &range);
  }
}
void DeviceMemoryAllocation::flush(VkDevice device) const {
  if (mapped()) {
    VkMappedMemoryRange range = {};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = block->handle();
    range.offset = offset;
    range.size = size;
    vkFlushMappedMemoryRanges(device, 1, &range);
  }
}

}  // namespace spokk
