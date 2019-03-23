#include "spokk_buffer.h"

#include "spokk_debug.h"
#include "spokk_device.h"
#include "spokk_utilities.h"

#include <cassert>
#include <cstring>
#include <memory>

namespace spokk {

Buffer::Buffer() : handle_{}, view_{} {}
Buffer::~Buffer() {}
VkResult Buffer::Create(const Device& device, const VkBufferCreateInfo& buffer_ci,
    VkMemoryPropertyFlags memory_properties, DeviceAllocationScope allocation_scope) {
  ZOMBO_ASSERT_RETURN(handle_ == VK_NULL_HANDLE, VK_ERROR_INITIALIZATION_FAILED, "Can't re-create an existing Buffer");

  VkMemoryRequirements mem_reqs = {};
  SPOKK_VK_CHECK(vkCreateBuffer(device, &buffer_ci, device.HostAllocator(), &handle_));
  // It's a validation error not to call this on every VkBuffer before binding its memory, even if you
  // know the results will be the same.
  vkGetBufferMemoryRequirements(device, handle_, &mem_reqs);

  nbytes_ = mem_reqs.size;
  VkResult result = device.DeviceAlloc(mem_reqs, memory_properties, allocation_scope, &memory_);
  if (result == VK_SUCCESS) {
    return vkBindBufferMemory(device, handle_, memory_.device_memory, memory_.offset);
  } else {
    Destroy(device);
    return result;
  }
}
VkResult Buffer::Load(const Device& device, ThsvsAccessType src_access, ThsvsAccessType dst_access,
    const void* src_data, size_t data_size, size_t src_offset, VkDeviceSize dst_offset) const {
  if (Handle() == VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED;  // Call Create() first!
  }
  VkResult result = VK_SUCCESS;
  if (memory_.Mapped()) {
    VkMappedMemoryRange mem_range = {};
    mem_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mem_range.memory = memory_.device_memory;
    mem_range.offset = memory_.offset;
    mem_range.size = Size();
    SPOKK_VK_CHECK(vkInvalidateMappedMemoryRanges(device, 1, &mem_range));
    memcpy(reinterpret_cast<uint8_t*>(Mapped()) + dst_offset, reinterpret_cast<const uint8_t*>(src_data) + src_offset,
        data_size);
    SPOKK_VK_CHECK(vkFlushMappedMemoryRanges(device, 1, &mem_range));
  } else {
    const DeviceQueue* transfer_queue = device.FindQueue(VK_QUEUE_TRANSFER_BIT);
    assert(transfer_queue != nullptr);
    std::unique_ptr<OneShotCommandPool> one_shot_cpool =
        my_make_unique<OneShotCommandPool>(device, *transfer_queue, transfer_queue->family, device.HostAllocator());
    VkCommandBuffer cb = one_shot_cpool->AllocateAndBegin();
    // Barrier between prior usage and transfer_write
    VkMemoryBarrier barrier = {};
    VkPipelineStageFlags barrier_src_stages = 0, barrier_dst_stages = 0;
    spokk::BuildVkMemoryBarrier(
        src_access, THSVS_ACCESS_TRANSFER_WRITE, &barrier_src_stages, &barrier_dst_stages, &barrier);
    vkCmdPipelineBarrier(cb, barrier_src_stages, barrier_dst_stages, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    // TODO(cort): staging buffer
    Buffer staging_buffer = {};
    if (data_size <= 65536 && (data_size % 4) == 0) {
      uintptr_t src_dwords = uintptr_t(src_data) + src_offset;
      ZOMBO_ASSERT((src_dwords % 4) == 0, "src_data (%p) + src_offset (%d) must be 4-byte aligned.", src_data,
          uint32_t(src_offset));
      vkCmdUpdateBuffer(cb, Handle(), dst_offset, data_size, reinterpret_cast<const uint32_t*>(src_dwords));
    } else {
      // TODO(cort): this should be replaced with a dedicated Buffer in the DeviceContext
      VkBufferCreateInfo staging_buffer_ci = {};
      staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      staging_buffer_ci.size = data_size;
      staging_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      staging_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      SPOKK_VK_CHECK(staging_buffer.Create(
          device, staging_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, spokk::DEVICE_ALLOCATION_SCOPE_FRAME));
      // barrier for staging buffer between host writes and transfer reads
      barrier_src_stages = 0;
      barrier_dst_stages = 0;
      spokk::BuildVkMemoryBarrier(
          THSVS_ACCESS_HOST_WRITE, THSVS_ACCESS_TRANSFER_READ, &barrier_src_stages, &barrier_dst_stages, &barrier);
      vkCmdPipelineBarrier(cb, barrier_src_stages, barrier_dst_stages, 0, 1, &barrier, 0, nullptr, 0, nullptr);
      // End TODO
      memcpy(staging_buffer.Mapped(), (uint8_t*)(src_data) + src_offset, data_size);
      VkBufferCopy copy_region = {};
      copy_region.srcOffset = 0;
      copy_region.dstOffset = dst_offset;
      copy_region.size = data_size;
      vkCmdCopyBuffer(cb, staging_buffer.Handle(), Handle(), 1, &copy_region);
    }
    // Barrier from transfer_write back to dst_access
    barrier_src_stages = 0;
    barrier_dst_stages = 0;
    spokk::BuildVkMemoryBarrier(
        THSVS_ACCESS_TRANSFER_WRITE, dst_access, &barrier_src_stages, &barrier_dst_stages, &barrier);
    vkCmdPipelineBarrier(cb, barrier_src_stages, barrier_dst_stages, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    if (staging_buffer.Handle() != VK_NULL_HANDLE) {
      staging_buffer.FlushHostCache(device);
    }
    result = one_shot_cpool->EndSubmitAndFree(&cb);
    if (staging_buffer.Handle() != VK_NULL_HANDLE) {
      staging_buffer.Destroy(device);  // TODO(cort): staging buffer
    }
  }
  return result;
}
VkResult Buffer::CreateView(const Device& device, VkFormat format) {
  if (Handle() == VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED;  // Call create() first!
  }
  VkBufferViewCreateInfo view_ci = {};
  view_ci.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
  view_ci.buffer = VK_NULL_HANDLE;  // filled in below
  view_ci.format = format;
  view_ci.offset = 0;  // relative to buffer, not memory block
  view_ci.range = VK_WHOLE_SIZE;
  view_ci.buffer = Handle();
  return vkCreateBufferView(device, &view_ci, device.HostAllocator(), &view_);
}
void Buffer::Destroy(const Device& device) {
  if (memory_.device_memory != VK_NULL_HANDLE) {
    device.DeviceFree(memory_);
  }
  if (view_ != VK_NULL_HANDLE) {
    vkDestroyBufferView(device, view_, device.HostAllocator());
  }
  if (handle_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, handle_, device.HostAllocator());
  }
}

VkResult Buffer::InvalidateHostCache(const Device& device, VkDeviceSize offset, VkDeviceSize nbytes) const {
  return memory_.InvalidateHostCache(device, memory_.offset + offset, nbytes);
}

VkResult Buffer::FlushHostCache(const Device& device, VkDeviceSize offset, VkDeviceSize nbytes) const {
  return memory_.FlushHostCache(device, memory_.offset + offset, nbytes);
}

}  // namespace spokk
