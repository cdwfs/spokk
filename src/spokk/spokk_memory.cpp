#include "spokk_memory.h"

#include "spokk_device.h"
#include "spokk_platform.h"

namespace spokk {

//
// DeviceMemoryAllocation
//
VkResult DeviceMemoryAllocation::InvalidateHostCache(
    VkDevice device, VkDeviceSize range_offset, VkDeviceSize range_size) const {
  if (!mapped) {
    return VK_SUCCESS;
  }
  VkMappedMemoryRange range = {};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = device_memory;
  range.offset = range_offset;
  range.size = range_size;
  return vkInvalidateMappedMemoryRanges(device, 1, &range);
}
VkResult DeviceMemoryAllocation::FlushHostCache(
    VkDevice device, VkDeviceSize range_offset, VkDeviceSize range_size) const {
  if (!mapped) {
    return VK_SUCCESS;
  }
  VkMappedMemoryRange range = {};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = device_memory;
  range.offset = range_offset;
  range.size = range_size;
  return vkFlushMappedMemoryRanges(device, 1, &range);
}

}  // namespace spokk
