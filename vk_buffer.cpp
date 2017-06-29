#include "vk_buffer.h"
#include "vk_debug.h"
#include "vk_utilities.h"

#include <cassert>
#include <cstring>
#include <memory>

namespace spokk {

PipelinedBuffer::PipelinedBuffer()
  : handles_{}, views_{}, depth_(0) {
}
PipelinedBuffer::~PipelinedBuffer() {
}
VkResult PipelinedBuffer::Create(const DeviceContext& device_context, uint32_t depth, const VkBufferCreateInfo& buffer_ci,
  VkMemoryPropertyFlags memory_properties, DeviceAllocationScope allocation_scope) {
  assert(depth_ == 0);
  depth_ = depth;
  if (depth > 0) {
    handles_.resize(depth);
    VkMemoryRequirements single_reqs = {};
    for(auto& buf : handles_) {
      SPOKK_VK_CHECK(vkCreateBuffer(device_context.Device(), &buffer_ci, device_context.HostAllocator(), &buf));
      // It's a validation error not to call this on every VkBuffer before binding its memory, even if you
      // know the results will be the same.
      vkGetBufferMemoryRequirements(device_context.Device(), buf, &single_reqs);
    }

    bytes_per_pframe_ = (single_reqs.size + (single_reqs.alignment-1)) & ~(single_reqs.alignment-1);
    VkMemoryRequirements full_reqs = single_reqs;
    full_reqs.size = bytes_per_pframe_ * depth;
    memory_ = device_context.DeviceAlloc(full_reqs, memory_properties, allocation_scope);
    if (memory_.block) {
      for(size_t iBuf = 0; iBuf < handles_.size(); ++iBuf) {
        SPOKK_VK_CHECK(vkBindBufferMemory(device_context.Device(), handles_[iBuf],
          memory_.block->Handle(), memory_.offset + iBuf * bytes_per_pframe_));
      }
    } else {
      Destroy(device_context);
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
  }
  return VK_SUCCESS;
}
VkResult PipelinedBuffer::Load(const DeviceContext& device_context, uint32_t pframe, const void *src_data, size_t data_size,
    size_t src_offset, VkDeviceSize dst_offset) const {
  if (Handle(pframe) == VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED; // Call Create() first!
  }
  VkResult result = VK_SUCCESS;
  if (memory_.Mapped()) {
    VkMappedMemoryRange pframe_range = {};
    pframe_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    pframe_range.memory = memory_.block->Handle();
    pframe_range.offset = memory_.offset + pframe * bytes_per_pframe_;
    pframe_range.size = bytes_per_pframe_;
    SPOKK_VK_CHECK(vkInvalidateMappedMemoryRanges(device_context.Device(), 1, &pframe_range));
    memcpy(reinterpret_cast<uint8_t*>(Mapped(pframe)) + dst_offset,
      reinterpret_cast<const uint8_t*>(src_data) + src_offset,data_size);
    SPOKK_VK_CHECK(vkFlushMappedMemoryRanges(device_context.Device(), 1, &pframe_range));
  } else {
    // TODO(cort): Maybe it's time for a BufferLoader class?
    const DeviceQueue* transfer_queue = device_context.FindQueue(VK_QUEUE_TRANSFER_BIT);
    assert(transfer_queue != nullptr);
    std::unique_ptr<OneShotCommandPool> one_shot_cpool = my_make_unique<OneShotCommandPool>(device_context.Device(),
      transfer_queue->handle, transfer_queue->family, device_context.HostAllocator());
    VkCommandBuffer cb = one_shot_cpool->AllocateAndBegin();
    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;  // TODO(cort): pass in more specific access flags?
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = Handle(pframe);
    barrier.offset = dst_offset;
    barrier.size = data_size;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
      0, nullptr, 1, &barrier, 0, nullptr);
    if (data_size <= 65536) {
      uintptr_t src_dwords = uintptr_t(src_data) + src_offset;
      ZOMBO_ASSERT((src_dwords % sizeof(uint32_t)) == 0, "src_data (%p) + src_offset (%d) must be 4-byte aligned.",
                   src_data, uint32_t(src_offset));
      vkCmdUpdateBuffer(cb, Handle(pframe), dst_offset, data_size,
        reinterpret_cast<const uint32_t*>(src_dwords));
    } else {
      assert(0); // TODO(cort): staging buffer? Multiple vkCmdUpdateBuffers? Ignore for now, buffers are small.
    }
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;  // TODO(cort): pass in more specific access flags
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
      0, nullptr, 1, &barrier, 0, nullptr);
    result = one_shot_cpool->EndSubmitAndFree(&cb);
  }
  return result;
}
VkResult PipelinedBuffer::CreateViews(const DeviceContext& device_context, VkFormat format) {
  if (depth_ == 0) {
    return VK_ERROR_INITIALIZATION_FAILED; // Call create() first!
  }
  views_.reserve(depth_);
  VkBufferViewCreateInfo view_ci = {};
  view_ci.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
  view_ci.buffer = VK_NULL_HANDLE;  // filled in below
  view_ci.format = format;
  view_ci.offset = 0;  // relative to buffer, not memory block
  view_ci.range = VK_WHOLE_SIZE;
  for(auto buf : handles_) {
    view_ci.buffer = buf;
    VkBufferView view = VK_NULL_HANDLE;
    SPOKK_VK_CHECK(vkCreateBufferView(device_context.Device(), &view_ci, device_context.HostAllocator(), &view));
    views_.push_back(view);
  }
  return VK_SUCCESS;
}
void PipelinedBuffer::Destroy(const DeviceContext& device_context) {
  if (memory_.block) {
    device_context.DeviceFree(memory_);
  }
  for(auto view : views_) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyBufferView(device_context.Device(), view, device_context.HostAllocator());
    }
  }
  views_.clear();
  for(auto buf : handles_) {
    if (buf != VK_NULL_HANDLE) {
      vkDestroyBuffer(device_context.Device(), buf, device_context.HostAllocator());
    }
  }
  handles_.clear();
  depth_ = 0;
}

void PipelinedBuffer::InvalidatePframeHostCache(uint32_t pframe, VkDeviceSize offset, VkDeviceSize nbytes) const {
  VkMappedMemoryRange range = {};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = memory_.block->Handle();
  range.offset = memory_.offset + pframe * bytes_per_pframe_ + offset;
  range.size = nbytes;
  return memory_.block->InvalidateHostCache(range);
}

void PipelinedBuffer::FlushPframeHostCache(uint32_t pframe, VkDeviceSize offset, VkDeviceSize nbytes) const {
  VkMappedMemoryRange range = {};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = memory_.block->Handle();
  range.offset = memory_.offset + pframe * bytes_per_pframe_ + offset;
  range.size = nbytes;
  return memory_.block->FlushHostCache(range);
}


}  // namespace spokk
