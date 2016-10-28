#include "platform.h"
#define VULKAN_CHECK(expr) ZOMBO_RETVAL_CHECK(VK_SUCCESS, expr)

// Must happen before any vulkan.h include, in order to get the platform-specific extensions included.
#if defined(ZOMBO_PLATFORM_WINDOWS)
# define VK_USE_PLATFORM_WIN32_KHR 1
#elif defined(ZOMBO_PLATFORM_POSIX)
# define VK_USE_PLATFORM_XCB_KHR 1
#elif defined(ZOMBO_PLATFORM_ANDROID)
# define VK_USE_PLATFORM_ANDROID_KHR 1
#else
# error Unsupported platform
#endif

#define CDS_VULKAN_IMPLEMENTATION
#include "cds_vulkan.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace {
    void my_glfw_error_callback(int error, const char *description) {
        fprintf( stderr, "GLFW Error %d: %s\n", error, description);
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL my_debug_report_callback(VkFlags msgFlags,
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
        return VK_FALSE; // false = don't bail out of an API call with validation failures.
    }

    VkSurfaceKHR my_get_vk_surface(VkInstance instance, const VkAllocationCallbacks *allocation_callbacks, void *userdata) {
        GLFWwindow *window = (GLFWwindow*)userdata;
        VkSurfaceKHR present_surface = VK_NULL_HANDLE;
        VULKAN_CHECK( glfwCreateWindowSurface(instance, window, allocation_callbacks, &present_surface) );
        return present_surface;
    }

    const uint32_t kWindowWidthDefault = 1280;
    const uint32_t kWindowHeightDefault = 720;
}  // namespace

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    const std::string application_name = "Vulkswagen";
    const std::string engine_name = "Zombo";

    // Initialize GLFW
    glfwSetErrorCallback(my_glfw_error_callback);
    if( !glfwInit() ) {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        return -1;
    }
    if (!glfwVulkanSupported()) {
        fprintf(stderr, "Vulkan is not available :(\n");
        return -1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(kWindowWidthDefault, kWindowHeightDefault, application_name.c_str(), NULL, NULL);

    vk::ApplicationInfo application_info(application_name.c_str(), 0x1000,
        engine_name.c_str(), 0x1001, VK_MAKE_VERSION(1,0,30));
    cdsvk::ContextCreateInfo context_ci = {};
    context_ci.allocation_callbacks = nullptr;
    context_ci.required_instance_layer_names = {
        "VK_LAYER_LUNARG_standard_validation",// TODO: fallback if standard_validation metalayer is not available
    };
    context_ci.optional_instance_layer_names = {
#if !defined(NDEBUG)
        // Do not explicitly enable! only needed to test VK_EXT_debug_marker support, and may generate other
        // spurious errors.
        //"VK_LAYER_RENDERDOC_Capture",
#endif
    };
    context_ci.required_instance_extension_names = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#else
#error Unsupported platform
#endif
    };
    context_ci.optional_instance_extension_names = {
#if !defined(NDEBUG)
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
    };
    context_ci.required_device_extension_names = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    context_ci.optional_device_extension_names = {
#if !defined(NDEBUG) && defined(VK_EXT_debug_marker)
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME, // will only be enabled if a layer supports it (currently, only RenderDoc's implicit layer)
#endif
    };
    context_ci.pfn_get_vk_surface = my_get_vk_surface;
    context_ci.get_vk_surface_userdata = window;
    context_ci.application_info = &application_info;
    context_ci.debug_report_callback = my_debug_report_callback;
    context_ci.debug_report_flags = vk::DebugReportFlagBitsEXT()
        | vk::DebugReportFlagBitsEXT::eError
        | vk::DebugReportFlagBitsEXT::eWarning
        | vk::DebugReportFlagBitsEXT::eInformation
        | vk::DebugReportFlagBitsEXT::ePerformanceWarning
        ;
    cdsvk::Context *context = new cdsvk::Context(context_ci);

    glfwTerminate();
    delete context;
    return 0;
}