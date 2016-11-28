#include "vk_init.h"
using namespace cdsvk;

#include <assert.h>

VkResult cdsvk::get_supported_instance_layers(const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
    std::vector<VkLayerProperties>* out_supported_layers, std::vector<const char*>* out_supported_layer_names) {
  out_supported_layers->clear();
  out_supported_layer_names->clear();
  uint32_t all_instance_layer_count = 0;
  std::vector<VkLayerProperties> all_instance_layers;
  VkResult result = VK_INCOMPLETE;
  do {
    result = vkEnumerateInstanceLayerProperties(&all_instance_layer_count, nullptr);
    if (result == VK_SUCCESS && all_instance_layer_count > 0) {
      all_instance_layers.resize(all_instance_layer_count);
      result = vkEnumerateInstanceLayerProperties(&all_instance_layer_count, all_instance_layers.data());
    }
  } while (result == VK_INCOMPLETE);
  for(const auto& layer : all_instance_layers) {
    if (layer.specVersion & (1<<31)) {
      return VK_ERROR_INITIALIZATION_FAILED; // We use the high bit to mark duplicates; it had better not be set up front!
    }
  }
  out_supported_layers->reserve(all_instance_layers.size());
  // Check optional layers first, removing duplicates (some loaders don't like duplicates).
  for(const auto &layer_name : optional_names) {
    for(auto& layer : all_instance_layers) {
      if (strcmp(layer_name, layer.layerName) == 0) {
        if ( (layer.specVersion & (1<<31)) == 0) {
          out_supported_layers->push_back(layer);
          layer.specVersion |= (1<<31);
        }
        break;
      }
    }
  }
  // TODO(cort): we could just blindly pass these in, and let
  // vkCreateInstance fail if they're not supported.
  for(const auto &layer_name : required_names) {
    bool found = false;
    for(auto& layer : all_instance_layers) {
      if (strcmp(layer_name, layer.layerName) == 0) {
        if ( (layer.specVersion & (1<<31)) == 0) {
          out_supported_layers->push_back(layer);
          layer.specVersion |= (1<<31);
        }
        found = true;
        break;
      }
    }
    if (!found) {
      out_supported_layers->clear();
      return VK_ERROR_LAYER_NOT_PRESENT;
    }
  }
  out_supported_layer_names->reserve(out_supported_layers->size());
  for(auto& layer : *out_supported_layers) {
    layer.specVersion ^= (1<<31);
    out_supported_layer_names->push_back(layer.layerName);
  }
  return VK_SUCCESS;
}

