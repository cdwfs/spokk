// Must happen before any vulkan.h include, in order to get the platform-specific extensions included.
#if defined(_MSC_VER)
# define VK_USE_PLATFORM_WIN32_KHR 1
# define PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(unix) || defined(__unix__) || defined(__unix)
# define VK_USE_PLATFORM_XCB_KHR 1
# define PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
#elif defined(__ANDROID__)
# define VK_USE_PLATFORM_ANDROID_KHR 1
# define PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#else
# error Unsupported platform
# define PLATFORM_SURFACE_EXTENSION_NAME "Unsupported platform",
#endif

#include "vk_application.h"
#include "vk_debug.h"
#include "vk_init.h"
#include "vk_texture.h"
using namespace cdsvk;

#include "platform.h"

#include <spirv_glsl.hpp>

#include <cassert>
#include <cstdio>

#define CDSVK__CLAMP(x, xmin, xmax) ( ((x)<(xmin)) ? (xmin) : ( ((x)>(xmax)) ? (xmax) : (x) ) )

namespace {

void my_glfw_error_callback(int error, const char *description) {
  fprintf( stderr, "GLFW Error %d: %s\n", error, description);
}

VKAPI_ATTR VkBool32 VKAPI_CALL my_debug_report_callback(VkFlags msgFlags,
    VkDebugReportObjectTypeEXT /*objType*/, uint64_t /*srcObject*/, size_t /*location*/, int32_t msgCode,
    const char *pLayerPrefix, const char *pMsg, void * /*pUserData*/) {
  char *message = (char*)malloc(strlen(pMsg)+100);
  assert(message);
  if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
    sprintf(message, "ERROR: [%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
  } else if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
    sprintf(message, "WARNING: [%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
  } else if (msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
    sprintf(message, "INFO: [%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
  } else if (msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
    sprintf(message, "PERFORMANCE WARNING: [%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
  } else {
    return VK_FALSE;
  }
#if 0//_WIN32
  MessageBoxA(NULL, message, "Alert", MB_OK);
#else
  printf("%s\n", message);
  fflush(stdout);
#endif
  free(message);
  if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
    return VK_TRUE; // bail out now if an error occurred
  } else {
    return VK_FALSE; // otherwise, try to soldier on.
  }
}

const uint32_t kWindowWidthDefault = 1280;
const uint32_t kWindowHeightDefault = 720;
}  // namespace

//
// InputState
//
void InputState::Update(void) {
  std::shared_ptr<GLFWwindow> w = window_.lock();
  assert(w != nullptr);
  GLFWwindow *pw = w.get();

  prev_ = current_;

  current_.digital[DIGITAL_LPAD_UP] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_W));
  current_.digital[DIGITAL_LPAD_LEFT] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_A));
  current_.digital[DIGITAL_LPAD_RIGHT] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_D));
  current_.digital[DIGITAL_LPAD_DOWN] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_S));

  double mx = 0, my = 0;
  glfwGetCursorPos(pw, &mx, &my);
  current_.analog[ANALOG_MOUSE_X] = (float)mx;
  current_.analog[ANALOG_MOUSE_Y] = (float)my;
}

//
// DeviceMemoryBlock
//
VkResult DeviceMemoryBlock::allocate(const DeviceContext& device_context, const VkMemoryAllocateInfo &alloc_info) {
  assert(handle_ == VK_NULL_HANDLE);
  VkResult result = vkAllocateMemory(device_context.device(), &alloc_info, device_context.host_allocator(), &handle_);
  if (result == VK_SUCCESS) {
    info_ = alloc_info;
    VkMemoryPropertyFlags properties = device_context.memory_type_properties(alloc_info.memoryTypeIndex);
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      result = vkMapMemory(device_context.device(), handle_, 0, VK_WHOLE_SIZE, 0, &mapped_);
    } else {
      mapped_ = nullptr;
    }
  }
  return result;
}
void DeviceMemoryBlock::free(const DeviceContext& device_context) {
  if (handle_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_context.device(), handle_, device_context.host_allocator());
    handle_ = VK_NULL_HANDLE;
    mapped_ = nullptr;
  }
}

//
// DeviceMemoryAllocation
//
void DeviceMemoryAllocation::invalidate(VkDevice device) const {
  if (mapped()) {
    VkMappedMemoryRange range = {};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = block->handle();
    range.offset = offset;
    range.size = size;
    vkInvalidateMappedMemoryRanges(device, 1, &range);
  }
}
void DeviceMemoryAllocation::flush(VkDevice device) const {
  if (mapped()) {
    VkMappedMemoryRange range = {};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = block->handle();
    range.offset = offset;
    range.size = size;
    vkFlushMappedMemoryRanges(device, 1, &range);
  }
}


//
// DeviceContext
//

DeviceContext::DeviceContext(VkDevice device, VkPhysicalDevice physical_device, VkPipelineCache pipeline_cache,
      const DeviceQueueContext *queue_contexts, uint32_t queue_context_count,
      const VkAllocationCallbacks *host_allocator, const DeviceAllocationCallbacks *device_allocator) :
    physical_device_(physical_device),
    device_(device),
    pipeline_cache_(pipeline_cache),
    host_allocator_(host_allocator),
    device_allocator_(device_allocator) {
  vkGetPhysicalDeviceProperties(physical_device_, &device_properties_);
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties_);
  queue_contexts_.insert(queue_contexts_.begin(), queue_contexts+0, queue_contexts+queue_context_count);
}
DeviceContext::~DeviceContext() {
}

