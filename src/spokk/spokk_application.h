#if !defined(SPOKK_APPLICATION_H)
#define SPOKK_APPLICATION_H

#include "spokk_platform.h"

#if defined(ZOMBO_PLATFORM_WINDOWS)
#include <wingdi.h>
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "spokk_buffer.h"
#include "spokk_device.h"
#include "spokk_image.h"
#include "spokk_input.h"
#include "spokk_memory.h"
#include "spokk_pipeline.h"
#include "spokk_time.h"
#include "spokk_utilities.h"
#include "spokk_vertex.h"

#include <array>
#include <cassert>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct VmaAllocator_T;  // from vk_mem_alloc.h

namespace spokk {

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
constexpr uint32_t PFRAME_COUNT = 2;

//
// Application base class
//
class Application {
public:
  struct QueueFamilyRequest {
    VkQueueFlags flags;  // Mask of features which must be supported by this queue family.
    bool support_present;  // If flags & VK_QUEUE_GRAPHICS_BIT, true means the queue can present to surface_.
    uint32_t queue_count;
    float priority;
  };

  struct CreateInfo {
    std::string app_name = "Spokk Application";
    uint32_t window_width = 1280, window_height = 720;
    bool enable_fullscreen = false;
    bool enable_graphics = true;
    // clang-format off
    VkDebugReportFlagsEXT debug_report_flags = 0
      | VK_DEBUG_REPORT_ERROR_BIT_EXT
      | VK_DEBUG_REPORT_WARNING_BIT_EXT
      ;
#if defined(VK_EXT_debug_utils)
    VkDebugUtilsMessageSeverityFlagsEXT debug_utils_severity_flags = 0
      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
      ;
    VkDebugUtilsMessageTypeFlagsEXT debug_utils_type_flags = 0
      | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
      ;
#endif  // defined(VK_EXT_debug_utils)

    std::vector<QueueFamilyRequest> queue_family_requests;
    std::vector<const char*> required_instance_layer_names = {};
    std::vector<const char*> required_instance_extension_names = {};
    std::vector<const char*> required_device_extension_names = {};
    std::vector<const char*> optional_instance_layer_names = {};
    std::vector<const char*> optional_instance_extension_names = {};
    std::vector<const char*> optional_device_extension_names = {};
    // If NULL, no device features are enabled. To easily enable all supported features,
    // pass EnableAllSupportedDeviceFeatures.
    SetDeviceFeaturesFunc pfn_set_device_features = nullptr;
    const VkAllocationCallbacks* host_allocator = nullptr;
  };

  explicit Application(const CreateInfo& ci);
  virtual ~Application();

  Application(const Application&) = delete;
  const Application& operator=(const Application&) = delete;

  int Run();

  // Update() is intended for non-graphics-related per-frame operations. When this
  // function is called, the input state has been updated for a new frame, but the
  // graphics resources this frame will use may stay be in use by a previous frame.
  virtual void Update(double dt) = 0;

  // When Render() is called, vkAcquireNextImageKHR() has already returned, and the
  // resources for the current pframe are guaranteed not to be in use by a previous
  // frame.
  virtual void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) = 0;

protected:
  // The first thing it does is call vkDeviceWaitIdle(), so subclasses can safely assume that
  // no resources are in use on the GPU and can be safely destroyed/recreated.
  virtual void HandleWindowResize(VkExtent2D new_window_extent) { (void)new_window_extent; }

  const VkAllocationCallbacks* host_allocator_ = nullptr;
  VkInstance instance_ = VK_NULL_HANDLE;
  VkDebugReportCallbackEXT debug_report_callback_ = VK_NULL_HANDLE;
#if defined(VK_EXT_debug_utils)
  VkDebugUtilsMessengerEXT debug_utils_msgr_ = VK_NULL_HANDLE;
#endif // defined(VK_EXT_debug_utils)
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;

  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkSurfaceFormatKHR swapchain_surface_format_ = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  VkExtent2D swapchain_extent_ = {};
  std::vector<VkImage> swapchain_images_ = {};
  std::vector<VkImageView> swapchain_image_views_ = {};
  std::vector<uint64_t> swapchain_image_frames_ = {};  // frame index most recently rendered by each swapchain image
  std::shared_ptr<GLFWwindow> window_ = nullptr;