VkResult cdsvk::get_supported_instance_extensions(const std::vector<VkLayerProperties>& enabled_instance_layers,
    const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
    std::vector<VkExtensionProperties>* out_supported_extensions, std::vector<const char*>* out_supported_extension_names) {
  out_supported_extensions->clear();
  out_supported_extension_names->clear();
  // Build list of unique instance extensions across all enabled instance layers
  std::vector<VkExtensionProperties> all_instance_extensions;
  for(int32_t iLayer = -1; iLayer < (int32_t)enabled_instance_layers.size(); ++iLayer) {
    const char *layer_name = (iLayer == -1) ? nullptr : enabled_instance_layers[iLayer].layerName;
    uint32_t layer_instance_extension_count = 0;
    std::vector<VkExtensionProperties> layer_instance_extensions;
    VkResult result = VK_INCOMPLETE;
    do {
      result = vkEnumerateInstanceExtensionProperties(layer_name, &layer_instance_extension_count, nullptr);
      if (result == VK_SUCCESS && layer_instance_extension_count > 0) {
        layer_instance_extensions.resize(layer_instance_extension_count);
        result = vkEnumerateInstanceExtensionProperties(layer_name, &layer_instance_extension_count,
          layer_instance_extensions.data());
      }
    } while (result == VK_INCOMPLETE);
    for(const auto &layer_extension : layer_instance_extensions) {
      bool found = false;
      const std::string extension_name_str(layer_extension.extensionName);
      for(const auto &extension : all_instance_extensions) {
        if (extension_name_str == extension.extensionName) {
          found = true;
          break;
        }
      }
      if (!found) {
        all_instance_extensions.push_back(layer_extension);
      }
    }
  }
  for(const auto& extension : all_instance_extensions) {
    if (extension.specVersion & (1<<31)) {
      return VK_ERROR_INITIALIZATION_FAILED; // We use the high bit to mark duplicates; it had better not be set up front!
    }
  }

  out_supported_extensions->reserve(all_instance_extensions.size());
  // Check optional extensions first, removing duplicates (some loaders don't like duplicates).
  for(const auto &extension_name : optional_names) {
    for(auto& extension : all_instance_extensions) {
      if (strcmp(extension_name, extension.extensionName) == 0) {
        if ( (extension.specVersion & (1<<31)) == 0) {
          out_supported_extensions->push_back(extension);
          extension.specVersion |= (1<<31);
        }
        break;
      }
    }
  }
  // TODO(cort): we could just blindly pass these in, and let
  // vkCreateInstance fail if they're not supported.
  for(const auto &extension_name : required_names) {
    bool found = false;
    for(auto& extension : all_instance_extensions) {
      if (strcmp(extension_name, extension.extensionName) == 0) {
        if ( (extension.specVersion & (1<<31)) == 0) {
          out_supported_extensions->push_back(extension);
          extension.specVersion |= (1<<31);
        }
        found = true;
        break;
      }
    }
    if (!found) {
      out_supported_extensions->clear();
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
  }

  out_supported_extension_names->reserve(out_supported_extensions->size());
  for(auto& extension : *out_supported_extensions) {
    extension.specVersion ^= (1<<31);
    out_supported_extension_names->push_back(extension.extensionName);
  }
  return VK_SUCCESS;
}

VkResult cdsvk::find_physical_device(const std::vector<QueueFamilyRequirements>& qf_reqs, VkInstance instance,
    VkPhysicalDevice *out_physical_device, std::vector<uint32_t>* out_queue_families) {
  *out_physical_device = VK_NULL_HANDLE;
  uint32_t physical_device_count = 0;
  std::vector<VkPhysicalDevice> all_physical_devices;
  VkResult result = VK_INCOMPLETE;
  do {
    result = vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
    if (result == VK_SUCCESS && physical_device_count > 0) {
      all_physical_devices.resize(physical_device_count);
      result = vkEnumeratePhysicalDevices(instance, &physical_device_count, all_physical_devices.data());
    }
  } while (result == VK_INCOMPLETE);
  out_queue_families->clear();
  out_queue_families->resize(qf_reqs.size(), VK_QUEUE_FAMILY_IGNORED);
  for(auto physical_device : all_physical_devices) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> all_queue_family_properties(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, all_queue_family_properties.data());
    bool pd_meets_requirements = true;
    for(uint32_t iReq=0; iReq < qf_reqs.size(); ++iReq) {
      auto &reqs = qf_reqs[iReq];
      bool found_qf = false;
      for(uint32_t iQF=0; (size_t)iQF < all_queue_family_properties.size(); ++iQF) {
        if (all_queue_family_properties[iQF].queueCount < reqs.minimum_queue_count) {
          continue;  // insufficient queue count
        } else if ((all_queue_family_properties[iQF].queueFlags & reqs.flags) != reqs.flags) {
          continue;  // family doesn't support all required operations
        }
        VkBool32 supports_present = VK_FALSE;
        if (reqs.present_surface != VK_NULL_HANDLE) {
          result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, iQF, reqs.present_surface, &supports_present);
          if (result != VK_SUCCESS) {
            return result;
          } else if (!supports_present) {
            continue;  // Queue family can not present to the provided surface
          }
        }
        // This family meets all requirements. Hooray!
        (*out_queue_families)[iReq] = iQF;
        found_qf = true;
        break;
      }
      if (!found_qf) {
        pd_meets_requirements = false;
        continue;
      }
    }
    if (pd_meets_requirements) {
      *out_physical_device = physical_device;
      return VK_SUCCESS;
    }
  }
  return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult cdsvk::get_supported_device_extensions(VkPhysicalDevice physical_device, const std::vector<VkLayerProperties>& enabled_instance_layers,
    const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
    std::vector<VkExtensionProperties>* out_supported_extensions, std::vector<const char*>* out_supported_extension_names) {
  out_supported_extensions->clear();
  std::vector<VkExtensionProperties> all_device_extensions;
  // Build list of unique device extensions across all enabled instance layers
  for(int32_t iLayer = -1; iLayer < (int32_t)enabled_instance_layers.size(); ++iLayer) {
    const char *layer_name = (iLayer == -1) ? nullptr : enabled_instance_layers[iLayer].layerName;
    uint32_t layer_device_extension_count = 0;
    std::vector<VkExtensionProperties> layer_device_extensions;
    VkResult result = VK_INCOMPLETE;
    do {
      result = vkEnumerateDeviceExtensionProperties(physical_device, layer_name, &layer_device_extension_count, nullptr);
      if (result == VK_SUCCESS && layer_device_extension_count > 0) {
        layer_device_extensions.resize(layer_device_extension_count);
        result = vkEnumerateDeviceExtensionProperties(physical_device, layer_name, &layer_device_extension_count,
          layer_device_extensions.data());
      }
    } while (result == VK_INCOMPLETE);
    for(const auto &layer_extension : layer_device_extensions) {
      bool found = false;
      const std::string extension_name_str(layer_extension.extensionName);
      for(const auto &extension : all_device_extensions) {
        if (extension_name_str == extension.extensionName) {
          found = true;
          break;
        }
      }
      if (!found) {
        all_device_extensions.push_back(layer_extension);
      }
    }
  }
  for(const auto& extension : all_device_extensions) {
    if (extension.specVersion & (1<<31)) {
      return VK_ERROR_INITIALIZATION_FAILED; // We use the high bit to mark duplicates; it had better not be set up front!
    }
  }

  // Check optional extensions first, removing duplicates (some loaders don't like duplicates).
  out_supported_extensions->reserve(all_device_extensions.size());
  for(const auto &extension_name : optional_names) {
    for(auto& extension : all_device_extensions) {
      if (strcmp(extension_name, extension.extensionName) == 0) {
        if ( (extension.specVersion & (1<<31)) == 0) {
          out_supported_extensions->push_back(extension);
          extension.specVersion |= (1<<31);
        }
        break;
      }
    }
  }
  // TODO(cort): we could just blindly pass these in, and let
  // vkCreateDevice fail if they're not supported.
  for(const auto &extension_name : required_names) {
    bool found = false;
    for(auto& extension : all_device_extensions) {
      if (strcmp(extension_name, extension.extensionName) == 0) {
        if ( (extension.specVersion & (1<<31)) == 0) {
          out_supported_extensions->push_back(extension);
          extension.specVersion |= (1<<31);
        }
        found = true;
        break;
      }
    }
    if (!found) {
      out_supported_extensions->clear();
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
  }

  out_supported_extension_names->reserve(out_supported_extensions->size());
  for(auto& extension : *out_supported_extensions) {
    extension.specVersion ^= (1<<31);
    out_supported_extension_names->push_back(extension.extensionName);
  }
  return VK_SUCCESS;
}
