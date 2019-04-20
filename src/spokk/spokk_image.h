#pragma once

#include "spokk_barrier.h"
#include "spokk_buffer.h"
#include "spokk_device.h"
#include "spokk_memory.h"
#include "spokk_utilities.h"

#include <memory>
#include <string>
#include <vector>

namespace spokk {

class Device;
struct DeviceMemoryAllocation;

struct Image {
  Image() : handle(VK_NULL_HANDLE), image_ci{}, view(VK_NULL_HANDLE), memory{} {}

  VkResult Create(const Device& device, const VkImageCreateInfo& image_ci,
      VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE);

  // synchronous. Returns 0 on success, non-zero on failure.
  int CreateFromFile(const Device& device, const DeviceQueue* queue, const std::string& filename,
      VkBool32 generate_mipmaps = VK_TRUE,
      ThsvsAccessType final_access = THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER);
  int LoadSubresourceFromMemory(const Device& device, const DeviceQueue* queue, const void* src_data, size_t src_nbytes,
      uint32_t src_row_nbytes, uint32_t src_layer_height, const VkImageSubresource& dst_subresource,
      ThsvsAccessType final_access = THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER);
  int GenerateMipmaps(const Device& device, const DeviceQueue* queue, const ThsvsImageBarrier& barrier, uint32_t layer,
      uint32_t src_mip_level, uint32_t mips_to_gen = VK_REMAINING_MIP_LEVELS);
  // TODO(cort): asynchronous variants of these functions, that take a VkEvent to set when the operation is complete.

  void Destroy(const Device& device);

  VkImage handle;
  VkImageCreateInfo image_ci;
  VkImageView view;
  DeviceMemoryAllocation memory;

private:
  // Precondition for the following function:
  // - cb is in a recordable state
  // - dst_image is owned by the queue family that cb will be submitted on.
  //   No queue family ownership transfers take place in this code.
  // - dst_barrier should contain the old & new access types, and is used to generate intermediate
  //   barriers with the appropriate endpoints.
  int GenerateMipmapsImpl(VkCommandBuffer cb, const ThsvsImageBarrier& dst_barrier, uint32_t layer,
      uint32_t src_mip_level, uint32_t mips_to_gen = VK_REMAINING_MIP_LEVELS);
};

}  // namespace spokk

