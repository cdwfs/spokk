#if !defined(VK_IMAGE_H)
#define VK_IMAGE_H

#include "vk_context.h"
#include "vk_memory.h"
#include "vk_utilities.h"

#include <memory>
#include <string>

namespace spokk {

class DeviceContext;
struct DeviceMemoryAllocation;

class ImageLoader {
public:
  explicit ImageLoader(const DeviceContext& device_context);
  ~ImageLoader();

  int load_from_file(VkImage *out_image, VkImageCreateInfo *out_image_ci,
    DeviceMemoryAllocation *out_memory, const std::string &filename, VkBool32 generate_mipmaps,
    VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VkAccessFlags final_access_flags = VK_ACCESS_SHADER_READ_BIT) const;

  int generate_mipmaps(VkImage image, const VkImageCreateInfo &image_ci,
    VkImageLayout input_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VkAccessFlags input_access_flags = VK_ACCESS_SHADER_READ_BIT,
    VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VkAccessFlags final_access_flags = VK_ACCESS_SHADER_READ_BIT) const;

private:
  int record_mipmap_generation(VkCommandBuffer cb, VkImage image, const VkImageCreateInfo &image_ci,
    VkImageLayout input_layout, VkAccessFlags input_access_flags,
    VkImageLayout final_layout, VkAccessFlags final_access_flags) const;

  const DeviceContext& device_context_;
  std::unique_ptr<OneShotCommandPool> one_shot_cpool_;
  VkQueue transfer_queue_;
  uint32_t transfer_queue_family_;
};

struct Image {
  Image() : handle(VK_NULL_HANDLE), view(VK_NULL_HANDLE), memory{} {}
  VkResult create(const DeviceContext& device_context, const VkImageCreateInfo image_ci,
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    DeviceAllocationScope allocation_scope = DEVICE_ALLOCATION_SCOPE_DEVICE);
  VkResult create_and_load(const DeviceContext& device_context, const ImageLoader& loader,
    const std::string& filename, VkBool32 generate_mipmaps = VK_TRUE,
    VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VkAccessFlags final_access_flags = VK_ACCESS_SHADER_READ_BIT);
  void destroy(const DeviceContext& device_context);
  VkImage handle;
  VkImageView view;
  DeviceMemoryAllocation memory;
};


}  // namespace spokk

#endif  // defined(VK_IMAGE_H)
