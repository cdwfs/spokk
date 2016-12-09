// Must happen before any vulkan.h include, in order to get the platform-specific extensions included.
#if defined(_MSC_VER)
# define VK_USE_PLATFORM_WIN32_KHR 1
# define PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif #elif defined(unix) || defined(__unix__) || defined(__unix)
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
#include "vk_init.h"
using namespace cdsvk;

#include "platform.h"

#include <cassert>
#include <cstdio>

// TODO(cort): proper return-value test
#if defined(_MSC_VER)
# define CDSVK__RETVAL_CHECK(expected, expr) \
  do {  \
    int err = (int)(expr);                             \
    if (err != (expected)) {                                            \
      printf("%s(%d): error in %s() -- %s returned %d", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
      __debugbreak();                                                   \
    }                                                                   \
    assert(err == (expected));                                          \
    __pragma(warning(push))                                             \
    __pragma(warning(disable:4127))                                 \
    } while(0)                                                      \
  __pragma(warning(pop))
#elif #elif defined(unix) || defined(__unix__) || defined(__unix)
# define CDSVK__RETVAL_CHECK(expected, expr) \
  do {  \
    int err = (int)(expr);                                                   \
    if (err != (expected)) {                                            \
      printf("%s(%d): error in %s() -- %s returned %d", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
      /*__asm__("int $3"); */                 \
    }                                                                   \
    assert(err == (expected));                                          \
  } while(0)
#elif defined(__ANDROID__)
# define CDSVK__RETVAL_CHECK(expected, expr) \
  do {  \
    int err = (int)(expr);                                                   \
    if (err != (expected)) {                                            \
      printf("%s(%d): error in %s() -- %s returned %d", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
      /*__asm__("int $3"); */                 \
    }                                                                   \
    assert(err == (expected));                                          \
  } while(0)
#else
# error Unsupported platform
#endif
#define CDSVK__CHECK(expr) CDSVK__RETVAL_CHECK(VK_SUCCESS, expr)

#define CDSVK__CLAMP(x, xmin, xmax) ( ((x)<(xmin)) ? (xmin) : ( ((x)>(xmax)) ? (xmax) : (x) ) )

namespace {
// Effective Modern C++, Item 21: make_unique() is C++14 only, but easy to implement in C++11.
template <typename T, typename... Ts>
std::unique_ptr<T> my_make_unique(Ts&&... params) {
  return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

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
const uint32_t kVframeCount = 2;
}  // namespace

Application::Application(const CreateInfo &ci) {
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

  // Initialize Vulkan
  std::vector<const char*> required_instance_layer_names = {};
  if (ci.enable_validation) {
    required_instance_layer_names.push_back("VK_LAYER_LUNARG_standard_validation");
  }
  std::vector<const char*> optional_instance_layer_names = {};
  std::vector<const char*> enabled_instance_layer_names;
  CDSVK__CHECK(cdsvk::get_supported_instance_layers(
    required_instance_layer_names, optional_instance_layer_names,
    &instance_layers_, &enabled_instance_layer_names));

  std::vector<const char*> required_instance_extension_names = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    PLATFORM_SURFACE_EXTENSION_NAME,
  };
  std::vector<const char*> optional_instance_extension_names = {};
  if (ci.enable_validation) {
    optional_instance_extension_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }
  std::vector<const char*> enabled_instance_extension_names;
  CDSVK__CHECK(cdsvk::get_supported_instance_extensions(instance_layers_,
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
  CDSVK__CHECK(vkCreateInstance(&instance_ci, allocation_callbacks_, &instance_));

  if (is_instance_extension_enabled(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) {
    VkDebugReportCallbackCreateInfoEXT debug_report_callback_ci = {};
    debug_report_callback_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_callback_ci.flags = 0
      | VK_DEBUG_REPORT_ERROR_BIT_EXT
      | VK_DEBUG_REPORT_WARNING_BIT_EXT
      | VK_DEBUG_REPORT_INFORMATION_BIT_EXT
      | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
      ;
    debug_report_callback_ci.pfnCallback = my_debug_report_callback;
    debug_report_callback_ci.pUserData = nullptr;
    CDSVK__CHECK(vkCreateDebugReportCallbackEXT(instance_, &debug_report_callback_ci, allocation_callbacks_, &debug_report_callback_));
    assert(debug_report_callback_ != VK_NULL_HANDLE);
  }

  CDSVK__CHECK( glfwCreateWindowSurface(instance_, window_.get(), allocation_callbacks_, &surface_) );

  // TODO(cort): expose in CreateInfo
  const std::vector<cdsvk::QueueFamilyRequirements> queue_family_reqs = {
    {VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT, surface_, 1},
  };
  std::vector<uint32_t> queue_family_indices;
  CDSVK__CHECK(cdsvk::find_physical_device(queue_family_reqs, instance_, &physical_device_, &queue_family_indices));
  std::vector<VkDeviceQueueCreateInfo> device_queue_cis = {};
  for(uint32_t iQF=0; iQF<(uint32_t)queue_family_indices.size(); ++iQF) {
    uint32_t queue_count = queue_family_reqs[iQF].minimum_queue_count;
    const std::vector<float> queue_priorities(queue_count, 0.0f);
    device_queue_cis.push_back({
      VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0,
      queue_family_indices[iQF], queue_count, queue_priorities.data()
    });
  };

  const std::vector<const char*> required_device_extension_names = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
  const std::vector<const char*> optional_device_extension_names = {
#if !defined(NDEBUG) && defined(VK_EXT_debug_marker)
    VK_EXT_DEBUG_MARKER_EXTENSION_NAME, // will only be enabled if a layer supports it (currently, only RenderDoc's implicit layer)
#endif
  };
  std::vector<const char*> enabled_device_extension_names;
  CDSVK__CHECK(cdsvk::get_supported_device_extensions(physical_device_, instance_layers_,
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
  CDSVK__CHECK(vkCreateDevice(physical_device_, &device_ci, allocation_callbacks_, &device_));

  // Create VkSwapchain
  if (surface_ != VK_NULL_HANDLE) {
    VkSurfaceCapabilitiesKHR surface_caps = {};
    CDSVK__CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &surface_caps));
    VkExtent2D swapchain_extent = surface_caps.currentExtent;
    if ((int32_t)swapchain_extent.width == -1) {
      assert( (int32_t)swapchain_extent.height == -1 );
      // TODO(cort): better defaults here, when we can't detect the present surface extent?
      swapchain_extent.width =
        CDSVK__CLAMP(ci.window_width, surface_caps.minImageExtent.width, surface_caps.maxImageExtent.width);
      swapchain_extent.height =
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
    swapchain_ci.imageExtent.width = swapchain_extent.width;
    swapchain_ci.imageExtent.height = swapchain_extent.height;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageUsage = swapchain_image_usage;
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.preTransform = surface_transform;
    swapchain_ci.compositeAlpha = composite_alpha;
    swapchain_ci.presentMode = present_mode;
    swapchain_ci.clipped = VK_TRUE;
    swapchain_ci.oldSwapchain = old_swapchain;
    CDSVK__CHECK(vkCreateSwapchainKHR(device_, &swapchain_ci, allocation_callbacks_, &swapchain_));
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
      CDSVK__CHECK(vkCreateImageView(device_, &image_view_ci, allocation_callbacks_, &view));
      swapchain_image_views_.push_back(view);
    }
  }

  init_successful = true;
}
Application::~Application() {
  if (device_) {
    vkDeviceWaitIdle(device_);

    for(auto& view : swapchain_image_views_) {
      vkDestroyImageView(device_, view, allocation_callbacks_);
      view = VK_NULL_HANDLE;
    }
  }
}

int Application::run() {
  if (!init_successful) {
    return -1;
  }

  const uint64_t clock_start = zomboClockTicks();
  uint64_t ticks_prev = clock_start;
  uint32_t frame_index = 0;
  while(!glfwWindowShouldClose(window_.get())) {
    uint64_t ticks_now = zomboClockTicks();
    const double dt = (float)zomboTicksToSeconds(ticks_now - ticks_prev);
    ticks_prev = ticks_now;

    update(dt);
    render();

    glfwPollEvents();
    frame_index += 1;
  }
  return 0;
}

void Application::update(double /*dt*/) {
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