const DeviceQueueContext* DeviceContext::find_queue_context(VkQueueFlags queue_flags,
    VkSurfaceKHR present_surface) const {
  // Search for an exact match first
  for(auto& queue : queue_contexts_) {
    if (queue.queueFlags == queue_flags) {
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
  for(auto& queue : queue_contexts_) {
    if ((queue.queueFlags & queue_flags) == queue_flags) {
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


//
// OneShotCommandPool
//
OneShotCommandPool::OneShotCommandPool(VkDevice device, VkQueue queue, uint32_t queue_family,
      const VkAllocationCallbacks *allocator) :
    device_(device),
    queue_(queue),
    queue_family_(queue_family),
    allocator_(allocator) {
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

VkCommandBuffer OneShotCommandPool::allocate_and_begin(void) const {
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
  }
  VkCommandBufferBeginInfo cb_begin_info = {};
  cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VkResult result = vkBeginCommandBuffer(cb, &cb_begin_info);
  if (VK_SUCCESS != result) {
    vkFreeCommandBuffers(device_, pool_, 1, &cb);
    return VK_NULL_HANDLE;
  }
  return cb;
}

VkResult OneShotCommandPool::end_submit_and_free(VkCommandBuffer *cb) const {
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

//
// Buffer
//
VkResult Buffer::create(const DeviceContext& device_context, const VkBufferCreateInfo buffer_ci,
    VkMemoryPropertyFlags memory_properties, DeviceAllocationScope allocation_scope) {
  VkResult result = vkCreateBuffer(device_context.device(), &buffer_ci, device_context.host_allocator(), &handle);
  if (result == VK_SUCCESS) {
    memory = device_context.device_alloc_and_bind_to_buffer(handle, memory_properties, allocation_scope);
    if (memory.block == nullptr) {
      vkDestroyBuffer(device_context.device(), handle, device_context.host_allocator());
      handle = VK_NULL_HANDLE;
      result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
  }
  return result;
}
VkResult Buffer::load(const DeviceContext& device_context, const void *src_data, size_t data_size, size_t src_offset,
    VkDeviceSize dst_offset) const {
  if (handle == VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED; // Call create() first!
  }
  VkResult result = VK_SUCCESS;
  if (memory.mapped()) {
    memory.invalidate(device_context.device());
    memcpy(memory.mapped(), reinterpret_cast<const uint8_t*>(src_data) + src_offset, data_size);
    memory.flush(device_context.device());
  } else {
    // TODO(cort): Maybe it's time for a BufferLoader class?
    const DeviceQueueContext* transfer_queue_context = device_context.find_queue_context(VK_QUEUE_TRANSFER_BIT);
    assert(transfer_queue_context != nullptr);
    VkQueue transfer_queue = transfer_queue_context->queue;
    uint32_t transfer_queue_family = transfer_queue_context->queue_family;
    std::unique_ptr<OneShotCommandPool> one_shot_cpool = my_make_unique<OneShotCommandPool>(device_context.device(),
      transfer_queue, transfer_queue_family, device_context.host_allocator());
    VkCommandBuffer cb = one_shot_cpool->allocate_and_begin();
    if (data_size <= 65536) {
      vkCmdUpdateBuffer(cb, handle, dst_offset, data_size, reinterpret_cast<const uint8_t*>(src_data) + src_offset);
    } else {
      assert(0); // TODO(cort): staging buffer? Multiple vkCmdUpdateBuffers? Ignore for now, buffers are small.
    }
    result = one_shot_cpool->end_submit_and_free(&cb);
  }
  return result;
}
VkResult Buffer::create_view(const DeviceContext& device_context, VkFormat format) {
  if (handle == VK_NULL_HANDLE) {
    return VK_ERROR_INITIALIZATION_FAILED; // Call create() first!
  }
  VkBufferViewCreateInfo view_ci = {};
  view_ci.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
  view_ci.buffer = handle;
  view_ci.format = format;
  view_ci.offset = 0;
  view_ci.range = VK_WHOLE_SIZE;
  VkResult result = vkCreateBufferView(device_context.device(), &view_ci, device_context.host_allocator(), &view);
  return result;
}
void Buffer::destroy(const DeviceContext& device_context) {
  device_context.device_free(memory);
  memory.block = nullptr;
  if (view != VK_NULL_HANDLE) {
    vkDestroyBufferView(device_context.device(), view, device_context.host_allocator());
    view = VK_NULL_HANDLE;
  }
  vkDestroyBuffer(device_context.device(), handle, device_context.host_allocator());
  handle = VK_NULL_HANDLE;
}

//
// Image
//
VkResult Image::create(const DeviceContext& device_context, const VkImageCreateInfo image_ci,
    VkMemoryPropertyFlags memory_properties, DeviceAllocationScope allocation_scope) {
  VkResult result = vkCreateImage(device_context.device(), &image_ci, device_context.host_allocator(), &handle);
  if (result == VK_SUCCESS) {
    memory = device_context.device_alloc_and_bind_to_image(handle, memory_properties, allocation_scope);
    if (memory.block == nullptr) {
      vkDestroyImage(device_context.device(), handle, device_context.host_allocator());
      handle = VK_NULL_HANDLE;
      result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    } else {
      VkImageViewCreateInfo view_ci = cdsvk::view_ci_from_image(handle, image_ci);
      result = vkCreateImageView(device_context.device(), &view_ci, device_context.host_allocator(), &view);
    }
  }
  return result;
}
VkResult Image::create_and_load(const DeviceContext& device_context, const TextureLoader& loader, const std::string& filename,
    VkBool32 generate_mipmaps, VkImageLayout final_layout, VkAccessFlags final_access_flags) {
  VkImageCreateInfo image_ci = {};
  int load_error = loader.load_vkimage_from_file(&handle, &image_ci, &memory,
    filename, generate_mipmaps, final_layout, final_access_flags);
  if (load_error) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  VkImageViewCreateInfo view_ci = cdsvk::view_ci_from_image(handle, image_ci);
  VkResult result = vkCreateImageView(device_context.device(), &view_ci, device_context.host_allocator(), &view);
  return result;
}
void Image::destroy(const DeviceContext& device_context) {
  device_context.device_free(memory);
  memory.block = nullptr;
  vkDestroyImageView(device_context.device(), view, device_context.host_allocator());
  view = VK_NULL_HANDLE;
  vkDestroyImage(device_context.device(), handle, device_context.host_allocator());
  handle = VK_NULL_HANDLE;
}

//
// MeshFormat
//
static const std::array<MeshFormat, VK_PRIMITIVE_TOPOLOGY_RANGE_SIZE * 2> g_empty_mesh_formats = {{
    // Primitive restart disabled
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_FALSE} },
  // Primitive restart enabled
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_TRUE} },
  }};
const MeshFormat* MeshFormat::get_empty(VkPrimitiveTopology topology, VkBool32 enable_primitive_restart) {
  uint32_t index = topology;
  if (enable_primitive_restart) {
    index += VK_PRIMITIVE_TOPOLOGY_RANGE_SIZE;
  }
  return &g_empty_mesh_formats[index];
}

void MeshFormat::finalize(VkPrimitiveTopology topology, VkBool32 enable_primitive_restart) {
  vertex_input_state_ci = {};
  vertex_input_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_state_ci.vertexBindingDescriptionCount = (uint32_t)vertex_buffer_bindings.size();
  vertex_input_state_ci.pVertexBindingDescriptions = vertex_buffer_bindings.data();
  vertex_input_state_ci.vertexAttributeDescriptionCount = (uint32_t)vertex_attributes.size();
  vertex_input_state_ci.pVertexAttributeDescriptions = vertex_attributes.data();
  input_assembly_state_ci = {};
  input_assembly_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_state_ci.topology = topology;
  input_assembly_state_ci.primitiveRestartEnable = enable_primitive_restart;
}


// Helper for SPIRV-Cross shader resource parsing
static void add_shader_resource_to_dset_layouts(std::vector<DescriptorSetLayoutInfo>& dset_layout_infos,
    const spirv_cross::CompilerGLSL& glsl, const spirv_cross::Resource& resource,
    VkDescriptorType desc_type, VkShaderStageFlagBits stage) {
  uint32_t dset_index     = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
  uint32_t binding_index = glsl.get_decoration(resource.id, spv::DecorationBinding);
  auto resource_type = glsl.get_type(resource.type_id);
  uint32_t array_size = 1;
  for(auto arr_size : resource_type.array) {
    array_size *= arr_size;
  }
  // Add new dset(s) if necessary
  if (dset_index >= dset_layout_infos.size()) {
    dset_layout_infos.resize(dset_index + 1);
  }
  DescriptorSetLayoutInfo& layout_info = dset_layout_infos[dset_index];
  // Is this binding already in use?
  bool found_binding = false;
  for(uint32_t iBinding=0; iBinding<layout_info.bindings.size(); ++iBinding) {
    auto& binding = layout_info.bindings[iBinding];
    if (binding.binding == binding_index) {
      assert(binding.descriptorType == desc_type);  // same binding appears twice with different types
      assert(binding.descriptorCount == array_size);  // same binding appears twice with different array sizes
      binding.stageFlags |= stage;
      auto& binding_info = layout_info.binding_infos[iBinding];
      binding_info.stage_names.push_back(std::make_tuple(stage, glsl.get_name(resource.id)));
      found_binding = true;
      break;
    }
  }
  if (!found_binding) {
    VkDescriptorSetLayoutBinding new_binding = {};
    new_binding.binding = binding_index;
    new_binding.descriptorType = desc_type;
    new_binding.descriptorCount = array_size;
    new_binding.stageFlags = stage;
    new_binding.pImmutableSamplers = nullptr;
    layout_info.bindings.push_back(new_binding);
    DescriptorSetLayoutBindingInfo new_binding_info = {};
    new_binding_info.stage_names.push_back(std::make_tuple(stage, glsl.get_name(resource.id)));
    layout_info.binding_infos.push_back(new_binding_info);
  }
}

static void parse_shader_resources(std::vector<DescriptorSetLayoutInfo>& dset_layout_infos,
    VkPushConstantRange& push_constant_range, const spirv_cross::CompilerGLSL& glsl, VkShaderStageFlagBits stage) {
  spirv_cross::ShaderResources resources = glsl.get_shader_resources();
  // handle shader resources
  for (auto &resource : resources.uniform_buffers) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage);
  }
  for (auto &resource : resources.storage_buffers) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);
  }
  for (auto &resource : resources.storage_images) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stage);
  }
  for (auto &resource : resources.sampled_images) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stage);
  }
  for (auto &resource : resources.separate_images) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stage);
  }
  for (auto &resource : resources.separate_samplers) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_SAMPLER, stage);
  }
  for (auto &resource : resources.subpass_inputs) {
    add_shader_resource_to_dset_layouts(dset_layout_infos, glsl, resource, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, stage);
  }
  // Handle push constants. Each shader is only allowed to have one push constant block.
  push_constant_range = {};
  push_constant_range.stageFlags = stage;
  for (auto &resource : resources.push_constant_buffers) {
    size_t min_offset = UINT32_MAX, max_offset = 0;
    auto ranges = glsl.get_active_buffer_ranges(resource.id);
    if (!ranges.empty()) {
      for(const auto& range : ranges) {
        if (range.offset < min_offset)
          min_offset = range.offset;
        if (range.offset + range.range > max_offset)
          max_offset = range.offset + range.range;
      }
      push_constant_range.offset = (uint32_t)min_offset;
      push_constant_range.size = (uint32_t)(max_offset - min_offset);
    }
  }
