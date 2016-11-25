#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <string>

// Vulkan initialization process:
// 1) Create a VkInstance. Use get_supported_instance_layers() and get_supported_instance_extensions()
//    to build the lists of layers and extensions for the VkInstanceCreateInfo.
// 2) Create VkDebugCallbackEXT.
// 3) Create VkSurfaceKHR.
// 4) Use find_physical_device() to identify a VkPhysicalDevice compatible with the provided
//    queue family features (including the ability to present to specific VkSurfaceKHRs).
// 5) Create VkDevice, using get_supported_device_extensions() to build the list of extensions for
//    the VkDeviceCreateInfo.

namespace cdsvk {

VkResult get_supported_instance_layers(const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
  std::vector<VkLayerProperties>* out_supported_layers, std::vector<const char*>* out_supported_layer_names);
VkResult get_supported_instance_extensions(const std::vector<VkLayerProperties>& enabled_instance_layers,
  const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
  std::vector<VkExtensionProperties>* out_supported_extensions, std::vector<const char*>* out_supported_extension_names);

struct QueueFamilyRequirements {
  VkQueueFlags flags;  // Mask of features which must be supported by this queue family.
  VkSurfaceKHR present_surface;  // If flags & VK_QUEUE_COMPUTE_BIT, this is the surface the queue must be able to present to. Otherwise, it is ignored.
  uint32_t minimum_queue_count;
};
VkResult find_physical_device(const std::vector<QueueFamilyRequirements>& qf_reqs, VkInstance instance,
  VkPhysicalDevice *out_physical_device, std::vector<uint32_t>* out_queue_families);

VkResult get_supported_device_extensions(VkPhysicalDevice physical_device, const std::vector<VkLayerProperties>& enabled_instance_layers,
  const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
  std::vector<VkExtensionProperties>* out_supported_extensions, std::vector<const char*>* out_supported_extension_names);

}  // namespace cdsvk
