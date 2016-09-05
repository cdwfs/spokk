#if !defined(VK_TEXTURE_H)
#define VK_TEXTURE_H

#include "stb_vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

int load_vkimage_from_file(VkImage *out_image, VkImageCreateInfo *out_image_ci,
    VkDeviceMemory *out_mem, VkDeviceSize *out_mem_offset,
    const stbvk_context *context, const char *filename, VkBool32 generate_mipmaps,
    VkImageLayout final_layout, VkAccessFlags final_access_flags);

int generate_vkimage_mipmaps(VkImage image, const VkImageCreateInfo *image_ci,
    const stbvk_context *context, VkImageLayout input_layout, VkAccessFlags input_access_flags,
    VkImageLayout final_layout, VkAccessFlags final_access_flags);

#ifdef __cplusplus
}
#endif

#endif  // defined(VK_TEXTURE_H)