#if 0
  // Handle stage inputs/outputs
  for (auto &resource : resources.stage_inputs) {
    uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
    uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
    uint32_t loc = glsl.get_decoration(resource.id, spv::DecorationLocation);
    printf("set = %4u, binding = %4u, loc = %4u: stage input '%s'\n", set, binding, loc, resource.name.c_str());
  }
  for (auto &resource : resources.stage_outputs) {
    uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
    uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
    uint32_t loc = glsl.get_decoration(resource.id, spv::DecorationLocation);
    printf("set = %4u, binding = %4u, loc = %4u: stage output '%s'\n", set, binding, loc, resource.name.c_str());
  }
  // ???
  for (auto &resource : resources.atomic_counters)
  {
    unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
    unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
    printf("set = %4u, binding = %4u: atomic counter '%s'\n", set, binding, resource.name.c_str());
  }
#endif
}

//
// Shader
//
VkResult Shader::create_and_load(const DeviceContext& device_context, const std::string& filename) {
  FILE *spv_file = fopen(filename.c_str(), "rb");
  if (!spv_file) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  fseek(spv_file, 0, SEEK_END);
  long spv_file_size = ftell(spv_file);
  fseek(spv_file, 0, SEEK_SET);
  VkResult result = create_and_load_from_file(device_context, spv_file, spv_file_size);
  fclose(spv_file);
  return result;
}
VkResult Shader::create_and_load_from_file(const DeviceContext& device_context, FILE *fp, int len) {
  assert((len % sizeof(uint32_t)) == 0);
  spirv.resize(len/sizeof(uint32_t));
  size_t bytes_read = fread(spirv.data(), 1, len, fp);
  if ( (int)bytes_read != len) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  spirv_cross::CompilerGLSL glsl(spirv);  // NOTE: throws an exception if you hand it malformed/invalid SPIRV.
  stage = VkShaderStageFlagBits(0);
  spv::ExecutionModel execution_model = glsl.get_execution_model();
  if        (execution_model == spv::ExecutionModelVertex) {
    stage = VK_SHADER_STAGE_VERTEX_BIT;
  } else if (execution_model == spv::ExecutionModelTessellationControl) {
    stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
  } else if (execution_model == spv::ExecutionModelTessellationEvaluation) {
    stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
  } else if (execution_model == spv::ExecutionModelGeometry) {
    stage = VK_SHADER_STAGE_GEOMETRY_BIT;
  } else if (execution_model == spv::ExecutionModelFragment) {
    stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  } else if (execution_model == spv::ExecutionModelGLCompute) {
    stage = VK_SHADER_STAGE_COMPUTE_BIT;
  }
  assert(stage != 0);

  parse_shader_resources(dset_layout_infos, push_constant_range, glsl, stage);

  VkShaderModuleCreateInfo shader_ci = {};
  shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_ci.codeSize = len;  // note: in bytes
  shader_ci.pCode = spirv.data();
  VkResult result = vkCreateShaderModule(device_context.device(), &shader_ci, device_context.host_allocator(), &handle);
  return result;
}
void Shader::destroy(const DeviceContext& device_context) {
  if (handle) {
    vkDestroyShaderModule(device_context.device(), handle, device_context.host_allocator());
    handle = VK_NULL_HANDLE;
  }
  unload_spirv();
  stage = (VkShaderStageFlagBits)0;
}

