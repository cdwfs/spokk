#include "spokk_platform.h"

// Must happen before any vulkan.h include, in order to get the platform-specific extensions included.
#if defined(ZOMBO_PLATFORM_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR 1
#define SPOKK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(ZOMBO_PLATFORM_POSIX)
#define VK_USE_PLATFORM_XCB_KHR 1
#define SPOKK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
#elif defined(ZOMBO_PLATFORM_ANDROID)
#define VK_USE_PLATFORM_ANDROID_KHR 1
#define SPOKK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#else
#error Unsupported platform
#define SPOKK_PLATFORM_SURFACE_EXTENSION_NAME "Unsupported platform",
#endif

#include "spokk_application.h"
#include "spokk_debug.h"
#include "spokk_image.h"
#include "spokk_utilities.h"
using namespace spokk;

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#define IM_ASSERT(_EXPR) ZOMBO_ASSERT(_EXPR, "IMGUI assertion failed: %s", #_EXPR)
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_STB_NAMESPACE ImGuiStb
#define IM_VEC2_CLASS_EXTRA    \
  ImVec2(const glm::vec2 &f) { \
    x = f.x;                   \
    y = f.y;                   \
  }                            \
  operator glm::vec2() const { return glm::vec2(x, y); }

#define IM_VEC4_CLASS_EXTRA    \
  ImVec4(const glm::vec4 &f) { \
    x = f.x;                   \
    y = f.y;                   \
    z = f.z;                   \
    w = f.w;                   \
  }                            \
  operator glm::vec4() const { return glm::vec4(x, y, z, w); }
#define IMGUI_VK_QUEUED_FRAMES spokk::PFRAME_COUNT;
// clang-format off
#include <imgui/imgui.h>
#include "spokk_imgui_impl_glfw_vulkan.h"
// clang-format on

#include <cassert>
#include <cstdio>
#include <cstring>

#define SPOKK__CLAMP(x, xmin, xmax) (((x) < (xmin)) ? (xmin) : (((x) > (xmax)) ? (xmax) : (x)))

namespace {

enum TimestampId {
  TIMESTAMP_ID_BEGIN_PRIMARY = 0,
  TIMESTAMP_ID_END_PRIMARY = 1,

  TIMESTAMP_ID_COUNT
};

void MyGlfwErrorCallback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL MyDebugReportCallback(VkFlags msgFlags, VkDebugReportObjectTypeEXT /*objType*/,
    uint64_t /*srcObject*/, size_t /*location*/, int32_t msgCode, const char *pLayerPrefix, const char *pMsg,
    void * /*pUserData*/) {
  const char *message_type = "????";
  if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
    message_type = "ERROR";
  } else if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
    message_type = "WARNING";
  } else if (msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
    message_type = "INFO";
  } else if (msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
    message_type = "PERFORMANCE_WARNING";
  } else if (msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
    message_type = "DEBUG";
  } else {
    ZOMBO_ERROR_RETURN(VK_FALSE, "Unrecognized msgFlags: %d", msgFlags);
  }

  int nchars = zomboSnprintf(nullptr, 0, "[%s %s 0x%08X]: %s", message_type, pLayerPrefix, msgCode, pMsg);
  char *output = (char *)malloc(nchars + 1);
  zomboSnprintf(output, nchars + 1, "[%s %s 0x%08X]: %s", message_type, pLayerPrefix, msgCode, pMsg);
#if 0  //_WIN32
  MessageBoxA(NULL, output, "Alert", MB_OK);
#else
  fprintf(stderr, "%s\n", output);
  fflush(stdout);
#endif
  free(output);
  return (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ? VK_TRUE : VK_FALSE;
}

