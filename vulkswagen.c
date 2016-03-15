#ifdef _WIN32
#	include <Windows.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Handy meta-macro to simplify repetitive error checking for APIs where every function returns
 * an error code (e.g. CUDA, C11 threads, most of the Windows API, etc.)
 *
 * Example usage:
 *     #define CUDA_CHECK(expr) RETVAL_CHECK(cudaSuccess, expr)
 *     ...
 *     CUDA_CHECK( cudaMemcpy(dst, src, size, cudaMemcpyHostToDevice) );
 *
 * To disable the checks in release builds, just redefine the macro:
 *     #define CUDA_CHECK(expr) expr
 *
 * As written, it's Visual C++ specific, but don't let that stop you!
 */
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
	VkImageAspectFlags aspectMask, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkImageMemoryBarrier imgMemoryBarrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = NULL,
		.srcAccessMask = 0,
		.dstAccessMask = 0, // overwritten below
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.image = image,
		.subresourceRange = {
			.aspectMask = 0,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	switch(newLayout) {
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Make sure anything that was copying from this image has completed.
		imgMemoryBarrier.dstAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		imgMemoryBarrier.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		imgMemoryBarrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Make sure any Copy or CPU writes to image are flushed
        imgMemoryBarrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
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
	const VkShaderModuleCreateInfo shaderModuleCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.codeSize = spvFileSize,
		.pCode = shaderCode,
	};
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
		"VK_LAYER_LUNARG_device_limits", // Tracks device features/limitations and reports an error if the app requests unsupported features. Work in progess.
		"VK_LAYER_LUNARG_draw_state", // Ensures resources bound to descriptor sets align with the specified set layout, validates image/buffer layout transitions, etc.
		"VK_LAYER_LUNARG_image", // Validates image parameters, formats, and correct use.
        "VK_LAYER_LUNARG_mem_tracker", // Tracks memory object lifetimes, binding errors, and other memory hazards.
		"VK_LAYER_LUNARG_object_tracker", // Tracks all Vulkan objects. Reports on invalid object use, and reports leaked objects at vkDestroyDevice() time.
		"VK_LAYER_LUNARG_param_checker", // Validates parameters of all API calls, checking for out-of-range/invalid enums, etc.
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
		.ppEnabledLayerNames = desiredDeviceLayerNames,
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
	VULKAN_CHECK( vkCreateSwapchainKHR(device, &swapchainCreateInfo, allocationCallbacks, &swapchain) );

	uint32_t swapchainImageCount = 0;
	VULKAN_CHECK( vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL) );
	VkImage *swapchainImages = (VkImage*)malloc(swapchainImageCount * sizeof(VkImage));
	VULKAN_CHECK( vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages) );

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
		setImageLayout(cmdBufSetup, imageViewCreateInfo.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		VULKAN_CHECK( vkCreateImageView(device, &imageViewCreateInfo, allocationCallbacks, &swapchainImageViews[iSCI]) );
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
	VULKAN_CHECK( vkCreateImage(device, &imageCreateInfoDepth, allocationCallbacks, &imageDepth) );
	VkMemoryRequirements memoryRequirementsDepth;
	vkGetImageMemoryRequirements(device, imageDepth, &memoryRequirementsDepth);
	VkMemoryAllocateInfo memoryAllocateInfoDepth = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = memoryRequirementsDepth.size,
		.memoryTypeIndex = 0, // filled in below
	};
	VkBool32 foundMemoryTypeDepth = getMemoryTypeFromProperties(&physicalDeviceMemoryProperties,
		memoryRequirementsDepth.memoryTypeBits, 0, &memoryAllocateInfoDepth.memoryTypeIndex);
	assert(foundMemoryTypeDepth);
	VkDeviceMemory imageDepthMemory = VK_NULL_HANDLE;
	VULKAN_CHECK( vkAllocateMemory(device, &memoryAllocateInfoDepth, allocationCallbacks, &imageDepthMemory) );
	VkDeviceSize imageDepthMemoryOffset = 0;
	VULKAN_CHECK( vkBindImageMemory(device, imageDepth, imageDepthMemory, imageDepthMemoryOffset) );
	setImageLayout(cmdBufSetup, imageDepth, VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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
	VULKAN_CHECK( vkCreateBuffer(device, &bufferCreateInfoVertices, allocationCallbacks, &bufferVertices) );
	VkMemoryRequirements memoryRequirementsVertices;
	vkGetBufferMemoryRequirements(device, bufferVertices, &memoryRequirementsVertices);
	VkMemoryAllocateInfo memoryAllocateInfoVertices = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = memoryRequirementsVertices.size,
		.memoryTypeIndex = 0,
	};
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
	assert(sizeof(pushConstants) <= deviceProps.limits.maxPushConstantsSize);
#ifdef _WIN32
	LARGE_INTEGER counterFreq;
	QueryPerformanceFrequency(&counterFreq);
	const double clocksToSeconds = 1.0 / (double)counterFreq.QuadPart;
	LARGE_INTEGER counterStart;
	QueryPerformanceCounter(&counterStart);