//
// ShaderPipeline
//
VkResult ShaderPipeline::create(const DeviceContext& device_context,
    const std::vector<ShaderPipelineEntry>& shader_entries) {
  // Determine active shader stages
  active_stages = 0;
  for(const auto& shader_entry : shader_entries) {
    if (shader_entry.shader == nullptr) {
      return VK_ERROR_INITIALIZATION_FAILED;  // NULL shader
    }
    if (active_stages & shader_entry.shader->stage) {
      return VK_ERROR_INITIALIZATION_FAILED;  // Duplicate shader stage
    }
    active_stages |= shader_entry.shader->stage;
  }
  constexpr std::array<VkShaderStageFlags, 5> valid_stage_combos = {{
      VK_SHADER_STAGE_COMPUTE_BIT,
      VK_SHADER_STAGE_VERTEX_BIT,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      // TODO(cort): tessellation? Or, hey, not.
    }};
  bool stages_are_valid = false;
  for(auto combo : valid_stage_combos) {
    if (active_stages == combo) {
      stages_are_valid = true;
      break;
    }
  }
  if (!stages_are_valid) {
    active_stages = 0;
    return VK_ERROR_INITIALIZATION_FAILED;  // Invalid combination of shader stages
  }

  // Merge shader resources from all active stages
  dset_layout_infos.clear();
  push_constant_ranges.clear();
  for(const auto& shader_entry : shader_entries) {
    // Grow descriptor set layout array if needed
    if (shader_entry.shader->dset_layout_infos.size() > dset_layout_infos.size()) {
      dset_layout_infos.resize(shader_entry.shader->dset_layout_infos.size());
    }
    // Add the push constants
    if (shader_entry.shader->push_constant_range.size > 0) {
      push_constant_ranges.push_back(shader_entry.shader->push_constant_range);
    }
    // Merge descriptor set layouts
    for(size_t iDS = 0; iDS < shader_entry.shader->dset_layout_infos.size(); ++iDS) {
      const DescriptorSetLayoutInfo& src_dset_layout_info = shader_entry.shader->dset_layout_infos[iDS];
      assert(src_dset_layout_info.bindings.size() == src_dset_layout_info.binding_infos.size());
      DescriptorSetLayoutInfo& dst_dset_layout_info = dset_layout_infos[iDS];
      for(size_t iSB = 0; iSB < src_dset_layout_info.bindings.size(); ++iSB) {
        const auto& src_binding = src_dset_layout_info.bindings[iSB];
        const auto& src_binding_info = src_dset_layout_info.binding_infos[iSB];
        assert(src_binding_info.stage_names.size() == 1);  // src bindings should only know about one stage
        bool found_binding = false;
        for(size_t iDB = 0; iDB < dst_dset_layout_info.bindings.size(); ++iDB) {
          auto& dst_binding = dst_dset_layout_info.bindings[iDB];
          auto& dst_binding_info = dst_dset_layout_info.binding_infos[iDB];
          if (src_binding.binding == dst_binding.binding) {
            // TODO(cort): these asserts may not be valid; it may be possible for types/counts to differ in compatible
            // ways.
            assert(src_binding.descriptorType == dst_binding.descriptorType);  // same binding used with different types in two stages
            assert(src_binding.descriptorCount == dst_binding.descriptorCount);  // same binding used with different array sizes in two stages
            // Found a match!
            assert(0 == (dst_binding.stageFlags & shader_entry.shader->stage));  // Of course we haven't processed this stage yet, right?
            dst_binding.stageFlags |= shader_entry.shader->stage;
            dst_binding_info.stage_names.push_back(src_binding_info.stage_names[0]);
            found_binding = true;
            break;
          }
        }
        if (!found_binding) {
          DescriptorSetLayoutBindingInfo new_binding_info = {};
          new_binding_info.stage_names.push_back(src_binding_info.stage_names[0]);
          dst_dset_layout_info.bindings.push_back(src_binding);
          dst_dset_layout_info.binding_infos.push_back(new_binding_info);
        }
      }
    }
  }

  // Create VkPipelineShaderStageCreateInfo for each stage
  shader_stage_cis.resize(shader_entries.size());
  entry_point_names.resize(shader_entries.size());
  for(size_t i=0; i<shader_entries.size(); ++i) {
    entry_point_names[i] = (shader_entries[i].entry_point != nullptr)
      ? std::string(shader_entries[i].entry_point)
      : "main";
    shader_stage_cis[i] = {};
    shader_stage_cis[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_cis[i].stage = shader_entries[i].shader->stage;
    shader_stage_cis[i].module = shader_entries[i].shader->handle;
    shader_stage_cis[i].pName = entry_point_names[i].data();
    shader_stage_cis[i].pSpecializationInfo = nullptr;  // TODO(cort): fill in at VkPipeline creation time
  }

  // Create the descriptor set layouts, now that their contents are known
  dset_layout_cis.resize(dset_layout_infos.size());
  dset_layouts.resize(dset_layout_infos.size());
  for(uint32_t iLayout = 0; iLayout < dset_layouts.size(); ++iLayout) {
    VkDescriptorSetLayoutCreateInfo& layout_ci = dset_layout_cis[iLayout];
    const DescriptorSetLayoutInfo& layout_info = dset_layout_infos[iLayout];
    assert(layout_info.bindings.size() == layout_info.binding_infos.size());
    layout_ci = {};
    layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.bindingCount = (uint32_t)layout_info.bindings.size();
    layout_ci.pBindings = layout_info.bindings.data();
    VkResult result = vkCreateDescriptorSetLayout(device_context.device(),
      &layout_ci, device_context.host_allocator(), &dset_layouts[iLayout]);
    if (result != VK_SUCCESS) {
      return result;
    }
  }
  // Create the pipeline layout
  VkPipelineLayoutCreateInfo pipeline_layout_ci = {};
  pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_ci.setLayoutCount = (uint32_t)dset_layouts.size();
  pipeline_layout_ci.pSetLayouts = dset_layouts.data();
  pipeline_layout_ci.pushConstantRangeCount = (uint32_t)push_constant_ranges.size();
  pipeline_layout_ci.pPushConstantRanges = push_constant_ranges.data();
  return vkCreatePipelineLayout(device_context.device(), &pipeline_layout_ci,
    device_context.host_allocator(), &pipeline_layout);
}
void ShaderPipeline::destroy(const DeviceContext& device_context) {
  for(auto dset_layout : dset_layouts) {
    vkDestroyDescriptorSetLayout(device_context.device(), dset_layout, device_context.host_allocator());
  }
  dset_layouts.clear();
  dset_layout_cis.clear();
  dset_layout_infos.clear();
  push_constant_ranges.clear();
  shader_stage_cis.clear();
  entry_point_names.clear();
  vkDestroyPipelineLayout(device_context.device(), pipeline_layout, device_context.host_allocator());
  pipeline_layout = VK_NULL_HANDLE;
}

//
// RenderPass
//
void RenderPass::finalize_subpasses(VkPipelineBindPoint bind_point, VkSubpassDescriptionFlags flags) {
  subpass_descs.resize(subpass_attachments.size());
  for(const auto& dep : subpass_dependencies) {
    // This is probably unnecessary; a mismatch would be caught by the validation layers at creation time.
    (void)dep;
    assert(dep.srcSubpass == VK_SUBPASS_EXTERNAL || dep.srcSubpass < subpass_descs.size());
    assert(dep.dstSubpass == VK_SUBPASS_EXTERNAL || dep.dstSubpass < subpass_descs.size());
  }
  subpass_multisample_state_cis.resize(subpass_attachments.size());
  for(size_t i=0; i<subpass_attachments.size(); ++i) {
    subpass_descs[i].flags = flags;
    subpass_descs[i].pipelineBindPoint = bind_point;
    subpass_descs[i].inputAttachmentCount = (uint32_t)subpass_attachments[i].input_refs.size();
    subpass_descs[i].pInputAttachments = subpass_attachments[i].input_refs.data();
    subpass_descs[i].colorAttachmentCount = (uint32_t)subpass_attachments[i].color_refs.size();
    subpass_descs[i].pColorAttachments = subpass_attachments[i].color_refs.data();
    assert(subpass_attachments[i].resolve_refs.empty() ||
      subpass_attachments[i].resolve_refs.size() == subpass_attachments[i].color_refs.size());
    subpass_descs[i].pResolveAttachments = subpass_attachments[i].resolve_refs.data();
    assert(subpass_attachments[i].depth_stencil_refs.size() <= 1);
    subpass_descs[i].pDepthStencilAttachment = subpass_attachments[i].depth_stencil_refs.empty()
      ? nullptr : &subpass_attachments[i].depth_stencil_refs[0];
    subpass_descs[i].preserveAttachmentCount = (uint32_t)subpass_attachments[i].preserve_indices.size();
    subpass_descs[i].pPreserveAttachments = subpass_attachments[i].preserve_indices.data();
    // All color and depth/stencil attachments used in a subpass must have the same sample count,
    // as specified by the graphics pipeline.
    subpass_multisample_state_cis[i].sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    if (subpass_descs[i].pDepthStencilAttachment) {
      subpass_multisample_state_cis[i].rasterizationSamples =
        attachment_descs[subpass_descs[i].pDepthStencilAttachment->attachment].samples;
    } else if (subpass_descs[i].colorAttachmentCount > 0) {
      subpass_multisample_state_cis[i].rasterizationSamples =
        attachment_descs[subpass_descs[i].pColorAttachments[0].attachment].samples;
    } else {
      // zero-attachment subpass. /shrug
      subpass_multisample_state_cis[i].rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    }
    // Reasonable defaults for the rest of these
    subpass_multisample_state_cis[i].sampleShadingEnable = VK_FALSE;
    subpass_multisample_state_cis[i].minSampleShading = 1.0f;
    subpass_multisample_state_cis[i].pSampleMask = nullptr;
    subpass_multisample_state_cis[i].alphaToCoverageEnable = VK_FALSE;
    subpass_multisample_state_cis[i].alphaToOneEnable = VK_FALSE;
  }
}

//
// ComputePipeline
//
ComputePipeline::ComputePipeline() : handle(VK_NULL_HANDLE), shader_pipeline(nullptr), ci{} {
}
VkResult ComputePipeline::create(const DeviceContext& device_context, const ShaderPipeline *shader_pipeline_in,
    bool defer_pipeline_creation) {
  this->shader_pipeline = shader_pipeline_in;
  assert(shader_pipeline->shader_stage_cis.size() == 1);
  assert(shader_pipeline->shader_stage_cis[0].stage == VK_SHADER_STAGE_COMPUTE_BIT);

  ci = {};
  ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  ci.flags = 0;  // TODO(cort): pass these in?
  ci.stage = shader_pipeline->shader_stage_cis[0];
  ci.layout = shader_pipeline->pipeline_layout;
  ci.basePipelineHandle = VK_NULL_HANDLE;
  ci.basePipelineIndex = 0;

  VkResult result = VK_SUCCESS;
  if (!defer_pipeline_creation) {
    result = vkCreateComputePipelines(device_context.device(), device_context.pipeline_cache(),
      1, &ci, device_context.host_allocator(), &handle);
  }
  return result;
}
void ComputePipeline::destroy(const DeviceContext& device_context) {
  if (handle != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_context.device(), handle, device_context.host_allocator());
    handle = VK_NULL_HANDLE;
  }
  shader_pipeline = nullptr;
}

