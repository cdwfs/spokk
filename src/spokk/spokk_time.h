#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace spokk {

class Device;  // from spokk_device.h

// Plan:
// - Application must call SetTargetFrame(frame_index) on a pool before writing any timestamps
//   for that frame.
//   Internally, this advances a counter to the next available bank of timestamps, and writes a
//   command that resets that bank's queries.
//   - Q: Is it possible to detect writes to timestamps between submission and SetTargetFrame?
//   - A: Yeah, probably.
// - Application then writes whatever timestamps it pleases.
//   - Q: is it an error to write the same timestamp ID twice in a single frame?
//   - A: Almost certainly, but let's make it configurable.
// - Application must do *something* to confirm that a particular frame's timestamps were actually
//   submitted, and to associate them with a swapchain image index.
//   - Q: Unfortunately this would need to happen for every pool involved in a particular CB submission.
//        I wish it could be made implicit. Would it be a fair assumption for now that I'm submitting
//        every command buffer I build?
// - Application retrieves query results.
//   - Option: in CPU code, one reasonably safe way to do this is to call vkAcquireNextImageKHR() and
//     then use the resulting swapchain image index to look up the appropriate bank of queries to
//     retrieve. But they're still not guaranteed to be ready, and if they're not, then what?
//   - Option: keep a small set of frames who've been submitted but not retrieved yet. Every
//     frame, attempt to retrieve the results of all frames in this set. If results are ready, remove
//     that frame from the set. If not, skip it until next time.
//     - Q: does AMD's driver store timers in host-visible memory, or is this a guaranteed transfer+block?
//   - Option: copy results into a GPU ring buffer. This is really only viable if the results are being
//     consumed by a shader, as there's no way to prevent the host from reading the results mid-write.
//     - command sequence:
//       - barrier1 = VkBufferMemoryBarrier(results_buffer, old=*_READ, new=TRANSFER_WRITE,
//                                          offset=(frame_index*frame_size) % buffer_size,
//                                          size=frame_size);
//       - vkCmdPipelineBarriers(barrier1);
//       - vkCmdCopyQueryPoolResults(results_buffer);
//       - vkCmdFillBuffer(); // write any metadata into the results: frame_index, etc.
//       - barrier2 = VkBufferMemoryBarrier(results_buffer, old=TRANSFER_WRITE, new=*_READ,
//                                          offset=(frame_index*frame_size) % buffer_size,
//                                          size=frame_size);
//       - barrier3 = VkBufferMemoryBarrier(result_buffer_pointer, old=*_READ, new=TRANSFER_WRITE,
//                                          offset=..., size=sizeof(uint32_t));
//       - vkCmdPipelineBarriers(barrier2, barrier3);
//       - vkCmdFillBuffer(); // update pointer to oldest entry in the ring buffer
//     - this sequence must happen outside a render pass.
//     - this sequence must either be in the same command buffer as the query writes, or in a separate CB
//       that waits on semaphore(s) signaled by the CBs in which the queries are written.
class TimestampQueryPool {
public:
  struct CreateInfo {
    uint32_t swapchain_image_count;
    uint32_t timestamp_id_count;
    uint32_t queue_family_index;  // used to query timestamp granularity
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

  // TODO(cort): I want the application to not need to manually track whether queries have been submitted for
  // a given swapchain image before retrieving results; if no queries were submitted, just treat all timestamps
  // as invalid / not ready. But I don't like this specific implementation:
  // 1) Emitting a WriteTimestamp command to a command buffer does not necessarily imply that CB will ever be submitted.
  // 2) There is currently an implicit requirement that SetTargetFrame() be called before WriteTimestamp(), or else
  //    the "queries written" flag will be cleared after it's set.
  // I need to make this API more foolproof, but this unblocks me for the new validation errors in SDK 1.0.68.
  std::vector<bool>
      queries_written_for_swapchain_image_;  // has at least one query been written for a given swapchain image
};
}  // namespace spokk
