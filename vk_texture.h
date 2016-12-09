#if !defined(VK_TEXTURE_H)
#define VK_TEXTURE_H

#include <vulkan/vulkan.h>

#include <memory>
#include <string>

namespace cdsvk {

class DeviceContext;
struct DeviceMemoryAllocation;

class TextureLoader {
public:
  explicit TextureLoader(const DeviceContext& device_context);
  ~TextureLoader();

  int load_vkimage_from_file(VkImage *out_image, VkImageCreateInfo *out_image_ci,
    DeviceMemoryAllocation *out_memory, const std::string &filename, VkBool32 generate_mipmaps,
    VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VkAccessFlags final_access_flags = VK_ACCESS_SHADER_READ_BIT) const;

  int generate_vkimage_mipmaps(VkImage image, const VkImageCreateInfo &image_ci,
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

}  // namespace cdsvk

#endif  // defined(VK_TEXTURE_H)
