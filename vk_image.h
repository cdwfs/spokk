#if !defined(VK_IMAGE_H)
#define VK_IMAGE_H

#include "vk_buffer.h"
#include "vk_context.h"
#include "vk_memory.h"
#include "vk_utilities.h"

#include <memory>
#include <string>
#include <vector>

namespace spokk {

class DeviceContext;
struct DeviceMemoryAllocation;

// This class manages copying pixel data to and from device-local memory, moving data through an internal staging buffer
// where necessary.
// TODO(cort): I need to revisit this class. The pipelined staging buffer is fine for steady-state work, but init-time
// blits are large and bursty. Allocating enough memory for PFRAME_COUNT * MAX_BURST_SIZE seems wasteful.
class ImageBlitter {
public:
  ImageBlitter();
  ~ImageBlitter();
  VkResult Create(const DeviceContext& device_context, uint32_t pframe_count, VkDeviceSize staging_bytes_per_pframe);
  void Destroy(const DeviceContext& device_context);

  // Advances the staging buffer to the next pframe, emptying the new pframe's staging buffer. Caller must ensure all
  // this pframe's transfers are complete before calling this function, to avoid stomping on any transfers that are still
  // in flight.
  void NextPframe(void);

  // Preconditions and postconditions for the following functions:
  // - cb is in a recordable state
  // - dst_image layout is TRANSFER_DST
  // - dst_image access flags include TRANSFER_WRITE_BIT
  // - dst_image is owned by the queue family that cb will be submitted on. No queue family ownership transfers take place in this code.
  // Returns 0 on success, non-zero on errors.
  int CopyMemoryToImage(VkCommandBuffer cb, VkImage dst_image, const void *src_data, VkFormat format, const VkBufferImageCopy& copy);

private:
  PipelinedBuffer staging_buffer_;
  uint32_t current_pframe_;
  VkDeviceSize current_offset_;
  struct Range {
    const void *start;
    const void *end;
  };
  std::vector<Range> staging_ranges_;
};

////////////////////////////////////////////////////////////////////

struct Image {
  Image() : handle(VK_NULL_HANDLE), view(VK_NULL_HANDLE), memory{} {}

  VkResult Create(const DeviceContext& device_context, const VkImageCreateInfo& image_ci,
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE);

  // synchronous. Returns 0 on success, non-zero on failure.
  int CreateFromFile(const DeviceContext& device_context, ImageBlitter& blitter, const DeviceQueue *queue,
    const std::string& filename, VkBool32 generate_mipmaps = VK_TRUE,
    VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VkAccessFlags final_access_flags = VK_ACCESS_SHADER_READ_BIT);
  int LoadSubresourceFromMemory(const DeviceContext& device_context, ImageBlitter& blitter, const DeviceQueue *queue,
    const void* src_data, uint32_t src_row_nbytes, uint32_t src_layer_height,
    const VkImageSubresource& dst_subresource, VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VkAccessFlags final_access_flags = VK_ACCESS_SHADER_READ_BIT);
  int GenerateMipmaps(const DeviceContext& device_context, const DeviceQueue *queue, const VkImageMemoryBarrier& barrier, 
    uint32_t layer, uint32_t src_mip_level, uint32_t mips_to_gen = VK_REMAINING_MIP_LEVELS);
  // TODO(cort): asynchronous versions of the above functions, that take a VkEvent to set when the operation is complete.

  void Destroy(const DeviceContext& device_context);

  VkImage handle;
  VkImageCreateInfo image_ci;
  VkImageView view;
  DeviceMemoryAllocation memory;

private:
  // Precondition for the following function:
  // - cb is in a recordable state
  // - dst_image is owned by the queue family that cb will be submitted on. No queue family ownership transfers take place in this code.
  int GenerateMipmapsImpl(VkCommandBuffer cb, const VkImageMemoryBarrier& dst_barrier,
    uint32_t layer, uint32_t src_mip_level, uint32_t mips_to_gen = VK_REMAINING_MIP_LEVELS);

};

}  // namespace spokk

#endif  // defined(VK_IMAGE_H)