VkResult FindPhysicalDevice(const std::vector<Application::QueueFamilyRequest> &qf_reqs, VkInstance instance,
    VkSurfaceKHR present_surface, VkPhysicalDevice *out_physical_device, std::vector<uint32_t> *out_queue_families) {
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
  for (auto physical_device : all_physical_devices) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> all_queue_family_properties(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, all_queue_family_properties.data());
    bool pd_meets_requirements = true;
    for (uint32_t iReq = 0; iReq < qf_reqs.size(); ++iReq) {
      auto &req = qf_reqs[iReq];
      bool found_qf = false;
      // First search for an *exact* match for the requested queue flags, so that users who request e.g. a dedicated
      // transfer queue are more likely to get one.
      for (uint32_t iQF = 0; (size_t)iQF < all_queue_family_properties.size(); ++iQF) {
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
        for (uint32_t iQF = 0; (size_t)iQF < all_queue_family_properties.size(); ++iQF) {
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

}  // namespace

//
// Application
//
Application::Application(const CreateInfo &ci) {
  if (ci.enable_graphics) {
    // Initialize GLFW
    glfwSetErrorCallback(MyGlfwErrorCallback);
    if (!glfwInit()) {
      fprintf(stderr, "Failed to initialize GLFW\n");
      return;
    }
    if (!glfwVulkanSupported()) {
      fprintf(stderr, "Vulkan is not available :(\n");
      return;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = std::shared_ptr<GLFWwindow>(
        glfwCreateWindow(ci.window_width, ci.window_height, ci.app_name.c_str(), NULL, NULL),
        [](GLFWwindow *w) { glfwDestroyWindow(w); });
    glfwSetInputMode(window_.get(), GLFW_STICKY_KEYS, 1);
    glfwPollEvents();  // dummy poll for first loop iteration

    input_state_.SetWindow(window_);
  }

  // Initialize Vulkan
  std::vector<const char *> required_instance_layer_names = {};
  std::vector<const char *> optional_instance_layer_names = {
#if defined(_DEBUG)
    "VK_LAYER_LUNARG_monitor",
#endif
  };
  if (ci.debug_report_flags != 0) {
#if defined(_DEBUG)
    optional_instance_layer_names.push_back("VK_LAYER_LUNARG_standard_validation");
#endif
  }
  std::vector<const char *> enabled_instance_layer_names = {};
  SPOKK_VK_CHECK(GetSupportedInstanceLayers(
      required_instance_layer_names, optional_instance_layer_names, &instance_layers_, &enabled_instance_layer_names));

  std::vector<const char *> required_instance_extension_names = {};
  if (ci.enable_graphics) {
    required_instance_extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    required_instance_extension_names.push_back(SPOKK_PLATFORM_SURFACE_EXTENSION_NAME);
  }
  std::vector<const char *> optional_instance_extension_names = {};
  if (ci.debug_report_flags != 0) {
    optional_instance_extension_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }
  std::vector<const char *> enabled_instance_extension_names;
  SPOKK_VK_CHECK(GetSupportedInstanceExtensions(instance_layers_, required_instance_extension_names,
      optional_instance_extension_names, &instance_extensions_, &enabled_instance_extension_names));

  VkApplicationInfo application_info = {};
  application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application_info.pApplicationName = ci.app_name.c_str();
  application_info.applicationVersion = 0x1000;
  application_info.pEngineName = "Spokk";
  application_info.engineVersion = 0x1001;
  application_info.apiVersion = VK_MAKE_VERSION(1, 0, 37);
  VkInstanceCreateInfo instance_ci = {};
  instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_ci.pApplicationInfo = &application_info;
  instance_ci.enabledLayerCount = (uint32_t)enabled_instance_layer_names.size();
  instance_ci.ppEnabledLayerNames = enabled_instance_layer_names.data();
  instance_ci.enabledExtensionCount = (uint32_t)enabled_instance_extension_names.size();
  instance_ci.ppEnabledExtensionNames = enabled_instance_extension_names.data();
  SPOKK_VK_CHECK(vkCreateInstance(&instance_ci, host_allocator_, &instance_));

  if (IsInstanceExtensionEnabled(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) {
    VkDebugReportCallbackCreateInfoEXT debug_report_callback_ci = {};
    debug_report_callback_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_callback_ci.flags = ci.debug_report_flags;
    debug_report_callback_ci.pfnCallback = MyDebugReportCallback;
    debug_report_callback_ci.pUserData = nullptr;
    auto create_debug_report_func =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugReportCallbackEXT");
    SPOKK_VK_CHECK(
        create_debug_report_func(instance_, &debug_report_callback_ci, host_allocator_, &debug_report_callback_));
  }

  if (ci.enable_graphics) {
    SPOKK_VK_CHECK(glfwCreateWindowSurface(instance_, window_.get(), host_allocator_, &surface_));
  }

  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  std::vector<uint32_t> queue_family_indices;
  SPOKK_VK_CHECK(
      FindPhysicalDevice(ci.queue_family_requests, instance_, surface_, &physical_device, &queue_family_indices));
  std::vector<VkDeviceQueueCreateInfo> device_queue_cis = {};
  uint32_t total_queue_count = 0;
  for (uint32_t iQF = 0; iQF < (uint32_t)ci.queue_family_requests.size(); ++iQF) {
    uint32_t queue_count = ci.queue_family_requests[iQF].queue_count;
    total_queue_count += queue_count;
  }
  std::vector<float> queue_priorities;
  queue_priorities.reserve(total_queue_count);
  for (uint32_t iQF = 0; iQF < (uint32_t)ci.queue_family_requests.size(); ++iQF) {
    uint32_t queue_count = ci.queue_family_requests[iQF].queue_count;
    device_queue_cis.push_back({VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, queue_family_indices[iQF],
        queue_count, queue_priorities.data()});
    queue_priorities.insert(queue_priorities.end(), queue_count, ci.queue_family_requests[iQF].priority);
  };
  ZOMBO_ASSERT(queue_priorities.size() == total_queue_count, "queue count mismatch");

  std::vector<const char *> required_device_extension_names = {};
  if (ci.enable_graphics) {
    required_device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    required_device_extension_names.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
  }
  const std::vector<const char *> optional_device_extension_names = {};
  std::vector<const char *> enabled_device_extension_names;
  SPOKK_VK_CHECK(GetSupportedDeviceExtensions(physical_device, instance_layers_, required_device_extension_names,
      optional_device_extension_names, &device_extensions_, &enabled_device_extension_names));

  VkPhysicalDeviceFeatures supported_device_features = {};
  vkGetPhysicalDeviceFeatures(physical_device, &supported_device_features);
  VkBool32 all_required_features_enabled = VK_TRUE;
  VkPhysicalDeviceFeatures enabled_device_features = {};
  if (ci.pfn_set_device_features != nullptr) {
    all_required_features_enabled = ci.pfn_set_device_features(supported_device_features, &enabled_device_features);
  }
  if (!all_required_features_enabled) {
    ZOMBO_ERROR("Device creation failed: not all required features are supported.");
    return;
  }

  VkDeviceCreateInfo device_ci = {};
  device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_ci.queueCreateInfoCount = (uint32_t)device_queue_cis.size();
  device_ci.pQueueCreateInfos = device_queue_cis.data();
  device_ci.enabledExtensionCount = (uint32_t)enabled_device_extension_names.size();
  device_ci.ppEnabledExtensionNames = enabled_device_extension_names.data();
  device_ci.pEnabledFeatures = &enabled_device_features;
  VkDevice logical_device = VK_NULL_HANDLE;
  SPOKK_VK_CHECK(vkCreateDevice(physical_device, &device_ci, host_allocator_, &logical_device));

  uint32_t total_queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &total_queue_family_count, nullptr);
  std::vector<VkQueueFamilyProperties> all_queue_family_properties(total_queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(
      physical_device, &total_queue_family_count, all_queue_family_properties.data());
  std::vector<DeviceQueue> queues;
  queues.reserve(total_queue_count);
  for (uint32_t iQFR = 0; iQFR < (uint32_t)ci.queue_family_requests.size(); ++iQFR) {
    const QueueFamilyRequest &qfr = ci.queue_family_requests[iQFR];
    const VkDeviceQueueCreateInfo &qci = device_queue_cis[iQFR];
    const VkQueueFamilyProperties &qfp = all_queue_family_properties[qci.queueFamilyIndex];
    DeviceQueue qc = {
        VK_NULL_HANDLE,
        qci.queueFamilyIndex,
        0.0f,
        qfp.queueFlags,
        qfp.timestampValidBits,
        qfp.minImageTransferGranularity,
        (qfr.support_present && ((qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)) ? surface_ : VK_NULL_HANDLE,
    };
    for (uint32_t iQ = 0; iQ < total_queue_count; ++iQ) {
      vkGetDeviceQueue(logical_device, qci.queueFamilyIndex, iQ, &qc.handle);
      qc.priority = qci.pQueuePriorities[iQ];
      queues.push_back(qc);
    }
  }
  ZOMBO_ASSERT(queues.size() == total_queue_count, "queue count mismatch");

  // TODO(cort): hmmm, maybe persist this across runs some day...
  VkPipelineCacheCreateInfo pipeline_cache_ci = {};
  pipeline_cache_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
  SPOKK_VK_CHECK(vkCreatePipelineCache(logical_device, &pipeline_cache_ci, host_allocator_, &pipeline_cache));

  // Populate the Device object, which from now on "owns" all of these Vulkan handles.
  device_.Create(logical_device, physical_device, pipeline_cache, queues.data(), (uint32_t)queues.size(),
      enabled_device_features, host_allocator_, device_allocator_);

  graphics_and_present_queue_ = device_.FindQueue(VK_QUEUE_GRAPHICS_BIT, surface_);

  if (ci.enable_graphics) {
    VkExtent2D default_extent = {ci.window_width, ci.window_height};
    CreateSwapchain(default_extent);

    // Create imgui render pass. This is an optional pass on the final swapchain image
    // to render the UI as an overlay. It's less performant than rendering the UI in one of
    // the app's main render pass, but less intrusive.
    imgui_render_pass_.attachment_descs.resize(1);
    imgui_render_pass_.attachment_descs[0].format = swapchain_surface_format_.format;
    imgui_render_pass_.attachment_descs[0].samples = VK_SAMPLE_COUNT_1_BIT;
    imgui_render_pass_.attachment_descs[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    imgui_render_pass_.attachment_descs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    imgui_render_pass_.attachment_descs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    imgui_render_pass_.attachment_descs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    imgui_render_pass_.attachment_descs[0].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imgui_render_pass_.attachment_descs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imgui_render_pass_.subpass_attachments.resize(1);
    imgui_render_pass_.subpass_attachments[0].color_refs.push_back({0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    imgui_render_pass_.subpass_dependencies.resize(1);
    imgui_render_pass_.subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    imgui_render_pass_.subpass_dependencies[0].dstSubpass = 0;
    imgui_render_pass_.subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    imgui_render_pass_.subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    imgui_render_pass_.subpass_dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imgui_render_pass_.subpass_dependencies[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    imgui_render_pass_.subpass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    SPOKK_VK_CHECK(imgui_render_pass_.Finalize(device_));
    // Create framebuffers for imgui render pass
    VkFramebufferCreateInfo framebuffer_ci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer_ci.renderPass = imgui_render_pass_.handle;
    framebuffer_ci.attachmentCount = 1;
    framebuffer_ci.width = swapchain_extent_.width;
    framebuffer_ci.height = swapchain_extent_.height;
    framebuffer_ci.layers = 1;
    imgui_framebuffers_.resize(swapchain_image_views_.size());
    for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
      framebuffer_ci.pAttachments = &swapchain_image_views_[i];
      SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &imgui_framebuffers_[i]));
    }

    InitImgui(imgui_render_pass_.handle);
    ShowImgui(false);
  }

  // Allocate command buffers
  VkCommandPoolCreateInfo cpool_ci = {};
  cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cpool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  cpool_ci.queueFamilyIndex = graphics_and_present_queue_->family;
  SPOKK_VK_CHECK(vkCreateCommandPool(device_, &cpool_ci, host_allocator_, &primary_cpool_));
  VkCommandBufferAllocateInfo cb_allocate_info = {};
  cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cb_allocate_info.commandPool = primary_cpool_;
  cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cb_allocate_info.commandBufferCount = (uint32_t)primary_command_buffers_.size();
  SPOKK_VK_CHECK(vkAllocateCommandBuffers(device_, &cb_allocate_info, primary_command_buffers_.data()));

  // Create the semaphores used to synchronize access to swapchain images
  VkSemaphoreCreateInfo semaphore_ci = {};
  semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  SPOKK_VK_CHECK(vkCreateSemaphore(device_, &semaphore_ci, host_allocator_, &image_acquire_semaphore_));
  SPOKK_VK_CHECK(vkCreateSemaphore(device_, &semaphore_ci, host_allocator_, &submit_complete_semaphore_));

  // Create the fences used to wait for each swapchain image's command buffer to be submitted.
  // This prevents re-writing the command buffer contents before it's been submitted and processed.
  VkFenceCreateInfo fence_ci = {};
  fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (auto &fence : submit_complete_fences_) {
    SPOKK_VK_CHECK(vkCreateFence(device_, &fence_ci, host_allocator_, &fence));
  }

  if (ci.enable_graphics) {
    // Create timestamp query pool
    TimestampQueryPool::CreateInfo tspool_ci = {};
    tspool_ci.swapchain_image_count = (uint32_t)swapchain_images_.size();
    tspool_ci.timestamp_id_count = TIMESTAMP_ID_COUNT;
    tspool_ci.queue_family_index = graphics_and_present_queue_->family;
    SPOKK_VK_CHECK(timestamp_query_pool_.Create(device_, tspool_ci));
  }

  init_successful_ = true;
}
Application::~Application() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);

    timestamp_query_pool_.Destroy(device_);

    DestroyImgui();
    for (auto fb : imgui_framebuffers_) {
      vkDestroyFramebuffer(device_, fb, host_allocator_);
    }
    imgui_render_pass_.Destroy(device_);

    vkDestroySemaphore(device_, image_acquire_semaphore_, host_allocator_);
    vkDestroySemaphore(device_, submit_complete_semaphore_, host_allocator_);
    for (auto fence : submit_complete_fences_) {
      vkDestroyFence(device_, fence, host_allocator_);
    }
    vkDestroyCommandPool(device_, primary_cpool_, host_allocator_);

    if (swapchain_ != VK_NULL_HANDLE) {
      for (auto &view : swapchain_image_views_) {
        vkDestroyImageView(device_, view, host_allocator_);
        view = VK_NULL_HANDLE;
      }
      vkDestroySwapchainKHR(device_, swapchain_, host_allocator_);
      swapchain_ = VK_NULL_HANDLE;
    }
  }
  if (surface_ != VK_NULL_HANDLE) {
    window_.reset();
    glfwTerminate();
  }
  device_.Destroy();
  if (debug_report_callback_ != VK_NULL_HANDLE) {
    auto destroy_debug_report_func =
        (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugReportCallbackEXT");
    destroy_debug_report_func(instance_, debug_report_callback_, host_allocator_);
  }
  if (surface_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, host_allocator_);
    surface_ = VK_NULL_HANDLE;
  }
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, host_allocator_);
    instance_ = VK_NULL_HANDLE;
  }
}

int Application::Run() {
  if (!init_successful_) {
    return -1;
  }

  const uint64_t clock_start = zomboClockTicks();
  uint64_t ticks_prev = clock_start;
  frame_index_ = 0;
  pframe_index_ = 0;
  while (!force_exit_ && !glfwWindowShouldClose(window_.get())) {
    // Check for window resize, and recreate the swapchain. Need to wait for an idle device first.
    // Provide a hook for application subclasses to respond to resize events
    {
      int window_width = -1, window_height = -1;
      glfwGetWindowSize(window_.get(), &window_width, &window_height);
      if (window_width != (int)swapchain_extent_.width || window_height != (int)swapchain_extent_.height) {
        VkExtent2D window_extent = {(uint32_t)window_width, (uint32_t)window_height};
        HandleWindowResize(window_extent);
      }
    }

    uint64_t ticks_now = zomboClockTicks();
    const double dt = zomboTicksToSeconds(ticks_now - ticks_prev);
    ticks_prev = ticks_now;
    const uint32_t cpu_stats_frame_index = uint32_t(frame_index_ % STATS_FRAME_COUNT);
    float total_frame_time = 1000.0f * (float)dt;
    average_total_frame_time_ms_ +=
        (total_frame_time - total_frame_times_ms_[cpu_stats_frame_index]) / (float)STATS_FRAME_COUNT;
    total_frame_times_ms_[cpu_stats_frame_index] = total_frame_time;
    // Some counters don't get updated until after we've already drawn the stats display,
    // so we zero them out initially. They'll get correct data for the long-term plots.
    submit_wait_times_ms_[cpu_stats_frame_index] = 0.0f;
    present_wait_times_ms_[cpu_stats_frame_index] = 0.0f;

    if (is_imgui_enabled_) {
      ImGui_ImplGlfwVulkan_NewFrame();

#if 0  // IMGUI demo window
      if (is_imgui_visible_) {
        ImGui::ShowTestWindow();
      }
#endif
    }

    ImGui::Begin("Present");
    const char *present_mode_combo_names = "IMMEDIATE\0MAILBOX\0FIFO\0FIFO_RELAXED\0\0";
    VkPresentModeKHR new_present_mode = swapchain_present_mode_;
    ImGui::Combo("Present Mode", (int *)&new_present_mode, present_mode_combo_names, 4);
    if (new_present_mode != swapchain_present_mode_) {
      swapchain_present_mode_ = new_present_mode;
      HandleWindowResize(swapchain_extent_);
    }
    ImGui::End();

    input_state_.Update();
    if (input_state_.IsPressed(InputState::DIGITAL_MENU)) {
      ShowImgui(!is_imgui_visible_);
    }
    Update(dt);
    if (force_exit_) {
      break;
    }

    // Wait for the command buffer previously used to generate this swapchain image to be submitted.
    uint64_t fence_wait_start_ticks = zomboClockTicks();
    vkWaitForFences(device_, 1, &submit_complete_fences_[pframe_index_], VK_TRUE, UINT64_MAX);
    fence_wait_times_ms_[cpu_stats_frame_index] =
        1000.0f * (float)zomboTicksToSeconds(zomboClockTicks() - fence_wait_start_ticks);
    vkResetFences(device_, 1, &submit_complete_fences_[pframe_index_]);

    // The host can now safely reset and rebuild this command buffer, even if the GPU hasn't finished presenting the
    // resulting frame yet.
    VkCommandBuffer cb = primary_command_buffers_[pframe_index_];

    // Retrieve the index of the next available swapchain index
    VkFence image_acquire_fence =
        VK_NULL_HANDLE;  // currently unused, but if you want the CPU to wait for an image to be acquired...
    uint32_t swapchain_image_index = 0;
    uint64_t acquire_wait_start_ticks = zomboClockTicks();
    VkResult acquire_result = vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX, image_acquire_semaphore_, image_acquire_fence, &swapchain_image_index);
    acquire_wait_times_ms_[cpu_stats_frame_index] =
        1000.0f * (float)zomboTicksToSeconds(zomboClockTicks() - acquire_wait_start_ticks);
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR || acquire_result == VK_SUBOPTIMAL_KHR) {
      // I've never actually seen these error codes returned, but if they were this is probably how they should be
      // handled.
      int window_width = -1, window_height = -1;
      glfwGetWindowSize(window_.get(), &window_width, &window_height);
      VkExtent2D window_extent = {(uint32_t)window_width, (uint32_t)window_height};
      HandleWindowResize(window_extent);
    } else {
      SPOKK_VK_CHECK(acquire_result);
    }

    // Retrieve timestamp values from the frame that just completed rendering.
    // Note that these times correspond to a frame some distance in the past (in contrast to
    // the CPU times, which are for the frame currently being prepared). Care must be taken:
    // 1) ...to ensure we retrieve the correct set of timestamps. To be safe, we use swapchain
    //    image index rather than pframe index to access the appropriate range of queries.
    // 2) ...to match up a set of GPU times to the corresponding CPU times.
    // 3) ...to not retrieve queries the first time a given swapchain image is being rendered
    //    (or, at least, to ignore the resulting error in that case)
    std::array<double, TIMESTAMP_ID_COUNT> timestamps_seconds;
    std::array<bool, TIMESTAMP_ID_COUNT> timestamps_validity;
    int64_t timestamps_frame_index = -1;
    SPOKK_VK_CHECK(timestamp_query_pool_.GetResults(device_, swapchain_image_index, TIMESTAMP_ID_COUNT,
        timestamps_seconds.data(), timestamps_validity.data(), &timestamps_frame_index));
    if (timestamps_validity[TIMESTAMP_ID_BEGIN_PRIMARY] && timestamps_validity[TIMESTAMP_ID_END_PRIMARY]) {
      float primary_time = 1000.0f *
          (float)(timestamps_seconds[TIMESTAMP_ID_END_PRIMARY] - timestamps_seconds[TIMESTAMP_ID_BEGIN_PRIMARY]);
      const uint32_t gpu_stats_frame_index = (uint32_t)(timestamps_frame_index % STATS_FRAME_COUNT);
      average_total_gpu_primary_time_ms_ +=
          (primary_time - total_gpu_primary_times_ms_[gpu_stats_frame_index]) / (float)STATS_FRAME_COUNT;
      total_gpu_primary_times_ms_[gpu_stats_frame_index] = primary_time;
    }
    // Zero out the GPU times for the CPU's current frame, to indicate that we don't know them yet.
    // total_gpu_primary_times_ms_[cpu_stats_frame_index] = 0.0f;

    ImGui::Begin("Stats");
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.6f, 0.6f, 0, 1));
    ImGui::PushStyleColor(ImGuiCol_PlotLinesHovered, ImVec4(1, 0, 1, 1));
    char average_total_frame_time_str[32];
    sprintf(average_total_frame_time_str, "avg: %.3fms", average_total_frame_time_ms_);
    ImGui::PlotLines("Frame Time (ms)", total_frame_times_ms_.data(), (int)total_frame_times_ms_.size(),
        cpu_stats_frame_index, average_total_frame_time_str, 0.0f, FLT_MAX, ImVec2((float)STATS_FRAME_COUNT, 60));
    ImGui::PlotLines("Fence (ms)", fence_wait_times_ms_.data(), (int)fence_wait_times_ms_.size(), cpu_stats_frame_index,
        nullptr, 0.0f, FLT_MAX, ImVec2((float)STATS_FRAME_COUNT, 60));
    ImGui::PlotLines("Acquire (ms)", acquire_wait_times_ms_.data(), (int)acquire_wait_times_ms_.size(),
        cpu_stats_frame_index, "min: 0.0\nmax: 0.0", 0.0f, FLT_MAX, ImVec2((float)STATS_FRAME_COUNT, 60));
    ImGui::PlotLines("Submit (ms)", submit_wait_times_ms_.data(), (int)submit_wait_times_ms_.size(),
        cpu_stats_frame_index, nullptr, 0.0f, FLT_MAX, ImVec2((float)STATS_FRAME_COUNT, 60));
    ImGui::PlotLines("Present (ms)", present_wait_times_ms_.data(), (int)present_wait_times_ms_.size(),
        cpu_stats_frame_index, nullptr, 0.0f, FLT_MAX, ImVec2((float)STATS_FRAME_COUNT, 60));
    char average_total_gpu_primary_time_str[32];
    sprintf(average_total_gpu_primary_time_str, "avg: %.3fms", average_total_gpu_primary_time_ms_);
    ImGui::PlotLines("Primary CB (ms)", total_gpu_primary_times_ms_.data(), (int)total_gpu_primary_times_ms_.size(),
        cpu_stats_frame_index, average_total_gpu_primary_time_str, 0.0f, FLT_MAX, ImVec2((float)STATS_FRAME_COUNT, 60));
    ImGui::PopStyleColor(2);
    ImGui::End();

    VkCommandBufferBeginInfo cb_begin_info = {};
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    SPOKK_VK_CHECK(vkBeginCommandBuffer(cb, &cb_begin_info));

    // Reset this frame's range of the query pools
    timestamp_query_pool_.SetTargetFrame(cb, swapchain_image_index, frame_index_);

    // Applications-specific render code
    timestamp_query_pool_.WriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TIMESTAMP_ID_BEGIN_PRIMARY);
    Render(cb, swapchain_image_index);
    if (force_exit_) {
      break;
    }
    timestamp_query_pool_.WriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TIMESTAMP_ID_END_PRIMARY);

    // optional UI render pass
    if (is_imgui_visible_) {
      imgui_render_pass_.begin_info.framebuffer = imgui_framebuffers_[swapchain_image_index];
      imgui_render_pass_.begin_info.renderArea.extent = swapchain_extent_;
      vkCmdBeginRenderPass(cb, &imgui_render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
      RenderImgui(cb);
      vkCmdEndRenderPass(cb);
    }

    SPOKK_VK_CHECK(vkEndCommandBuffer(cb));
    const VkPipelineStageFlags submit_wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_acquire_semaphore_;
    submit_info.pWaitDstStageMask = &submit_wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cb;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &submit_complete_semaphore_;
    uint64_t submit_wait_start_ticks = zomboClockTicks();
    SPOKK_VK_CHECK(
        vkQueueSubmit(*graphics_and_present_queue_, 1, &submit_info, submit_complete_fences_[pframe_index_]));
    submit_wait_times_ms_[cpu_stats_frame_index] =
        1000.0f * (float)zomboTicksToSeconds(zomboClockTicks() - submit_wait_start_ticks);
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = NULL;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain_;
    present_info.pImageIndices = &swapchain_image_index;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &submit_complete_semaphore_;
    uint64_t present_wait_start_ticks = zomboClockTicks();
    VkResult present_result = vkQueuePresentKHR(*graphics_and_present_queue_, &present_info);
    present_wait_times_ms_[cpu_stats_frame_index] =
        1000.0f * (float)zomboTicksToSeconds(zomboClockTicks() - present_wait_start_ticks);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
      // I've never actually seen these error codes returned, but if they were this is probably how they should be
      // handled.
      int window_width = -1, window_height = -1;
      glfwGetWindowSize(window_.get(), &window_width, &window_height);
      VkExtent2D window_extent = {(uint32_t)window_width, (uint32_t)window_height};
      HandleWindowResize(window_extent);
    } else {
      SPOKK_VK_CHECK(present_result);
    }

    glfwPollEvents();
    frame_index_ += 1;
    pframe_index_ = (pframe_index_ + 1) % PFRAME_COUNT;
  }
  return 0;
}

