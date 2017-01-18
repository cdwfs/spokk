#if !defined(VK_UTILITIES_H)
#define VK_UTILITIES_H

#include <vulkan/vulkan.h>

#include <memory>
#include <mutex>
#include <vector>
#include <string>

namespace spokk {

// Effective Modern C++, Item 21: make_unique() is C++14 only, but easy to implement in C++11.
template <typename T, typename... Ts>
std::unique_ptr<T> my_make_unique(Ts&&... params) {
  return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

VkImageAspectFlags GetImageAspectFlags(VkFormat format);

// Utilities for constructing viewports and scissor rects
inline VkViewport ExtentToViewport(const VkExtent2D extent, float zMin = 0.0f, float zMax = 1.0f) {
  return {0, 0, (float)extent.width, (float)extent.height, zMin, zMax};
}
inline VkRect2D ExtentToRect2D(const VkExtent2D extent) {
  return {0, 0, extent.width, extent.height};
}
inline VkViewport Rect2DToViewport(VkRect2D rect, float z_min = 0.0f, float z_max = 1.0f) {
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
  VkCommandBuffer AllocateAndBegin(void) const;
  // Ends recording on the command buffer, submits it, waits for it to complete, and returns
  // the command buffer to the pool.
  VkResult EndSubmitAndFree(VkCommandBuffer *cb) const;

private:
  VkCommandPool pool_ = VK_NULL_HANDLE;
  mutable std::mutex pool_mutex_ = {};

  // Cached handles -- do not delete!
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  uint32_t queue_family_ = VK_QUEUE_FAMILY_IGNORED;
  const VkAllocationCallbacks *allocator_ = nullptr;
};

VkResult GetSupportedInstanceLayers(const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
  std::vector<VkLayerProperties>* out_supported_layers, std::vector<const char*>* out_supported_layer_names);

VkResult GetSupportedInstanceExtensions(const std::vector<VkLayerProperties>& enabled_instance_layers,
  const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
  std::vector<VkExtensionProperties>* out_supported_extensions, std::vector<const char*>* out_supported_extension_names);

VkResult GetSupportedDeviceExtensions(VkPhysicalDevice physical_device, const std::vector<VkLayerProperties>& enabled_instance_layers,
  const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
  std::vector<VkExtensionProperties>* out_supported_extensions, std::vector<const char*>* out_supported_extension_names);

VkImageViewCreateInfo GetImageViewCreateInfo(VkImage image, const VkImageCreateInfo &image_ci);

VkSamplerCreateInfo GetSamplerCreateInfo(VkFilter min_mag_filter, VkSamplerMipmapMode mipmap_mode, VkSamplerAddressMode address_mode);

}  // namespace spokk

#endif // !defined(VK_UTILITIES_H)