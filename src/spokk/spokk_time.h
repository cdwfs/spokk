#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace spokk {

class Device;  // from spokk_device.h

class TimestampQueryPool {
public:
  struct CreateInfo {
    uint32_t swapchain_image_count;
    uint32_t timestamp_id_count;
    uint32_t queue_family_index;
  };

  VkResult Create(const Device& device, const CreateInfo& ci);
  void Destroy(const Device& device);

  // Resets query results for the specified swapchain image, and sets the target frame for
  // subsequent calls to WriteTimestamp.
  // swapchain_image_index is the important thing to get right. frame_index is just optional
  // metadata that gets passed back by GetResults(), so that the caller knows when the results
  // they're retrieving were captured.
  void SetTargetFrame(VkCommandBuffer cb, uint32_t swapchain_image_index, int64_t frame_index);
  // Pretty straightforward wrapper around vkCmdWriteTimestamp(). Each timestamp should only be written
  // once per frame.
  void WriteTimestamp(VkCommandBuffer cb, VkPipelineStageFlagBits stage, uint32_t timestamp_id);

  // Retrieve the timestamp values for the specified swapchain_image_index.
  // This function must be called after vkAcquireNextImageKHR().
  // The timestamp_count must match the timestamp_id_count value passed when the pool was created,
  // and is the number of elements that will be written to out_timestamp_seconds and out_timestamp_validity.
  // After a successful call, out_timestamp_seconds[id] contains the value of the id'th timestamp for
  // the specified swapchain image, pre-converted to seconds. out_timestamp_validity[id] will be true if the data
  // in out_timestamp_seconds[id] was available; if false for a particular timestamp ID, its reading should
  // be ignored. It will also be false for timestamps not written for the specified swapchain image.
  // To ensure all timestamps written for a frame have available values, pass a VkFence to vkAcquireNextImage()
  // and wait on it before calling GetResults().
  // out_frame_index will contain the frame_index passed to SetTargetFrame() for the specific swapchain image.
  VkResult GetResults(const Device& device, uint32_t swapchain_image_index, uint32_t timestamp_count,
      double* out_timestamp_seconds, bool* out_timestamp_validity, int64_t* out_frame_index);

private:
  VkQueryPool qpool_ = VK_NULL_HANDLE;
  uint64_t timestamp_valid_mask_;  // queue_family_props.timestampValidBits
  double seconds_per_tick_;  // device.properties.limits.timestampPeriod / 1e9;
  int32_t target_swapchain_image_index_ = -1;
  uint32_t timestamp_id_count_ = 0;
  std::vector<int64_t> swapchain_image_frame_indices_;  // one per swapchain image
};
}  // namespace spokk
