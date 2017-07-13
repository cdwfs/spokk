#pragma once

#include "spokk_buffer.h"
#include "spokk_context.h"
#include "spokk_memory.h"
#include "spokk_utilities.h"

#include <memory>
#include <string>
#include <vector>

namespace spokk {

class DeviceContext;
struct DeviceMemoryAllocation;

struct Image {
  Image() : handle(VK_NULL_HANDLE), view(VK_NULL_HANDLE), memory{} {}

  VkResult Create(const DeviceContext& device_context, const VkImageCreateInfo& image_ci,
      VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE);

  // synchronous. Returns 0 on success, non-zero on failure.
  int CreateFromFile(const DeviceContext& device_context, const DeviceQueue* queue, const std::string& filename,
      VkBool32 generate_mipmaps = VK_TRUE, VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VkAccessFlags final_access_flags = VK_ACCESS_SHADER_READ_BIT);
  int LoadSubresourceFromMemory(const DeviceContext& device_context, const DeviceQueue* queue, const void* src_data,
      uint32_t src_row_nbytes, uint32_t src_layer_height, const VkImageSubresource& dst_subresource,
      VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VkAccessFlags final_access_flags = VK_ACCESS_SHADER_READ_BIT);
  int GenerateMipmaps(const DeviceContext& device_context, const DeviceQueue* queue,
      const VkImageMemoryBarrier& barrier, uint32_t layer, uint32_t src_mip_level,
      uint32_t mips_to_gen = VK_REMAINING_MIP_LEVELS);
  // TODO(cort): asynchronous variants of these functions, that take a VkEvent to set when the operation is complete.

  void Destroy(const DeviceContext& device_context);

  VkImage handle;
  VkImageCreateInfo image_ci;
  VkImageView view;
  DeviceMemoryAllocation memory;

private:
  // Precondition for the following function:
  // - cb is in a recordable state
  // - dst_image is owned by the queue family that cb will be submitted on.
  //   No queue family ownership transfers take place in this code.
  int GenerateMipmapsImpl(VkCommandBuffer cb, const VkImageMemoryBarrier& dst_barrier, uint32_t layer,
      uint32_t src_mip_level, uint32_t mips_to_gen = VK_REMAINING_MIP_LEVELS);
};

}  // namespace spokk
