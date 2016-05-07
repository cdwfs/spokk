#ifdef _WIN32
#   include <Windows.h>
#endif

#include "platform.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VULKAN_CHECK(expr) ZOMBO_RETVAL_CHECK(VK_SUCCESS, expr)

#define kDemoTextureCount 1U
#define kWindowWidthDefault 1280U
#define kWindowHeightDefault 720U


static void myGlfwErrorCallback(int error, const char *description)
{
    fprintf( stderr, "GLFW Error %d: %s\n", error, description);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFunc(VkFlags msgFlags,
    VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode,
    const char *pLayerPrefix, const char *pMsg, void *pUserData) {

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

static VkBool32 getMemoryTypeFromProperties(const VkPhysicalDeviceMemoryProperties *memoryProperties,
    uint32_t memoryTypeBits, VkFlags requirementsMask, uint32_t *outMemoryTypeIndex) {
    assert(sizeof(memoryTypeBits)*8 == VK_MAX_MEMORY_TYPES);
    for(uint32_t iMemType=0; iMemType<VK_MAX_MEMORY_TYPES; iMemType+=1) {
        if (	(memoryTypeBits & (1<<iMemType)) != 0
            &&	(memoryProperties->memoryTypes[iMemType].propertyFlags & requirementsMask) == requirementsMask) {
            *outMemoryTypeIndex = iMemType;
            return VK_TRUE;
        }
    }
    return VK_FALSE;
}

static void setImageLayout(VkCommandBuffer cmdBuf, VkImage image,
        VkImageSubresourceRange subresourceRange, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkAccessFlags srcAccessMask) {
    VkImageMemoryBarrier imgMemoryBarrier = {};
    imgMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imgMemoryBarrier.pNext = NULL;
    imgMemoryBarrier.srcAccessMask = srcAccessMask;
    imgMemoryBarrier.dstAccessMask = 0; // overwritten below
    imgMemoryBarrier.oldLayout = oldLayout;
    imgMemoryBarrier.newLayout = newLayout;
    imgMemoryBarrier.image = image;
    imgMemoryBarrier.subresourceRange = subresourceRange;
    switch(oldLayout) {
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        imgMemoryBarrier.srcAccessMask |= VK_ACCESS_HOST_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        imgMemoryBarrier.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        imgMemoryBarrier.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;

    }

    switch(newLayout) {
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        imgMemoryBarrier.dstAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        imgMemoryBarrier.dstAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        imgMemoryBarrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        imgMemoryBarrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        imgMemoryBarrier.srcAccessMask |= VK_ACCESS_HOST_WRITE_BIT;
        imgMemoryBarrier.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
        // Make sure any Copy or CPU writes to image are flushed
        imgMemoryBarrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
        imgMemoryBarrier.dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        break;
    }

    VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    // TODO(cort): 
    VkDependencyFlags dependencyFlags = 0;
    uint32_t memoryBarrierCount = 0;
    const VkMemoryBarrier *memoryBarriers = NULL;
    uint32_t bufferMemoryBarrierCount = 0;
    const VkBufferMemoryBarrier *bufferMemoryBarriers = NULL;
    uint32_t imageMemoryBarrierCount = 1;
    vkCmdPipelineBarrier(cmdBuf, srcStages, dstStages, dependencyFlags,
        memoryBarrierCount, memoryBarriers,
        bufferMemoryBarrierCount, bufferMemoryBarriers,
        imageMemoryBarrierCount, &imgMemoryBarrier);
}

VkShaderModule readSpvFile(VkDevice device, const VkAllocationCallbacks *allocationCallbacks, const char *spvFilePath) {
    FILE *spvFile = fopen(spvFilePath, "rb");
    if (!spvFile) {
        return VK_NULL_HANDLE;
    }
    fseek(spvFile, 0, SEEK_END);
    long spvFileSize = ftell(spvFile);
    fseek(spvFile, 0, SEEK_SET);
    void *shaderCode = malloc(spvFileSize);
    size_t bytesRead = fread(shaderCode, 1, spvFileSize, spvFile);
    fclose(spvFile);
    if (bytesRead != spvFileSize) {
        free(shaderCode);
        return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pNext = NULL;
    shaderModuleCreateInfo.flags = 0;
    shaderModuleCreateInfo.codeSize = spvFileSize;
    shaderModuleCreateInfo.pCode = (uint32_t*)shaderCode;
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateShaderModule(device, &shaderModuleCreateInfo, allocationCallbacks, &shaderModule) );
    free(shaderCode);
    return shaderModule;
}

int main(int argc, char *argv[]) {
    //
    // Initialise GLFW
    //

    // Set a callback to handle GLFW errors (*not* Vulkan errors! That comes later)
    glfwSetErrorCallback(myGlfwErrorCallback);

    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        return -1;
    }
    if (!glfwVulkanSupported()) {
        fprintf(stderr, "Vulkan is not available :(\n");
        return -1;
    }

    // Create Vulkan instance
    const char *layerName = NULL;
    uint32_t instanceExtensionCount = 0;
    VULKAN_CHECK( vkEnumerateInstanceExtensionProperties(layerName, &instanceExtensionCount, NULL) );
    VkExtensionProperties *instanceExtensionProperties = (VkExtensionProperties*)malloc(instanceExtensionCount * sizeof(VkExtensionProperties));
    VULKAN_CHECK( vkEnumerateInstanceExtensionProperties(layerName, &instanceExtensionCount, instanceExtensionProperties) );
    const char **instanceExtensionNames = (const char**)malloc(instanceExtensionCount * sizeof(const char**));
    for(uint32_t iExt=0; iExt<instanceExtensionCount; iExt+=1) {
        instanceExtensionNames[iExt] = instanceExtensionProperties[iExt].extensionName;
    }

    uint32_t instanceLayerCount = 0;
    VULKAN_CHECK( vkEnumerateInstanceLayerProperties(&instanceLayerCount, NULL) );
    VkLayerProperties *instanceLayerProperties = (VkLayerProperties*)malloc(instanceLayerCount * sizeof(VkLayerProperties) );
    VULKAN_CHECK( vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties) );
    const char *desiredInstanceLayerNames[] = {
        //"VK_LAYER_LUNARG_api_dump", // prints each vk API call (+inputs and outputs) to stdout
        "VK_LAYER_LUNARG_core_validation", // Ensures resources bound to descriptor sets align with the specified set layout, validates image/buffer layout transitions, etc.
        "VK_LAYER_LUNARG_device_limits", // Tracks device features/limitations and reports an error if the app requests unsupported features. Work in progess.
        "VK_LAYER_LUNARG_image", // Validates image parameters, formats, and correct use.
        "VK_LAYER_LUNARG_object_tracker", // Tracks all Vulkan objects. Reports on invalid object use, and reports leaked objects at vkDestroyDevice() time.
        "VK_LAYER_LUNARG_parameter_validation", // Validates parameters of all API calls, checking for out-of-range/invalid enums, etc.
        //"VK_LAYER_LUNARG_screenshot",
        "VK_LAYER_LUNARG_swapchain", // Validates that swap chains are set up correctly.
        "VK_LAYER_GOOGLE_threading", // Checks multithreading API calls for validity.
        "VK_LAYER_GOOGLE_unique_objects", // Forces all objects to have unique handle values. Must be the last layer loaded (closest to the driver) to be useful.
        //"VK_LAYER_LUNARG_vktrace",
        //"VK_LAYER_RENDERDOC_Capture",
    };
    uint32_t desiredInstanceLayerCount = sizeof(desiredInstanceLayerNames) / sizeof(desiredInstanceLayerNames[0]);
    for(uint32_t iLayer=0; iLayer<desiredInstanceLayerCount; iLayer+=1) {
        VkBool32 foundLayer = VK_FALSE;
        for(uint32_t iInstanceLayer=0; iInstanceLayer<instanceLayerCount; iInstanceLayer+=1) {
            if (0 == strcmp(desiredInstanceLayerNames[iLayer], instanceLayerProperties[iInstanceLayer].layerName)) {
                foundLayer = VK_TRUE;
                break;
            }
        }
        if (!foundLayer) {
            fprintf(stderr, "Support for requested instance layer '%s' could not be found.\n",
                desiredInstanceLayerNames[iLayer]);
            return 0;
        }
    }

    const VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "Vulkswagen",
        .applicationVersion = 0x1000,
        .pEngineName = "Zombo",
        .engineVersion = 0x1001,
        .apiVersion = VK_MAKE_VERSION(1,0,0),
    };

    const VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &applicationInfo,
        .enabledLayerCount = desiredInstanceLayerCount,
        .ppEnabledLayerNames = desiredInstanceLayerNames,
        .enabledExtensionCount = instanceExtensionCount,
        .ppEnabledExtensionNames = instanceExtensionNames,
    };

    VkInstance instance;
    VkAllocationCallbacks *allocationCallbacks = NULL;
    VULKAN_CHECK( vkCreateInstance(&instanceCreateInfo, allocationCallbacks, &instance) );
    printf("Created Vulkan instance with extensions:\n");
    for(uint32_t iExt=0; iExt<instanceCreateInfo.enabledExtensionCount; iExt+=1) {
        printf("- %s\n", instanceCreateInfo.ppEnabledExtensionNames[iExt]);
    }
    printf("and instance layers:\n");
    for(uint32_t iLayer=0; iLayer<instanceCreateInfo.enabledLayerCount; iLayer+=1) {
        printf("- %s\n", instanceCreateInfo.ppEnabledLayerNames[iLayer]);
    }
    
    // Query Vulkan physical devices, queues, and queue families.
    uint32_t physicalDeviceCount = 0;
    VULKAN_CHECK( vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, NULL) );
    VkPhysicalDevice *physicalDevices = (VkPhysicalDevice*)malloc(physicalDeviceCount * sizeof(VkPhysicalDevice));
    VULKAN_CHECK( vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices) );
    VkPhysicalDevice physicalDevice = NULL;
    VkPhysicalDeviceProperties deviceProps;
    for(uint32_t iPD=0; iPD<physicalDeviceCount; iPD+=1) {
        physicalDevice = physicalDevices[iPD];
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);
        printf("Physical device #%u: '%s', API version %u.%u.%u\n",
            iPD,
            deviceProps.deviceName,
            VK_VERSION_MAJOR(deviceProps.apiVersion),
            VK_VERSION_MINOR(deviceProps.apiVersion),
            VK_VERSION_PATCH(deviceProps.apiVersion));
        // TODO: choose the best device, if more than one is found. For now, we just pick the first one we find.
    }

    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

    VkPhysicalDeviceFeatures physicalDeviceFeaturesAll;
    vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeaturesAll);

    uint32_t deviceLayerCount = 0;
    VULKAN_CHECK( vkEnumerateDeviceLayerProperties(physicalDevice, &deviceLayerCount, NULL) );
    VkLayerProperties *deviceLayerProperties = (VkLayerProperties*)malloc(deviceLayerCount * sizeof(VkLayerProperties) );
    VULKAN_CHECK( vkEnumerateDeviceLayerProperties(physicalDevice, &deviceLayerCount, deviceLayerProperties) );
    char **desiredDeviceLayerNames = (char**)malloc( (desiredInstanceLayerCount+1)*sizeof(char**));
    memcpy(desiredDeviceLayerNames, desiredInstanceLayerNames, desiredInstanceLayerCount*sizeof(char*));
    uint32_t desiredDeviceLayerCount = desiredInstanceLayerCount;
    // standard_validation is a meta-layer that loads a bunch of the other validation layers. But it seems to
    // fail if the underlying layers aren't loaded at instance creation time, so...
    //desiredDeviceLayerNames[desiredDeviceLayerCount++] = "VK_LAYER_LUNARG_standard_validation";
    for(uint32_t iLayer=0; iLayer<desiredInstanceLayerCount; iLayer+=1) {
        VkBool32 foundLayer = VK_FALSE;
        for(uint32_t iDeviceLayer=0; iDeviceLayer<deviceLayerCount; iDeviceLayer+=1) {
            if (0 == strcmp(desiredDeviceLayerNames[iLayer], deviceLayerProperties[iDeviceLayer].layerName)) {
                foundLayer = VK_TRUE;
                break;
            }
        }
        if (!foundLayer) {
            fprintf(stderr, "Support for requested device layer '%s' could not be found.\n",
                desiredDeviceLayerNames[iLayer]);
            return 0;
        }
    }

    uint32_t deviceExtensionCount = 0;
    VULKAN_CHECK( vkEnumerateDeviceExtensionProperties(physicalDevice, layerName, &deviceExtensionCount, NULL) );
    VkExtensionProperties *deviceExtensionProperties = (VkExtensionProperties*)malloc(deviceExtensionCount*sizeof(VkExtensionProperties));
    VULKAN_CHECK( vkEnumerateDeviceExtensionProperties(physicalDevice, layerName, &deviceExtensionCount, deviceExtensionProperties) );
    const char **deviceExtensionNames = (const char**)malloc(deviceExtensionCount * sizeof(const char**));
    for(uint32_t iExt=0; iExt<deviceExtensionCount; iExt+=1) {
        deviceExtensionNames[iExt] = deviceExtensionProperties[iExt].extensionName;
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);
    VkQueueFamilyProperties *queueFamilyProperties = (VkQueueFamilyProperties*)malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties);
    VkDeviceQueueCreateInfo *deviceQueueCreateInfos = (VkDeviceQueueCreateInfo*)malloc(queueFamilyCount * sizeof(VkDeviceQueueCreateInfo));
    for(uint32_t iQF=0; iQF<queueFamilyCount; iQF+=1) {
        float *queuePriorities = (float*)malloc(queueFamilyProperties[iQF].queueCount * sizeof(float));
        for(uint32_t iQ=0; iQ<queueFamilyProperties[iQF].queueCount; ++iQ) {
            queuePriorities[iQ] = 1.0f;
        }
        deviceQueueCreateInfos[iQF].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        deviceQueueCreateInfos[iQF].pNext = NULL;
        deviceQueueCreateInfos[iQF].flags = 0;
        deviceQueueCreateInfos[iQF].queueFamilyIndex = iQF;
        deviceQueueCreateInfos[iQF].queueCount = queueFamilyProperties[iQF].queueCount;
        deviceQueueCreateInfos[iQF].pQueuePriorities = queuePriorities;
    }

    // Create Vulkan logical device
    const VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueCreateInfoCount = queueFamilyCount,
        .pQueueCreateInfos = deviceQueueCreateInfos,
        .enabledLayerCount = desiredDeviceLayerCount,
        .ppEnabledLayerNames = (const char* const*)desiredDeviceLayerNames,
        .enabledExtensionCount = deviceExtensionCount,
        .ppEnabledExtensionNames = deviceExtensionNames,
        .pEnabledFeatures = NULL, // &physicalDeviceFeaturesAll,
    };
    VkDevice device;
    VULKAN_CHECK( vkCreateDevice(physicalDevice, &deviceCreateInfo, allocationCallbacks, &device) );
    printf("Created Vulkan logical device with extensions:\n");
    for(uint32_t iExt=0; iExt<deviceCreateInfo.enabledExtensionCount; iExt+=1) {
        printf("- %s\n", deviceCreateInfo.ppEnabledExtensionNames[iExt]);
    }
    printf("and device layers:\n");
    for(uint32_t iLayer=0; iLayer<deviceCreateInfo.enabledLayerCount; iLayer+=1) {
        printf("- %s\n", deviceCreateInfo.ppEnabledLayerNames[iLayer]);
    }

    // Set up debug report callback
    PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = 
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = 
        (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    const VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
        .pfnCallback = debugReportCallbackFunc,
        .pUserData = NULL,
    };
    VkDebugReportCallbackEXT debugReportCallback = VK_NULL_HANDLE;
    VULKAN_CHECK( CreateDebugReportCallback(instance, &debugReportCallbackCreateInfo, allocationCallbacks, &debugReportCallback) );

    uint32_t queueFamilyIndex = 0;
    VkQueue *queues = (VkQueue*)malloc(queueFamilyProperties[queueFamilyIndex].queueCount * sizeof(VkQueue));
    uint32_t graphicsQueueIndex = 0, presentQueueIndex = 0;
    for(uint32_t iQ=0; iQ<queueFamilyProperties[queueFamilyIndex].queueCount; iQ+=1) {
        vkGetDeviceQueue(device, queueFamilyIndex, iQ, &queues[iQ]);
    }
    if (0 == (queueFamilyProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
        fprintf(stderr, "ERROR: Queue family does not support graphics.\n");
        return -1;
    }
    // Wraps vkGetPhysicalDevice*PresentationSupportKHR()
    if (!glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice, queueFamilyIndex)) {
        fprintf(stderr, "ERROR: Queue family does not support presentation.\n");
        return -1;
    }

    // Create command buffer pool & command buffers
    const VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // allows reseting individual command buffers from this pool
        .queueFamilyIndex = 0,
    };
    VkCommandPool commandPool;
    VULKAN_CHECK( vkCreateCommandPool(device, &commandPoolCreateInfo, allocationCallbacks, &commandPool) );

    const VkCommandBufferAllocateInfo commandBufferAllocateInfoPrimary = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmdBufSetup, cmdBufDraw;
    VULKAN_CHECK( vkAllocateCommandBuffers(device, &commandBufferAllocateInfoPrimary, &cmdBufSetup) );
    VULKAN_CHECK( vkAllocateCommandBuffers(device, &commandBufferAllocateInfoPrimary, &cmdBufDraw) );

    // Record the setup command buffer
    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = NULL;
    commandBufferBeginInfo.flags = 0;
    commandBufferBeginInfo.pInheritanceInfo = NULL; // must be non-NULL for secondary command buffers
    VULKAN_CHECK( vkBeginCommandBuffer(cmdBufSetup, &commandBufferBeginInfo) );

    // Create GLFW window, Vulken window surface, and swapchain
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(kWindowWidthDefault, kWindowHeightDefault, "Vulkswagen", NULL, NULL);

    VkSurfaceKHR surface;
    VULKAN_CHECK( glfwCreateWindowSurface(instance, window, allocationCallbacks, &surface) ); // wraps vkCreate*SurfaceKHR() for the current platform

    // Iterate over each queue to learn whether it supports presenting:
    VkBool32 *supportsPresent = (VkBool32 *)malloc(queueFamilyCount * sizeof(VkBool32));
    for (uint32_t iQF = 0; iQF < queueFamilyCount; iQF += 1) {
        VULKAN_CHECK( vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, iQF, surface, &supportsPresent[iQF]) );
    }
    assert(supportsPresent[0]);
    free(supportsPresent);

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VULKAN_CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities) );
    VkExtent2D swapchainExtent;
    if (surfaceCapabilities.currentExtent.width == (uint32_t)-1) {
        assert(surfaceCapabilities.currentExtent.height == (uint32_t)-1);
        swapchainExtent.width = kWindowWidthDefault;
        swapchainExtent.height = kWindowHeightDefault;
    } else {
        swapchainExtent = surfaceCapabilities.currentExtent;
        if (	swapchainExtent.width  != kWindowWidthDefault
            ||	swapchainExtent.height != kWindowHeightDefault) {
            // TODO(cort): update rendering dimensions to match swap chain size. For now, assume this never happens.
            assert(0);
        }
    }
    uint32_t deviceSurfaceFormatCount = 0;
    VULKAN_CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &deviceSurfaceFormatCount, NULL) );
    VkSurfaceFormatKHR *deviceSurfaceFormats = (VkSurfaceFormatKHR*)malloc(deviceSurfaceFormatCount * sizeof(VkSurfaceFormatKHR));
    VULKAN_CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &deviceSurfaceFormatCount, deviceSurfaceFormats) );
    VkFormat surfaceColorFormat;
    if (deviceSurfaceFormatCount == 1 && deviceSurfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
        // No preferred format.
        surfaceColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    } else {
        assert(deviceSurfaceFormatCount >= 1);
        surfaceColorFormat = deviceSurfaceFormats[0].format;
    }
    VkColorSpaceKHR surfaceColorSpace = deviceSurfaceFormats[0].colorSpace;

    uint32_t deviceSurfacePresentModeCount = 0;
    VULKAN_CHECK( vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &deviceSurfacePresentModeCount, NULL) );
    VkPresentModeKHR *deviceSurfacePresentModes = (VkPresentModeKHR*)malloc(deviceSurfacePresentModeCount * sizeof(VkPresentModeKHR));
    VULKAN_CHECK( vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &deviceSurfacePresentModeCount, deviceSurfacePresentModes) );
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR; // TODO(cort): make sure this mode is supported, or pick a different one?

    uint32_t desiredSwapchainImageCount = surfaceCapabilities.minImageCount+1;
    if (	surfaceCapabilities.maxImageCount > 0
        &&	desiredSwapchainImageCount > surfaceCapabilities.maxImageCount) {
        desiredSwapchainImageCount = surfaceCapabilities.maxImageCount;
    }
    VkSurfaceTransformFlagBitsKHR preTransform;
    if (0 != (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)) {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        preTransform = surfaceCapabilities.currentTransform;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = NULL;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = desiredSwapchainImageCount;
    swapchainCreateInfo.imageFormat = surfaceColorFormat;
    swapchainCreateInfo.imageColorSpace = surfaceColorSpace;
    swapchainCreateInfo.imageExtent.width = swapchainExtent.width;
    swapchainCreateInfo.imageExtent.height = swapchainExtent.height;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = preTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.queueFamilyIndexCount = 0;
    swapchainCreateInfo.pQueueFamilyIndices = NULL;
    swapchainCreateInfo.presentMode = swapchainPresentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateSwapchainKHR(device, &swapchainCreateInfo, allocationCallbacks, &swapchain) );

    uint32_t swapchainImageCount = 0;
    VULKAN_CHECK( vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL) );
    VkImage *swapchainImages = (VkImage*)malloc(swapchainImageCount * sizeof(VkImage));
    VULKAN_CHECK( vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages) );

    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = NULL;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.format = surfaceColorFormat;
    imageViewCreateInfo.components = {};
    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    imageViewCreateInfo.subresourceRange = {};
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.image = VK_NULL_HANDLE; // filled in below
    VkImageView *swapchainImageViews = (VkImageView*)malloc(swapchainImageCount * sizeof(VkImageView));
    for(uint32_t iSCI=0; iSCI<swapchainImageCount; iSCI+=1) {
        imageViewCreateInfo.image = swapchainImages[iSCI];
        // Render loop will expect image to have been used before and in
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR layout and will change to
        // COLOR_ATTACHMENT_OPTIMAL, so init the image to that state.
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;
        setImageLayout(cmdBufSetup, imageViewCreateInfo.image, subresourceRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0);
        VULKAN_CHECK( vkCreateImageView(device, &imageViewCreateInfo, allocationCallbacks, &swapchainImageViews[iSCI]) );
    }
    uint32_t swapchainCurrentBufferIndex = 0;

    // Create depth buffer
    VkFormat surfaceDepthFormat = VK_FORMAT_D16_UNORM;
    VkImageCreateInfo imageCreateInfoDepth = {};
    imageCreateInfoDepth.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfoDepth.pNext = NULL;
    imageCreateInfoDepth.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfoDepth.format = surfaceDepthFormat;
    imageCreateInfoDepth.extent = {};
    imageCreateInfoDepth.extent.width = kWindowWidthDefault;
    imageCreateInfoDepth.extent.height = kWindowHeightDefault;
    imageCreateInfoDepth.extent.depth = 1;
    imageCreateInfoDepth.mipLevels = 1;
    imageCreateInfoDepth.arrayLayers = 1;
    imageCreateInfoDepth.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfoDepth.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfoDepth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageCreateInfoDepth.flags = 0;
    VkImage imageDepth = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateImage(device, &imageCreateInfoDepth, allocationCallbacks, &imageDepth) );
    VkMemoryRequirements memoryRequirementsDepth;
    vkGetImageMemoryRequirements(device, imageDepth, &memoryRequirementsDepth);
    VkMemoryAllocateInfo memoryAllocateInfoDepth = {};
    memoryAllocateInfoDepth.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfoDepth.pNext = NULL;
    memoryAllocateInfoDepth.allocationSize = memoryRequirementsDepth.size;
    memoryAllocateInfoDepth.memoryTypeIndex = 0; // filled in below
    VkBool32 foundMemoryTypeDepth = getMemoryTypeFromProperties(&physicalDeviceMemoryProperties,
        memoryRequirementsDepth.memoryTypeBits, 0, &memoryAllocateInfoDepth.memoryTypeIndex);
    assert(foundMemoryTypeDepth);
    VkDeviceMemory imageDepthMemory = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateMemory(device, &memoryAllocateInfoDepth, allocationCallbacks, &imageDepthMemory) );
    VkDeviceSize imageDepthMemoryOffset = 0;
    VULKAN_CHECK( vkBindImageMemory(device, imageDepth, imageDepthMemory, imageDepthMemoryOffset) );
    VkImageSubresourceRange depthSubresourceRange = {};
    depthSubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthSubresourceRange.baseMipLevel = 0;
    depthSubresourceRange.levelCount = 1;
    depthSubresourceRange.baseArrayLayer = 0;
    depthSubresourceRange.layerCount = 1;
    setImageLayout(cmdBufSetup, imageDepth, depthSubresourceRange,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0);
    VkImageViewCreateInfo imageViewCreateInfoDepth = {};
    imageViewCreateInfoDepth.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfoDepth.pNext = NULL;
    imageViewCreateInfoDepth.image = imageDepth;
    imageViewCreateInfoDepth.format = surfaceDepthFormat;
    imageViewCreateInfoDepth.subresourceRange = {};
    imageViewCreateInfoDepth.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    imageViewCreateInfoDepth.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfoDepth.subresourceRange.levelCount = 1;
    imageViewCreateInfoDepth.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfoDepth.subresourceRange.layerCount = 1;
    imageViewCreateInfoDepth.flags = 0;
    imageViewCreateInfoDepth.viewType = VK_IMAGE_VIEW_TYPE_2D;
    VkImageView imageDepthView;
    VULKAN_CHECK( vkCreateImageView(device, &imageViewCreateInfoDepth, allocationCallbacks, &imageDepthView) );

    // Create vertex buffer
    const float vertices[] = {
        //0,1,2: position  4,5,6: texcoord
        -0.75f, -0.75f, 1.00f,	0.0f, 0.0f, 0.0f,
         0.75f, -0.75f, 1.00f,	1.0f, 0.0f, 0.25f,
        -0.75f,  0.75f, 1.00f,	0.0f, 1.0f, 0.5f,
         0.75f,  0.75f, 1.00f,	1.0f, 1.0f, 0.75f,
    };
    const uint32_t kVertexBufferBindId = 0;
    VkVertexInputBindingDescription vertexInputBindingDescription = {};
    vertexInputBindingDescription.binding = kVertexBufferBindId;
    vertexInputBindingDescription.stride = 3*sizeof(float) + 3*sizeof(float);
    vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] = {};
    vertexInputAttributeDescriptions[0].binding = kVertexBufferBindId;
    vertexInputAttributeDescriptions[0].location = 0;
    vertexInputAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[0].offset = 0;
    vertexInputAttributeDescriptions[1].binding = kVertexBufferBindId;
    vertexInputAttributeDescriptions[1].location = 1;
    vertexInputAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[1].offset = 3*sizeof(float);
    VkBufferCreateInfo bufferCreateInfoVertices = {};
    bufferCreateInfoVertices.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfoVertices.pNext = NULL;
    bufferCreateInfoVertices.size = sizeof(vertices);
    bufferCreateInfoVertices.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferCreateInfoVertices.flags = 0;
    VkBuffer bufferVertices = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateBuffer(device, &bufferCreateInfoVertices, allocationCallbacks, &bufferVertices) );
    VkMemoryRequirements memoryRequirementsVertices;
    vkGetBufferMemoryRequirements(device, bufferVertices, &memoryRequirementsVertices);
    VkMemoryAllocateInfo memoryAllocateInfoVertices = {};
    memoryAllocateInfoVertices.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfoVertices.pNext = NULL;
    memoryAllocateInfoVertices.allocationSize = memoryRequirementsVertices.size;
    memoryAllocateInfoVertices.memoryTypeIndex = 0;
    VkBool32 foundMemoryTypeVertices = getMemoryTypeFromProperties(&physicalDeviceMemoryProperties,
        memoryRequirementsVertices.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        &memoryAllocateInfoVertices.memoryTypeIndex);
    assert(foundMemoryTypeVertices);
    VkDeviceMemory bufferVerticesMemory = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateMemory(device, &memoryAllocateInfoVertices, allocationCallbacks, &bufferVerticesMemory) );
    VkDeviceSize bufferVerticesMemoryOffset = 0;
    VkMemoryMapFlags bufferVerticesMemoryMapFlags = 0;
    void *bufferVerticesMapped = NULL;
    VULKAN_CHECK( vkMapMemory(device, bufferVerticesMemory, bufferVerticesMemoryOffset,
        memoryAllocateInfoVertices.allocationSize, bufferVerticesMemoryMapFlags, &bufferVerticesMapped) );
    memcpy(bufferVerticesMapped, vertices, sizeof(vertices));
    //vkUnmapMemory(device, bufferVerticesMapped); // TODO: see if validation layer catches this error
    vkUnmapMemory(device, bufferVerticesMemory);
    VULKAN_CHECK( vkBindBufferMemory(device, bufferVertices, bufferVerticesMemory, bufferVerticesMemoryOffset) );
    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
    pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInputStateCreateInfo.pNext = NULL;
    pipelineVertexInputStateCreateInfo.flags = 0;
    pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
    pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = sizeof(vertexInputAttributeDescriptions) / sizeof(vertexInputAttributeDescriptions[0]);
    pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescriptions;

    // Create push constants
    struct {
        float time[4]; // .x=seconds, .yzw=???
    } pushConstants = {0,0,0,0};
    assert(sizeof(pushConstants) <= deviceProps.limits.maxPushConstantsSize);
    uint64_t counterStart = zomboClockTicks();
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(pushConstants);

    // Create Vulkan descriptor layout & pipeline layout
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
    descriptorSetLayoutBinding.binding = 0;
    descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorSetLayoutBinding.descriptorCount = kDemoTextureCount;
    descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayoutBinding.pImmutableSamplers = NULL;
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = NULL;
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, allocationCallbacks, &descriptorSetLayout) );
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = NULL;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, allocationCallbacks, &pipelineLayout) );

    // Create render pass
    VkAttachmentDescription attachmentDescriptions[2];
    attachmentDescriptions[0] = {};
    attachmentDescriptions[0].flags = 0;
    attachmentDescriptions[0].format = surfaceColorFormat;
    attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentDescriptions[1] = {};
    attachmentDescriptions[1].flags = 0;
    attachmentDescriptions[1].format = surfaceDepthFormat;
    attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference attachmentReferenceColor = {};
    attachmentReferenceColor.attachment = 0;
    attachmentReferenceColor.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference attachmentReferenceDepth = {};
    attachmentReferenceDepth.attachment = 1;
    attachmentReferenceDepth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpassDescription = {};
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = NULL;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &attachmentReferenceColor;
    subpassDescription.pResolveAttachments = NULL;
    subpassDescription.pDepthStencilAttachment = &attachmentReferenceDepth;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = NULL;
    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = NULL;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.attachmentCount = sizeof(attachmentDescriptions) / sizeof(attachmentDescriptions[0]);
    renderPassCreateInfo.pAttachments = attachmentDescriptions;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 0;
    renderPassCreateInfo.pDependencies = NULL;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateRenderPass(device, &renderPassCreateInfo, allocationCallbacks, &renderPass) );

    // Load shaders
    VkShaderModule vertexShaderModule   = readSpvFile(device, allocationCallbacks, "tri.vert.spv");
    assert(vertexShaderModule != VK_NULL_HANDLE);
    VkShaderModule fragmentShaderModule = readSpvFile(device, allocationCallbacks, "tri.frag.spv");
    assert(fragmentShaderModule != VK_NULL_HANDLE);

    // Load textures, create sampler and image view
    const uint32_t kTextureLayerCount = 32;
    int texWidth, texHeight, texComp;
    {
        uint32_t *pixels = (uint32_t*)stbi_load("trevor/trevor-0.png", &texWidth, &texHeight, &texComp, 4);
        stbi_image_free(pixels);
    }
    VkFormat surfaceTextureFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormatProperties textureFormatProperties = {0};
    vkGetPhysicalDeviceFormatProperties(physicalDevice, surfaceTextureFormat, &textureFormatProperties);
    if (0 == (textureFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        // TODO(cort): use a staging pass to transfer the linear source image to a tiled format.
        fprintf(stderr, "ERROR: linear texture sampling is not supported on this hardware.\n");
        exit(1);
    }
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = NULL;
    imageCreateInfo.flags = 0;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = surfaceTextureFormat;
    imageCreateInfo.extent = {};
    imageCreateInfo.extent.width = texWidth;
    imageCreateInfo.extent.height = texHeight;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = kTextureLayerCount;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = NULL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage textureImage;
    VkImageFormatProperties imageFormatProperties = {0};
    VULKAN_CHECK( vkGetPhysicalDeviceImageFormatProperties(physicalDevice,
        imageCreateInfo.format, imageCreateInfo.imageType, imageCreateInfo.tiling,
        imageCreateInfo.usage, 0, &imageFormatProperties) );
    assert(kTextureLayerCount <= imageFormatProperties.maxArrayLayers);
    VULKAN_CHECK( vkCreateImage(device, &imageCreateInfo, allocationCallbacks, &textureImage) );
    VkMemoryRequirements memoryRequirements = {0};
    vkGetImageMemoryRequirements(device, textureImage, &memoryRequirements);
    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = NULL;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = 0; // filled in below
    VkBool32 foundMemoryType = getMemoryTypeFromProperties(&physicalDeviceMemoryProperties,
        memoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &memoryAllocateInfo.memoryTypeIndex);
    assert(foundMemoryType);
    VkDeviceMemory textureDeviceMemory = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateMemory(device, &memoryAllocateInfo, allocationCallbacks, &textureDeviceMemory) );
    VkDeviceSize textureMemoryOffset = 0;
    VULKAN_CHECK( vkBindImageMemory(device, textureImage, textureDeviceMemory, textureMemoryOffset) );
    VkImageSubresourceRange textureImageSubresourceRange = {};
    textureImageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    textureImageSubresourceRange.baseMipLevel = 0;
    textureImageSubresourceRange.levelCount = 1;
    textureImageSubresourceRange.baseArrayLayer = 0;
    textureImageSubresourceRange.layerCount = kTextureLayerCount;
    setImageLayout(cmdBufSetup, textureImage, textureImageSubresourceRange,
        imageCreateInfo.initialLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0);
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext = NULL;
    samplerCreateInfo.flags = 0;
    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 1;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    VkSampler sampler;
    VULKAN_CHECK( vkCreateSampler(device, &samplerCreateInfo, allocationCallbacks, &sampler) );
    VkImageViewCreateInfo textureImageViewCreateInfo = {};
    textureImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    textureImageViewCreateInfo.pNext = NULL;
    textureImageViewCreateInfo.flags = 0;
    textureImageViewCreateInfo.image = VK_NULL_HANDLE; // filled in below
    textureImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    textureImageViewCreateInfo.format = surfaceTextureFormat;
    textureImageViewCreateInfo.components = {};
    textureImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    textureImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    textureImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    textureImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    textureImageViewCreateInfo.subresourceRange = {};
    textureImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    textureImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    textureImageViewCreateInfo.subresourceRange.levelCount = 1;
    textureImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    textureImageViewCreateInfo.subresourceRange.layerCount = kTextureLayerCount;
    VkImageView textureImageViews[kDemoTextureCount];
    for(uint32_t iTexture=0; iTexture<kDemoTextureCount; iTexture+=1) {
        textureImageViewCreateInfo.image = textureImage;
        VULKAN_CHECK( vkCreateImageView(device, &textureImageViewCreateInfo, allocationCallbacks, &textureImageViews[iTexture]) );
    }
    
    // Load individual texture layers into staging textures, and copy them into the final texture.
    VkImageCreateInfo stagingImageCreateInfo = {};
    stagingImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    stagingImageCreateInfo.pNext = NULL;
    stagingImageCreateInfo.flags = 0;
    stagingImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    stagingImageCreateInfo.format = surfaceTextureFormat;
    stagingImageCreateInfo.extent = {};
    stagingImageCreateInfo.extent.width = texWidth;
    stagingImageCreateInfo.extent.height = texHeight;
    stagingImageCreateInfo.extent.depth = 1;
    stagingImageCreateInfo.mipLevels = 1;
    stagingImageCreateInfo.arrayLayers = 1;
    stagingImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    stagingImageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    stagingImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    stagingImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    stagingImageCreateInfo.queueFamilyIndexCount = 0;
    stagingImageCreateInfo.pQueueFamilyIndices = NULL;
    stagingImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    VkImage *stagingTextureImages = (VkImage*)malloc(kTextureLayerCount * sizeof(VkImage));
    for(uint32_t iLayer=0; iLayer<kTextureLayerCount; iLayer += 1) {
        VULKAN_CHECK( vkCreateImage(device, &stagingImageCreateInfo, allocationCallbacks, &stagingTextureImages[iLayer]) );
        VkMemoryRequirements memoryRequirements = {0};
        vkGetImageMemoryRequirements(device, stagingTextureImages[iLayer], &memoryRequirements);
        VkMemoryAllocateInfo memoryAllocateInfo = {};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext = NULL;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = 0; // filled in below
        VkBool32 foundMemoryType = getMemoryTypeFromProperties(&physicalDeviceMemoryProperties,
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &memoryAllocateInfo.memoryTypeIndex);
        assert(foundMemoryType);
        VkDeviceMemory textureDeviceMemory = VK_NULL_HANDLE;
        VULKAN_CHECK( vkAllocateMemory(device, &memoryAllocateInfo, allocationCallbacks, &textureDeviceMemory) );
        VkDeviceSize textureMemoryOffset = 0;
        VULKAN_CHECK( vkBindImageMemory(device, stagingTextureImages[iLayer], textureDeviceMemory, textureMemoryOffset) );

        VkImageSubresource textureImageSubresource = {};
        textureImageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        textureImageSubresource.mipLevel = 0;
        textureImageSubresource.arrayLayer = 0;
        VkSubresourceLayout subresourceLayout = {};
        VkMemoryMapFlags memoryMapFlags = 0;
        vkGetImageSubresourceLayout(device, stagingTextureImages[iLayer], &textureImageSubresource, &subresourceLayout);
        void *mappedTextureData = NULL;
        VULKAN_CHECK( vkMapMemory(device, textureDeviceMemory, textureMemoryOffset,
            memoryRequirements.size, memoryMapFlags, &mappedTextureData) );
        char imagePath[128];
        zomboSnprintf(imagePath, 127, "trevor/trevor-%u.png", iLayer);
        imagePath[127] = 0;
        int width=0,height=0,comp=0;
        uint32_t *pixels = (uint32_t*)stbi_load(imagePath, &width, &height, &comp, 4);
        for(int32_t iY=0; iY<texHeight; iY+=1) {
            uint32_t *row = (uint32_t*)( (char*)mappedTextureData + iY*subresourceLayout.rowPitch );
            for(int32_t iX=0; iX<texWidth; iX+=1) {
                row[iX] = pixels[iY*texWidth+iX];
            }
        }
        stbi_image_free(pixels);
        vkUnmapMemory(device, textureDeviceMemory);
        VkImageSubresourceRange stagingImageSubresourceRange = {};
        stagingImageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        stagingImageSubresourceRange.baseMipLevel = 0;
        stagingImageSubresourceRange.levelCount = 1;
        stagingImageSubresourceRange.baseArrayLayer = 0;
        stagingImageSubresourceRange.layerCount = 1;
        setImageLayout(cmdBufSetup, stagingTextureImages[iLayer], stagingImageSubresourceRange,
            stagingImageCreateInfo.initialLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0);

        VkImageCopy copyRegion = {};
        copyRegion.srcSubresource = {};
        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.baseArrayLayer = 0;
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.srcSubresource.mipLevel = 0;
        copyRegion.srcOffset = {0,0,0};
        copyRegion.dstSubresource = {};
        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.baseArrayLayer = iLayer;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.dstSubresource.mipLevel = 0;
        copyRegion.dstOffset = {0,0,0};
        copyRegion.extent = stagingImageCreateInfo.extent;
        vkCmdCopyImage(cmdBufSetup,
            stagingTextureImages[iLayer], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    }
    const VkImageLayout textureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    setImageLayout(cmdBufSetup, textureImage, textureImageSubresourceRange,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, textureImageLayout, 0);

    // Create Vulkan pipeline & graphics state
    VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE] = {}; // filled in below
    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {};
    pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateCreateInfo.pNext = NULL;
    pipelineDynamicStateCreateInfo.flags = 0;
    pipelineDynamicStateCreateInfo.dynamicStateCount = 0; // filled in below
    pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStateEnables; // filled in below
    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {};
    pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyStateCreateInfo.pNext = NULL;
    pipelineInputAssemblyStateCreateInfo.flags = 0;
    pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
    pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipelineRasterizationStateCreateInfo.pNext = NULL;
    pipelineRasterizationStateCreateInfo.flags = 0;
    pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
    VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
    pipelineColorBlendAttachmentState.colorWriteMask = 0xF;
    pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
    pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipelineColorBlendStateCreateInfo.pNext = NULL;
    pipelineColorBlendStateCreateInfo.flags = 0;
    pipelineColorBlendStateCreateInfo.attachmentCount = 1;
    pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;
    VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
    pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipelineViewportStateCreateInfo.pNext = NULL;
    pipelineViewportStateCreateInfo.flags = 0;
    pipelineViewportStateCreateInfo.viewportCount = 1;
    pipelineViewportStateCreateInfo.scissorCount = 1;
    dynamicStateEnables[pipelineDynamicStateCreateInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamicStateEnables[pipelineDynamicStateCreateInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilCreateInfo = {};
    pipelineDepthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipelineDepthStencilCreateInfo.pNext = NULL;
    pipelineDepthStencilCreateInfo.flags = 0;
    pipelineDepthStencilCreateInfo.depthTestEnable = VK_TRUE;
    pipelineDepthStencilCreateInfo.depthWriteEnable = VK_TRUE;
    pipelineDepthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineDepthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
    pipelineDepthStencilCreateInfo.back = {};
    pipelineDepthStencilCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
    pipelineDepthStencilCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
    pipelineDepthStencilCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
    pipelineDepthStencilCreateInfo.front = {};
    pipelineDepthStencilCreateInfo.front.failOp = VK_STENCIL_OP_KEEP;
    pipelineDepthStencilCreateInfo.front.passOp = VK_STENCIL_OP_KEEP;
    pipelineDepthStencilCreateInfo.front.compareOp = VK_COMPARE_OP_ALWAYS;
    pipelineDepthStencilCreateInfo.stencilTestEnable = VK_FALSE;
    const VkSampleMask *sampleMask = NULL;
    VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
    pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultisampleStateCreateInfo.pNext = NULL;
    pipelineMultisampleStateCreateInfo.flags = 0;
    pipelineMultisampleStateCreateInfo.pSampleMask = sampleMask;
    pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineMultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCacheCreateInfo.pNext = NULL;
    pipelineCacheCreateInfo.flags = 0;
    pipelineCacheCreateInfo.initialDataSize = 0;
    pipelineCacheCreateInfo.pInitialData = NULL;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreatePipelineCache(device, &pipelineCacheCreateInfo, allocationCallbacks, &pipelineCache) );
    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[2] = {};
    pipelineShaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineShaderStageCreateInfos[0].module = vertexShaderModule;
    pipelineShaderStageCreateInfos[0].pName = "main";
    pipelineShaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineShaderStageCreateInfos[1].module = fragmentShaderModule;
    pipelineShaderStageCreateInfos[1].pName = "main";
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.pNext = NULL;
    graphicsPipelineCreateInfo.flags = 0;
    graphicsPipelineCreateInfo.layout = pipelineLayout;
    graphicsPipelineCreateInfo.stageCount = sizeof(pipelineShaderStageCreateInfos) / sizeof(pipelineShaderStageCreateInfos[0]);
    graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfos;
    graphicsPipelineCreateInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
    graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
    graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
    graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
    graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
    graphicsPipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilCreateInfo;
    graphicsPipelineCreateInfo.renderPass = renderPass;
    graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
    VkPipeline pipelineGraphics = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, allocationCallbacks, &pipelineGraphics) );
    // These get destroyed now, I guess? The pipeline must keep a reference internally?
    vkDestroyPipelineCache(device, pipelineCache, allocationCallbacks);
    vkDestroyShaderModule(device, vertexShaderModule, allocationCallbacks);
    vkDestroyShaderModule(device, fragmentShaderModule, allocationCallbacks);

    // Create Vulkan descriptor pool and descriptor set
    VkDescriptorPoolSize descriptorPoolSize = {};
    descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorPoolSize.descriptorCount = kDemoTextureCount;
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = NULL;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, allocationCallbacks, &descriptorPool) );
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = NULL;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet) );
    VkDescriptorImageInfo descriptorImageInfos[kDemoTextureCount] = {0};
    for(uint32_t iTexture=0; iTexture<kDemoTextureCount; iTexture += 1) {
        descriptorImageInfos[iTexture].sampler = sampler;
        descriptorImageInfos[iTexture].imageView = textureImageViews[iTexture];
        descriptorImageInfos[iTexture].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext = NULL;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.descriptorCount = kDemoTextureCount;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pImageInfo = descriptorImageInfos;
    vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);

    // Create framebuffers
    VkImageView attachmentImageViews[2] = {
        VK_NULL_HANDLE, // filled in below
        imageDepthView,
    };
    VkFramebufferCreateInfo framebufferCreateInfo = {};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.pNext = NULL;
    framebufferCreateInfo.flags = 0;
    framebufferCreateInfo.renderPass = renderPass;
    framebufferCreateInfo.attachmentCount = sizeof(attachmentImageViews) / sizeof(attachmentImageViews[0]);
    framebufferCreateInfo.pAttachments = attachmentImageViews;
    framebufferCreateInfo.width = kWindowWidthDefault;
    framebufferCreateInfo.height = kWindowHeightDefault;
    framebufferCreateInfo.layers = 1;
    VkFramebuffer *framebuffers = (VkFramebuffer*)malloc(swapchainImageCount * sizeof(VkFramebuffer));
    for(uint32_t iFB=0; iFB<swapchainImageCount; iFB += 1) {
        attachmentImageViews[0] = swapchainImageViews[iFB];
        VULKAN_CHECK( vkCreateFramebuffer(device, &framebufferCreateInfo, allocationCallbacks, &framebuffers[iFB]) );
    }

    // Submit the setup command buffer
    VULKAN_CHECK( vkEndCommandBuffer(cmdBufSetup) );
    VkSubmitInfo submitInfoSetup = {};
    submitInfoSetup.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfoSetup.pNext = NULL;
    submitInfoSetup.waitSemaphoreCount = 0;
    submitInfoSetup.pWaitSemaphores = NULL;
    submitInfoSetup.pWaitDstStageMask = NULL;
    submitInfoSetup.commandBufferCount = 1;
    submitInfoSetup.pCommandBuffers = &cmdBufSetup;
    submitInfoSetup.signalSemaphoreCount = 0;
    submitInfoSetup.pSignalSemaphores = NULL;
    VkFence submitFence = VK_NULL_HANDLE;
    VULKAN_CHECK( vkQueueSubmit(queues[0], 1, &submitInfoSetup, submitFence) );
    VULKAN_CHECK( vkQueueWaitIdle(queues[0]) );
    vkFreeCommandBuffers(device, commandPool, 1, &cmdBufSetup);
    cmdBufSetup = VK_NULL_HANDLE;

