#pragma once

#ifdef _MSC_VER
#include <windows.h>
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>

namespace cdsvk {
  class Application {
  public:
    struct CreateInfo {
      std::string app_name = "Spokk Application";
      uint32_t window_width = 1920, window_height = 1080;
      bool enable_fullscreen = false;
      bool enable_validation = true;
      bool enable_vsync = true;
    };

    explicit Application(const CreateInfo &ci);
    virtual ~Application();

    Application(const Application&) = delete;
    const Application& operator=(const Application&) = delete;

    int run();

    virtual void update(double dt);
    virtual void render();

  protected:
    bool is_instance_layer_enabled(const std::string& layer_name) const;
    bool is_instance_extension_enabled(const std::string& layer_name) const;
    bool is_device_extension_enabled(const std::string& layer_name) const;

    const VkAllocationCallbacks *allocation_callbacks_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    std::vector<VkLayerProperties> instance_layers_ = {};
    std::vector<VkExtensionProperties> instance_extensions_ = {};
    VkDebugReportCallbackEXT debug_report_callback_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceFeatures physical_device_features_ = {};
    VkDevice device_ = VK_NULL_HANDLE;
    std::vector<VkExtensionProperties> device_extensions_ = {};
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkSurfaceFormatKHR swapchain_surface_format_ = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    std::vector<VkImage> swapchain_images_ = {};
    std::vector<VkImageView> swapchain_image_views_ = {};
    VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;
    
    std::shared_ptr<GLFWwindow> window_ = nullptr;

  private:
    bool init_successful = false;

  };
}