bool Application::IsInstanceLayerEnabled(const std::string &layer_name) const {
  for (const auto &layer : instance_layers_) {
    if (layer_name == layer.layerName) {
      return true;
    }
  }
  return false;
}
bool Application::IsInstanceExtensionEnabled(const std::string &extension_name) const {
  for (const auto &extension : instance_extensions_) {
    if (extension_name == extension.extensionName) {
      return true;
    }
  }
  return false;
}
bool Application::IsDeviceExtensionEnabled(const std::string &extension_name) const {
  for (const auto &extension : device_extensions_) {
    if (extension_name == extension.extensionName) {
      return true;
    }
  }
  return false;
}

void Application::HandleWindowResize(VkExtent2D new_window_extent) {
  SPOKK_VK_CHECK(vkDeviceWaitIdle(device_));
  SPOKK_VK_CHECK(CreateSwapchain(new_window_extent));

  // Create framebuffers for imgui render pass
  VkFramebufferCreateInfo framebuffer_ci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  framebuffer_ci.renderPass = imgui_render_pass_.handle;
  framebuffer_ci.attachmentCount = 1;
  framebuffer_ci.width = swapchain_extent_.width;
  framebuffer_ci.height = swapchain_extent_.height;
  framebuffer_ci.layers = 1;
  imgui_framebuffers_.resize(swapchain_image_views_.size());
  for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
    if (imgui_framebuffers_[i] != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device_, imgui_framebuffers_[i], host_allocator_);
    }
    framebuffer_ci.pAttachments = &swapchain_image_views_[i];
    SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &imgui_framebuffers_[i]));
  }
}

