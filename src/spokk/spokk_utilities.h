#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace spokk {

// Effective Modern C++, Item 21: make_unique() is C++14 only, but easy to implement in C++11.
template <typename T, typename... Ts>
std::unique_ptr<T> my_make_unique(Ts&&... params) {
  return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

VkImageAspectFlags GetImageAspectFlags(VkFormat format);

// Utilities for constructing viewports and scissor rects
// Note: output viewports have negative heights, to flip the Y axis to a left-handed NDC space
inline VkViewport ExtentToViewport(const VkExtent2D extent, float z_min = 0.0f, float z_max = 1.0f) {
  return {
      // clang-format off
    0, 
    (float)extent.height,
    (float)extent.width,
    -(float)extent.height,
    z_min,
    z_max
      // clang-format on
  };
}
inline VkRect2D ExtentToRect2D(const VkExtent2D extent) { return {{0, 0}, {extent.width, extent.height}}; }
inline VkViewport Rect2DToViewport(VkRect2D rect, float z_min = 0.0f, float z_max = 1.0f) {
  return {
      // clang-format off
    (float)rect.offset.x,
    (float)(rect.offset.y + rect.extent.height),
    (float)rect.extent.width,
    -(float)rect.extent.height,
    z_min,
    z_max
      // clang-format on
  };
}

// clang-format off
inline VkBool32 IsDepthFormat(VkFormat format) {
  return (format == VK_FORMAT_X8_D24_UNORM_PACK32 ||
      format == VK_FORMAT_D16_UNORM ||
      format == VK_FORMAT_D16_UNORM_S8_UINT ||
      format == VK_FORMAT_D24_UNORM_S8_UINT ||
      format == VK_FORMAT_D32_SFLOAT ||
      format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}
// clang-format on

// Initializing VkClearValue objects is a pain. Let's fix that.
inline VkClearValue CreateColorClearValue(float r, float g, float b, float a = 1.0f) {
  VkClearValue clear;
  clear.color.float32[0] = r;
  clear.color.float32[1] = g;
  clear.color.float32[2] = b;
  clear.color.float32[3] = a;
  return clear;
}
inline VkClearValue CreateDepthClearValue(float depth, uint32_t stencil) {
  VkClearValue clear;
  clear.depthStencil.depth = depth;
  clear.depthStencil.stencil = stencil;
  return clear;
}

// Determine the number of mip levels for a given base image extent. This includes the base level;
// for valid extents (w/h/d all >= 1), the result will also be >= 1.
uint32_t GetMaxMipLevels(VkExtent3D base_extent);

//
// Helpers for configuring device features. Passed in Application::CreateInfo.
// First parameter is the features supported by the device.
// Second parameter is the features that will be enabled.
// if an optional feature isn't set in supported_features, just don't set it.
// if a required feature isn't set in supported_features, return VK_FALSE to indicate
// that device creation should fail.
// If all required features are supported, set them in enabled_features and return VK_TRUE.
//
typedef VkBool32 (*SetDeviceFeaturesFunc)(
    const VkPhysicalDeviceFeatures& supported_features, VkPhysicalDeviceFeatures* enabled_features);
// Shortcut: enable features used by the spokk framework
VkBool32 EnableMinimumDeviceFeatures(
    const VkPhysicalDeviceFeatures& supported_features, VkPhysicalDeviceFeatures* enabled_features);
// Shortcut: enable all supported features.
VkBool32 EnableAllSupportedDeviceFeatures(
    const VkPhysicalDeviceFeatures& supported_features, VkPhysicalDeviceFeatures* enabled_features);

//
// Simplifies quick, synchronous, single-shot command buffers.
//
class OneShotCommandPool {
public:
  OneShotCommandPool(
      VkDevice device, VkQueue queue, uint32_t queue_family, const VkAllocationCallbacks* allocator = nullptr);
  ~OneShotCommandPool();

  // Allocates a new single shot command buffer and puts it into the recording state.
  // Commands can be written immediately.
  VkCommandBuffer AllocateAndBegin(void) const;
  // Ends recording on the command buffer, submits it, waits for it to complete, and returns
  // the command buffer to the pool.
  VkResult EndSubmitAndFree(VkCommandBuffer* cb) const;
  // In the event of an error, this variant skips submission and simply returns the CB to the pool.
  VkResult EndAbortAndFree(VkCommandBuffer* cb) const;

private:
  VkCommandPool pool_ = VK_NULL_HANDLE;
  mutable std::mutex pool_mutex_ = {};

  // Cached handles -- do not delete!
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  uint32_t queue_family_ = VK_QUEUE_FAMILY_IGNORED;
  const VkAllocationCallbacks* allocator_ = nullptr;
};

VkResult GetSupportedInstanceLayers(const std::vector<const char*>& required_names,
    const std::vector<const char*>& optional_names, std::vector<VkLayerProperties>* out_supported_layers,
    std::vector<const char*>* out_supported_layer_names);

VkResult GetSupportedInstanceExtensions(const std::vector<VkLayerProperties>& enabled_instance_layers,
    const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
    std::vector<VkExtensionProperties>* out_supported_extensions,
    std::vector<const char*>* out_supported_extension_names);

VkResult GetSupportedDeviceExtensions(VkPhysicalDevice physical_device,
    const std::vector<VkLayerProperties>& enabled_instance_layers, const std::vector<const char*>& required_names,
    const std::vector<const char*>& optional_names, std::vector<VkExtensionProperties>* out_supported_extensions,
    std::vector<const char*>* out_supported_extension_names);

VkImageViewCreateInfo GetImageViewCreateInfo(VkImage image, const VkImageCreateInfo& image_ci);

VkSamplerCreateInfo GetSamplerCreateInfo(
    VkFilter min_mag_filter, VkSamplerMipmapMode mipmap_mode, VkSamplerAddressMode address_mode);

}  // namespace spokk