#else
#	error need timer code here
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
	VULKAN_CHECK( vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, allocationCallbacks, &descriptorSetLayout) );
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
	VULKAN_CHECK( vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, allocationCallbacks, &pipelineLayout) );

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
	VULKAN_CHECK( vkGetPhysicalDeviceImageFormatProperties(physicalDevice,
		imageCreateInfo.format, imageCreateInfo.imageType, imageCreateInfo.tiling,
		imageCreateInfo.usage, 0, &imageFormatProperties) );
	assert(kTextureLayerCount <= imageFormatProperties.maxArrayLayers);
	VULKAN_CHECK( vkCreateImage(device, &imageCreateInfo, allocationCallbacks, &textureImage) );
	VkMemoryRequirements memoryRequirements = {0};
	vkGetImageMemoryRequirements(device, textureImage, &memoryRequirements);
	VkMemoryAllocateInfo memoryAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = 0, // filled in below
	};
	VkBool32 foundMemoryType = getMemoryTypeFromProperties(&physicalDeviceMemoryProperties,
		memoryRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&memoryAllocateInfo.memoryTypeIndex);
	assert(foundMemoryType);
	VkDeviceMemory textureDeviceMemory = VK_NULL_HANDLE;
	VULKAN_CHECK( vkAllocateMemory(device, &memoryAllocateInfo, allocationCallbacks, &textureDeviceMemory) );
	VkDeviceSize textureMemoryOffset = 0;
	VULKAN_CHECK( vkBindImageMemory(device, textureImage, textureDeviceMemory, textureMemoryOffset) );
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
	VULKAN_CHECK( vkCreateSampler(device, &samplerCreateInfo, allocationCallbacks, &sampler) );
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
		VULKAN_CHECK( vkCreateImageView(device, &textureImageViewCreateInfo, allocationCallbacks, &textureImageViews[iTexture]) );
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
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VkImage *stagingTextureImages = (VkImage*)malloc(kTextureLayerCount * sizeof(VkImage));
	for(uint32_t iLayer=0; iLayer<kTextureLayerCount; iLayer += 1) {
		VULKAN_CHECK( vkCreateImage(device, &stagingImageCreateInfo, allocationCallbacks, &stagingTextureImages[iLayer]) );
		VkMemoryRequirements memoryRequirements = {0};
		vkGetImageMemoryRequirements(device, stagingTextureImages[iLayer], &memoryRequirements);
		VkMemoryAllocateInfo memoryAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = NULL,
			.allocationSize = memoryRequirements.size,
			.memoryTypeIndex = 0, // filled in below
		};
		VkBool32 foundMemoryType = getMemoryTypeFromProperties(&physicalDeviceMemoryProperties,
			memoryRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&memoryAllocateInfo.memoryTypeIndex);
		assert(foundMemoryType);
		VkDeviceMemory textureDeviceMemory = VK_NULL_HANDLE;
		VULKAN_CHECK( vkAllocateMemory(device, &memoryAllocateInfo, allocationCallbacks, &textureDeviceMemory) );
		VkDeviceSize textureMemoryOffset = 0;
		VULKAN_CHECK( vkBindImageMemory(device, stagingTextureImages[iLayer], textureDeviceMemory, textureMemoryOffset) );

		const VkImageSubresource textureImageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.arrayLayer = 0,
		};
		VkSubresourceLayout subresourceLayout = {0};
		VkMemoryMapFlags memoryMapFlags = 0;
		vkGetImageSubresourceLayout(device, stagingTextureImages[iLayer], &textureImageSubresource, &subresourceLayout);
		void *mappedTextureData = NULL;
		VULKAN_CHECK( vkMapMemory(device, textureDeviceMemory, textureMemoryOffset,
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
		vkUnmapMemory(device, textureDeviceMemory);
		setImageLayout(cmdBufSetup, stagingTextureImages[iLayer], VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

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
	VkImageLayout textureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	setImageLayout(cmdBufSetup, textureImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, textureImageLayout);

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
	VULKAN_CHECK( vkCreatePipelineCache(device, &pipelineCacheCreateInfo, allocationCallbacks, &pipelineCache) );
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
	VULKAN_CHECK( vkCreateGraphicsPipelines(device, pipelineCache, 1, &graphicsPipelineCreateInfo, allocationCallbacks, &pipelineGraphics) );
	// These get destroyed now, I guess? The pipeline must keep a reference internally?
	vkDestroyPipelineCache(device, pipelineCache, allocationCallbacks);
	vkDestroyShaderModule(device, vertexShaderModule, allocationCallbacks);
	vkDestroyShaderModule(device, fragmentShaderModule, allocationCallbacks);

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
	VULKAN_CHECK( vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, allocationCallbacks, &descriptorPool) );
	const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = NULL,
		.descriptorPool = descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descriptorSetLayout,
	};
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VULKAN_CHECK( vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet) );
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
	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);

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
		VULKAN_CHECK( vkCreateFramebuffer(device, &framebufferCreateInfo, allocationCallbacks, &framebuffers[iFB]) );
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
		const VkSemaphoreCreateInfo semaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
		};
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
		setImageLayout(cmdBufDraw, swapchainImages[currentBufferIndex],
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
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
		VULKAN_CHECK( vkQueueSubmit(queues[0], 1, &submitInfoDraw, nullFence) );
		const VkPresentInfoKHR presentInfo = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = NULL,
			.swapchainCount = 1,
			.pSwapchains = &swapchain,
			.pImageIndices = &currentBufferIndex,
		};
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