//
// GraphicsPipeline
//
GraphicsPipeline::GraphicsPipeline() :
    handle(VK_NULL_HANDLE), mesh_format(nullptr), shader_pipeline(nullptr), render_pass(nullptr), subpass(0), dynamic_states{},
    ci{}, tessellation_state_ci{}, viewport_state_ci{}, viewports{}, scissor_rects{}, rasterization_state_ci{},
    depth_stencil_state_ci{}, color_blend_state_ci{}, color_blend_attachment_states{}, dynamic_state_ci{} {
}
VkResult GraphicsPipeline::create(const DeviceContext& device_context, const MeshFormat *mesh_format_in, const ShaderPipeline *shader_pipeline_in, const RenderPass *render_pass_in, uint32_t subpass_in,
    const std::vector<VkDynamicState> dynamic_states_in, const VkViewport viewport, const VkRect2D scissor_rect, bool defer_pipeline_creation) {
  this->mesh_format = mesh_format_in;
  this->shader_pipeline = shader_pipeline_in;
  this->render_pass = render_pass_in;
  this->subpass = subpass_in;
  this->dynamic_states = std::move(dynamic_states_in);

  tessellation_state_ci = {};
  tessellation_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

  viewports.resize(1);
  viewports[0] = viewport;
  scissor_rects.resize(1);
  scissor_rects[0] = scissor_rect;
  viewport_state_ci = {};
  viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state_ci.viewportCount = (uint32_t)viewports.size();
  viewport_state_ci.pViewports = viewports.data();
  viewport_state_ci.scissorCount = (uint32_t)scissor_rects.size();
  viewport_state_ci.pScissors = scissor_rects.data();

  rasterization_state_ci = {};
  rasterization_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization_state_ci.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization_state_ci.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterization_state_ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterization_state_ci.lineWidth = 1.0f;

  VkBool32 subpass_has_depth_attachment = (render_pass->subpass_descs[subpass].pDepthStencilAttachment != nullptr)
    ? VK_TRUE : VK_FALSE;
  depth_stencil_state_ci = {};
  depth_stencil_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_state_ci.depthTestEnable = subpass_has_depth_attachment;
  depth_stencil_state_ci.depthWriteEnable = subpass_has_depth_attachment;
  depth_stencil_state_ci.depthCompareOp = VK_COMPARE_OP_LESS;

  color_blend_attachment_states.resize(render_pass->subpass_descs[subpass].colorAttachmentCount);
  for(auto& attachment : color_blend_attachment_states) {
    attachment = {};
    attachment.blendEnable = VK_FALSE;
    attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  }
  color_blend_state_ci = {};
  color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend_state_ci.attachmentCount = (uint32_t)color_blend_attachment_states.size();
  color_blend_state_ci.pAttachments = color_blend_attachment_states.data();

  dynamic_state_ci = {};
  dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state_ci.dynamicStateCount = (uint32_t)dynamic_states.size();
  dynamic_state_ci.pDynamicStates = dynamic_states.data();

  ci = {};
  ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  ci.flags = 0;  // TODO(cort): pass these in?
  ci.stageCount = (uint32_t)shader_pipeline->shader_stage_cis.size();
  ci.pStages = shader_pipeline->shader_stage_cis.data();
  ci.pVertexInputState = &(mesh_format->vertex_input_state_ci);
  ci.pInputAssemblyState = &(mesh_format->input_assembly_state_ci);
  ci.pTessellationState = &tessellation_state_ci;
  ci.pViewportState = &viewport_state_ci;
  ci.pRasterizationState = &rasterization_state_ci;
  ci.pMultisampleState = &(render_pass->subpass_multisample_state_cis[subpass]);
  ci.pDepthStencilState = &depth_stencil_state_ci;
  ci.pColorBlendState = &color_blend_state_ci;
  ci.pDynamicState = &dynamic_state_ci;
  ci.layout = shader_pipeline->pipeline_layout;
  ci.renderPass = render_pass->handle;
  ci.subpass = subpass;
  ci.basePipelineHandle = VK_NULL_HANDLE;
  ci.basePipelineIndex = 0;

  VkResult result = VK_SUCCESS;
  if (!defer_pipeline_creation) {
    result = vkCreateGraphicsPipelines(device_context.device(), device_context.pipeline_cache(),
      1, &ci, device_context.host_allocator(), &handle);
  }
  return result;
}
void GraphicsPipeline::destroy(const DeviceContext& device_context) {
  if (handle != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_context.device(), handle, device_context.host_allocator());
    handle = VK_NULL_HANDLE;
  }
  mesh_format = nullptr;
  shader_pipeline = nullptr;
  render_pass = nullptr;
  subpass = 0;
}

//
// DescriptorPool
//
DescriptorPool::DescriptorPool() : handle(VK_NULL_HANDLE), ci{}, pool_sizes{} {
  ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  ci.flags = 0;  // overriden in finalize()
  ci.poolSizeCount = (uint32_t)pool_sizes.size();
  ci.pPoolSizes = pool_sizes.data();
  for(size_t i = 0; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i) {
    pool_sizes[i].type = (VkDescriptorType)i;
    pool_sizes[i].descriptorCount = 0;
  }
}
void DescriptorPool::add(uint32_t layout_count, const VkDescriptorSetLayoutCreateInfo* dset_layout_cis, const uint32_t* dsets_per_layout) {
  for(uint32_t iLayout = 0; iLayout < layout_count; ++iLayout) {
    const VkDescriptorSetLayoutCreateInfo &layout = dset_layout_cis[iLayout];
    uint32_t dset_count = (dsets_per_layout != nullptr) ? dsets_per_layout[iLayout] : 1;
    add(layout, dset_count);
  }
}
void DescriptorPool::add(const VkDescriptorSetLayoutCreateInfo& dset_layout, uint32_t dset_count) {
  for(uint32_t iBinding = 0; iBinding < dset_layout.bindingCount; ++iBinding) {
    const VkDescriptorSetLayoutBinding &binding = dset_layout.pBindings[iBinding];
    pool_sizes[binding.descriptorType].descriptorCount += binding.descriptorCount * dset_count;
  }
  ci.maxSets += dset_count;
}
VkResult DescriptorPool::finalize(const DeviceContext& device_context, VkDescriptorPoolCreateFlags flags) {
  ci.flags = flags;
  VkResult result = vkCreateDescriptorPool(device_context.device(), &ci, device_context.host_allocator(), &handle);
  return result;
}
void DescriptorPool::destroy(const DeviceContext& device_context) {
  if (handle != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device_context.device(), handle, device_context.host_allocator());
    handle = VK_NULL_HANDLE;
  }
}
VkResult DescriptorPool::allocate_sets(const DeviceContext& device_context, uint32_t dset_count,
    const VkDescriptorSetLayout *dset_layouts, VkDescriptorSet *out_dsets) const {
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = handle;
  alloc_info.descriptorSetCount = dset_count;
  alloc_info.pSetLayouts = dset_layouts;
  VkResult result = vkAllocateDescriptorSets(device_context.device(), &alloc_info, out_dsets);
  return result;
}
VkDescriptorSet DescriptorPool::allocate_set(const DeviceContext& device_context,
    VkDescriptorSetLayout dset_layout) const {
  VkDescriptorSet dset = VK_NULL_HANDLE;
  allocate_sets(device_context, 1, &dset_layout, &dset);
  return dset;
}
void DescriptorPool::free_sets(DeviceContext& device_context, uint32_t set_count, const VkDescriptorSet* sets) const {
  if (ci.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) {
    vkFreeDescriptorSets(device_context.device(), handle, set_count, sets);
  }
}
void DescriptorPool::free_set(DeviceContext& device_context, VkDescriptorSet set) const {
  if (ci.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) {
    vkFreeDescriptorSets(device_context.device(), handle, 1, &set);
  }
}

