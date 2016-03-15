#ifdef _WIN32
#   include <Windows.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_VULKAN_IMPLEMENTATION
#include "stb_vulkan.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RETVAL_CHECK(expected, expr) do { \
        int err = (expr); \
        if (err != (expected)) { \
            printf("%s(%d): error in %s() -- %s returned %d\n", __FILE__, __LINE__, __FUNCTION__, #expr, err); \
            __debugbreak(); \
        } \
        assert(err == (expected)); \
        __pragma(warning(push)) \
        __pragma(warning(disable:4127)) \
    } while(0) \
    __pragma(warning(pop))
#define VULKAN_CHECK(expr) RETVAL_CHECK(VK_SUCCESS, expr)

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
    static_assert(sizeof(memoryTypeBits)*8 == VK_MAX_MEMORY_TYPES, "expected VK_MAX_MEMORY_TYPES=32");
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
        VkAccessFlagBits srcAccessMask) {
    VkImageMemoryBarrier imgMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = 0, // overwritten below
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange = subresourceRange,
    };
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

    const VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "Vulkswagen",
        .applicationVersion = 0x1000,
        .pEngineName = "Zombo",
        .engineVersion = 0x1001,
        .apiVersion = VK_MAKE_VERSION(1,0,0),
    };
    const stbvk_context_create_info contextCreateInfo = {
        .allocation_callbacks = NULL,
        .enable_standard_validation_layers = VK_TRUE,
        .application_info = &applicationInfo,
    };
    stbvk_context context;
    stbvk_init_context(&contextCreateInfo, &context);

    // Set up debug report callback
    PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(context.instance, "vkCreateDebugReportCallbackEXT");
    PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback =
        (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(context.instance, "vkDestroyDebugReportCallbackEXT");
    const VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
        .pfnCallback = debugReportCallbackFunc,
        .pUserData = NULL,
    };
    VkDebugReportCallbackEXT debugReportCallback = VK_NULL_HANDLE;
    VULKAN_CHECK( CreateDebugReportCallback(context.instance, &debugReportCallbackCreateInfo, context.allocation_callbacks, &debugReportCallback) );

    // Wraps vkGetPhysicalDevice*PresentationSupportKHR()
    if (!glfwGetPhysicalDevicePresentationSupport(context.instance, context.physical_device, context.queue_family_index)) {
        fprintf(stderr, "ERROR: Queue family does not support presentation.\n");
        return -1;
    }

    // Allocate command buffers
    const VkCommandBufferAllocateInfo commandBufferAllocateInfoPrimary = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = context.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmdBufSetup, cmdBufDraw;
    VULKAN_CHECK( vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfoPrimary, &cmdBufSetup) );
    VULKAN_CHECK( vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfoPrimary, &cmdBufDraw) );

    // Record the setup command buffer
    const VkCommandBufferBeginInfo commandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = 0,
        .pInheritanceInfo = NULL, // must be non-NULL for secondary command buffers
    };
    VULKAN_CHECK( vkBeginCommandBuffer(cmdBufSetup, &commandBufferBeginInfo) );

    // Create GLFW window, Vulken window surface, and swapchain
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(kWindowWidthDefault, kWindowHeightDefault, "Vulkswagen", NULL, NULL);

    VkSurfaceKHR surface;
    VULKAN_CHECK( glfwCreateWindowSurface(context.instance, window, context.allocation_callbacks, &surface) ); // wraps vkCreate*SurfaceKHR() for the current platform

    VkBool32 queueFamilySupportsPresent = VK_FALSE;
    VULKAN_CHECK( vkGetPhysicalDeviceSurfaceSupportKHR(context.physical_device, context.queue_family_index,
        surface, &queueFamilySupportsPresent) );

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VULKAN_CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physical_device, surface, &surfaceCapabilities) );
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
    VULKAN_CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR(context.physical_device, surface, &deviceSurfaceFormatCount, NULL) );
    VkSurfaceFormatKHR *deviceSurfaceFormats = (VkSurfaceFormatKHR*)malloc(deviceSurfaceFormatCount * sizeof(VkSurfaceFormatKHR));
    VULKAN_CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR(context.physical_device, surface, &deviceSurfaceFormatCount, deviceSurfaceFormats) );
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
    VULKAN_CHECK( vkGetPhysicalDeviceSurfacePresentModesKHR(context.physical_device, surface, &deviceSurfacePresentModeCount, NULL) );
    VkPresentModeKHR *deviceSurfacePresentModes = (VkPresentModeKHR*)malloc(deviceSurfacePresentModeCount * sizeof(VkPresentModeKHR));
    VULKAN_CHECK( vkGetPhysicalDeviceSurfacePresentModesKHR(context.physical_device, surface, &deviceSurfacePresentModeCount, deviceSurfacePresentModes) );
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR; // TODO(cort): make sure this mode is supported, or pick a different one?

    uint32_t desiredSwapchainImageCount = surfaceCapabilities.minImageCount+1;
    if (	surfaceCapabilities.maxImageCount > 0
        &&	desiredSwapchainImageCount > surfaceCapabilities.maxImageCount) {
        desiredSwapchainImageCount = surfaceCapabilities.maxImageCount;
    }
    VkSurfaceTransformFlagsKHR preTransform;
    if (0 != (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)) {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        preTransform = surfaceCapabilities.currentTransform;
    }

    const VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = NULL,
        .surface = surface,
        .minImageCount = desiredSwapchainImageCount,
        .imageFormat = surfaceColorFormat,
        .imageColorSpace = surfaceColorSpace,
        .imageExtent = {
            .width = swapchainExtent.width,
            .height = swapchainExtent.height,
        },
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = preTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .imageArrayLayers = 1,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .presentMode = swapchainPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateSwapchainKHR(context.device, &swapchainCreateInfo, context.allocation_callbacks, &swapchain) );

    uint32_t swapchainImageCount = 0;
    VULKAN_CHECK( vkGetSwapchainImagesKHR(context.device, swapchain, &swapchainImageCount, NULL) );
    VkImage *swapchainImages = (VkImage*)malloc(swapchainImageCount * sizeof(VkImage));
    VULKAN_CHECK( vkGetSwapchainImagesKHR(context.device, swapchain, &swapchainImageCount, swapchainImages) );

    VkImageViewCreateInfo imageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .format = surfaceColorFormat,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
            },
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .image = VK_NULL_HANDLE, // filled in below
    };
    VkImageView *swapchainImageViews = (VkImageView*)malloc(swapchainImageCount * sizeof(VkImageView));
    for(uint32_t iSCI=0; iSCI<swapchainImageCount; iSCI+=1) {
        imageViewCreateInfo.image = swapchainImages[iSCI];
        // Render loop will expect image to have been used before and in
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR layout and will change to
        // COLOR_ATTACHMENT_OPTIMAL, so init the image to that state.
        const VkImageSubresourceRange subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        setImageLayout(cmdBufSetup, imageViewCreateInfo.image, subresourceRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0);
        VULKAN_CHECK( vkCreateImageView(context.device, &imageViewCreateInfo, context.allocation_callbacks, &swapchainImageViews[iSCI]) );
    }
    uint32_t swapchainCurrentBufferIndex = 0;

    // Create depth buffer
    VkFormat surfaceDepthFormat = VK_FORMAT_D16_UNORM;
    const VkImageCreateInfo imageCreateInfoDepth = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = surfaceDepthFormat,
        .extent = {
            .width = kWindowWidthDefault,
            .height = kWindowHeightDefault,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .flags = 0,
    };
    VkImage imageDepth = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateImage(context.device, &imageCreateInfoDepth, context.allocation_callbacks, &imageDepth) );
    VkMemoryRequirements memoryRequirementsDepth;
    vkGetImageMemoryRequirements(context.device, imageDepth, &memoryRequirementsDepth);
    VkMemoryAllocateInfo memoryAllocateInfoDepth = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = memoryRequirementsDepth.size,
        .memoryTypeIndex = 0, // filled in below
    };
    VkBool32 foundMemoryTypeDepth = getMemoryTypeFromProperties(&context.physical_device_memory_properties,
        memoryRequirementsDepth.memoryTypeBits, 0, &memoryAllocateInfoDepth.memoryTypeIndex);
    assert(foundMemoryTypeDepth);
    VkDeviceMemory imageDepthMemory = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateMemory(context.device, &memoryAllocateInfoDepth, context.allocation_callbacks, &imageDepthMemory) );
    VkDeviceSize imageDepthMemoryOffset = 0;
    VULKAN_CHECK( vkBindImageMemory(context.device, imageDepth, imageDepthMemory, imageDepthMemoryOffset) );
    const VkImageSubresourceRange depthSubresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    setImageLayout(cmdBufSetup, imageDepth, depthSubresourceRange,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0);
    const VkImageViewCreateInfo imageViewCreateInfoDepth = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .image = imageDepth,
        .format = surfaceDepthFormat,
        .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .flags = 0,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
    };
    VkImageView imageDepthView;
    VULKAN_CHECK( vkCreateImageView(context.device, &imageViewCreateInfoDepth, context.allocation_callbacks, &imageDepthView) );

    // Create vertex buffer
    const float vertices[] = {
        //0,1,2: position  4,5,6: texcoord
        -0.75f, -0.75f, 1.00f,	0.0f, 0.0f, 0.0f,
         0.75f, -0.75f, 1.00f,	1.0f, 0.0f, 0.25f,
        -0.75f,  0.75f, 1.00f,	0.0f, 1.0f, 0.5f,
         0.75f,  0.75f, 1.00f,	1.0f, 1.0f, 0.75f,
    };
    const uint32_t kVertexBufferBindId = 0;
    const VkVertexInputBindingDescription vertexInputBindingDescriptions[] = {
        [0] = {
            .binding = kVertexBufferBindId,
            .stride = 3*sizeof(float) + 3*sizeof(float),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
    };
    const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        [0] = {
            .binding = kVertexBufferBindId,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0,
        }, [1] = {
            .binding = kVertexBufferBindId,
            .location = 1,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 3*sizeof(float),
        },
    };
    const VkBufferCreateInfo bufferCreateInfoVertices = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .size = sizeof(vertices),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .flags = 0,
    };
    VkBuffer bufferVertices = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateBuffer(context.device, &bufferCreateInfoVertices, context.allocation_callbacks, &bufferVertices) );
    VkMemoryRequirements memoryRequirementsVertices;
    vkGetBufferMemoryRequirements(context.device, bufferVertices, &memoryRequirementsVertices);
    VkMemoryAllocateInfo memoryAllocateInfoVertices = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = memoryRequirementsVertices.size,
        .memoryTypeIndex = 0,
    };
    VkBool32 foundMemoryTypeVertices = getMemoryTypeFromProperties(&context.physical_device_memory_properties,
        memoryRequirementsVertices.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        &memoryAllocateInfoVertices.memoryTypeIndex);
    assert(foundMemoryTypeVertices);
    VkDeviceMemory bufferVerticesMemory = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateMemory(context.device, &memoryAllocateInfoVertices, context.allocation_callbacks, &bufferVerticesMemory) );
    VkDeviceSize bufferVerticesMemoryOffset = 0;
    VkMemoryMapFlags bufferVerticesMemoryMapFlags = 0;
    void *bufferVerticesMapped = NULL;
    VULKAN_CHECK( vkMapMemory(context.device, bufferVerticesMemory, bufferVerticesMemoryOffset,
        memoryAllocateInfoVertices.allocationSize, bufferVerticesMemoryMapFlags, &bufferVerticesMapped) );
    memcpy(bufferVerticesMapped, vertices, sizeof(vertices));
    //vkUnmapMemory(device, bufferVerticesMapped); // TODO: see if validation layer catches this error
    vkUnmapMemory(context.device, bufferVerticesMemory);
    VULKAN_CHECK( vkBindBufferMemory(context.device, bufferVertices, bufferVerticesMemory, bufferVerticesMemoryOffset) );
    const VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .vertexBindingDescriptionCount = sizeof(vertexInputBindingDescriptions) / sizeof(vertexInputBindingDescriptions[0]),
        .pVertexBindingDescriptions = vertexInputBindingDescriptions,
        .vertexAttributeDescriptionCount = sizeof(vertexInputAttributeDescriptions) / sizeof(vertexInputAttributeDescriptions[0]),
        .pVertexAttributeDescriptions = vertexInputAttributeDescriptions,
    };

    // Create push constants
    struct {
        float time[4]; // .x=seconds, .yzw=???
    } pushConstants = {0,0,0,0};
    assert(sizeof(pushConstants) <= context.physical_device_properties.limits.maxPushConstantsSize);
