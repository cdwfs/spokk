#pragma once

#ifdef _MSC_VER
#include <windows.h>
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vk_buffer.h"
#include "vk_image.h"
#include "vk_utilities.h"
#include "vk_vertex.h"

#include "vk_context.h"
#include "vk_memory.h"
#include "vk_pipeline.h"

#include <array>
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

#if defined(SPOKK_ENABLE_SHADERC)
#include <shaderc/shaderc.hpp>
#endif  // defined(SPOKK_ENABLE_SHADERC)

namespace spokk {

class InputState {
public:
  InputState()
      : window_{}
      , current_{}
      , prev_{} {

  }
  explicit InputState(const std::shared_ptr<GLFWwindow>& window)
      : window_(window)
      , current_{}
      , prev_{} {
  }
  ~InputState() = default;

  void SetWindow(const std::shared_ptr<GLFWwindow>& window) {
    window_ = window;
  }

  enum Digital {
    DIGITAL_LPAD_UP    =  0,
    DIGITAL_LPAD_LEFT  =  1,
    DIGITAL_LPAD_RIGHT =  2,
    DIGITAL_LPAD_DOWN  =  3,
    DIGITAL_RPAD_UP    =  4,
    DIGITAL_RPAD_LEFT  =  5,
    DIGITAL_RPAD_RIGHT =  6,
    DIGITAL_RPAD_DOWN  =  7,

    DIGITAL_COUNT
  };
  enum Analog {
    ANALOG_L_X     = 0,
    ANALOG_L_Y     = 1,
    ANALOG_R_X     = 2,
    ANALOG_R_Y     = 3,
    ANALOG_MOUSE_X = 4,
    ANALOG_MOUSE_Y = 5,

    ANALOG_COUNT
  };
  void Update();
  bool IsPressed(Digital id) const  { return  current_.digital[id] && !prev_.digital[id]; }
  bool IsReleased(Digital id) const { return !current_.digital[id] &&  prev_.digital[id]; }
  bool GetDigital(Digital id) const { return  current_.digital[id]; }
  float GetAnalog(Analog id) const  { return  current_.analog[id]; }

private:
  struct {
    std::array<bool, DIGITAL_COUNT> digital;
    std::array<float, ANALOG_COUNT> analog;
  } current_, prev_;
  std::weak_ptr<GLFWwindow> window_;
};

// How many frames can be pipelined ("in flight") simultaneously? The higher the count, the more independent copies
// of various resources (anything changing per frame) must be created and maintained in memory.
// 1 = CPU and GPU run synchronously, each idling while the other works. Safe, but slow.
// 2 = GPU renders from N while CPU builds commands for frame N+1. Usually a safe choice.
//     If the CPU finishes early, it will block until the GPU is finished.
// 3 = GPU renders from N, while CPU builds commands for frame N+1. This mode is best when using the
//     MAILBOX present mode; it prevents the CPU from ever blocking on the GPU. If the CPU finishes
//     early, it can queue frame N+1 for presentation and get started on frame N+2; if it finishes *that*
//     before the GPU finishes frame N, then frame N+1 is discarded and frame N+2 is queued for
//     presentation instead, and the CPU starts work on frame N+3. And so on.
const uint32_t PFRAME_COUNT = 2;

//
// Application base class
//
class Application {
public:
  struct QueueFamilyRequest {
    VkQueueFlags flags;  // Mask of features which must be supported by this queue family.
    bool support_present;  // If flags & VK_QUEUE_GRAPHICS_BIT, support_present=true means the queue must support presentation to the application's VkSurfaceKHR.
    uint32_t queue_count;
    float priority;
  };

  struct CreateInfo {
    std::string app_name = "Spokk Application";
    uint32_t window_width = 1280, window_height = 720;
    bool enable_graphics = true;
    bool enable_fullscreen = false;
    bool enable_vsync = true;
    VkDebugReportFlagsEXT debug_report_flags = 
#ifdef NDEBUG
      0;
#else
      VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
#endif
    std::vector<QueueFamilyRequest> queue_family_requests;
  };

  explicit Application(const CreateInfo &ci);
  virtual ~Application();

  Application(const Application&) = delete;
  const Application& operator=(const Application&) = delete;

  int Run();

  virtual void Update(double dt);
  virtual void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) = 0;

protected:
  bool IsInstanceLayerEnabled(const std::string& layer_name) const;
  bool IsInstanceExtensionEnabled(const std::string& layer_name) const;
  bool IsDeviceExtensionEnabled(const std::string& layer_name) const;

  const VkAllocationCallbacks *host_allocator_ = nullptr;
  const DeviceAllocationCallbacks *device_allocator_ = nullptr;
  VkInstance instance_ = VK_NULL_HANDLE;
  std::vector<VkLayerProperties> instance_layers_ = {};
  std::vector<VkExtensionProperties> instance_extensions_ = {};
  VkDebugReportCallbackEXT debug_report_callback_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkPhysicalDeviceFeatures physical_device_features_ = {};
  VkDevice device_ = VK_NULL_HANDLE;
  std::vector<VkExtensionProperties> device_extensions_ = {};
  std::vector<DeviceQueue> queues_;

  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkSurfaceFormatKHR swapchain_surface_format_ = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  VkExtent2D swapchain_extent_;
  std::vector<VkImage> swapchain_images_ = {};
  std::vector<VkImageView> swapchain_image_views_ = {};

  VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;
    
  std::shared_ptr<GLFWwindow> window_ = nullptr;

  InputState input_state_;

  // handles refer to this application's device_, queues_, etc.
  DeviceContext device_context_;

  uint32_t frame_index_;  // Frame number since launch
  uint32_t pframe_index_;  // current pframe (pipelined frame) index; cycles from 0 to PFRAME_COUNT-1, then back to 0.

  bool force_exit_ = false;  // Application can set this to true to exit at the next available chance.

private:
  bool init_successful = false;

  // TODO(cort): Do apps need to know the graphics/present queue, so they can transition resources to it if necessary?
  const DeviceQueue* graphics_and_present_queue_;
  VkCommandPool primary_cpool_;
  std::array<VkCommandBuffer, PFRAME_COUNT> primary_command_buffers_;
  VkSemaphore image_acquire_semaphore_;
  VkSemaphore submit_complete_semaphore_;
  std::array<VkFence, PFRAME_COUNT> submit_complete_fences_;
};

}  // namespace spokk