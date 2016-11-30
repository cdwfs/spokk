#if !defined(VK_TEXTURE_H)
#define VK_TEXTURE_H

#include "vk_application.h"
#include <string>

namespace cdsvk {

class TextureLoader {
public:
  struct CreateInfo {
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t transfer_queue_family;
    VkQueue transfer_queue;
    const VkAllocationCallbacks *allocator;
  };

  explicit TextureLoader(const TextureLoader::CreateInfo& ci);
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

  OneShotCommandPool one_shot_cpool_;

  // Cached Vulkan handles; do not delete!
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;  // only needed for validation; could maybe skip it.
  VkDevice device_ = VK_NULL_HANDLE;
  const VkAllocationCallbacks *allocator_;
  uint32_t transfer_queue_family_;
};

}  // namespace cdsvk

#endif  // defined(VK_TEXTURE_H)
