#if !defined(VK_UTILITIES_H)
#define VK_UTILITIES_H

#include <vulkan/vulkan.h>

#include <mutex>
#include <vector>
#include <string>

namespace spokk {

// Effective Modern C++, Item 21: make_unique() is C++14 only, but easy to implement in C++11.
template <typename T, typename... Ts>
std::unique_ptr<T> my_make_unique(Ts&&... params) {
  return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

VkImageAspectFlags vk_format_to_image_aspect_flags(VkFormat format);

// Utilities for constructing viewports and scissor rects
inline VkViewport vk_extent_to_viewport(const VkExtent2D extent, float zMin = 0.0f, float zMax = 1.0f) {
  return {0, 0, (float)extent.width, (float)extent.height, zMin, zMax};
}
inline VkRect2D vk_extent_to_rect2d(const VkExtent2D extent) {
  return {0, 0, extent.width, extent.height};
}
inline VkViewport vk_rect2d_to_viewport(VkRect2D rect, float z_min = 0.0f, float z_max = 1.0f) {
  return {
    (float)rect.offset.x, (float)rect.offset.y,
    (float)rect.extent.width, (float)rect.extent.height,
    z_min, z_max
  };
}

//
// Simplifies quick, synchronous, single-shot command buffers.
//
class OneShotCommandPool {
public:
  OneShotCommandPool(VkDevice device, VkQueue queue, uint32_t queue_family,
    const VkAllocationCallbacks *allocator = nullptr);
  ~OneShotCommandPool();

  // Allocates a new single shot command buffer and puts it into the recording state.
  // Commands can be written immediately.
  VkCommandBuffer allocate_and_begin(void) const;
  // Ends recording on the command buffer, submits it, waits for it to complete, and returns
  // the command buffer to the pool.
  VkResult end_submit_and_free(VkCommandBuffer *cb) const;

private:
  VkCommandPool pool_ = VK_NULL_HANDLE;
  mutable std::mutex pool_mutex_ = {};

  // Cached handles -- do not delete!
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  uint32_t queue_family_ = VK_QUEUE_FAMILY_IGNORED;
  const VkAllocationCallbacks *allocator_ = nullptr;
};

VkResult get_supported_instance_layers(const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
  std::vector<VkLayerProperties>* out_supported_layers, std::vector<const char*>* out_supported_layer_names);

VkResult get_supported_instance_extensions(const std::vector<VkLayerProperties>& enabled_instance_layers,
  const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
  std::vector<VkExtensionProperties>* out_supported_extensions, std::vector<const char*>* out_supported_extension_names);

VkResult get_supported_device_extensions(VkPhysicalDevice physical_device, const std::vector<VkLayerProperties>& enabled_instance_layers,
  const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
  std::vector<VkExtensionProperties>* out_supported_extensions, std::vector<const char*>* out_supported_extension_names);

VkImageViewCreateInfo view_ci_from_image(VkImage image, const VkImageCreateInfo &image_ci);

VkSamplerCreateInfo get_sampler_ci(VkFilter min_mag_filter, VkSamplerMipmapMode mipmap_mode, VkSamplerAddressMode address_mode);

}  // namespace spokk

#endif // !defined(VK_UTILITIES_H)