//
// DescriptorSetWriter
//
DescriptorSetWriter::DescriptorSetWriter(const VkDescriptorSetLayoutCreateInfo &layout_ci) :
    image_infos{}, buffer_infos{}, texel_buffer_views{}, binding_writes{layout_ci.bindingCount} {
  // First time through, we're building a total count of each class of descriptor.
  uint32_t image_count = 0, buffer_count = 0, texel_buffer_count = 0;
  for(uint32_t iBinding = 0; iBinding < layout_ci.bindingCount; ++iBinding) {
    const VkDescriptorSetLayoutBinding &binding = layout_ci.pBindings[iBinding];
    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
      texel_buffer_count += binding.descriptorCount;
    } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
      buffer_count += binding.descriptorCount;
    } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
      image_count += binding.descriptorCount;
    }
  }
  image_infos.resize(image_count);
  buffer_infos.resize(buffer_count);
  texel_buffer_views.resize(texel_buffer_count);

  // Now we can populate the Writes, and point each Write to the appropriate info(s).
  uint32_t next_image_info = 0, next_buffer_info = 0, next_texel_buffer_view = 0;
  for(uint32_t iBinding = 0; iBinding < layout_ci.bindingCount; ++iBinding) {
    const VkDescriptorSetLayoutBinding &binding = layout_ci.pBindings[iBinding];
    VkWriteDescriptorSet& write = binding_writes[iBinding];
    write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = VK_NULL_HANDLE;  // filled in just-in-time when writing
    write.dstBinding = binding.binding;
    write.dstArrayElement = 0;
    write.descriptorCount = binding.descriptorCount;
    write.descriptorType = binding.descriptorType;
    if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
      write.pTexelBufferView = &texel_buffer_views[next_texel_buffer_view];
      next_texel_buffer_view += binding.descriptorCount;
    } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
      write.pBufferInfo = &buffer_infos[next_buffer_info];
      next_buffer_info += binding.descriptorCount;
    } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
        binding.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
      write.pImageInfo = &image_infos[next_image_info];
      next_image_info += write.descriptorCount;
    }
  }
  assert(next_texel_buffer_view == texel_buffer_views.size());
  assert(next_buffer_info == buffer_infos.size());
  assert(next_image_info == image_infos.size());
}
void DescriptorSetWriter::bind_image(VkImageView view, VkImageLayout layout, VkSampler sampler, uint32_t binding, uint32_t array_element) {
  // TODO(cort): make this search more efficient!
  VkWriteDescriptorSet *write = nullptr;
  for(size_t iWrite=0; iWrite<binding_writes.size(); ++iWrite) {
    if (binding_writes[iWrite].dstBinding == binding) {
      write = &binding_writes[iWrite];
      break;
    }
  }
  assert(write != nullptr);
  assert(array_element < write->descriptorCount);
  assert(write->pImageInfo != nullptr); // is this binding really an image-type descriptor?
  auto *pImageInfo = const_cast<VkDescriptorImageInfo*>(write->pImageInfo);
  pImageInfo->imageView = view;
  pImageInfo->imageLayout = layout;
  pImageInfo->sampler = sampler;
}
void DescriptorSetWriter::bind_buffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t binding, uint32_t array_element) {
  // TODO(cort): make this search more efficient!
  VkWriteDescriptorSet *write = nullptr;
  for(size_t iWrite=0; iWrite<binding_writes.size(); ++iWrite) {
    if (binding_writes[iWrite].dstBinding == binding) {
      write = &binding_writes[iWrite];
      break;
    }
  }
  assert(write != nullptr);
  assert(array_element < write->descriptorCount);
  assert(write->pBufferInfo != nullptr); // is this binding really a buffer-type descriptor?
  auto *pBufferInfo = const_cast<VkDescriptorBufferInfo*>(write->pBufferInfo);
  pBufferInfo->buffer = buffer;
  pBufferInfo->offset = offset;
  pBufferInfo->range = range;
}
void DescriptorSetWriter::bind_texel_buffer(VkBufferView view, uint32_t binding, uint32_t array_element) {
  // TODO(cort): make this search more efficient!
  VkWriteDescriptorSet *write = nullptr;
  for(size_t iWrite=0; iWrite<binding_writes.size(); ++iWrite) {
    if (binding_writes[iWrite].dstBinding == binding) {
      write = &binding_writes[iWrite];
      break;
    }
  }
  assert(write != nullptr);
  assert(array_element < write->descriptorCount);
  assert(write->pTexelBufferView != nullptr); // is this binding really a texel buffer descriptor?
  auto *pTexelBufferView = const_cast<VkBufferView*>(write->pTexelBufferView);
  *pTexelBufferView = view;
}
void DescriptorSetWriter::write_all_to_dset(const DeviceContext& device_context, VkDescriptorSet dset) {
  for(auto& write : binding_writes) {
    write.dstSet = dset;
  }
  vkUpdateDescriptorSets(device_context.device(),
    (uint32_t)binding_writes.size(), binding_writes.data(), 0, nullptr);
}
void DescriptorSetWriter::write_one_to_dset(const DeviceContext& device_context, VkDescriptorSet dset,
    uint32_t binding, uint32_t array_element) {
  assert(binding < binding_writes.size());
  VkWriteDescriptorSet writeCopy = binding_writes[binding];
  assert(array_element < writeCopy.dstArrayElement);
  writeCopy.dstSet = dset;
  writeCopy.dstArrayElement = array_element;
  if (writeCopy.pImageInfo != nullptr) {
    writeCopy.pImageInfo += array_element;
  } else if (writeCopy.pBufferInfo != nullptr) {
    writeCopy.pBufferInfo += array_element;
  } else if (writeCopy.pTexelBufferView != nullptr) {
    writeCopy.pTexelBufferView += array_element;
  }
  vkUpdateDescriptorSets(device_context.device(), 1, &writeCopy, 0, nullptr);
}