bool Application::InitImgui(VkRenderPass ui_render_pass) {
  VkDescriptorPoolSize pool_size[11] = {
      // clang-format off
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}, 
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
      // clang-format on
  };
  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000 * 11;
  pool_info.poolSizeCount = 11;
  pool_info.pPoolSizes = pool_size;
  SPOKK_VK_CHECK(vkCreateDescriptorPool(device_, &pool_info, device_.HostAllocator(), &imgui_dpool_));

  // Setup ImGui binding
  ImGui_ImplGlfwVulkan_Init_Data init_data = {};
  init_data.allocator = const_cast<VkAllocationCallbacks *>(device_.HostAllocator());
  init_data.gpu = device_.Physical();
  init_data.device = device_.Logical();
  init_data.render_pass = ui_render_pass;
  init_data.pipeline_cache = device_.PipelineCache();
  init_data.descriptor_pool = imgui_dpool_;
  init_data.check_vk_result = [](VkResult result) { SPOKK_VK_CHECK(result); };
  bool install_glfw_input_callbacks = true;
  bool init_success = ImGui_ImplGlfwVulkan_Init(window_.get(), install_glfw_input_callbacks, &init_data);
  ZOMBO_ASSERT_RETURN(init_success, false, "IMGUI init failed");

  // Load Fonts
  // (there is a default font, this is only if you want to change it. see extra_fonts/README.txt for more details)
  // ImGuiIO& io = ImGui::GetIO();
  // io.Fonts->AddFontDefault();
  // io.Fonts->AddFontFromFileTTF("../../extra_fonts/Cousine-Regular.ttf", 15.0f);
  // io.Fonts->AddFontFromFileTTF("../../extra_fonts/DroidSans.ttf", 16.0f);
  // io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyClean.ttf", 13.0f);
  // io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyTiny.ttf", 10.0f);
  // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

  // Upload Fonts
  VkCommandPoolCreateInfo cpool_ci = {};
  cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cpool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  cpool_ci.queueFamilyIndex = graphics_and_present_queue_->family;
  VkCommandPool cpool = VK_NULL_HANDLE;
  SPOKK_VK_CHECK(vkCreateCommandPool(device_, &cpool_ci, device_.HostAllocator(), &cpool));
  VkCommandBufferAllocateInfo cb_allocate_info = {};
  cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cb_allocate_info.commandPool = cpool;
  cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cb_allocate_info.commandBufferCount = 1;
  VkCommandBuffer cb = VK_NULL_HANDLE;
  SPOKK_VK_CHECK(vkAllocateCommandBuffers(device_, &cb_allocate_info, &cb));
  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  SPOKK_VK_CHECK(vkBeginCommandBuffer(cb, &begin_info));
  bool font_create_success = ImGui_ImplGlfwVulkan_CreateFontsTexture(cb);
  ZOMBO_ASSERT_RETURN(font_create_success, false, "IMGUI failed to create fonts");
  VkSubmitInfo end_info = {};
  end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  end_info.commandBufferCount = 1;
  end_info.pCommandBuffers = &cb;
  SPOKK_VK_CHECK(vkEndCommandBuffer(cb));
  SPOKK_VK_CHECK(vkQueueSubmit(*(device_.FindQueue(VK_QUEUE_GRAPHICS_BIT)), 1, &end_info, VK_NULL_HANDLE));
  SPOKK_VK_CHECK(vkDeviceWaitIdle(device_));
  ImGui_ImplGlfwVulkan_InvalidateFontUploadObjects();
  vkDestroyCommandPool(device_, cpool, device_.HostAllocator());

  is_imgui_enabled_ = true;
  ShowImgui(true);

  return true;
}

