#ifdef _WIN32
#   include <Windows.h>
#endif

#include "platform.h"

// Must happen before any vulkan.h include
#if defined(ZOMBO_PLATFORM_WINDOWS)
# define VK_USE_PLATFORM_WIN32_KHR 1
#elif defined(ZOMBO_PLATFORM_POSIX)
# define VK_USE_PLATFORM_XCB_KHR 1
#elif defined(ZOMBO_PLATFORM_ANDROID)
# define VK_USE_PLATFORM_ANDROID_KHR 1
#else
# error Unsupported platform
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifdef _MSC_VER
#   pragma warning(push)
#   pragma warning(disable:4244) // implicit variable truncation (e.g. int32_t -> int16_t)
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#ifdef _MSC_VER
#   pragma warning(pop)
#endif

#define STB_VULKAN_IMPLEMENTATION
#include "stb_vulkan.h"

#define CDS_MESH_IMPLEMENTATION
#include "cds_mesh.h"

#include <mathfu/vector.h>
#include <mathfu/glsl_mappings.h>

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

static VkResult my_stbvk_init_context(stbvk_context_create_info const *createInfo, GLFWwindow *window, stbvk_context *c)
{
    VkResult result = VK_SUCCESS;
    *c = {};
    c->allocation_callbacks = createInfo->allocation_callbacks;

    result = stbvk_init_instance(createInfo, c);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    // wraps vkCreate*SurfaceKHR() for the current platform
    VkSurfaceKHR presentSurface = VK_NULL_HANDLE;
    VULKAN_CHECK( glfwCreateWindowSurface(c->instance, window, c->allocation_callbacks, &presentSurface) );

    result = stbvk_init_device(createInfo, presentSurface, c);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    result = stbvk_init_command_pool(createInfo, c);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    result = stbvk_init_swapchain(createInfo, c, VK_NULL_HANDLE);

    return result;
}


