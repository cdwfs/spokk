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
VkResult DeviceMemoryBlock::Allocate(const DeviceContext& device_context, const VkMemoryAllocateInfo &alloc_info) {
  assert(handle_ == VK_NULL_HANDLE);
  VkResult result = vkAllocateMemory(device_context.Device(), &alloc_info, device_context.HostAllocator(), &handle_);
  if (result == VK_SUCCESS) {
    info_ = alloc_info;
    device_ = device_context.Device();
    VkMemoryPropertyFlags properties = device_context.MemoryTypeProperties(alloc_info.memoryTypeIndex);
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      result = vkMapMemory(device_context.Device(), handle_, 0, VK_WHOLE_SIZE, 0, &mapped_);
    } else {
      mapped_ = nullptr;
    }
  }
  return result;
}
void DeviceMemoryBlock::Free(const DeviceContext& device_context) {
  if (handle_ != VK_NULL_HANDLE) {
    assert(device_context.Device() == device_);
    vkFreeMemory(device_context.Device(), handle_, device_context.HostAllocator());
    handle_ = VK_NULL_HANDLE;
    mapped_ = nullptr;
  }
}

void DeviceMemoryBlock::InvalidateHostCache(const VkMappedMemoryRange& range) const {
  if (mapped_ != nullptr) {
    vkInvalidateMappedMemoryRanges(device_, 1, &range);
  }
}
void DeviceMemoryBlock::FlushHostCache(const VkMappedMemoryRange& range) const {
  if (mapped_ != nullptr) {
    vkFlushMappedMemoryRanges(device_, 1, &range);
  }
}


//
// DeviceMemoryAllocation
//
void DeviceMemoryAllocation::InvalidateHostCache() const {
  VkMappedMemoryRange range = {};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = block->Handle();
  range.offset = offset;
  range.size = size;
  block->InvalidateHostCache(range);
}
void DeviceMemoryAllocation::FlushHostCache() const {
  VkMappedMemoryRange range = {};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = block->Handle();
  range.offset = offset;
  range.size = size;
  block->FlushHostCache(range);
}

}  // namespace spokk
