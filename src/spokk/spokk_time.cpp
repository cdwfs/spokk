#include "spokk_time.h"

#include "spokk_debug.h"
#include "spokk_device.h"

#include <string.h>

namespace spokk {

VkResult TimestampQueryPool::Create(const Device& device, const CreateInfo& ci) {
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device.Physical(), &queue_family_count, nullptr);
  if (ci.queue_family_index >= queue_family_count) {
    return VK_ERROR_VALIDATION_FAILED_EXT;
  }
  std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device.Physical(), &queue_family_count, queue_family_properties.data());
  timestamp_valid_mask_ = (queue_family_properties[ci.queue_family_index].timestampValidBits == 64)
      ? UINT64_MAX
      : ((1ULL << queue_family_properties[ci.queue_family_index].timestampValidBits) - 1);

  seconds_per_tick_ = (double)device.Properties().limits.timestampPeriod / 1e9;
  timestamp_id_count_ = ci.timestamp_id_count;
  swapchain_image_frame_indices_.resize(ci.swapchain_image_count, -1);
  queries_written_for_swapchain_image_.resize(ci.swapchain_image_count, false);

  VkQueryPoolCreateInfo qpool_ci = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  qpool_ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
  qpool_ci.queryCount = ci.swapchain_image_count * ci.timestamp_id_count;
  VkResult result = vkCreateQueryPool(device, &qpool_ci, device.HostAllocator(), &qpool_);
  if (result != VK_SUCCESS) {
    return result;
  }
  return VK_SUCCESS;
}
void TimestampQueryPool::Destroy(const Device& device) {
  if (qpool_ != VK_NULL_HANDLE) {
    vkDestroyQueryPool(device, qpool_, device.HostAllocator());
    qpool_ = VK_NULL_HANDLE;
  }
}

void TimestampQueryPool::SetTargetFrame(VkCommandBuffer cb, uint32_t swapchain_image_index, int64_t frame_index) {
  if (swapchain_image_index >= (uint32_t)swapchain_image_frame_indices_.size()) {
    return;  // invalid swapchain image index
  }
  target_swapchain_image_index_ = (int32_t)swapchain_image_index;
  swapchain_image_frame_indices_[swapchain_image_index] = frame_index;

  // Reset this frame's range of the query pools
  const uint32_t query_base_index = target_swapchain_image_index_ * timestamp_id_count_;
  vkCmdResetQueryPool(cb, qpool_, query_base_index, timestamp_id_count_);
  queries_written_for_swapchain_image_[target_swapchain_image_index_] = false;
}

void TimestampQueryPool::WriteTimestamp(VkCommandBuffer cb, VkPipelineStageFlagBits stage, uint32_t timestamp_id) {
  if (timestamp_id >= timestamp_id_count_) {
    return;  // invalid timestamp ID
  }
  const uint32_t query_base_index = target_swapchain_image_index_ * timestamp_id_count_;
  queries_written_for_swapchain_image_[target_swapchain_image_index_] = true;
  vkCmdWriteTimestamp(cb, stage, qpool_, query_base_index + timestamp_id);
}

struct TimestampValue {
  uint64_t ticks;
  uint64_t available;
};
VkResult TimestampQueryPool::GetResults(const Device& device, uint32_t swapchain_image_index, uint32_t timestamp_count,
    double* out_timestamp_seconds, bool* out_timestamp_validity, int64_t* out_frame_index) {
  if (timestamp_count != timestamp_id_count_) {
    return VK_ERROR_VALIDATION_FAILED_EXT;
  }
  if (swapchain_image_index >= (uint32_t)swapchain_image_frame_indices_.size()) {
    return VK_ERROR_VALIDATION_FAILED_EXT;
  }
  memset(out_timestamp_validity, 0, timestamp_id_count_ * sizeof(bool));
  if (!queries_written_for_swapchain_image_[swapchain_image_index]) {
    // no queries written for this swapchain image = all timestamps invalid
    return VK_SUCCESS;
  }
  std::vector<TimestampValue> timestamps_raw(timestamp_id_count_, {0, 0});
  uint32_t query_base_index = swapchain_image_index * timestamp_id_count_;
  VkResult timestamp_result = vkGetQueryPoolResults(device, qpool_, query_base_index, timestamp_id_count_,
      timestamps_raw.size() * sizeof(TimestampValue), timestamps_raw.data(), sizeof(TimestampValue),
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
  if (timestamp_result == VK_NOT_READY) {
    return VK_SUCCESS;  // all have been marked invalid
  } else if (timestamp_result != VK_SUCCESS) {
    return timestamp_result;  // all have been marked invalid
  } else {
    for (uint32_t tsid = 0; tsid < timestamp_id_count_; ++tsid) {
      if (timestamps_raw[tsid].available != 0) {
        // mask out the valid bits of the timestamp
        timestamps_raw[tsid].ticks &= timestamp_valid_mask_;
        // Convert timestamp readings to seconds
        out_timestamp_seconds[tsid] = (double)timestamps_raw[tsid].ticks * seconds_per_tick_;
        out_timestamp_validity[tsid] = true;
      }
    }
  }
  if (out_frame_index) {
    *out_frame_index = swapchain_image_frame_indices_[swapchain_image_index];
  }
  swapchain_image_frame_indices_[swapchain_image_index] = -1;
  return VK_SUCCESS;
}

}  // namespace spokk
