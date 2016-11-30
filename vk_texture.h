#if !defined(VK_TEXTURE_H)
#define VK_TEXTURE_H

#include "vk_application.h"
#include <memory>
#include <string>

namespace cdsvk {

class TextureLoader {
public:
  explicit TextureLoader(const DeviceContext* device_context);
  ~TextureLoader();

  int load_vkimage_from_file(VkImage *out_image, VkImageCreateInfo *out_image_ci,
    VkDeviceMemory *out_mem, VkDeviceSize *out_mem_offset,
    const std::string &filename, VkBool32 generate_mipmaps,
    VkImageLayout final_layout, VkAccessFlags final_access_flags) const;

  int generate_vkimage_mipmaps(VkImage image, const VkImageCreateInfo &image_ci,
    VkImageLayout input_layout, VkAccessFlags input_access_flags,
    VkImageLayout final_layout, VkAccessFlags final_access_flags) const;

private:
  int record_mipmap_generation(VkCommandBuffer cb, VkImage image, const VkImageCreateInfo &image_ci,
    VkImageLayout input_layout, VkAccessFlags input_access_flags,
    VkImageLayout final_layout, VkAccessFlags final_access_flags) const;

  const DeviceContext* device_context_; // TODO(cort): shared_ptr?
  std::unique_ptr<OneShotCommandPool> one_shot_cpool_;
  VkQueue transfer_queue_;
  uint32_t transfer_queue_family_;
};

}  // namespace cdsvk

#endif  // defined(VK_TEXTURE_H)