//
// Application
//
Application::Application(const CreateInfo &ci) {
  if (ci.enable_graphics) {
    // Initialize GLFW
    glfwSetErrorCallback(my_glfw_error_callback);
    if( !glfwInit() ) {
      fprintf( stderr, "Failed to initialize GLFW\n" );
      return;
    }
    if (!glfwVulkanSupported()) {
      fprintf(stderr, "Vulkan is not available :(\n");
      return;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = std::shared_ptr<GLFWwindow>(
      glfwCreateWindow(kWindowWidthDefault, kWindowHeightDefault, ci.app_name.c_str(), NULL, NULL),
      [](GLFWwindow *w){ glfwDestroyWindow(w); });
    glfwSetInputMode(window_.get(), GLFW_STICKY_KEYS, 1);
    glfwSetInputMode(window_.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwPollEvents(); // dummy poll for first loop iteration

    input_state_.set_window(window_);
  }

  // Initialize Vulkan
  std::vector<const char*> required_instance_layer_names = {};
  if (ci.enable_validation) {
    required_instance_layer_names.push_back("VK_LAYER_LUNARG_standard_validation");
  }
  std::vector<const char*> optional_instance_layer_names = {};
  std::vector<const char*> enabled_instance_layer_names;
  CDSVK_CHECK(cdsvk::get_supported_instance_layers(
    required_instance_layer_names, optional_instance_layer_names,
    &instance_layers_, &enabled_instance_layer_names));

  std::vector<const char*> required_instance_extension_names = {
  };
  if (ci.enable_graphics) {
    required_instance_extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    required_instance_extension_names.push_back(PLATFORM_SURFACE_EXTENSION_NAME);
  }
  std::vector<const char*> optional_instance_extension_names = {};
  if (ci.enable_validation) {
    optional_instance_extension_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }
  std::vector<const char*> enabled_instance_extension_names;
  CDSVK_CHECK(cdsvk::get_supported_instance_extensions(instance_layers_,
    required_instance_extension_names, optional_instance_extension_names,
    &instance_extensions_, &enabled_instance_extension_names));

  VkApplicationInfo application_info = {};
  application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application_info.pApplicationName = ci.app_name.c_str();
  application_info.applicationVersion = 0x1000;
  application_info.pEngineName = "Spokk";
  application_info.engineVersion = 0x1001;
  application_info.apiVersion = VK_MAKE_VERSION(1,0,33);
  VkInstanceCreateInfo instance_ci = {};
  instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_ci.pApplicationInfo = &application_info;
  instance_ci.enabledLayerCount       = (uint32_t)enabled_instance_layer_names.size();
  instance_ci.ppEnabledLayerNames     = enabled_instance_layer_names.data();
  instance_ci.enabledExtensionCount   = (uint32_t)enabled_instance_extension_names.size();
  instance_ci.ppEnabledExtensionNames = enabled_instance_extension_names.data();
  CDSVK_CHECK(vkCreateInstance(&instance_ci, allocation_callbacks_, &instance_));

  if (is_instance_extension_enabled(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) {
    VkDebugReportCallbackCreateInfoEXT debug_report_callback_ci = {};
    debug_report_callback_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_callback_ci.flags = 0  // TODO(cort): pass these in!
      | VK_DEBUG_REPORT_ERROR_BIT_EXT
      | VK_DEBUG_REPORT_WARNING_BIT_EXT
      // | VK_DEBUG_REPORT_INFORMATION_BIT_EXT
      // | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
      ;
    debug_report_callback_ci.pfnCallback = my_debug_report_callback;
    debug_report_callback_ci.pUserData = nullptr;
    auto create_debug_report_func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_,
      "vkCreateDebugReportCallbackEXT");
    CDSVK_CHECK(create_debug_report_func(instance_, &debug_report_callback_ci, allocation_callbacks_, &debug_report_callback_));
    assert(debug_report_callback_ != VK_NULL_HANDLE);
  }

  if (ci.enable_graphics) {
    CDSVK_CHECK( glfwCreateWindowSurface(instance_, window_.get(), allocation_callbacks_, &surface_) );
  }

  std::vector<uint32_t> queue_family_indices;
  CDSVK_CHECK(find_physical_device(ci.queue_family_requests, instance_, surface_, &physical_device_, &queue_family_indices));
  std::vector<VkDeviceQueueCreateInfo> device_queue_cis = {};
  uint32_t total_queue_count = 0;
  for(uint32_t iQF=0; iQF<(uint32_t)ci.queue_family_requests.size(); ++iQF) {
    uint32_t queue_count = ci.queue_family_requests[iQF].queue_count;
    total_queue_count += queue_count;
  }
  std::vector<float> queue_priorities;
  queue_priorities.reserve(total_queue_count);
  for(uint32_t iQF=0; iQF<(uint32_t)ci.queue_family_requests.size(); ++iQF) {
    uint32_t queue_count = ci.queue_family_requests[iQF].queue_count;
    device_queue_cis.push_back({
      VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0,
      queue_family_indices[iQF], queue_count, queue_priorities.data()
    });
    queue_priorities.insert(queue_priorities.end(), queue_count, ci.queue_family_requests[iQF].priority);
  };
  assert(queue_priorities.size() == total_queue_count);

  std::vector<const char*> required_device_extension_names = {
  };
  if (ci.enable_graphics) {
    required_device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }
  const std::vector<const char*> optional_device_extension_names = {
#if !defined(NDEBUG) && defined(VK_EXT_debug_marker)
    VK_EXT_DEBUG_MARKER_EXTENSION_NAME, // will only be enabled if a layer supports it (currently, only RenderDoc's implicit layer)
#endif
  };
  std::vector<const char*> enabled_device_extension_names;
  CDSVK_CHECK(cdsvk::get_supported_device_extensions(physical_device_, instance_layers_,
    required_device_extension_names, optional_device_extension_names,
    &device_extensions_, &enabled_device_extension_names));

  vkGetPhysicalDeviceFeatures(physical_device_, &physical_device_features_);

  VkDeviceCreateInfo device_ci = {};
  device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_ci.queueCreateInfoCount = (uint32_t)device_queue_cis.size();
  device_ci.pQueueCreateInfos = device_queue_cis.data();
  device_ci.enabledExtensionCount = (uint32_t)enabled_device_extension_names.size();
  device_ci.ppEnabledExtensionNames = enabled_device_extension_names.data();
  device_ci.pEnabledFeatures = &physical_device_features_;
  CDSVK_CHECK(vkCreateDevice(physical_device_, &device_ci, allocation_callbacks_, &device_));

  uint32_t total_queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &total_queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> all_queue_family_properties(total_queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &total_queue_family_count, all_queue_family_properties.data());
  queue_contexts_.reserve(total_queue_count);
  for(uint32_t iQFR=0; iQFR<(uint32_t)ci.queue_family_requests.size(); ++iQFR) {
    const QueueFamilyRequest &qfr = ci.queue_family_requests[iQFR];
    const VkDeviceQueueCreateInfo& qci = device_queue_cis[iQFR];
    const VkQueueFamilyProperties& qfp = all_queue_family_properties[qci.queueFamilyIndex];
    DeviceQueueContext qc = {
      VK_NULL_HANDLE,
      qci.queueFamilyIndex,
      0.0f,
      qfp.queueFlags,
      qfp.timestampValidBits,
      qfp.minImageTransferGranularity,
      (qfr.support_present && ((qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)) ? surface_ : VK_NULL_HANDLE,
    };
    for(uint32_t iQ=0; iQ<total_queue_count; ++iQ) {
      vkGetDeviceQueue(device_, qci.queueFamilyIndex, iQ, &qc.queue);
      qc.priority = qci.pQueuePriorities[iQ];
      queue_contexts_.push_back(qc);
    }
  }
  assert(queue_contexts_.size() == total_queue_count);

  VkPipelineCacheCreateInfo pipeline_cache_ci = {};
  pipeline_cache_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  CDSVK_CHECK(vkCreatePipelineCache(device_, &pipeline_cache_ci, allocation_callbacks_, &pipeline_cache_));

  device_context_ = DeviceContext(device_, physical_device_, pipeline_cache_, queue_contexts_.data(),
    (uint32_t)queue_contexts_.size(), allocation_callbacks_, nullptr);

  // Create VkSwapchain
  if (ci.enable_graphics && surface_ != VK_NULL_HANDLE) {
    VkSurfaceCapabilitiesKHR surface_caps = {};
    CDSVK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &surface_caps));
    swapchain_extent_ = surface_caps.currentExtent;
    if ((int32_t)swapchain_extent_.width == -1) {
      assert( (int32_t)swapchain_extent_.height == -1 );
      swapchain_extent_.width =
        CDSVK__CLAMP(ci.window_width, surface_caps.minImageExtent.width, surface_caps.maxImageExtent.width);
      swapchain_extent_.height =
        CDSVK__CLAMP(ci.window_height, surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height);
    }

    uint32_t device_surface_format_count = 0;
    std::vector<VkSurfaceFormatKHR> device_surface_formats;
    VkResult result = VK_INCOMPLETE;
    do {
      result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &device_surface_format_count, nullptr);
      if (result == VK_SUCCESS && device_surface_format_count > 0) {
        device_surface_formats.resize(device_surface_format_count);
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &device_surface_format_count,
          device_surface_formats.data());
      }
    } while (result == VK_INCOMPLETE);
    if (device_surface_formats.size() == 1 && device_surface_formats[0].format == VK_FORMAT_UNDEFINED) {
      // No preferred format.
      swapchain_surface_format_.format = VK_FORMAT_B8G8R8A8_UNORM;
      swapchain_surface_format_.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    } else {
      assert(device_surface_formats.size() >= 1);
      swapchain_surface_format_ = device_surface_formats[0];
    }

    uint32_t device_present_mode_count = 0;
    std::vector<VkPresentModeKHR> device_present_modes;
    do {
      result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &device_present_mode_count, nullptr);
      if (result == VK_SUCCESS && device_present_mode_count > 0) {
        device_present_modes.resize(device_present_mode_count);
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &device_present_mode_count,
          device_present_modes.data());
      }
    } while (result == VK_INCOMPLETE);
    VkPresentModeKHR present_mode;
    if (!ci.enable_vsync) {
      present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    } else {
      bool found_mailbox_mode = false;
      for(auto mode : device_present_modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
          found_mailbox_mode = true;
          break;
        }
      }
      present_mode = found_mailbox_mode ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
    }

    uint32_t desired_swapchain_image_count = surface_caps.minImageCount+1;
    if (surface_caps.maxImageCount > 0 && desired_swapchain_image_count > surface_caps.maxImageCount) {
      desired_swapchain_image_count = surface_caps.maxImageCount;
    }

    VkSurfaceTransformFlagBitsKHR surface_transform = surface_caps.currentTransform;

    VkImageUsageFlags swapchain_image_usage = 0
      | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      ;
    assert( (surface_caps.supportedUsageFlags & swapchain_image_usage) == swapchain_image_usage );

    assert(surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkSwapchainKHR old_swapchain = VK_NULL_HANDLE;
    VkSwapchainCreateInfoKHR swapchain_ci = {};
    swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.surface = surface_;
    swapchain_ci.minImageCount = desired_swapchain_image_count;
    swapchain_ci.imageFormat = swapchain_surface_format_.format;
    swapchain_ci.imageColorSpace = swapchain_surface_format_.colorSpace;
    swapchain_ci.imageExtent.width = swapchain_extent_.width;
    swapchain_ci.imageExtent.height = swapchain_extent_.height;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageUsage = swapchain_image_usage;
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.preTransform = surface_transform;
    swapchain_ci.compositeAlpha = composite_alpha;
    swapchain_ci.presentMode = present_mode;
    swapchain_ci.clipped = VK_TRUE;
    swapchain_ci.oldSwapchain = old_swapchain;
    CDSVK_CHECK(vkCreateSwapchainKHR(device_, &swapchain_ci, allocation_callbacks_, &swapchain_));
    if (old_swapchain != VK_NULL_HANDLE) {
      assert(0); // TODO(cort): handle this at some point
    }

    uint32_t swapchain_image_count = 0;
    do {
      result = vkGetSwapchainImagesKHR(device_, swapchain_, &swapchain_image_count, nullptr);
      if (result == VK_SUCCESS && swapchain_image_count > 0) {
        swapchain_images_.resize(swapchain_image_count);
        result = vkGetSwapchainImagesKHR(device_, swapchain_, &swapchain_image_count, swapchain_images_.data());
      }
    } while (result == VK_INCOMPLETE);
    VkImageViewCreateInfo image_view_ci = {};
    image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_ci.image = VK_NULL_HANDLE; // filled in below
    image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_ci.format = swapchain_surface_format_.format;
    image_view_ci.components = {};
    image_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_ci.subresourceRange = {};
    image_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_ci.subresourceRange.baseMipLevel = 0;
    image_view_ci.subresourceRange.levelCount = 1;
    image_view_ci.subresourceRange.baseArrayLayer = 0;
    image_view_ci.subresourceRange.layerCount = 1;
    swapchain_image_views_.reserve(swapchain_images_.size());
    for(auto image : swapchain_images_) {
      image_view_ci.image = image;
      VkImageView view = VK_NULL_HANDLE;
      CDSVK_CHECK(vkCreateImageView(device_, &image_view_ci, allocation_callbacks_, &view));
      swapchain_image_views_.push_back(view);
    }
  }

  init_successful = true;
}
Application::~Application() {
  if (device_) {
    vkDeviceWaitIdle(device_);

    vkDestroyPipelineCache(device_, pipeline_cache_, allocation_callbacks_);

    for(auto& view : swapchain_image_views_) {
      vkDestroyImageView(device_, view, allocation_callbacks_);
      view = VK_NULL_HANDLE;
    }
    if (swapchain_ != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(device_, swapchain_, allocation_callbacks_);
      swapchain_ = VK_NULL_HANDLE;
    }
  }
  if (surface_ != VK_NULL_HANDLE) {
    window_.reset();
    glfwTerminate();
  }
  vkDestroyDevice(device_, allocation_callbacks_);
  device_ = VK_NULL_HANDLE;
  if (debug_report_callback_ != VK_NULL_HANDLE) {
    auto destroy_debug_report_func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugReportCallbackEXT");
    destroy_debug_report_func(instance_, debug_report_callback_, allocation_callbacks_);
  }
  if (surface_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, allocation_callbacks_);
    surface_ = VK_NULL_HANDLE;
  }
  vkDestroyInstance(instance_, allocation_callbacks_);
  instance_ = VK_NULL_HANDLE;
}