void Application::ShowImgui(bool visible) {
  if (visible && !is_imgui_visible_) {
    // invisible -> visible
    ImGui_ImplGlfwVulkan_Show();
  } else if (!visible && is_imgui_visible_) {
    // visible -> invisible
    ImGui_ImplGlfwVulkan_Hide();
    input_state_.ClearHistory();
  }
  is_imgui_visible_ = visible;
}

void Application::RenderImgui(VkCommandBuffer cb) const { ImGui_ImplGlfwVulkan_Render(cb); }

void Application::DestroyImgui(void) {
  if (device_.Logical() != VK_NULL_HANDLE && imgui_dpool_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);

    ImGui_ImplGlfwVulkan_Shutdown();
    vkDestroyDescriptorPool(device_, imgui_dpool_, device_.HostAllocator());
    imgui_dpool_ = VK_NULL_HANDLE;

    ShowImgui(false);
    is_imgui_enabled_ = false;
  }
}

VkResult Application::CreateSwapchain(VkExtent2D extent) {
  ZOMBO_ASSERT(surface_ != VK_NULL_HANDLE, "CreateSwapchain() assumes a non-null VkSurfaceKHR!");

  // Clean up old swapchain images/image views if necessary
  for (auto view : swapchain_image_views_) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(device_, view, host_allocator_);
    }
  }
  swapchain_image_views_.clear();
  swapchain_images_.clear();

  VkSurfaceCapabilitiesKHR surface_caps = {};
  SPOKK_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_.Physical(), surface_, &surface_caps));

  // If the surface's current width is -1, this special value indicates that its dimensions will
  // be determined by the application-provided extent during swapchain creation.
  swapchain_extent_ = surface_caps.currentExtent;
  if ((int32_t)swapchain_extent_.width == -1) {
    swapchain_extent_.width =
        SPOKK__CLAMP(extent.width, surface_caps.minImageExtent.width, surface_caps.maxImageExtent.width);
    swapchain_extent_.height =
        SPOKK__CLAMP(extent.height, surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height);
  }

  uint32_t device_surface_format_count = 0;
  std::vector<VkSurfaceFormatKHR> device_surface_formats;
  VkResult result = VK_INCOMPLETE;
  do {
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device_.Physical(), surface_, &device_surface_format_count, nullptr);
    if (result == VK_SUCCESS && device_surface_format_count > 0) {
      device_surface_formats.resize(device_surface_format_count);
      result = vkGetPhysicalDeviceSurfaceFormatsKHR(
          device_.Physical(), surface_, &device_surface_format_count, device_surface_formats.data());
    }
  } while (result == VK_INCOMPLETE);
  if (device_surface_formats.size() == 1 && device_surface_formats[0].format == VK_FORMAT_UNDEFINED) {
    // No preferred format.
    swapchain_surface_format_.format = VK_FORMAT_B8G8R8A8_UNORM;
    swapchain_surface_format_.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  } else {
    ZOMBO_ASSERT(device_surface_formats.size() >= 1, "Device must support >0 surface formats");
    swapchain_surface_format_ = device_surface_formats[0];
  }

  uint32_t device_present_mode_count = 0;
  std::vector<VkPresentModeKHR> device_present_modes;
  do {
    result =
        vkGetPhysicalDeviceSurfacePresentModesKHR(device_.Physical(), surface_, &device_present_mode_count, nullptr);
    if (result == VK_SUCCESS && device_present_mode_count > 0) {
      device_present_modes.resize(device_present_mode_count);
      result = vkGetPhysicalDeviceSurfacePresentModesKHR(
          device_.Physical(), surface_, &device_present_mode_count, device_present_modes.data());
    }
  } while (result == VK_INCOMPLETE);
  std::array<bool, 4> present_mode_supported = {};
  for (auto mode : device_present_modes) {
    present_mode_supported[mode] = true;
  }
  // TODO(https://github.com/cdwfs/spokk/issues/12): Put this logic under application control
  // TODO(https://github.com/cdwfs/spokk/issues/30): Let this be tweaked at runtime through imgui
  if (swapchain_present_mode_ >= present_mode_supported.size()) {
    fprintf(stderr, "ERROR: invalid present mode (%d); defaulting to FIFO\n", swapchain_present_mode_);
    swapchain_present_mode_ = VK_PRESENT_MODE_FIFO_KHR;
  } else if (!present_mode_supported[swapchain_present_mode_]) {
    fprintf(stderr, "ERROR: unsupported present mode (%d); defaulting to FIFO\n", swapchain_present_mode_);
    swapchain_present_mode_ = VK_PRESENT_MODE_FIFO_KHR;
  }
  ZOMBO_ASSERT(present_mode_supported[VK_PRESENT_MODE_FIFO_KHR], "FIFO present mode unsupported?!?");

  uint32_t desired_swapchain_image_count = surface_caps.minImageCount + 1;
  if (surface_caps.maxImageCount > 0 && desired_swapchain_image_count > surface_caps.maxImageCount) {
    desired_swapchain_image_count = surface_caps.maxImageCount;
  }

  VkSurfaceTransformFlagBitsKHR surface_transform = surface_caps.currentTransform;

  VkImageUsageFlags swapchain_image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  assert((surface_caps.supportedUsageFlags & swapchain_image_usage) == swapchain_image_usage);

  assert(surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
  VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

  VkSwapchainKHR old_swapchain = swapchain_;
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
  swapchain_ci.presentMode = swapchain_present_mode_;
  swapchain_ci.clipped = VK_TRUE;
  swapchain_ci.oldSwapchain = old_swapchain;
  SPOKK_VK_CHECK(vkCreateSwapchainKHR(device_, &swapchain_ci, host_allocator_, &swapchain_));
  if (old_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device_, old_swapchain, host_allocator_);
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
  image_view_ci.image = VK_NULL_HANDLE;  // filled in below
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
  for (auto image : swapchain_images_) {
    image_view_ci.image = image;
    VkImageView view = VK_NULL_HANDLE;
    SPOKK_VK_CHECK(vkCreateImageView(device_, &image_view_ci, host_allocator_, &view));
    swapchain_image_views_.push_back(view);
  }
  swapchain_image_frames_.resize(swapchain_images_.size(), 0);
  return VK_SUCCESS;
}
