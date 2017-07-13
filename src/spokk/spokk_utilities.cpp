#include "spokk_utilities.h"

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cstring>

namespace spokk {

uint32_t GetMaxMipLevels(VkExtent3D base_extent) {
  if (base_extent.width == 0 || base_extent.height == 0 || base_extent.depth == 0) {
    return 0;
  }
  uint32_t count = 1;
  while (base_extent.width > 1 || base_extent.height > 1 || base_extent.depth > 1) {
    count += 1;
    base_extent.width = std::max(base_extent.width / 2, 1U);
    base_extent.height = std::max(base_extent.height / 2, 1U);
    base_extent.depth = std::max(base_extent.depth / 2, 1U);
  }
  return count;
}

VkImageAspectFlags GetImageAspectFlags(VkFormat format) {
  switch (format) {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  case VK_FORMAT_UNDEFINED:
    return static_cast<VkImageAspectFlagBits>(0);
  default:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

//
// OneShotCommandPool
//
OneShotCommandPool::OneShotCommandPool(
    VkDevice device, VkQueue queue, uint32_t queue_family, const VkAllocationCallbacks* allocator)
  : device_(device), queue_(queue), queue_family_(queue_family), allocator_(allocator) {
  VkCommandPoolCreateInfo cpool_ci = {};
  cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cpool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  cpool_ci.queueFamilyIndex = queue_family_;
  VkResult result = vkCreateCommandPool(device_, &cpool_ci, allocator, &pool_);
  assert(result == VK_SUCCESS);
}
OneShotCommandPool::~OneShotCommandPool() {
  if (pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, pool_, allocator_);
    pool_ = VK_NULL_HANDLE;
  }
}

VkCommandBuffer OneShotCommandPool::AllocateAndBegin(void) const {
  VkCommandBuffer cb = VK_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    VkCommandBufferAllocateInfo cb_allocate_info = {};
    cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_allocate_info.commandPool = pool_;
    cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_allocate_info.commandBufferCount = 1;
    if (VK_SUCCESS != vkAllocateCommandBuffers(device_, &cb_allocate_info, &cb)) {
      return VK_NULL_HANDLE;
    }
    VkCommandBufferBeginInfo cb_begin_info = {};
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult result = vkBeginCommandBuffer(cb, &cb_begin_info);
    if (VK_SUCCESS != result) {
      vkFreeCommandBuffers(device_, pool_, 1, &cb);
      return VK_NULL_HANDLE;
    }
  }
  return cb;
}

VkResult OneShotCommandPool::EndAbortAndFree(VkCommandBuffer* cb) const {
  VkResult result = vkEndCommandBuffer(*cb);
  {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    vkFreeCommandBuffers(device_, pool_, 1, cb);
  }
  *cb = VK_NULL_HANDLE;
  return result;
}

VkResult OneShotCommandPool::EndSubmitAndFree(VkCommandBuffer* cb) const {
  VkResult result = vkEndCommandBuffer(*cb);
  if (result == VK_SUCCESS) {
    VkFenceCreateInfo fence_ci = {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    result = vkCreateFence(device_, &fence_ci, allocator_, &fence);
    if (result == VK_SUCCESS) {
      VkSubmitInfo submit_info = {};
      submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submit_info.commandBufferCount = 1;
      submit_info.pCommandBuffers = cb;
      result = vkQueueSubmit(queue_, 1, &submit_info, fence);
      if (result == VK_SUCCESS) {
        result = vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX);
      }
    }
    vkDestroyFence(device_, fence, allocator_);
  }
  {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    vkFreeCommandBuffers(device_, pool_, 1, cb);
  }
  *cb = VK_NULL_HANDLE;
  return result;
}

#define SPOKK_ENABLE_REQUIRED_FEATURE(feature)      \
  if (!supported_features.feature) return VK_FALSE; \
  enabled_features->feature = VK_TRUE
#define SPOKK_ENABLE_OPTIONAL_FEATURE(feature) enabled_features->feature = supported_features.feature
VkBool32 EnableMinimumDeviceFeatures(
    const VkPhysicalDeviceFeatures& supported_features, VkPhysicalDeviceFeatures* enabled_features) {
  SPOKK_ENABLE_REQUIRED_FEATURE(samplerAnisotropy);
  SPOKK_ENABLE_REQUIRED_FEATURE(textureCompressionBC);
  return VK_TRUE;
}
VkBool32 EnableAllSupportedDeviceFeatures(
    const VkPhysicalDeviceFeatures& supported_features, VkPhysicalDeviceFeatures* enabled_features) {
  *enabled_features = supported_features;
  return VK_TRUE;
}
#undef SPOKK_ENABLE_REQUIRED_FEATURE
#undef SPOKK_ENABLE_OPTIONAL_FEATURE

VkResult GetSupportedInstanceLayers(const std::vector<const char*>& required_names,
    const std::vector<const char*>& optional_names, std::vector<VkLayerProperties>* out_supported_layers,
    std::vector<const char*>* out_supported_layer_names) {
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
  if (result != VK_SUCCESS) {
    return result;
  }
  for (const auto& layer : all_instance_layers) {
    if (layer.specVersion & (1 << 31)) {
      return VK_ERROR_INITIALIZATION_FAILED;  // We use the high bit to mark duplicates; it can't be set on input!
    }
  }
  out_supported_layers->reserve(all_instance_layers.size());
  // Check optional layers first, removing duplicates (some loaders don't like duplicates).
  for (const auto& layer_name : optional_names) {
    for (auto& layer : all_instance_layers) {
      if (strcmp(layer_name, layer.layerName) == 0) {
        if ((layer.specVersion & (1 << 31)) == 0) {
          out_supported_layers->push_back(layer);
          layer.specVersion |= (1 << 31);
        }
        break;
      }
    }
  }
  for (const auto& layer_name : required_names) {
    bool found = false;
    for (auto& layer : all_instance_layers) {
      if (strcmp(layer_name, layer.layerName) == 0) {
        if ((layer.specVersion & (1 << 31)) == 0) {
          out_supported_layers->push_back(layer);
          layer.specVersion |= (1 << 31);
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
  for (auto& layer : *out_supported_layers) {
    layer.specVersion ^= (1 << 31);
    out_supported_layer_names->push_back(layer.layerName);
  }
  return VK_SUCCESS;
}

VkResult GetSupportedInstanceExtensions(const std::vector<VkLayerProperties>& enabled_instance_layers,
    const std::vector<const char*>& required_names, const std::vector<const char*>& optional_names,
    std::vector<VkExtensionProperties>* out_supported_extensions,
    std::vector<const char*>* out_supported_extension_names) {
  out_supported_extensions->clear();
  out_supported_extension_names->clear();
  // Build list of unique instance extensions across all enabled instance layers
  std::vector<VkExtensionProperties> all_instance_extensions;
  for (int32_t iLayer = -1; iLayer < (int32_t)enabled_instance_layers.size(); ++iLayer) {
    const char* layer_name = (iLayer == -1) ? nullptr : enabled_instance_layers[iLayer].layerName;
    uint32_t layer_instance_extension_count = 0;
    std::vector<VkExtensionProperties> layer_instance_extensions;
    VkResult result = VK_INCOMPLETE;
    do {
      result = vkEnumerateInstanceExtensionProperties(layer_name, &layer_instance_extension_count, nullptr);
      if (result == VK_SUCCESS && layer_instance_extension_count > 0) {
        layer_instance_extensions.resize(layer_instance_extension_count);
        result = vkEnumerateInstanceExtensionProperties(
            layer_name, &layer_instance_extension_count, layer_instance_extensions.data());
      }
    } while (result == VK_INCOMPLETE);
    if (result != VK_SUCCESS) {
      return result;
    }
    for (const auto& layer_extension : layer_instance_extensions) {
      bool found = false;
      const std::string extension_name_str(layer_extension.extensionName);
      for (const auto& extension : all_instance_extensions) {
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
  for (const auto& extension : all_instance_extensions) {
    if (extension.specVersion & (1 << 31)) {
      return VK_ERROR_INITIALIZATION_FAILED;  // We use the high bit to mark duplicates; it can't be set on input!
    }
  }

  out_supported_extensions->reserve(all_instance_extensions.size());
  // Check optional extensions first, removing duplicates (some loaders don't like duplicates).
  for (const auto& extension_name : optional_names) {
    for (auto& extension : all_instance_extensions) {
      if (strcmp(extension_name, extension.extensionName) == 0) {
        if ((extension.specVersion & (1 << 31)) == 0) {
          out_supported_extensions->push_back(extension);
          extension.specVersion |= (1 << 31);
        }
        break;
      }
    }
  }
  for (const auto& extension_name : required_names) {
    bool found = false;
    for (auto& extension : all_instance_extensions) {
      if (strcmp(extension_name, extension.extensionName) == 0) {
        if ((extension.specVersion & (1 << 31)) == 0) {
          out_supported_extensions->push_back(extension);
          extension.specVersion |= (1 << 31);
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
  for (auto& extension : *out_supported_extensions) {
    extension.specVersion ^= (1 << 31);
    out_supported_extension_names->push_back(extension.extensionName);
  }
  return VK_SUCCESS;
}

VkResult GetSupportedDeviceExtensions(VkPhysicalDevice physical_device,
    const std::vector<VkLayerProperties>& enabled_instance_layers, const std::vector<const char*>& required_names,
    const std::vector<const char*>& optional_names, std::vector<VkExtensionProperties>* out_supported_extensions,
    std::vector<const char*>* out_supported_extension_names) {
  out_supported_extensions->clear();
  std::vector<VkExtensionProperties> all_device_extensions;
  // Build list of unique device extensions across all enabled instance layers
  for (int32_t iLayer = -1; iLayer < (int32_t)enabled_instance_layers.size(); ++iLayer) {
    const char* layer_name = (iLayer == -1) ? nullptr : enabled_instance_layers[iLayer].layerName;
    uint32_t layer_device_extension_count = 0;
    std::vector<VkExtensionProperties> layer_device_extensions;
    VkResult result = VK_INCOMPLETE;
    do {
      result =
          vkEnumerateDeviceExtensionProperties(physical_device, layer_name, &layer_device_extension_count, nullptr);
      if (result == VK_SUCCESS && layer_device_extension_count > 0) {
        layer_device_extensions.resize(layer_device_extension_count);
        result = vkEnumerateDeviceExtensionProperties(
            physical_device, layer_name, &layer_device_extension_count, layer_device_extensions.data());
      }
    } while (result == VK_INCOMPLETE);
    for (const auto& layer_extension : layer_device_extensions) {
      bool found = false;
      const std::string extension_name_str(layer_extension.extensionName);
      for (const auto& extension : all_device_extensions) {
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
  for (const auto& extension : all_device_extensions) {
    if (extension.specVersion & (1 << 31)) {
      return VK_ERROR_INITIALIZATION_FAILED;  // We use the high bit to mark duplicates; it can't be set on input!
    }
  }

  // Check optional extensions first, removing duplicates (some loaders don't like duplicates).
  out_supported_extensions->reserve(all_device_extensions.size());
  for (const auto& extension_name : optional_names) {
    for (auto& extension : all_device_extensions) {
      if (strcmp(extension_name, extension.extensionName) == 0) {
        if ((extension.specVersion & (1 << 31)) == 0) {
          out_supported_extensions->push_back(extension);
          extension.specVersion |= (1 << 31);
        }
        break;
      }
    }
  }
  for (const auto& extension_name : required_names) {
    bool found = false;
    for (auto& extension : all_device_extensions) {
      if (strcmp(extension_name, extension.extensionName) == 0) {
        if ((extension.specVersion & (1 << 31)) == 0) {
          out_supported_extensions->push_back(extension);
          extension.specVersion |= (1 << 31);
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
  for (auto& extension : *out_supported_extensions) {
    extension.specVersion ^= (1 << 31);
    out_supported_extension_names->push_back(extension.extensionName);
  }
  return VK_SUCCESS;
}

VkImageViewCreateInfo GetImageViewCreateInfo(VkImage image, const VkImageCreateInfo& image_ci) {
  VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
  if (image_ci.imageType == VK_IMAGE_TYPE_1D) {
    view_type = (image_ci.arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_1D_ARRAY;
  } else if (image_ci.imageType == VK_IMAGE_TYPE_2D) {
    if (image_ci.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) {
      assert((image_ci.arrayLayers) % 6 == 0);
      view_type = (image_ci.arrayLayers == 6) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    } else {
      view_type = (image_ci.arrayLayers == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }
  } else if (image_ci.imageType == VK_IMAGE_TYPE_3D) {
    view_type = VK_IMAGE_VIEW_TYPE_3D;
  }
  VkImageViewCreateInfo view_ci = {};
  view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_ci.image = image;
  view_ci.viewType = view_type;
  view_ci.format = image_ci.format;
  view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  view_ci.subresourceRange.aspectMask = GetImageAspectFlags(image_ci.format);
  view_ci.subresourceRange.baseMipLevel = 0;
  view_ci.subresourceRange.levelCount = image_ci.mipLevels;
  view_ci.subresourceRange.baseArrayLayer = 0;
  view_ci.subresourceRange.layerCount = image_ci.arrayLayers;
  return view_ci;
}

VkSamplerCreateInfo GetSamplerCreateInfo(
    VkFilter min_mag_filter, VkSamplerMipmapMode mipmap_mode, VkSamplerAddressMode address_mode) {
  VkSamplerCreateInfo ci = {};
  ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  ci.magFilter = min_mag_filter;
  ci.minFilter = min_mag_filter;
  ci.mipmapMode = mipmap_mode;
  ci.addressModeU = address_mode;
  ci.addressModeV = address_mode;
  ci.addressModeW = address_mode;
  ci.mipLodBias = 0.0f;
  ci.anisotropyEnable = (min_mag_filter != VK_FILTER_NEAREST) ? VK_TRUE : VK_FALSE;
  ci.maxAnisotropy = ci.anisotropyEnable ? 16.0f : 1.0f;
  ci.compareOp = VK_COMPARE_OP_NEVER;
  ci.minLod = 0.0f;
  ci.maxLod = FLT_MAX;
  ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  ci.unnormalizedCoordinates = VK_FALSE;
  return ci;
}

}  // namespace spokk