int Application::run() {
  if (!init_successful) {
    return -1;
  }

  const uint64_t clock_start = zomboClockTicks();
  uint64_t ticks_prev = clock_start;
  frame_index_ = 0;
  vframe_index_ = 0;
  while(!force_exit_ && !glfwWindowShouldClose(window_.get())) {
    uint64_t ticks_now = zomboClockTicks();
    const double dt = (float)zomboTicksToSeconds(ticks_now - ticks_prev);
    ticks_prev = ticks_now;

    update(dt);
    if (force_exit_) {
      break;
    }
    render();
    if (force_exit_) {
      break;
    }

    glfwPollEvents();
    frame_index_ += 1;
    vframe_index_ += 1;
    if (vframe_index_ == VFRAME_COUNT) {
      vframe_index_ = 0;
    }
  }
  return 0;
}

void Application::update(double /*dt*/) {
  input_state_.Update();
}
void Application::render() {
}

bool Application::is_instance_layer_enabled(const std::string& layer_name) const {
  for(const auto &layer : instance_layers_) {
    if (layer_name == layer.layerName) {
      return true;
    }
  }
  return false;
}
bool Application::is_instance_extension_enabled(const std::string& extension_name) const {
  for(const auto &extension : instance_extensions_) {
    if (extension_name == extension.extensionName) {
      return true;
    }
  }
  return false;
}
bool Application::is_device_extension_enabled(const std::string& extension_name) const {
  for(const auto &extension : device_extensions_) {
    if (extension_name == extension.extensionName) {
      return true;
    }
  }
  return false;
}

VkResult Application::find_physical_device(const std::vector<QueueFamilyRequest>& qf_reqs, VkInstance instance,
    VkSurfaceKHR present_surface, VkPhysicalDevice *out_physical_device, std::vector<uint32_t>* out_queue_families) {
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
      auto &req = qf_reqs[iReq];
      bool found_qf = false;
      // First search for an *exact* match for the requested queue flags, so that users who request e.g. a dedicated
      // transfer queue are more likely to get one.
      for(uint32_t iQF=0; (size_t)iQF < all_queue_family_properties.size(); ++iQF) {
        if (all_queue_family_properties[iQF].queueCount < req.queue_count) {
          continue;  // insufficient queue count
        } else if (all_queue_family_properties[iQF].queueFlags != req.flags) {
          continue;  // family doesn't the exact requested operations
        }
        VkBool32 supports_present = VK_FALSE;
        if (req.flags & VK_QUEUE_GRAPHICS_BIT && present_surface != VK_NULL_HANDLE) {
          result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, iQF, present_surface, &supports_present);
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
        // Search again; this time, accept any queue family that supports the requested flags, even if it supports
        // additional operations.
        for(uint32_t iQF=0; (size_t)iQF < all_queue_family_properties.size(); ++iQF) {
          if (all_queue_family_properties[iQF].queueCount < req.queue_count) {
            continue;  // insufficient queue count
          } else if ((all_queue_family_properties[iQF].queueFlags & req.flags) != req.flags) {
            continue;  // family doesn't support all required operations
          }
          VkBool32 supports_present = VK_FALSE;
          if (req.flags & VK_QUEUE_GRAPHICS_BIT && present_surface != VK_NULL_HANDLE) {
            result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, iQF, present_surface, &supports_present);
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