#if 0
    // Set a callback to receive keyboard input
    glfwSetKeyCallback(window, myGlfwKeyCallback);
    // Set callbacks for mouse input
    g_camera = new ZomboLite::CameraMaya(kWindowWidthDefault, kWindowHeightDefault);
    glfwSetMouseButtonCallback(window, myGlfwMouseButtonCallback);
    glfwSetCursorPosCallback(window, myGlfwCursorPosCallback);
    glfwSetWindowSizeCallback(window, myGlfwWindowSizeCallback);
#endif

    uint32_t frameIndex = 0;
    while(!glfwWindowShouldClose(window)) {
        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.pNext = NULL;
        semaphoreCreateInfo.flags = 0;
        VkSemaphore presentCompleteSemaphore = VK_NULL_HANDLE;
        VULKAN_CHECK( vkCreateSemaphore(device, &semaphoreCreateInfo, allocationCallbacks, &presentCompleteSemaphore) );

        // Retrieve the index of the next available swapchain index
        VkFence presentCompleteFence = VK_NULL_HANDLE; // TODO(cort): unused
        uint32_t currentBufferIndex = 0;
        VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, presentCompleteSemaphore,
            presentCompleteFence, &currentBufferIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
        } else if (result == VK_SUBOPTIMAL_KHR) {
            // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
        } else {
            VULKAN_CHECK(result);
        }

        // Draw!
        VkCommandBufferInheritanceInfo cmdBufDrawInheritanceInfo = {};
        cmdBufDrawInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        cmdBufDrawInheritanceInfo.pNext = NULL;
        cmdBufDrawInheritanceInfo.renderPass = VK_NULL_HANDLE;
        cmdBufDrawInheritanceInfo.subpass = 0;
        cmdBufDrawInheritanceInfo.framebuffer = VK_NULL_HANDLE;
        cmdBufDrawInheritanceInfo.occlusionQueryEnable = VK_FALSE;
        cmdBufDrawInheritanceInfo.queryFlags = 0;
        cmdBufDrawInheritanceInfo.pipelineStatistics = 0;
        VkCommandBufferBeginInfo cmdBufDrawBeginInfo = {};
        cmdBufDrawBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufDrawBeginInfo.pNext = NULL;
        cmdBufDrawBeginInfo.flags = 0;
        cmdBufDrawBeginInfo.pInheritanceInfo = &cmdBufDrawInheritanceInfo;

        VULKAN_CHECK( vkBeginCommandBuffer(cmdBufDraw, &cmdBufDrawBeginInfo) );
        VkImageSubresourceRange swapchainSubresourceRange = {};
        swapchainSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchainSubresourceRange.baseMipLevel = 0;
        swapchainSubresourceRange.levelCount = 1;
        swapchainSubresourceRange.baseArrayLayer = 0;
        swapchainSubresourceRange.layerCount = 1;
        setImageLayout(cmdBufDraw, swapchainImages[currentBufferIndex],
            swapchainSubresourceRange,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0);
        // TODO(cort): is a sync point needed here? tri.c submits this layout change as a separate
        // command buffer, then waits for the queue to be idle.

        VkClearValue clearValues[2] = {};
        clearValues[0].color.float32[0] = (float)(frameIndex%256)/255.0f,
        clearValues[0].color.float32[1] = (float)(frameIndex%512)/512.0f,
        clearValues[0].color.float32[2] = (float)(frameIndex%1024)/1023.0f,
        clearValues[0].color.float32[3] = 1.0f;
        clearValues[1].depthStencil.depth = 1.0f;
        clearValues[1].depthStencil.stencil = 0;
        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = NULL;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = framebuffers[currentBufferIndex];
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width  = kWindowWidthDefault;
        renderPassBeginInfo.renderArea.extent.height = kWindowHeightDefault;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;
        vkCmdBeginRenderPass(cmdBufDraw, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdBufDraw, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineGraphics);
        vkCmdBindDescriptorSets(cmdBufDraw, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,1,&descriptorSet, 0,NULL);
        pushConstants.time[0] = (float)( zomboTicksToSeconds(zomboClockTicks() - counterStart) );
        vkCmdPushConstants(cmdBufDraw, pipelineLayout, pushConstantRange.stageFlags,
            pushConstantRange.offset, pushConstantRange.size, &pushConstants);
        VkViewport viewport = {};
        viewport.width  = (float)kWindowWidthDefault;
        viewport.height = (float)kWindowHeightDefault;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBufDraw, 0,1, &viewport);
        VkRect2D scissorRect = {};
        scissorRect.extent.width  = kWindowWidthDefault;
        scissorRect.extent.height = kWindowHeightDefault;
        scissorRect.offset.x = 0;
        scissorRect.offset.y = 0;
        vkCmdSetScissor(cmdBufDraw, 0,1, &scissorRect);
        const VkDeviceSize vertexBufferOffsets[1] = {};
        vkCmdBindVertexBuffers(cmdBufDraw, kVertexBufferBindId,1, &bufferVertices, vertexBufferOffsets);
        vkCmdDraw(cmdBufDraw, 4,1,0,0);

        vkCmdEndRenderPass(cmdBufDraw);
        VkImageMemoryBarrier prePresentBarrier = {};
        prePresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        prePresentBarrier.pNext = NULL;
        prePresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        prePresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        prePresentBarrier.subresourceRange = {};
        prePresentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        prePresentBarrier.subresourceRange.baseMipLevel = 0;
        prePresentBarrier.subresourceRange.levelCount = 1;
        prePresentBarrier.subresourceRange.baseArrayLayer = 0;
        prePresentBarrier.subresourceRange.layerCount = 1;
        prePresentBarrier.image = swapchainImages[currentBufferIndex],
        vkCmdPipelineBarrier(cmdBufDraw, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0,
            NULL, 1, &prePresentBarrier);
        VULKAN_CHECK( vkEndCommandBuffer(cmdBufDraw) );
        VkFence nullFence = VK_NULL_HANDLE;
        const VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        VkSubmitInfo submitInfoDraw = {};
        submitInfoDraw.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfoDraw.pNext = NULL;
        submitInfoDraw.waitSemaphoreCount = 1;
        submitInfoDraw.pWaitSemaphores = &presentCompleteSemaphore;
        submitInfoDraw.pWaitDstStageMask = &pipelineStageFlags;
        submitInfoDraw.commandBufferCount = 1;
        submitInfoDraw.pCommandBuffers = &cmdBufDraw;
        submitInfoDraw.signalSemaphoreCount = 0;
        submitInfoDraw.pSignalSemaphores = NULL;
        VULKAN_CHECK( vkQueueSubmit(queues[0], 1, &submitInfoDraw, nullFence) );
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = NULL;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &currentBufferIndex;
        result = vkQueuePresentKHR(queues[0], &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
        } else if (result == VK_SUBOPTIMAL_KHR) {
            // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
        } else {
            VULKAN_CHECK(result);
        }
        VULKAN_CHECK( vkQueueWaitIdle(queues[0]) );

        // glfwSwapBuffers(window); // Not necessary in Vulkan
        glfwPollEvents();
        vkDestroySemaphore(device, presentCompleteSemaphore, allocationCallbacks); // TODO(cort): create/destroy every frame?
        frameIndex += 1;
    }

    vkDeviceWaitIdle(device);
    for(uint32_t iFB=0; iFB<swapchainImageCount; iFB+=1) {
        vkDestroyFramebuffer(device, framebuffers[iFB], allocationCallbacks);
        vkDestroyImageView(device, swapchainImageViews[iFB], allocationCallbacks);
    }
    free(framebuffers);
    free(swapchainImageViews);

    vkDestroyImageView(device, imageDepthView, allocationCallbacks);
    vkFreeMemory(device, imageDepthMemory, allocationCallbacks);
    vkDestroyImage(device, imageDepth, allocationCallbacks);

    vkFreeMemory(device, bufferVerticesMemory, allocationCallbacks);
    vkDestroyBuffer(device, bufferVertices, allocationCallbacks);

    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, allocationCallbacks);
    vkDestroyDescriptorPool(device, descriptorPool, allocationCallbacks);

    vkFreeCommandBuffers(device, commandPool, 1, &cmdBufDraw);
    vkDestroyCommandPool(device, commandPool, allocationCallbacks);

    vkDestroyRenderPass(device, renderPass, allocationCallbacks);

    vkDestroyImage(device, textureImage, allocationCallbacks);
    vkFreeMemory(device, textureDeviceMemory, allocationCallbacks);
    for(uint32_t iTex=0; iTex<kDemoTextureCount; ++iTex) {
        vkDestroyImageView(device, textureImageViews[iTex], allocationCallbacks);
    }
    for(uint32_t iLayer=0; iLayer<kTextureLayerCount; ++iLayer) {
        vkDestroyImage(device, stagingTextureImages[iLayer], allocationCallbacks);
        //vkFreeMemory(device, stagingTextureDeviceMemory, allocationCallbacks); // LEAKED!
    }
    free(stagingTextureImages);

    vkDestroySampler(device, sampler, allocationCallbacks);

    vkDestroyPipelineLayout(device, pipelineLayout, allocationCallbacks);
    vkDestroyPipeline(device, pipelineGraphics, allocationCallbacks);

    vkDestroySwapchainKHR(device, swapchain, allocationCallbacks);
    DestroyDebugReportCallback(instance, debugReportCallback, allocationCallbacks);

    vkDestroyDevice(device, allocationCallbacks);
    vkDestroySurfaceKHR(instance, surface, allocationCallbacks);
    glfwTerminate();
    vkDestroyInstance(instance, allocationCallbacks);
    return 0;
}