  InputState input_state_;

  // handles refer to this application's device_, queues_, etc.
  Device device_ = {};

  // Queue used by the framework for primary graphics/command buffer submission.
  const DeviceQueue* graphics_and_present_queue_;

  uint64_t frame_index_;  // Frame number since launch
  uint32_t pframe_index_;  // current pframe (pipelined frame) index; cycles from 0 to PFRAME_COUNT-1, then back to 0.

  bool force_exit_ = false;  // Application can set this to true to exit at the next available chance.

private:
  // Initialize imgui. The provided render pass must be the one that will be active when
  // RenderImgui() will be called.
  bool InitImgui(VkRenderPass ui_render_pass, uint32_t ui_subpass = 0);
  // If visible=true, the imgui will be rendered, the cursor will be visible, and any
  // keyboard/mouse consumed by imgui will be ignored by InputState.
  // If visible=false, imgui will not be rendered (but UI controls throughout the code will still
  // be processed, so if they're expensive, maybe make them conditional). The mouse cursor will
  // be hidden, and InputState will get updated keyboard/mouse input every frame.
  void ShowImgui(bool visible);
  // Cleans up all IMGUI resources. This is automatically called during application shutdown, but
  // would need to be called manually to reinitialize the GUI subsystem at runtime (e,g. with a
  // different render pass).
  // Safe to call, even if IMGUI was not initialized or has already been destroyed.
  void DestroyImgui(void);

  void HandleWindowResizeInternal(VkExtent2D new_window_extent);

  VkResult CreateSwapchain(VkExtent2D extent);

  bool init_successful_ = false;
  bool is_graphics_app_ = false;

  VkCommandPool primary_cpool_ = VK_NULL_HANDLE;
  std::array<VkCommandBuffer, PFRAME_COUNT> primary_command_buffers_;
  VkSemaphore image_acquire_semaphore_ = VK_NULL_HANDLE;
  VkSemaphore submit_complete_semaphore_ = VK_NULL_HANDLE;
  std::array<VkFence, PFRAME_COUNT> submit_complete_fences_ = {};

  bool is_imgui_visible_ = false;  // Tracks whether the UI is visible or not.
  RenderPass imgui_render_pass_ = {};
  std::vector<VkFramebuffer> imgui_framebuffers_ = {};

  TimestampQueryPool timestamp_query_pool_;

  // Changing this takes effect at the next HandleWindowResize().
  VkPresentModeKHR swapchain_present_mode_ = VK_PRESENT_MODE_FIFO_KHR;

  // Frame timing stats
  static constexpr uint32_t STATS_FRAME_COUNT = 100;
  std::array<float, STATS_FRAME_COUNT> total_frame_times_ms_ = {};
  std::array<float, STATS_FRAME_COUNT> fence_wait_times_ms_ = {};
  std::array<float, STATS_FRAME_COUNT> acquire_wait_times_ms_ = {};
  std::array<float, STATS_FRAME_COUNT> submit_wait_times_ms_ = {};
  std::array<float, STATS_FRAME_COUNT> present_wait_times_ms_ = {};
  std::array<float, STATS_FRAME_COUNT> total_gpu_primary_times_ms_ = {};  // time to execute primary command buffer

  float average_total_frame_time_ms_ = 0;
  float average_total_gpu_primary_time_ms_ = 0;

  VmaAllocator_T* vma_allocator_ = nullptr;
  DeviceAllocationCallbacks device_allocator_ = {};
};

}  // namespace spokk

#endif  // !defined(SPOKK_APPLICATION_H)