int main(int argc, char *argv[]) {
    (void)argc;
(    void)argv;
    //
    // Initialise GLFW
    //
    const char *applicationName = "Vulkswagen";

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
    // Create GLFW window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(kWindowWidthDefault, kWindowHeightDefault, applicationName, NULL, NULL);


    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pNext = NULL;
    applicationInfo.pApplicationName = applicationName;
    applicationInfo.applicationVersion = 0x1000;
    applicationInfo.pEngineName = "Zombo";
    applicationInfo.engineVersion = 0x1001;
    applicationInfo.apiVersion = VK_MAKE_VERSION(1,0,17);
    const char *required_instance_layers[] = {
        "VK_LAYER_GOOGLE_threading",
        "VK_LAYER_LUNARG_parameter_validation",
        "VK_LAYER_LUNARG_device_limits",
        "VK_LAYER_LUNARG_object_tracker",
        "VK_LAYER_LUNARG_image",
        "VK_LAYER_LUNARG_core_validation",
        "VK_LAYER_LUNARG_swapchain",
        "VK_LAYER_GOOGLE_unique_objects",
    };
    const char *required_instance_extensions[] = {
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
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    };
    const char *required_device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    stbvk_context_create_info contextCreateInfo = {};
    contextCreateInfo.allocation_callbacks = NULL;
    contextCreateInfo.required_instance_layer_count     = sizeof(required_instance_layers) / sizeof(required_instance_layers[0]);
    contextCreateInfo.required_instance_layer_names     = required_instance_layers;
    contextCreateInfo.required_instance_extension_count = sizeof(required_instance_extensions) / sizeof(required_instance_extensions[0]);
    contextCreateInfo.required_instance_extension_names = required_instance_extensions;
    contextCreateInfo.required_device_extension_count   = sizeof(required_device_extensions) / sizeof(required_device_extensions[0]);
    contextCreateInfo.required_device_extension_names   = required_device_extensions;
    contextCreateInfo.application_info = &applicationInfo;
    contextCreateInfo.debug_report_callback = debugReportCallbackFunc;
    contextCreateInfo.debug_report_callback_user_data = NULL;
    stbvk_context context = {};
    my_stbvk_init_context(&contextCreateInfo, window, &context);

    // Allocate command buffer
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = NULL;
    commandBufferAllocateInfo.commandPool = context.command_pool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &commandBuffer) );

    // Record the setup command buffer
    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = NULL;
    commandBufferBeginInfo.flags = 0;
    commandBufferBeginInfo.pInheritanceInfo = NULL; // must be non-NULL for secondary command buffers
    VULKAN_CHECK( vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) );

    // Create depth buffer
    stbvk_image_create_info depth_image_create_info = {};
    depth_image_create_info.image_type = VK_IMAGE_TYPE_2D;
    depth_image_create_info.format = VK_FORMAT_D16_UNORM;
    depth_image_create_info.extent = {kWindowWidthDefault, kWindowHeightDefault, 1};
    depth_image_create_info.mip_levels = 1;
    depth_image_create_info.array_layers = 1;
    depth_image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    depth_image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_image_create_info.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_image_create_info.memory_properties_mask = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    depth_image_create_info.view_type = VK_IMAGE_VIEW_TYPE_2D;
    stbvk_image depth_image;
    VULKAN_CHECK( stbvk_image_create(&context, &depth_image_create_info, &depth_image) );
    stbvk_set_image_layout(commandBuffer, depth_image.image,
      depth_image.image_view_create_info.subresourceRange,
      depth_image_create_info.initial_layout,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0);

    cdsm_metadata_t mesh_metadata = {};
    size_t mesh_vertices_size = 0, mesh_indices_size = 0;
    enum
    {
        MESH_TYPE_CUBE     = 0,
        MESH_TYPE_SPHERE   = 1,
        MESH_TYPE_AXES     = 3,
        MESH_TYPE_CYLINDER = 2,
    } meshType = MESH_TYPE_SPHERE;
    cdsm_cube_recipe_t cube_recipe = {};
    cube_recipe.min_extent = {-1,-1,-1};
    cube_recipe.max_extent = {+1,+1,+1};
    cdsm_sphere_recipe_t sphere_recipe = {};
    sphere_recipe.latitudinal_segments = 300;
    sphere_recipe.longitudinal_segments = 300;
    sphere_recipe.radius = 0.5f;
    cdsm_cylinder_recipe_t cylinder_recipe = {};
    cylinder_recipe.length = 1.0f;
    cylinder_recipe.axial_segments = 3;
    cylinder_recipe.radial_segments = 60;
    cylinder_recipe.radius0 = -1.0f;
    cylinder_recipe.radius1 = 1.0f;
    cdsm_axes_recipe_t axes_recipe = {};
    axes_recipe.length = 1.0f;
    if      (meshType == MESH_TYPE_CUBE)
        cdsm_create_cube(&mesh_metadata, NULL, &mesh_vertices_size, NULL, &mesh_indices_size, &cube_recipe);
    else if (meshType == MESH_TYPE_SPHERE)
        cdsm_create_sphere(&mesh_metadata, NULL, &mesh_vertices_size, NULL, &mesh_indices_size, &sphere_recipe);
    else if (meshType == MESH_TYPE_AXES)
        cdsm_create_axes(&mesh_metadata, NULL, &mesh_vertices_size, NULL, &mesh_indices_size, &axes_recipe);
    else if (meshType == MESH_TYPE_CYLINDER)
        cdsm_create_cylinder(&mesh_metadata, NULL, &mesh_vertices_size, NULL, &mesh_indices_size, &cylinder_recipe);
    VkPrimitiveTopology primitive_topology = VK_PRIMITIVE_TOPOLOGY_RANGE_SIZE;
    if (mesh_metadata.primitive_type == CDSM_PRIMITIVE_TYPE_TRIANGLE_LIST)
        primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    else if (mesh_metadata.primitive_type == CDSM_PRIMITIVE_TYPE_LINE_LIST)
        primitive_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    else
    {
        assert(0); // unknown primitive topology
    }

    // Create index buffer
    VkIndexType indexType = VK_INDEX_TYPE_UINT32;
    VkBufferCreateInfo bufferCreateInfoIndices = {};
    bufferCreateInfoIndices.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfoIndices.pNext = NULL;
    bufferCreateInfoIndices.size = mesh_indices_size;
    bufferCreateInfoIndices.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufferCreateInfoIndices.flags = 0;
    VkBuffer bufferIndices = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateBuffer(context.device, &bufferCreateInfoIndices, context.allocation_callbacks, &bufferIndices) );
    VkMemoryRequirements memoryRequirementsIndices;
    vkGetBufferMemoryRequirements(context.device, bufferIndices, &memoryRequirementsIndices);
    VkMemoryAllocateInfo memoryAllocateInfoIndices = {};
    memoryAllocateInfoIndices.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfoIndices.pNext = NULL;
    memoryAllocateInfoIndices.allocationSize = memoryRequirementsIndices.size;
    memoryAllocateInfoIndices.memoryTypeIndex = 0;
    VkBool32 foundMemoryTypeIndices = stbvk_get_memory_type_from_properties(&context.physical_device_memory_properties,
        memoryRequirementsIndices.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &memoryAllocateInfoIndices.memoryTypeIndex);
    assert(foundMemoryTypeIndices);
    VkDeviceMemory bufferIndicesMemory = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateMemory(context.device, &memoryAllocateInfoIndices, context.allocation_callbacks, &bufferIndicesMemory) );
    VkDeviceSize bufferIndicesMemoryOffset = 0;
    VULKAN_CHECK( vkBindBufferMemory(context.device, bufferIndices, bufferIndicesMemory, bufferIndicesMemoryOffset) );

    // Create vertex buffer
    VkBufferCreateInfo bufferCreateInfoVertices = {};
    bufferCreateInfoVertices.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfoVertices.pNext = NULL;
    bufferCreateInfoVertices.size = mesh_vertices_size;
    bufferCreateInfoVertices.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferCreateInfoVertices.flags = 0;
    VkBuffer bufferVertices = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateBuffer(context.device, &bufferCreateInfoVertices, context.allocation_callbacks, &bufferVertices) );
    VkMemoryRequirements memoryRequirementsVertices;
    vkGetBufferMemoryRequirements(context.device, bufferVertices, &memoryRequirementsVertices);
    VkMemoryAllocateInfo memoryAllocateInfoVertices = {};
    memoryAllocateInfoVertices.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfoVertices.pNext = NULL;
    memoryAllocateInfoVertices.allocationSize = memoryRequirementsVertices.size;
    memoryAllocateInfoVertices.memoryTypeIndex = 0;
    VkBool32 foundMemoryTypeVertices = stbvk_get_memory_type_from_properties(&context.physical_device_memory_properties,
        memoryRequirementsVertices.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &memoryAllocateInfoVertices.memoryTypeIndex);
    assert(foundMemoryTypeVertices);
    VkDeviceMemory bufferVerticesMemory = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateMemory(context.device, &memoryAllocateInfoVertices, context.allocation_callbacks, &bufferVerticesMemory) );
    VkDeviceSize bufferVerticesMemoryOffset = 0;
    VULKAN_CHECK( vkBindBufferMemory(context.device, bufferVertices, bufferVerticesMemory, bufferVerticesMemoryOffset) );
    stbvk_vertex_buffer_layout vertexBufferLayout = {};
    vertexBufferLayout.stride = sizeof(cdsm_vertex_t);
    vertexBufferLayout.attribute_count = 3;
    vertexBufferLayout.attributes[0].binding = 0;
    vertexBufferLayout.attributes[0].location = 0;
    vertexBufferLayout.attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexBufferLayout.attributes[0].offset = offsetof(cdsm_vertex_t, position);
    vertexBufferLayout.attributes[1].binding = 0;
    vertexBufferLayout.attributes[1].location = 1;
    vertexBufferLayout.attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexBufferLayout.attributes[1].offset = offsetof(cdsm_vertex_t, normal);
    vertexBufferLayout.attributes[2].binding = 0;
    vertexBufferLayout.attributes[2].location = 2;
    vertexBufferLayout.attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertexBufferLayout.attributes[2].offset = offsetof(cdsm_vertex_t, texcoord);

    // Populate vertex/index buffers
    VkMemoryMapFlags bufferIndicesMemoryMapFlags = 0;
    void *bufferIndicesMapped = NULL;
    VULKAN_CHECK( vkMapMemory(context.device, bufferIndicesMemory, bufferIndicesMemoryOffset,
        memoryAllocateInfoIndices.allocationSize, bufferIndicesMemoryMapFlags, &bufferIndicesMapped) );
    VkMemoryMapFlags bufferVerticesMemoryMapFlags = 0;
    void *bufferVerticesMapped = NULL;
    VULKAN_CHECK( vkMapMemory(context.device, bufferVerticesMemory, bufferVerticesMemoryOffset,
        memoryAllocateInfoVertices.allocationSize, bufferVerticesMemoryMapFlags, &bufferVerticesMapped) );
    if      (meshType == MESH_TYPE_CUBE)
        cdsm_create_cube(&mesh_metadata, (cdsm_vertex_t*)bufferVerticesMapped, &mesh_vertices_size,
            (cdsm_index_t*)bufferIndicesMapped, &mesh_indices_size, &cube_recipe);
    else if (meshType == MESH_TYPE_SPHERE)
        cdsm_create_sphere(&mesh_metadata, (cdsm_vertex_t*)bufferVerticesMapped, &mesh_vertices_size,
            (cdsm_index_t*)bufferIndicesMapped, &mesh_indices_size, &sphere_recipe);
    else if (meshType == MESH_TYPE_AXES)
        cdsm_create_axes(&mesh_metadata, (cdsm_vertex_t*)bufferVerticesMapped, &mesh_vertices_size,
            (cdsm_index_t*)bufferIndicesMapped, &mesh_indices_size, &axes_recipe);
    else if (meshType == MESH_TYPE_CYLINDER)
        cdsm_create_cylinder(&mesh_metadata, (cdsm_vertex_t*)bufferVerticesMapped, &mesh_vertices_size,
            (cdsm_index_t*)bufferIndicesMapped, &mesh_indices_size, &cylinder_recipe);
    vkUnmapMemory(context.device, bufferIndicesMemory);
    vkUnmapMemory(context.device, bufferVerticesMemory);

    // Create push constants
    struct {
        float time[4]; // .x=seconds, .yzw=???
        mathfu::mat4 o2w;
        mathfu::mat4 proj;
        mathfu::mat4 n2w;
    } pushConstants = {};
    assert(sizeof(pushConstants) <= context.physical_device_properties.limits.maxPushConstantsSize);
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
    VULKAN_CHECK( vkCreateDescriptorSetLayout(context.device, &descriptorSetLayoutCreateInfo, context.allocation_callbacks, &descriptorSetLayout) );
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = NULL;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreatePipelineLayout(context.device, &pipelineLayoutCreateInfo, context.allocation_callbacks, &pipelineLayout) );

    // Load shaders
    VkShaderModule vertexShaderModule = stbvk_load_shader(&context, "tri.vert.spv");
    assert(vertexShaderModule != VK_NULL_HANDLE);
    VkShaderModule fragmentShaderModule = stbvk_load_shader(&context, "tri.frag.spv");
    assert(fragmentShaderModule != VK_NULL_HANDLE);

    // Load textures, create sampler and image view
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
    VULKAN_CHECK( vkCreateSampler(context.device, &samplerCreateInfo, context.allocation_callbacks, &sampler) );

    stbvk_image texture_image = {};
    int texture_load_error = stbvk_image_load_from_dds_file(&context, "trevor/trevor.dds", &texture_image);
    assert(texture_load_error == 0);

    // Create render pass
    enum
    {
        kColorAttachmentIndex = 0,
        kDepthAttachmentIndex = 1,
        kTextureAttachmentIndex = 2,
        kAttachmentCount
    };
    VkAttachmentDescription attachmentDescriptions[kAttachmentCount] = {};
    attachmentDescriptions[kColorAttachmentIndex].flags = 0;
    attachmentDescriptions[kColorAttachmentIndex].format = context.swapchain_surface_format.format; // TODO(cort): does this NEED to match the swapchain format?
    attachmentDescriptions[kColorAttachmentIndex].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[kColorAttachmentIndex].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[kColorAttachmentIndex].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[kColorAttachmentIndex].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[kColorAttachmentIndex].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[kColorAttachmentIndex].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[kColorAttachmentIndex].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachmentDescriptions[kDepthAttachmentIndex].flags = 0;
    attachmentDescriptions[kDepthAttachmentIndex].format = depth_image.image_view_create_info.format;
    attachmentDescriptions[kDepthAttachmentIndex].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[kDepthAttachmentIndex].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[kDepthAttachmentIndex].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[kDepthAttachmentIndex].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[kDepthAttachmentIndex].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[kDepthAttachmentIndex].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentDescriptions[kDepthAttachmentIndex].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentDescriptions[kTextureAttachmentIndex].flags = 0;
    attachmentDescriptions[kTextureAttachmentIndex].format = VK_FORMAT_R8G8B8A8_UNORM;//surfaceTextureFormat;
    attachmentDescriptions[kTextureAttachmentIndex].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[kTextureAttachmentIndex].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachmentDescriptions[kTextureAttachmentIndex].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[kTextureAttachmentIndex].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[kTextureAttachmentIndex].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[kTextureAttachmentIndex].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachmentDescriptions[kTextureAttachmentIndex].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference attachmentReferenceColor = {};
    attachmentReferenceColor.attachment = kColorAttachmentIndex;
    attachmentReferenceColor.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference attachmentReferenceDepth = {};
    attachmentReferenceDepth.attachment = kDepthAttachmentIndex;
    attachmentReferenceDepth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference attachmentReferenceTexture = {};
    attachmentReferenceTexture.attachment = kTextureAttachmentIndex;
    attachmentReferenceTexture.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkSubpassDescription subpassDescription = {};
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 1;
    subpassDescription.pInputAttachments = &attachmentReferenceTexture;
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
    VULKAN_CHECK( vkCreateRenderPass(context.device, &renderPassCreateInfo, context.allocation_callbacks, &renderPass) );

    // Create framebuffers
    // TODO(cort): is it desirable to create a framebuffer for every swap chain image,
    // to decouple the majority of application command buffers from the present queue?
    // Or is that an unnecessary image copy?
    VkImageView attachmentImageViews[kAttachmentCount] = {};
    attachmentImageViews[kColorAttachmentIndex] = VK_NULL_HANDLE; // filled in below;
    attachmentImageViews[kDepthAttachmentIndex] = depth_image.image_view;
    attachmentImageViews[kTextureAttachmentIndex] = texture_image.image_view;
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
    VkFramebuffer *framebuffers = (VkFramebuffer*)malloc(context.swapchain_image_count * sizeof(VkFramebuffer));
    for(uint32_t iFB=0; iFB<context.swapchain_image_count; iFB += 1) {
        attachmentImageViews[kColorAttachmentIndex] = context.swapchain_image_views[iFB];
        VULKAN_CHECK( vkCreateFramebuffer(context.device, &framebufferCreateInfo, context.allocation_callbacks, &framebuffers[iFB]) );
    }

    // Create Vulkan graphics pipeline
    stbvk_graphics_pipeline_settings_vsps graphicsPipelineSettings = {};
    graphicsPipelineSettings.vertex_buffer_layout = vertexBufferLayout;
    graphicsPipelineSettings.dynamic_state_mask = 0
        | (1<<VK_DYNAMIC_STATE_VIEWPORT)
        | (1<<VK_DYNAMIC_STATE_SCISSOR)
        ;
    graphicsPipelineSettings.primitive_topology = primitive_topology;
    graphicsPipelineSettings.pipeline_layout = pipelineLayout;
    graphicsPipelineSettings.render_pass = renderPass;
    graphicsPipelineSettings.subpass = 0;
    graphicsPipelineSettings.subpass_color_attachment_count = 1;
    graphicsPipelineSettings.vertex_shader = vertexShaderModule;
    graphicsPipelineSettings.fragment_shader = fragmentShaderModule;
    stbvk_graphics_pipeline_create_info graphicsPipelineCreateInfo = {};
    stbvk_prepare_graphics_pipeline_create_info_vsps(&graphicsPipelineSettings, &graphicsPipelineCreateInfo);
    VkPipeline pipelineGraphics = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateGraphicsPipelines(context.device, context.pipeline_cache, 1,
        &graphicsPipelineCreateInfo.graphics_pipeline_create_info,
        context.allocation_callbacks, &pipelineGraphics) );

    // Create Vulkan descriptor pool and descriptor set
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VULKAN_CHECK( stbvk_create_descriptor_pool(&context, &descriptorSetLayoutCreateInfo, 1, 0, &descriptorPool) );
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = NULL;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateDescriptorSets(context.device, &descriptorSetAllocateInfo, &descriptorSet) );
    VkDescriptorImageInfo descriptorImageInfos[kDemoTextureCount] = {0};
    for(uint32_t iTexture=0; iTexture<kDemoTextureCount; iTexture += 1) {
        descriptorImageInfos[iTexture].sampler = sampler;
        descriptorImageInfos[iTexture].imageView = texture_image.image_view;
        descriptorImageInfos[iTexture].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext = NULL;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.descriptorCount = kDemoTextureCount;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pImageInfo = descriptorImageInfos;
    vkUpdateDescriptorSets(context.device, 1, &writeDescriptorSet, 0, NULL);

    // Submit the setup command buffer
    VULKAN_CHECK( vkEndCommandBuffer(commandBuffer) );
    VkSubmitInfo submitInfoSetup = {};
    submitInfoSetup.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfoSetup.pNext = NULL;
    submitInfoSetup.waitSemaphoreCount = 0;
    submitInfoSetup.pWaitSemaphores = NULL;
    submitInfoSetup.pWaitDstStageMask = NULL;
    submitInfoSetup.commandBufferCount = 1;
    submitInfoSetup.pCommandBuffers = &commandBuffer;
    submitInfoSetup.signalSemaphoreCount = 0;
    submitInfoSetup.pSignalSemaphores = NULL;
    VkFence submitFence = VK_NULL_HANDLE;
    VULKAN_CHECK( vkQueueSubmit(context.graphics_queue, 1, &submitInfoSetup, submitFence) );
    VULKAN_CHECK( vkQueueWaitIdle(context.graphics_queue) );

    // Create the semaphores used to synchronize access to swapchain images
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = NULL;
    semaphoreCreateInfo.flags = 0;
    VkSemaphore swapchainImageReady = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateSemaphore(context.device, &semaphoreCreateInfo, context.allocation_callbacks, &swapchainImageReady) );
    VkSemaphore renderingComplete = VK_NULL_HANDLE;
    VULKAN_CHECK( vkCreateSemaphore(context.device, &semaphoreCreateInfo, context.allocation_callbacks, &renderingComplete) );

    uint32_t frameIndex = 0;

    const mathfu::mat4 clip_fixup(
        +1.0f, +0.0f, +0.0f, +0.0f,
        +0.0f, -1.0f, +0.0f, +0.0f,
        +0.0f, +0.0f, +0.5f, +0.5f,
        +0.0f, +0.0f, +0.0f, +1.0f);

    uint64_t counterStart = zomboClockTicks();
    while(!glfwWindowShouldClose(window)) {
        // Retrieve the index of the next available swapchain index
        VkFence presentCompleteFence = VK_NULL_HANDLE; // TODO(cort): unused
        VkResult result = vkAcquireNextImageKHR(context.device, context.swapchain, UINT64_MAX, swapchainImageReady,
            presentCompleteFence, &context.swapchain_image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
        } else if (result == VK_SUBOPTIMAL_KHR) {
            // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
        } else {
            VULKAN_CHECK(result);
        }

        // Draw!
        VkCommandBufferBeginInfo cmdBufDrawBeginInfo = {};
        cmdBufDrawBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufDrawBeginInfo.pNext = NULL;
        cmdBufDrawBeginInfo.flags = 0;
        cmdBufDrawBeginInfo.pInheritanceInfo = NULL;
        VULKAN_CHECK( vkBeginCommandBuffer(commandBuffer, &cmdBufDrawBeginInfo) );

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
        renderPassBeginInfo.framebuffer = framebuffers[context.swapchain_image_index];
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width  = kWindowWidthDefault;
        renderPassBeginInfo.renderArea.extent.height = kWindowHeightDefault;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;
        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineGraphics);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,1,&descriptorSet, 0,NULL);
        pushConstants.time[0] = (float)( zomboTicksToSeconds(zomboClockTicks() - counterStart) );
        mathfu::quat q = mathfu::quat::FromAngleAxis(pushConstants.time[0], mathfu::vec3(0,1,0));
        pushConstants.o2w = mathfu::mat4::Identity()
            * mathfu::mat4::FromTranslationVector( mathfu::vec3(0, -1.25f, -3.5f) )
            * q.ToMatrix4()
            //* mathfu::mat4::FromScaleVector( mathfu::vec3(0.1f, 0.1f, 0.1f) )
            ;
        pushConstants.proj = clip_fixup * mathfu::mat4::Perspective(
            (float)M_PI_4,
            (float)kWindowWidthDefault/(float)kWindowHeightDefault,
            0.01f, 100.0f);
        pushConstants.n2w = mathfu::mat4::Identity()
            * q.ToMatrix4()
            //* mathfu::mat4::FromScaleVector( mathfu::vec3(0.1f, 0.1f, 0.1f) )
            ;
        pushConstants.n2w = pushConstants.n2w.Inverse().Transpose();
        vkCmdPushConstants(commandBuffer, pipelineLayout, pushConstantRange.stageFlags,
            pushConstantRange.offset, pushConstantRange.size, &pushConstants);
        VkViewport viewport = {};
        viewport.width  = (float)kWindowWidthDefault;
        viewport.height = (float)kWindowHeightDefault;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0,1, &viewport);
        VkRect2D scissorRect = {};
        scissorRect.extent.width  = kWindowWidthDefault;
        scissorRect.extent.height = kWindowHeightDefault;
        scissorRect.offset.x = 0;
        scissorRect.offset.y = 0;
        vkCmdSetScissor(commandBuffer, 0,1, &scissorRect);
        const VkDeviceSize vertexBufferOffsets[1] = {};
        vkCmdBindVertexBuffers(commandBuffer, 0,1, &bufferVertices, vertexBufferOffsets);
        const VkDeviceSize indexBufferOffset = 0;
        vkCmdBindIndexBuffer(commandBuffer, bufferIndices, indexBufferOffset, indexType);
        const uint32_t indexCount = mesh_metadata.index_count;
        const uint32_t instanceCount = 1;
        vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0,0,0);

        vkCmdEndRenderPass(commandBuffer);
        VULKAN_CHECK( vkEndCommandBuffer(commandBuffer) );
        VkFence nullFence = VK_NULL_HANDLE;
        const VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submitInfoDraw = {};
        submitInfoDraw.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfoDraw.pNext = NULL;
        submitInfoDraw.waitSemaphoreCount = 1;
        submitInfoDraw.pWaitSemaphores = &swapchainImageReady;
        submitInfoDraw.pWaitDstStageMask = &pipelineStageFlags;
        submitInfoDraw.commandBufferCount = 1;
        submitInfoDraw.pCommandBuffers = &commandBuffer;
        submitInfoDraw.signalSemaphoreCount = 1;
        submitInfoDraw.pSignalSemaphores = &renderingComplete;
        VULKAN_CHECK( vkQueueSubmit(context.graphics_queue, 1, &submitInfoDraw, nullFence) );
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = NULL;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &context.swapchain;
        presentInfo.pImageIndices = &context.swapchain_image_index;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderingComplete;
        result = vkQueuePresentKHR(context.present_queue, &presentInfo); // TODO(cort): concurrent image access required?
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
        } else if (result == VK_SUBOPTIMAL_KHR) {
            // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
        } else {
            VULKAN_CHECK(result);
        }
        VULKAN_CHECK( vkQueueWaitIdle(context.present_queue) );

        glfwPollEvents();
        frameIndex += 1;
    }

    vkDeviceWaitIdle(context.device);

    vkDestroySemaphore(context.device, swapchainImageReady, context.allocation_callbacks);
    vkDestroySemaphore(context.device, renderingComplete, context.allocation_callbacks);

    for(uint32_t iFB=0; iFB<context.swapchain_image_count; iFB+=1) {
        vkDestroyFramebuffer(context.device, framebuffers[iFB], context.allocation_callbacks);
    }
    free(framebuffers);

    stbvk_image_destroy(&context, &depth_image);

    vkFreeMemory(context.device, bufferVerticesMemory, context.allocation_callbacks);
    vkDestroyBuffer(context.device, bufferVertices, context.allocation_callbacks);

    vkFreeMemory(context.device, bufferIndicesMemory, context.allocation_callbacks);
    vkDestroyBuffer(context.device, bufferIndices, context.allocation_callbacks);

    vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout, context.allocation_callbacks);
    vkDestroyDescriptorPool(context.device, descriptorPool, context.allocation_callbacks);

    vkDestroyRenderPass(context.device, renderPass, context.allocation_callbacks);

    vkDestroyShaderModule(context.device, vertexShaderModule, context.allocation_callbacks);
    vkDestroyShaderModule(context.device, fragmentShaderModule, context.allocation_callbacks);

    stbvk_image_destroy(&context, &texture_image);
    vkDestroySampler(context.device, sampler, context.allocation_callbacks);

    vkDestroyPipelineLayout(context.device, pipelineLayout, context.allocation_callbacks);
    vkDestroyPipeline(context.device, pipelineGraphics, context.allocation_callbacks);

    vkFreeCommandBuffers(context.device, context.command_pool, 1, &commandBuffer);

    glfwTerminate();
    stbvk_destroy_context(&context);
    return 0;
}
