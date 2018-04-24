#include "spokk_buffer.h"

#include "spokk_debug.h"
#include "spokk_device.h"
#include "spokk_utilities.h"

#include <cassert>
#include <cstring>
#include <memory>

namespace spokk {

PipelinedBuffer::PipelinedBuffer() : handles_{}, views_{}, depth_(0) {}
PipelinedBuffer::~PipelinedBuffer() {}
VkResult PipelinedBuffer::Create(const Device& device, uint32_t depth, const VkBufferCreateInfo& buffer_ci,
    VkMemoryPropertyFlags memory_properties, DeviceAllocationScope allocation_scope) {
  ZOMBO_ASSERT_RETURN(handles_.empty(), VK_ERROR_INITIALIZATION_FAILED, "Can't re-create an existing Buffer");
  depth_ = depth;
  if (depth > 0) {
    handles_.resize(depth);
    VkMemoryRequirements single_reqs = {};
    for (auto& buf : handles_) {
      SPOKK_VK_CHECK(vkCreateBuffer(device, &buffer_ci, device.HostAllocator(), &buf));
      // It's a validation error not to call this on every VkBuffer before binding its memory, even if you
      // know the results will be the same.
      vkGetBufferMemoryRequirements(device, buf, &single_reqs);
    }

    bytes_per_pframe_ = (single_reqs.size + (single_reqs.alignment - 1)) & ~(single_reqs.alignment - 1);
    VkMemoryRequirements full_reqs = single_reqs;
    full_reqs.size = bytes_per_pframe_ * depth;
    VkResult result = device.DeviceAlloc(full_reqs, memory_properties, allocation_scope, &memory_);
    if (result == VK_SUCCESS) {
      for (size_t iBuf = 0; iBuf < handles_.size(); ++iBuf) {
        SPOKK_VK_CHECK(vkBindBufferMemory(
            device, handles_[iBuf], memory_.device_memory, memory_.offset + iBuf * bytes_per_pframe_));
      }
    } else {
      Destroy(device);
      return result;
    }
  }
  return VK_SUCCESS;
}
VkResult PipelinedBuffer::Load(const Device& device, uint32_t pframe, ThsvsAccessType src_access,
    ThsvsAccessType dst_access, const void* src_data, size_t data_size, size_t src_offset,
    VkDeviceSize dst_offset) const {
  if (Handle(pframe) == VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED;  // Call Create() first!
  }
  VkResult result = VK_SUCCESS;
  if (memory_.Mapped()) {
    VkMappedMemoryRange pframe_range = {};
    pframe_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    pframe_range.memory = memory_.device_memory;
    pframe_range.offset = memory_.offset + pframe * bytes_per_pframe_;
    pframe_range.size = bytes_per_pframe_;
    SPOKK_VK_CHECK(vkInvalidateMappedMemoryRanges(device, 1, &pframe_range));
    memcpy(reinterpret_cast<uint8_t*>(Mapped(pframe)) + dst_offset,
        reinterpret_cast<const uint8_t*>(src_data) + src_offset, data_size);
    SPOKK_VK_CHECK(vkFlushMappedMemoryRanges(device, 1, &pframe_range));
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
      vkCmdUpdateBuffer(cb, Handle(pframe), dst_offset, data_size, reinterpret_cast<const uint32_t*>(src_dwords));
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
      vkCmdCopyBuffer(cb, staging_buffer.Handle(), Handle(pframe), 1, &copy_region);
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
VkResult PipelinedBuffer::CreateViews(const Device& device, VkFormat format) {
  if (depth_ == 0) {
    return VK_ERROR_INITIALIZATION_FAILED;  // Call create() first!
  }
  views_.reserve(depth_);
  VkBufferViewCreateInfo view_ci = {};
  view_ci.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
  view_ci.buffer = VK_NULL_HANDLE;  // filled in below
  view_ci.format = format;
  view_ci.offset = 0;  // relative to buffer, not memory block
  view_ci.range = VK_WHOLE_SIZE;
  for (auto buf : handles_) {
    view_ci.buffer = buf;
    VkBufferView view = VK_NULL_HANDLE;
    SPOKK_VK_CHECK(vkCreateBufferView(device, &view_ci, device.HostAllocator(), &view));
    views_.push_back(view);
  }
  return VK_SUCCESS;
}
void PipelinedBuffer::Destroy(const Device& device) {
  if (memory_.device_memory != VK_NULL_HANDLE) {
    device.DeviceFree(memory_);
  }
  for (auto view : views_) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyBufferView(device, view, device.HostAllocator());
    }
  }
  views_.clear();
  for (auto buf : handles_) {
    if (buf != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, buf, device.HostAllocator());
    }
  }
  handles_.clear();
  depth_ = 0;
}

VkResult PipelinedBuffer::InvalidatePframeHostCache(
    const Device& device, uint32_t pframe, VkDeviceSize offset, VkDeviceSize nbytes) const {
  return memory_.InvalidateHostCache(device, memory_.offset + pframe * bytes_per_pframe_ + offset, nbytes);
}

VkResult PipelinedBuffer::FlushPframeHostCache(
    const Device& device, uint32_t pframe, VkDeviceSize offset, VkDeviceSize nbytes) const {
  return memory_.FlushHostCache(device, memory_.offset + pframe * bytes_per_pframe_ + offset, nbytes);
}

}  // namespace spokk
