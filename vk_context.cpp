#include "vk_context.h"

#include <cassert>

namespace spokk {

DeviceContext::DeviceContext(VkDevice device, VkPhysicalDevice physical_device, VkPipelineCache pipeline_cache,
  const DeviceQueue *queues, uint32_t queue_count,
  const VkAllocationCallbacks *host_allocator, const DeviceAllocationCallbacks *device_allocator) :
  physical_device_(physical_device),
  device_(device),
  pipeline_cache_(pipeline_cache),
  host_allocator_(host_allocator),
  device_allocator_(device_allocator) {
  vkGetPhysicalDeviceProperties(physical_device_, &device_properties_);
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties_);
  queues_.insert(queues_.begin(), queues+0, queues+queue_count);
}
DeviceContext::~DeviceContext() {
}

const DeviceQueue* DeviceContext::find_queue(VkQueueFlags queue_flags,
  VkSurfaceKHR present_surface) const {
  // Search for an exact match first
  for(auto& queue : queues_) {
    if (queue.flags == queue_flags) {
      // Make sure presentation requirement is met, if necessary.
      if ((queue_flags | VK_QUEUE_GRAPHICS_BIT) != 0 && present_surface != VK_NULL_HANDLE) {
        if (queue.present_surface != present_surface) {
          continue;
        }
      }
      return &queue;
    }
  }
  // Next pass looks for anything with the right flags set
  for(auto& queue : queues_) {
    if ((queue.flags & queue_flags) == queue_flags) {
      // Make sure presentation requirement is met, if necessary.
      if ((queue_flags | VK_QUEUE_GRAPHICS_BIT) != 0 && present_surface != VK_NULL_HANDLE) {
        if (queue.present_surface != present_surface) {
          continue;
        }
      }
      return &queue;
    }
  }
  // No match for you!
  return nullptr;
}

uint32_t DeviceContext::find_memory_type_index(const VkMemoryRequirements &memory_reqs,
  VkMemoryPropertyFlags memory_properties_mask) const {
  for(uint32_t iMemType=0; iMemType<VK_MAX_MEMORY_TYPES; ++iMemType) {
    if ((memory_reqs.memoryTypeBits & (1<<iMemType)) != 0
      && (memory_properties_.memoryTypes[iMemType].propertyFlags & memory_properties_mask) == memory_properties_mask) {
      return iMemType;
    }
  }
  return VK_MAX_MEMORY_TYPES; // invalid index
}
VkMemoryPropertyFlags DeviceContext::memory_type_properties(uint32_t memory_type_index) const {
  if (memory_type_index >= memory_properties_.memoryTypeCount) {
    return (VkMemoryPropertyFlags)0;
  }
  return memory_properties_.memoryTypes[memory_type_index].propertyFlags;
}

DeviceMemoryAllocation DeviceContext::device_alloc(const VkMemoryRequirements &mem_reqs, VkMemoryPropertyFlags memory_properties_mask,
  DeviceAllocationScope scope) const {
  if (device_allocator_ != nullptr) {
    return device_allocator_->pfnAllocation(device_allocator_->pUserData, *this, mem_reqs, memory_properties_mask, scope);
  } else {
    DeviceMemoryAllocation allocation = {};
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type_index(mem_reqs, memory_properties_mask);
    if (alloc_info.memoryTypeIndex != VK_MAX_MEMORY_TYPES) {
      DeviceMemoryBlock *block = new DeviceMemoryBlock;
      VkResult result = block->allocate(*this, alloc_info);
      if (result == VK_SUCCESS) {
        allocation.block = block;
        allocation.offset = 0;
        allocation.size = alloc_info.allocationSize;
      } else {
        delete block;
      }
    }
    return allocation;
  }
}
void DeviceContext::device_free(DeviceMemoryAllocation allocation) const {
  if (allocation.block != nullptr) {
    if (device_allocator_ != nullptr) {
      return device_allocator_->pfnFree(device_allocator_->pUserData, *this, allocation);
    } else {
      assert(allocation.offset == 0);
      assert(allocation.size == allocation.block->info().allocationSize);
      allocation.block->free(*this);
    }
  }
}
DeviceMemoryAllocation DeviceContext::device_alloc_and_bind_to_image(VkImage image, VkMemoryPropertyFlags memory_properties_mask,
  DeviceAllocationScope scope) const {
  VkMemoryRequirements mem_reqs = {};
  vkGetImageMemoryRequirements(device_, image, &mem_reqs);
  DeviceMemoryAllocation allocation = device_alloc(mem_reqs, memory_properties_mask, scope);
  if (allocation.block != nullptr) {
    VkResult result = vkBindImageMemory(device_, image, allocation.block->handle(), allocation.offset);
    if (result != VK_SUCCESS) {
      device_free(allocation);
      allocation.block = nullptr;
    }
  }
  return allocation;
}
DeviceMemoryAllocation DeviceContext::device_alloc_and_bind_to_buffer(VkBuffer buffer, VkMemoryPropertyFlags memory_properties_mask,
  DeviceAllocationScope scope) const {
  VkMemoryRequirements mem_reqs = {};
  vkGetBufferMemoryRequirements(device_, buffer, &mem_reqs);
  DeviceMemoryAllocation allocation = device_alloc(mem_reqs, memory_properties_mask, scope);
  if (allocation.block != nullptr) {
    VkResult result = vkBindBufferMemory(device_, buffer, allocation.block->handle(), allocation.offset);
    if (result != VK_SUCCESS) {
      device_free(allocation);
      allocation.block = nullptr;
    }
  }
  return allocation;
}

void *DeviceContext::host_alloc(size_t size, size_t alignment, VkSystemAllocationScope scope) const {
  if (host_allocator_) {
    return host_allocator_->pfnAllocation(host_allocator_->pUserData,
      size, alignment, scope);
  } else {
#if defined(_MSC_VER)
    return _mm_malloc(size, alignment);
#else
    return malloc(size); // TODO(cort): ignores alignment :(
#endif
  }
}
void DeviceContext::host_free(void *ptr) const {
  if (host_allocator_) {
    return host_allocator_->pfnFree(host_allocator_->pUserData, ptr);
  } else {
#if defined(_MSC_VER)
    return _mm_free(ptr);
#else
    return free(ptr);
#endif
  }
}

}  // namespace spokk
