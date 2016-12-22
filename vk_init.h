#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <string>

namespace cdsvk {

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

}  // namespace cdsvk