#ifdef _WIN32
    LARGE_INTEGER counterFreq;
    QueryPerformanceFrequency(&counterFreq);
    const double clocksToSeconds = 1.0 / (double)counterFreq.QuadPart;
    LARGE_INTEGER counterStart;
    QueryPerformanceCounter(&counterStart);
#else
#   error need timer code here
#endif
    const VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(pushConstants),
    };

    // Create Vulkan descriptor layout & pipeline layout
    const VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[] = {
        [0] = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = kDemoTextureCount,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
        },
    };
    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .bindingCount = sizeof(descriptorSetLayoutBindings) / sizeof(descriptorSetLayoutBindings[0]),
        .pBindings = descriptorSetLayoutBindings,
    };
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateDescriptorSetLayout(context.device, &descriptorSetLayoutCreateInfo, context.allocation_callbacks, &descriptorSetLayout) );
    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    };
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreatePipelineLayout(context.device, &pipelineLayoutCreateInfo, context.allocation_callbacks, &pipelineLayout) );

    // Create render pass
    const VkAttachmentDescription attachmentDescriptions[] = {
        [0] = {
            .flags = 0,
            .format = surfaceColorFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        }, [1] = {
            .flags = 0,
            .format = surfaceDepthFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },
    };
    const VkAttachmentReference attachmentReferenceColor = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    const VkAttachmentReference attachmentReferenceDepth = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    const VkSubpassDescription subpassDescriptions[] = {
        [0] = {
            .flags = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = NULL,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentReferenceColor,
            .pResolveAttachments = NULL,
            .pDepthStencilAttachment = &attachmentReferenceDepth,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = NULL,
        },
    };
    const VkRenderPassCreateInfo renderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .attachmentCount = sizeof(attachmentDescriptions) / sizeof(attachmentDescriptions[0]),
        .pAttachments = attachmentDescriptions,
        .subpassCount = sizeof(subpassDescriptions) / sizeof(subpassDescriptions[0]),
        .pSubpasses = subpassDescriptions,
        .dependencyCount = 0,
        .pDependencies = NULL,
    };
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateRenderPass(context.device, &renderPassCreateInfo, context.allocation_callbacks, &renderPass) );

    // Load shaders
    VkShaderModule vertexShaderModule = stbvk_load_shader(&context, "tri.vert.spv");
    assert(vertexShaderModule != VK_NULL_HANDLE);
    VkShaderModule fragmentShaderModule = stbvk_load_shader(&context, "tri.frag.spv");
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
    vkGetPhysicalDeviceFormatProperties(context.physical_device, surfaceTextureFormat, &textureFormatProperties);
    if (0 == (textureFormatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        // TODO(cort): use a staging pass to transfer the linear source image to a tiled format.
        fprintf(stderr, "ERROR: linear texture sampling is not supported on this hardware.\n");
        exit(1);
    }
    const VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = surfaceTextureFormat,
        .extent = {
            .width = texWidth,
            .height = texHeight,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = kTextureLayerCount,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage textureImage;
    VkImageFormatProperties imageFormatProperties = {0};
    VULKAN_CHECK( vkGetPhysicalDeviceImageFormatProperties(context.physical_device,
        imageCreateInfo.format, imageCreateInfo.imageType, imageCreateInfo.tiling,
        imageCreateInfo.usage, 0, &imageFormatProperties) );
    assert(kTextureLayerCount <= imageFormatProperties.maxArrayLayers);
    VULKAN_CHECK( vkCreateImage(context.device, &imageCreateInfo, context.allocation_callbacks, &textureImage) );
    VkMemoryRequirements memoryRequirements = {0};
    vkGetImageMemoryRequirements(context.device, textureImage, &memoryRequirements);
    VkMemoryAllocateInfo memoryAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = 0, // filled in below
    };
    VkBool32 foundMemoryType = getMemoryTypeFromProperties(&context.physical_device_memory_properties,
        memoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &memoryAllocateInfo.memoryTypeIndex);
    assert(foundMemoryType);
    VkDeviceMemory textureDeviceMemory = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateMemory(context.device, &memoryAllocateInfo, context.allocation_callbacks, &textureDeviceMemory) );
    VkDeviceSize textureMemoryOffset = 0;
    VULKAN_CHECK( vkBindImageMemory(context.device, textureImage, textureDeviceMemory, textureMemoryOffset) );
    VkImageSubresourceRange textureImageSubresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = kTextureLayerCount,
    };
    setImageLayout(cmdBufSetup, textureImage, textureImageSubresourceRange,
        imageCreateInfo.initialLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0);
    const VkSamplerCreateInfo samplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE,
    };
    VkSampler sampler;
    VULKAN_CHECK( vkCreateSampler(context.device, &samplerCreateInfo, context.allocation_callbacks, &sampler) );
    VkImageViewCreateInfo textureImageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = VK_NULL_HANDLE, // filled in below
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = surfaceTextureFormat,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = kTextureLayerCount,
        },
    };
    VkImageView textureImageViews[kDemoTextureCount];
    for(uint32_t iTexture=0; iTexture<kDemoTextureCount; iTexture+=1) {
        textureImageViewCreateInfo.image = textureImage;
        VULKAN_CHECK( vkCreateImageView(context.device, &textureImageViewCreateInfo, context.allocation_callbacks, &textureImageViews[iTexture]) );
    }
    
    // Load individual texture layers into staging textures, and copy them into the final texture.
    const VkImageCreateInfo stagingImageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = surfaceTextureFormat,
        .extent = {
            .width = texWidth,
            .height = texHeight,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
    };
    VkImage *stagingTextureImages = (VkImage*)malloc(kTextureLayerCount * sizeof(VkImage));
    for(uint32_t iLayer=0; iLayer<kTextureLayerCount; iLayer += 1) {
        VULKAN_CHECK( vkCreateImage(context.device, &stagingImageCreateInfo, context.allocation_callbacks, &stagingTextureImages[iLayer]) );
        VkMemoryRequirements memoryRequirements = {0};
        vkGetImageMemoryRequirements(context.device, stagingTextureImages[iLayer], &memoryRequirements);
        VkMemoryAllocateInfo memoryAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = NULL,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = 0, // filled in below
        };
        VkBool32 foundMemoryType = getMemoryTypeFromProperties(&context.physical_device_memory_properties,
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &memoryAllocateInfo.memoryTypeIndex);
        assert(foundMemoryType);
        VkDeviceMemory textureDeviceMemory = VK_NULL_HANDLE;
        VULKAN_CHECK( vkAllocateMemory(context.device, &memoryAllocateInfo, context.allocation_callbacks, &textureDeviceMemory) );
        VkDeviceSize textureMemoryOffset = 0;
        VULKAN_CHECK( vkBindImageMemory(context.device, stagingTextureImages[iLayer], textureDeviceMemory, textureMemoryOffset) );

        const VkImageSubresource textureImageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .arrayLayer = 0,
        };
        VkSubresourceLayout subresourceLayout = {0};
        VkMemoryMapFlags memoryMapFlags = 0;
        vkGetImageSubresourceLayout(context.device, stagingTextureImages[iLayer], &textureImageSubresource, &subresourceLayout);
        void *mappedTextureData = NULL;
        VULKAN_CHECK( vkMapMemory(context.device, textureDeviceMemory, textureMemoryOffset,
            memoryRequirements.size, memoryMapFlags, &mappedTextureData) );
        char imagePath[128];
        _snprintf(imagePath, 127, "trevor/trevor-%u.png", iLayer);
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
        vkUnmapMemory(context.device, textureDeviceMemory);
        const VkImageSubresourceRange stagingImageSubresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        setImageLayout(cmdBufSetup, stagingTextureImages[iLayer], stagingImageSubresourceRange,
            stagingImageCreateInfo.initialLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0);

        const VkImageCopy copyRegion = {
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseArrayLayer = 0,
                .layerCount = 1,
                .mipLevel = 0,
            },
            .srcOffset = {0,0,0},
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseArrayLayer = iLayer,
                .layerCount = 1,
                .mipLevel = 0,
            },
            .dstOffset = {0,0,0},
            .extent = stagingImageCreateInfo.extent,
        };
        vkCmdCopyImage(cmdBufSetup,
            stagingTextureImages[iLayer], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    }
    const VkImageLayout textureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    setImageLayout(cmdBufSetup, textureImage, textureImageSubresourceRange,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, textureImageLayout, 0);

    // Create Vulkan pipeline & graphics state
    VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE] = {0}; // filled in below
    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .dynamicStateCount = 0, // filled in below
        .pDynamicStates = dynamicStateEnables, // filled in below
    };
    const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };
    const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };
    const VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentStates[1] = {
        [0] = {
            .colorWriteMask = 0xF,
            .blendEnable = VK_FALSE,
        },
    };
    const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .attachmentCount = 1,
        .pAttachments = pipelineColorBlendAttachmentStates,
    };
    const VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    dynamicStateEnables[pipelineDynamicStateCreateInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamicStateEnables[pipelineDynamicStateCreateInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
    const VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .back = {
            .failOp = VK_STENCIL_OP_KEEP,
            .passOp = VK_STENCIL_OP_KEEP,
            .compareOp = VK_COMPARE_OP_ALWAYS,
        },
        .front = {
            .failOp = VK_STENCIL_OP_KEEP,
            .passOp = VK_STENCIL_OP_KEEP,
            .compareOp = VK_COMPARE_OP_ALWAYS,
        },
        .stencilTestEnable = VK_FALSE,
    };
    const VkSampleMask *sampleMask = NULL;
    const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pSampleMask = sampleMask,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };
    const VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .initialDataSize = 0,
        .pInitialData = NULL,
    };
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreatePipelineCache(context.device, &pipelineCacheCreateInfo, context.allocation_callbacks, &pipelineCache) );
    const VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[2] = {
        [0] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShaderModule,
            .pName = "main",
        }, [1] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShaderModule,
            .pName = "main",
        },
    };
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .layout = pipelineLayout,
        .stageCount = sizeof(pipelineShaderStageCreateInfos) / sizeof(pipelineShaderStageCreateInfos[0]),
        .pStages = pipelineShaderStageCreateInfos,
        .pVertexInputState = &pipelineVertexInputStateCreateInfo,
        .pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo,
        .pRasterizationState = &pipelineRasterizationStateCreateInfo,
        .pColorBlendState = &pipelineColorBlendStateCreateInfo,
        .pMultisampleState = &pipelineMultisampleStateCreateInfo,
        .pViewportState = &pipelineViewportStateCreateInfo,
        .pDepthStencilState = &pipelineDepthStencilCreateInfo,
        .renderPass = renderPass,
        .pDynamicState = &pipelineDynamicStateCreateInfo,
    };
    VkPipeline pipelineGraphics = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateGraphicsPipelines(context.device, pipelineCache, 1, &graphicsPipelineCreateInfo, context.allocation_callbacks, &pipelineGraphics) );
    // These get destroyed now, I guess? The pipeline must keep a reference internally?
    vkDestroyPipelineCache(context.device, pipelineCache, context.allocation_callbacks);
    vkDestroyShaderModule(context.device, vertexShaderModule, context.allocation_callbacks);
    vkDestroyShaderModule(context.device, fragmentShaderModule, context.allocation_callbacks);

    // Create Vulkan descriptor pool and descriptor set
    const VkDescriptorPoolSize descriptorPoolSize = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = kDemoTextureCount,
    };
    const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &descriptorPoolSize,
    };
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateDescriptorPool(context.device, &descriptorPoolCreateInfo, context.allocation_callbacks, &descriptorPool) );
    const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout,
    };
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateDescriptorSets(context.device, &descriptorSetAllocateInfo, &descriptorSet) );
    VkDescriptorImageInfo descriptorImageInfos[kDemoTextureCount] = {0};
    for(uint32_t iTexture=0; iTexture<kDemoTextureCount; iTexture += 1) {
        descriptorImageInfos[iTexture].sampler = sampler;
        descriptorImageInfos[iTexture].imageView = textureImageViews[iTexture];
        descriptorImageInfos[iTexture].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
    const VkWriteDescriptorSet writeDescriptorSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = NULL,
        .dstSet = descriptorSet,
        .descriptorCount = kDemoTextureCount,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = descriptorImageInfos,
    };
    vkUpdateDescriptorSets(context.device, 1, &writeDescriptorSet, 0, NULL);

    // Create framebuffers
    VkImageView attachmentImageViews[2] = {
        [0] = VK_NULL_HANDLE, // filled in below
        [1] = imageDepthView,
    };
    const VkFramebufferCreateInfo framebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = renderPass,
        .attachmentCount = sizeof(attachmentImageViews) / sizeof(attachmentImageViews[0]),
        .pAttachments = attachmentImageViews,
        .width = kWindowWidthDefault,
        .height = kWindowHeightDefault,
        .layers = 1,
    };
    VkFramebuffer *framebuffers = (VkFramebuffer*)malloc(swapchainImageCount * sizeof(VkFramebuffer));
    for(uint32_t iFB=0; iFB<swapchainImageCount; iFB += 1) {
        attachmentImageViews[0] = swapchainImageViews[iFB];
        VULKAN_CHECK( vkCreateFramebuffer(context.device, &framebufferCreateInfo, context.allocation_callbacks, &framebuffers[iFB]) );
    }

    // Submit the setup command buffer
    VULKAN_CHECK( vkEndCommandBuffer(cmdBufSetup) );
    const VkSubmitInfo submitInfoSetup = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBufSetup,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };
    VkFence submitFence = VK_NULL_HANDLE;
    VULKAN_CHECK( vkQueueSubmit(context.queues[0], 1, &submitInfoSetup, submitFence) );
    VULKAN_CHECK( vkQueueWaitIdle(context.queues[0]) );
    vkFreeCommandBuffers(context.device, context.command_pool, 1, &cmdBufSetup);
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
        const VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        };
        VkSemaphore presentCompleteSemaphore = VK_NULL_HANDLE;
        VULKAN_CHECK( vkCreateSemaphore(context.device, &semaphoreCreateInfo, context.allocation_callbacks, &presentCompleteSemaphore) );

        // Retrieve the index of the next available swapchain index
        VkFence presentCompleteFence = VK_NULL_HANDLE; // TODO(cort): unused
        uint32_t currentBufferIndex = 0;
        VkResult result = vkAcquireNextImageKHR(context.device, swapchain, UINT64_MAX, presentCompleteSemaphore,
            presentCompleteFence, &currentBufferIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
        } else if (result == VK_SUBOPTIMAL_KHR) {
            // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
        } else {
            VULKAN_CHECK(result);
        }

        // Draw!
        const VkCommandBufferInheritanceInfo cmdBufDrawInheritanceInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
            .pNext = NULL,
            .renderPass = VK_NULL_HANDLE,
            .subpass = 0,
            .framebuffer = VK_NULL_HANDLE,
            .occlusionQueryEnable = VK_FALSE,
            .queryFlags = 0,
            .pipelineStatistics = 0,
        };
        const VkCommandBufferBeginInfo cmdBufDrawBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = 0,
            .pInheritanceInfo = &cmdBufDrawInheritanceInfo,
        };

        VULKAN_CHECK( vkBeginCommandBuffer(cmdBufDraw, &cmdBufDrawBeginInfo) );
        const VkImageSubresourceRange swapchainSubresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        setImageLayout(cmdBufDraw, swapchainImages[currentBufferIndex],
            swapchainSubresourceRange,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0);
        // TODO(cort): is a sync point needed here? tri.c submits this layout change as a separate
        // command buffer, then waits for the queue to be idle.

        const VkClearValue clearValues[2] = {
            [0] = { .color.float32 = {(float)(frameIndex%256)/255.0f, (float)(frameIndex%512)/511.0f, (float)(frameIndex%1024)/1023.0f, 1.0f}},
            [1] = { .depthStencil = {.depth=1.0f, .stencil=0}},
        };
        const VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = NULL,
            .renderPass = renderPass,
            .framebuffer = framebuffers[currentBufferIndex],
            .renderArea.offset.x = 0,
            .renderArea.offset.y = 0,
            .renderArea.extent.width  = kWindowWidthDefault,
            .renderArea.extent.height = kWindowHeightDefault,
            .clearValueCount = 2,
            .pClearValues = clearValues,
        };
        vkCmdBeginRenderPass(cmdBufDraw, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdBufDraw, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineGraphics);
        vkCmdBindDescriptorSets(cmdBufDraw, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,1,&descriptorSet, 0,NULL);
        LARGE_INTEGER counterTime;
        QueryPerformanceCounter(&counterTime);
        pushConstants.time[0] = (float)( (double)(counterTime.QuadPart - counterStart.QuadPart) * clocksToSeconds );
        vkCmdPushConstants(cmdBufDraw, pipelineLayout, pushConstantRange.stageFlags,
            pushConstantRange.offset, pushConstantRange.size, &pushConstants);
        const VkViewport viewport = {
            .width  = (float)kWindowWidthDefault,
            .height = (float)kWindowHeightDefault,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmdBufDraw, 0,1, &viewport);
        const VkRect2D scissorRect = {
            .extent.width  = kWindowWidthDefault,
            .extent.height = kWindowHeightDefault,
            .offset.x = 0,
            .offset.y = 0,
        };
        vkCmdSetScissor(cmdBufDraw, 0,1, &scissorRect);
        const VkDeviceSize vertexBufferOffsets[1] = {0};
        vkCmdBindVertexBuffers(cmdBufDraw, kVertexBufferBindId,1, &bufferVertices, vertexBufferOffsets);
        vkCmdDraw(cmdBufDraw, 4,1,0,0);

        vkCmdEndRenderPass(cmdBufDraw);
        const VkImageMemoryBarrier prePresentBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .image = swapchainImages[currentBufferIndex],
        };
        vkCmdPipelineBarrier(cmdBufDraw, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0,
            NULL, 1, &prePresentBarrier);
        VULKAN_CHECK( vkEndCommandBuffer(cmdBufDraw) );
        VkFence nullFence = VK_NULL_HANDLE;
        const VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        const VkSubmitInfo submitInfoDraw = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &presentCompleteSemaphore,
            .pWaitDstStageMask = &pipelineStageFlags,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmdBufDraw,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = NULL,
        };
        VULKAN_CHECK( vkQueueSubmit(context.queues[0], 1, &submitInfoDraw, nullFence) );
        const VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = NULL,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &currentBufferIndex,
        };
        result = vkQueuePresentKHR(context.queues[0], &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
        } else if (result == VK_SUBOPTIMAL_KHR) {
            // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
        } else {
            VULKAN_CHECK(result);
        }
        VULKAN_CHECK( vkQueueWaitIdle(context.queues[0]) );

        // glfwSwapBuffers(window); // Not necessary in Vulkan
        glfwPollEvents();
        vkDestroySemaphore(context.device, presentCompleteSemaphore, context.allocation_callbacks); // TODO(cort): create/destroy every frame?
        frameIndex += 1;
    }

    vkDeviceWaitIdle(context.device);
    for(uint32_t iFB=0; iFB<swapchainImageCount; iFB+=1) {
        vkDestroyFramebuffer(context.device, framebuffers[iFB], context.allocation_callbacks);
        vkDestroyImageView(context.device, swapchainImageViews[iFB], context.allocation_callbacks);
    }
    free(framebuffers);
    free(swapchainImageViews);

    vkDestroyImageView(context.device, imageDepthView, context.allocation_callbacks);
    vkFreeMemory(context.device, imageDepthMemory, context.allocation_callbacks);
    vkDestroyImage(context.device, imageDepth, context.allocation_callbacks);

    vkFreeMemory(context.device, bufferVerticesMemory, context.allocation_callbacks);
    vkDestroyBuffer(context.device, bufferVertices, context.allocation_callbacks);

    vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout, context.allocation_callbacks);
    vkDestroyDescriptorPool(context.device, descriptorPool, context.allocation_callbacks);

    vkFreeCommandBuffers(context.device, context.command_pool, 1, &cmdBufDraw);

    vkDestroyRenderPass(context.device, renderPass, context.allocation_callbacks);

    vkDestroyImage(context.device, textureImage, context.allocation_callbacks);
    vkFreeMemory(context.device, textureDeviceMemory, context.allocation_callbacks);
    for(uint32_t iTex=0; iTex<kDemoTextureCount; ++iTex) {
        vkDestroyImageView(context.device, textureImageViews[iTex], context.allocation_callbacks);
    }
    for(uint32_t iLayer=0; iLayer<kTextureLayerCount; ++iLayer) {
        vkDestroyImage(context.device, stagingTextureImages[iLayer], context.allocation_callbacks);
        //vkFreeMemory(device, stagingTextureDeviceMemory, allocationCallbacks); // LEAKED!
    }
    free(stagingTextureImages);

    vkDestroySampler(context.device, sampler, context.allocation_callbacks);

    vkDestroyPipelineLayout(context.device, pipelineLayout, context.allocation_callbacks);
    vkDestroyPipeline(context.device, pipelineGraphics, context.allocation_callbacks);

    vkDestroySwapchainKHR(context.device, swapchain, context.allocation_callbacks);
    DestroyDebugReportCallback(context.instance, debugReportCallback, context.allocation_callbacks);

    vkDestroySurfaceKHR(context.instance, surface, context.allocation_callbacks);
    glfwTerminate();
    stbvk_destroy_context(&context);
    return 0;
}
