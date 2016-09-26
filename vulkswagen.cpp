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

#define CDS_MESH_IMPLEMENTATION
#include "cds_mesh.h"

#include "vk_texture.h"

#define STB_VULKAN_IMPLEMENTATION
#include "stb_vulkan.h"

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
#define kVframeCount 2U

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
    applicationInfo.apiVersion = VK_MAKE_VERSION(1,0,21);
    const char *required_instance_layers[] = {
        "VK_LAYER_LUNARG_standard_validation", // TODO: fallback if standard_validation metalayer is not available
    };
    const char *optional_instance_layers[] = {
#if !defined(NDEBUG)
        // Do not explicitly enable! only needed to test VK_EXT_debug_marker support, and may generate other
        // spurious errors.
        //"VK_LAYER_RENDERDOC_Capture",
#endif
        "" // placeholder; empty initializers arrays aren't allowed
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
    };
    const char *optional_instance_extensions[] = {
#if !defined(NDEBUG)
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
        "" // placeholder; empty initializers arrays aren't allowed
    };
    const char *required_device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    const char *optional_device_extensions[] = {
#if !defined(NDEBUG) && defined(VK_EXT_debug_marker)
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME, // will only be enabled if a layer supports it (currently, only RenderDoc's implicit layer)
#endif
        ""  // placeholder; empty initializers arrays aren't allowed
    };
    stbvk_context_create_info contextCreateInfo = {};
    contextCreateInfo.allocation_callbacks = NULL;
    contextCreateInfo.required_instance_layer_count     = sizeof(required_instance_layers) / sizeof(required_instance_layers[0]);
    contextCreateInfo.required_instance_layer_names     = required_instance_layers;
    contextCreateInfo.required_instance_extension_count = sizeof(required_instance_extensions) / sizeof(required_instance_extensions[0]);
    contextCreateInfo.required_instance_extension_names = required_instance_extensions;
    contextCreateInfo.required_device_extension_count   = sizeof(required_device_extensions) / sizeof(required_device_extensions[0]);
    contextCreateInfo.required_device_extension_names   = required_device_extensions;
    contextCreateInfo.optional_instance_layer_count     = sizeof(optional_instance_layers) / sizeof(optional_instance_layers[0]);
    contextCreateInfo.optional_instance_layer_names     = optional_instance_layers;
    contextCreateInfo.optional_instance_extension_count = sizeof(optional_instance_extensions) / sizeof(optional_instance_extensions[0]);
    contextCreateInfo.optional_instance_extension_names = optional_instance_extensions;
    contextCreateInfo.optional_device_extension_count   = sizeof(optional_device_extensions) / sizeof(optional_device_extensions[0]);
    contextCreateInfo.optional_device_extension_names   = optional_device_extensions;
    contextCreateInfo.application_info = &applicationInfo;
    contextCreateInfo.debug_report_callback = debugReportCallbackFunc;
    contextCreateInfo.debug_report_flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    contextCreateInfo.debug_report_callback_user_data = NULL;
#if defined(DISABLE_VALIDATION_LAYERS)
    contextCreateInfo.required_instance_layer_count = 0;
    contextCreateInfo.optional_instance_layer_count = 0;
    contextCreateInfo.debug_report_callback = NULL;
    contextCreateInfo.debug_report_flags = 0;
#endif
    stbvk_context context = {};
    VULKAN_CHECK(my_stbvk_init_context(&contextCreateInfo, window, &context));

    stbvk_device_memory_arena *device_arena = NULL;

    // Allocate command buffers
    VkCommandPoolCreateInfo command_pool_ci = {};
    command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_ci.pNext = NULL;
    command_pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT // not reusing command buffer contents yet!
        | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // needed to call begin without an explicit reset
    command_pool_ci.queueFamilyIndex = context.graphics_queue_family_index;
    VkCommandPool command_pool = stbvk_create_command_pool(&context, &command_pool_ci, "Command Pool");
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = NULL;
    commandBufferAllocateInfo.commandPool = command_pool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = kVframeCount;
    VkCommandBuffer *commandBuffers = (VkCommandBuffer*)malloc(commandBufferAllocateInfo.commandBufferCount * sizeof(VkCommandBuffer));
    VULKAN_CHECK( vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, commandBuffers) );

    // Create depth buffer
    VkImageCreateInfo depth_image_create_info = {};
    depth_image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depth_image_create_info.imageType = VK_IMAGE_TYPE_2D;
    depth_image_create_info.format = VK_FORMAT_UNDEFINED; // filled in below
    depth_image_create_info.extent = {kWindowWidthDefault, kWindowHeightDefault, 1};
    depth_image_create_info.mipLevels = 1;
    depth_image_create_info.arrayLayers = 1;
    depth_image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    depth_image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depth_image_create_info.queueFamilyIndexCount = 0;
    depth_image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    const VkFormat depth_format_candidates[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
    };
    for(auto format : depth_format_candidates)
    {
        VkFormatProperties format_properties = {};
        vkGetPhysicalDeviceFormatProperties(context.physical_device, format, &format_properties);
        if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
        {
            depth_image_create_info.format = format;
            break;
        }
    }
    assert(depth_image_create_info.format != VK_FORMAT_UNDEFINED);
    VkImage depth_image = stbvk_create_image(&context, &depth_image_create_info, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        "depth buffer image");
    VkDeviceMemory depth_image_mem = VK_NULL_HANDLE;
    VkDeviceSize depth_image_mem_offset = 0;
    VULKAN_CHECK(stbvk_allocate_and_bind_image_memory(&context, depth_image, device_arena,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "depth buffer image memory",
        &depth_image_mem, &depth_image_mem_offset));
    VkImageView depth_image_view = stbvk_create_image_view_from_image(&context, depth_image,
        &depth_image_create_info, "depth buffer image view");

    cdsm_metadata_t mesh_metadata = {};
    size_t mesh_vertices_size = 0, mesh_indices_size = 0;
    enum
    {
        MESH_TYPE_CUBE     = 0,
        MESH_TYPE_SPHERE   = 1,
        MESH_TYPE_AXES     = 3,
        MESH_TYPE_CYLINDER = 2,
    } meshType = MESH_TYPE_CUBE;
    cdsm_cube_recipe_t cube_recipe = {};
    cube_recipe.min_extent = {-0.2f,-0.2f,-0.2f};
    cube_recipe.max_extent = {+0.2f,+0.2f,+0.2f};
    cdsm_sphere_recipe_t sphere_recipe = {};
    sphere_recipe.latitudinal_segments = 30;
    sphere_recipe.longitudinal_segments = 30;
    sphere_recipe.radius = 0.2f;
    cdsm_cylinder_recipe_t cylinder_recipe = {};
    cylinder_recipe.length = 0.3f;
    cylinder_recipe.axial_segments = 3;
    cylinder_recipe.radial_segments = 60;
    cylinder_recipe.radius0 = 0.3f;
    cylinder_recipe.radius1 = 0.4f;
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
    bufferCreateInfoIndices.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferCreateInfoIndices.flags = 0;
    VkBuffer bufferIndices = stbvk_create_buffer(&context, &bufferCreateInfoIndices, "index buffer");
    VkDeviceMemory bufferIndicesMem = VK_NULL_HANDLE;
    VkDeviceSize bufferIndicesMemOffset = 0;
    VULKAN_CHECK(stbvk_allocate_and_bind_buffer_memory(&context, bufferIndices, device_arena,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "index buffer memory", &bufferIndicesMem, &bufferIndicesMemOffset));

    // Define vertex stream layouts
    const cdsm_vertex_layout_t src_vertex_layout = {
        32, 3, {
            {0, 0, CDSM_ATTRIBUTE_FORMAT_R32G32B32_FLOAT},
            {1, 12, CDSM_ATTRIBUTE_FORMAT_R32G32B32_FLOAT},
            {2,24, CDSM_ATTRIBUTE_FORMAT_R32G32_FLOAT},
        }
    };
    const cdsm_vertex_layout_t dst_vertex_layout = {
        22, 3, {
            {0, 0, CDSM_ATTRIBUTE_FORMAT_R32G32B32_FLOAT},
            {1, 12, CDSM_ATTRIBUTE_FORMAT_R16G16B16_SNORM},
            {2,18, CDSM_ATTRIBUTE_FORMAT_R16G16_FLOAT},
        }
    };
    stbvk_vertex_buffer_layout vertexBufferLayout = {};
    vertexBufferLayout.stride = dst_vertex_layout.stride;
    vertexBufferLayout.attribute_count = dst_vertex_layout.attribute_count;
    vertexBufferLayout.attributes[0].binding = 0;
    vertexBufferLayout.attributes[0].location = 0;
    vertexBufferLayout.attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexBufferLayout.attributes[0].offset = dst_vertex_layout.attributes[0].offset;
    vertexBufferLayout.attributes[1].binding = 0;
    vertexBufferLayout.attributes[1].location = 1;
    vertexBufferLayout.attributes[1].format = VK_FORMAT_R16G16B16_SNORM; // TODO(cort): convert from CDSM_* enum
    vertexBufferLayout.attributes[1].offset = dst_vertex_layout.attributes[1].offset;
    vertexBufferLayout.attributes[2].binding = 0;
    vertexBufferLayout.attributes[2].location = 2;
    vertexBufferLayout.attributes[2].format = VK_FORMAT_R16G16_SFLOAT;
    vertexBufferLayout.attributes[2].offset = dst_vertex_layout.attributes[2].offset;

    // Create vertex buffer
    VkBufferCreateInfo bufferCreateInfoVertices = {};
    bufferCreateInfoVertices.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfoVertices.pNext = NULL;
    bufferCreateInfoVertices.size = mesh_metadata.vertex_count * dst_vertex_layout.stride;
    bufferCreateInfoVertices.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferCreateInfoVertices.flags = 0;
    VkBuffer bufferVertices = stbvk_create_buffer(&context, &bufferCreateInfoVertices, "vertex buffer");
    VkDeviceMemory bufferVerticesMem = VK_NULL_HANDLE;
    VkDeviceSize bufferVerticesMemOffset = 0;
    VULKAN_CHECK(stbvk_allocate_and_bind_buffer_memory(&context, bufferVertices, device_arena,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "vertex buffer memory", &bufferVerticesMem, &bufferVerticesMemOffset));

    // Populate vertex/index buffers
    void *index_buffer_contents  = malloc(mesh_indices_size);
    void *vertex_buffer_contents_temp = malloc(mesh_vertices_size);
    void *vertex_buffer_contents = malloc(mesh_metadata.vertex_count * dst_vertex_layout.stride);
    if      (meshType == MESH_TYPE_CUBE)
        cdsm_create_cube(&mesh_metadata, (cdsm_vertex_t*)vertex_buffer_contents_temp, &mesh_vertices_size,
            (cdsm_index_t*)index_buffer_contents, &mesh_indices_size, &cube_recipe);
    else if (meshType == MESH_TYPE_SPHERE)
        cdsm_create_sphere(&mesh_metadata, (cdsm_vertex_t*)vertex_buffer_contents_temp, &mesh_vertices_size,
            (cdsm_index_t*)index_buffer_contents, &mesh_indices_size, &sphere_recipe);
    else if (meshType == MESH_TYPE_AXES)
        cdsm_create_axes(&mesh_metadata, (cdsm_vertex_t*)vertex_buffer_contents_temp, &mesh_vertices_size,
            (cdsm_index_t*)index_buffer_contents, &mesh_indices_size, &axes_recipe);
    else if (meshType == MESH_TYPE_CYLINDER)
        cdsm_create_cylinder(&mesh_metadata, (cdsm_vertex_t*)vertex_buffer_contents_temp, &mesh_vertices_size,
            (cdsm_index_t*)index_buffer_contents, &mesh_indices_size, &cylinder_recipe);
    cdsm_convert_vertex_buffer(vertex_buffer_contents_temp, &src_vertex_layout,
        vertex_buffer_contents, &dst_vertex_layout, mesh_metadata.vertex_count);
    free(vertex_buffer_contents_temp);

    VULKAN_CHECK( stbvk_buffer_load_contents(&context, bufferIndices, &bufferCreateInfoIndices,
        0, index_buffer_contents, mesh_indices_size, VK_ACCESS_INDEX_READ_BIT) );
    VULKAN_CHECK( stbvk_buffer_load_contents(&context, bufferVertices, &bufferCreateInfoVertices,
        0, vertex_buffer_contents, mesh_metadata.vertex_count * dst_vertex_layout.stride,
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) );
    free(index_buffer_contents);
    free(vertex_buffer_contents);

    // Create buffer of per-mesh object-to-world matrices. TODO(cort): This should be per-vframe.
    const uint32_t mesh_count = 1024;
    VkBufferCreateInfo o2w_buffer_create_info = {};
    o2w_buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    o2w_buffer_create_info.pNext = NULL;
    o2w_buffer_create_info.size = mesh_count * sizeof(mathfu::mat4);
    o2w_buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    o2w_buffer_create_info.flags = 0;
    VkBuffer o2w_buffer = stbvk_create_buffer(&context, &o2w_buffer_create_info, "o2w buffer");
    VkDeviceMemory o2w_buffer_mem = VK_NULL_HANDLE;
    VkDeviceSize o2w_buffer_mem_offset = 0;
    VULKAN_CHECK(stbvk_allocate_and_bind_buffer_memory(&context, o2w_buffer, device_arena,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "o2w buffer memory",
        &o2w_buffer_mem, &o2w_buffer_mem_offset));

    // Create push constants.  TODO(cort): this should be a per-vframe uniform buffer.
    struct {
        mathfu::vec4_packed time; // .x=seconds, .yzw=???
        mathfu::vec4_packed eye;  // .xyz=world-space eye position, .w=???
        mathfu::mat4 viewproj;
    } pushConstants = {};
    assert(sizeof(pushConstants) <= context.physical_device_properties.limits.maxPushConstantsSize);
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(pushConstants);

    // Create Vulkan descriptor layout & pipeline layout
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {};
    descriptorSetLayoutBindings[0].binding = 0;
    descriptorSetLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorSetLayoutBindings[0].descriptorCount = kDemoTextureCount;
    descriptorSetLayoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayoutBindings[0].pImmutableSamplers = NULL;
    descriptorSetLayoutBindings[1].binding = 1;
    descriptorSetLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorSetLayoutBindings[1].descriptorCount = 1;
    descriptorSetLayoutBindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    descriptorSetLayoutBindings[1].pImmutableSamplers = NULL;
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = NULL;
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.bindingCount = sizeof(descriptorSetLayoutBindings) / sizeof(descriptorSetLayoutBindings[0]);
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
    VkDescriptorSetLayout descriptorSetLayout = stbvk_create_descriptor_set_layout(&context, &descriptorSetLayoutCreateInfo, "descriptor set layout");
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = NULL;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    VkPipelineLayout pipelineLayout = stbvk_create_pipeline_layout(&context, &pipelineLayoutCreateInfo, "pipeline layout");

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
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = 16;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    VkSampler sampler= stbvk_create_sampler(&context, &samplerCreateInfo, "default sampler");

    const char *texture_filename = "trevor/redf.ktx";
    VkImage texture_image = VK_NULL_HANDLE;
    VkImageCreateInfo texture_image_create_info;
    VkDeviceMemory texture_image_mem = VK_NULL_HANDLE;
    VkDeviceSize texture_image_mem_offset = 0;
    int texture_load_error = load_vkimage_from_file(&texture_image, &texture_image_create_info,
        &texture_image_mem, &texture_image_mem_offset, &context, texture_filename, VK_TRUE,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
    assert(!texture_load_error); (void)texture_load_error;
    VkImageView texture_image_view = stbvk_create_image_view_from_image(&context, texture_image,
        &texture_image_create_info, "texture image view");

    // Create render pass
    enum
    {
        kColorAttachmentIndex = 0,
        kDepthAttachmentIndex = 1,
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
    attachmentDescriptions[kDepthAttachmentIndex].format = depth_image_create_info.format;
    attachmentDescriptions[kDepthAttachmentIndex].samples = depth_image_create_info.samples;
    attachmentDescriptions[kDepthAttachmentIndex].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[kDepthAttachmentIndex].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[kDepthAttachmentIndex].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[kDepthAttachmentIndex].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[kDepthAttachmentIndex].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[kDepthAttachmentIndex].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference attachmentReferenceColor = {};
    attachmentReferenceColor.attachment = kColorAttachmentIndex;
    attachmentReferenceColor.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference attachmentReferenceDepth = {};
    attachmentReferenceDepth.attachment = kDepthAttachmentIndex;
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
    VkRenderPass renderPass = stbvk_create_render_pass(&context, &renderPassCreateInfo, "default render pass");

    // Create framebuffers
    VkImageView attachmentImageViews[kAttachmentCount] = {};
    attachmentImageViews[kColorAttachmentIndex] = VK_NULL_HANDLE; // filled in below;
    attachmentImageViews[kDepthAttachmentIndex] = depth_image_view;
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
        framebuffers[iFB] = stbvk_create_framebuffer(&context, &framebufferCreateInfo, "default framebuffer");
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
    if (mesh_metadata.front_face == CDSM_FRONT_FACE_CW)
        graphicsPipelineCreateInfo.rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    VkPipeline pipelineGraphics = stbvk_create_graphics_pipeline(&context,
        &graphicsPipelineCreateInfo.graphics_pipeline_create_info, "default graphics pipeline");

    // Create Vulkan descriptor pool and descriptor set.
    // TODO(cort): the current descriptors are constant; we'd need a set per swapchain if it was going to change
    // per-frame.
    VkDescriptorPool descriptorPool = stbvk_create_descriptor_pool_from_layout(&context,
        &descriptorSetLayoutCreateInfo, 1, 0, "Descriptor pool");
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = NULL;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VULKAN_CHECK( vkAllocateDescriptorSets(context.device, &descriptorSetAllocateInfo, &descriptorSet) );
    VULKAN_CHECK(stbvk_name_descriptor_set(context.device, descriptorSet, "default descriptor set"));
    VkDescriptorImageInfo descriptorImageInfos[kDemoTextureCount] = {0};
    for(uint32_t iTexture=0; iTexture<kDemoTextureCount; iTexture += 1) {
        descriptorImageInfos[iTexture].sampler = sampler;
        descriptorImageInfos[iTexture].imageView = texture_image_view;
        descriptorImageInfos[iTexture].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext = NULL;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.descriptorCount = kDemoTextureCount;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pImageInfo = descriptorImageInfos;
    vkUpdateDescriptorSets(context.device, 1, &writeDescriptorSet, 0, NULL);
    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = o2w_buffer;
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range = VK_WHOLE_SIZE;
    writeDescriptorSet.dstBinding = 1;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
    vkUpdateDescriptorSets(context.device, 1,&writeDescriptorSet, 0,NULL);

    // Create the semaphores used to synchronize access to swapchain images
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = NULL;
    semaphoreCreateInfo.flags = 0;
    VkSemaphore swapchainImageReady = stbvk_create_semaphore(&context, &semaphoreCreateInfo, "image ready semaphore");
    VkSemaphore renderingComplete = stbvk_create_semaphore(&context, &semaphoreCreateInfo, "rendering complete semaphore");

    // Create the fences used to wait for each swapchain image's command buffer to be submitted.
    // This prevents re-writing the command buffer contents before it's been submitted and processed.
    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.pNext = NULL;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkFence *queue_submitted_fences = (VkFence*)malloc(kVframeCount * sizeof(VkFence));
    for(uint32_t iFence=0; iFence<kVframeCount; ++iFence)
    {
        queue_submitted_fences[iFence] = stbvk_create_fence(&context, &fence_create_info, "queue submitted fence");
    }
    uint32_t frameIndex = 0;

    const mathfu::mat4 clip_fixup(
        +1.0f, +0.0f, +0.0f, +0.0f,
        +0.0f, -1.0f, +0.0f, +0.0f,
        +0.0f, +0.0f, +0.5f, +0.5f,
        +0.0f, +0.0f, +0.0f, +1.0f);

    // Create timestamp query pools.
    typedef enum
    {
        TIMESTAMP_ID_BEGIN_FRAME = 0,
        TIMESTAMP_ID_END_FRAME = 1,
        TIMESTAMP_ID_RANGE_SIZE
    } TimestampId;
    VkQueryPoolCreateInfo timestamp_query_pool_create_info = {};
    timestamp_query_pool_create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    timestamp_query_pool_create_info.pNext = NULL;
    timestamp_query_pool_create_info.flags = 0;
    timestamp_query_pool_create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    timestamp_query_pool_create_info.queryCount = TIMESTAMP_ID_RANGE_SIZE;
    timestamp_query_pool_create_info.pipelineStatistics = 0;
    VkQueryPool *timestamp_query_pools = (VkQueryPool*)malloc(kVframeCount * sizeof(VkQueryPool));
    for(uint32_t iPool=0; iPool<kVframeCount; ++iPool)
    {
        timestamp_query_pools[iPool] = stbvk_create_query_pool(&context, &timestamp_query_pool_create_info, "timestamp query pool");
    }
    uint64_t counterStart = zomboClockTicks();
    double timestampSecondsPrevious[TIMESTAMP_ID_RANGE_SIZE] = {};
    uint32_t vframeIndex = 0;
    while(!glfwWindowShouldClose(window)) {
        // Wait for the command buffer previously used to generate this swapchain image to be submitted.
        // TODO(cort): this does not guarantee memory accesses from that submission will be visible on the host;
        // there'd need to be a memory barrier for that.
        vkWaitForFences(context.device, 1, &queue_submitted_fences[vframeIndex], VK_TRUE, UINT64_MAX);
        vkResetFences(context.device, 1, &queue_submitted_fences[vframeIndex]);

        // The host can now safely reset and rebuild this command buffer, even if the GPU hasn't finished presenting the
        // resulting frame yet. The swapchainImageReady semaphore handles waiting for the present to complete before the
        // new command buffer contents are submitted.
        // The host could also wait for the previous present to complete by passing a VkFence to vkAcquireNextImageKHR()
        // and waiting on it, but that would only be necessary if the host were going to be modifying/destroying the
        // swapchain image or using its contents.
        VkCommandBuffer commandBuffer = commandBuffers[vframeIndex];
        VkQueryPool timestamp_query_pool = timestamp_query_pools[vframeIndex];

        // Read the timestamp query results from the frame that was just presented. We'll process and report them later,
        // but we need to get the old data out before we reuse the query pool for the current frame.
        // TODO(cort): We can't pass VK_QUERY_RESULT_WAIT_BIT, because it hangs forever on the first frame.
        // We either need to seed some dummy values or just accept that the result for the first few frames may be VK_NOT_READY.
        uint64_t timestamps[TIMESTAMP_ID_RANGE_SIZE] = {};
        VkQueryResultFlags query_result_flags = VK_QUERY_RESULT_64_BIT;
        VkResult get_timestamps_result = vkGetQueryPoolResults(context.device, timestamp_query_pool,
            0,TIMESTAMP_ID_RANGE_SIZE, sizeof(timestamps),
            timestamps, sizeof(timestamps[0]), query_result_flags);
        assert(get_timestamps_result == VK_SUCCESS || get_timestamps_result == VK_NOT_READY);

        // Update object-to-world matrices.
        // TODO(cort): multi-buffer this data. And maybe make it device-local, with a staging buffer?
        // No memory barrier needed if it's HOST_COHERENT.
        const float seconds_elapsed = (float)( zomboTicksToSeconds(zomboClockTicks() - counterStart) );
        VkMemoryMapFlags o2w_buffer_map_flags = 0;
        mathfu::vec4 *mapped_o2w_buffer = NULL;
        VULKAN_CHECK(vkMapMemory(context.device, o2w_buffer_mem, 0, o2w_buffer_create_info.size,
            o2w_buffer_map_flags, (void**)&mapped_o2w_buffer));
        for(int iMesh=0; iMesh<mesh_count; ++iMesh)
        {
            mathfu::quat q = mathfu::quat::FromAngleAxis(seconds_elapsed + (float)iMesh, mathfu::vec3(0,1,0));
            mathfu::mat4 o2w = mathfu::mat4::Identity()
                * mathfu::mat4::FromTranslationVector(mathfu::vec3(
                    4.0f * cosf((1.0f+0.001f*iMesh) * seconds_elapsed + float(149*iMesh) + 0.0f) + 0.0f,
                    2.5f * sinf(1.5f * seconds_elapsed + float(13*iMesh) + 5.0f) + 0.0f,
                    3.0f * sinf(0.25f * seconds_elapsed + float(51*iMesh) + 2.0f) - 2.0f
                    ))
                * q.ToMatrix4()
                //* mathfu::mat4::FromScaleVector( mathfu::vec3(0.1f, 0.1f, 0.1f) )
                ;
            mapped_o2w_buffer[iMesh*4+0] = mathfu::vec4(o2w[ 0], o2w[ 1], o2w[ 2], o2w[ 3]);
            mapped_o2w_buffer[iMesh*4+1] = mathfu::vec4(o2w[ 4], o2w[ 5], o2w[ 6], o2w[ 7]);
            mapped_o2w_buffer[iMesh*4+2] = mathfu::vec4(o2w[ 8], o2w[ 9], o2w[10], o2w[11]);
            mapped_o2w_buffer[iMesh*4+3] = mathfu::vec4(o2w[12], o2w[13], o2w[14], o2w[15]);
        }
        vkUnmapMemory(context.device, o2w_buffer_mem);

        // Retrieve the index of the next available swapchain index
        uint32_t swapchain_image_index = UINT32_MAX;
        VkFence image_acquired_fence = VK_NULL_HANDLE; // currently unused, but if you want the CPU to wait for an image to be acquired...
        VkResult result = vkAcquireNextImageKHR(context.device, context.swapchain, UINT64_MAX, swapchainImageReady,
            image_acquired_fence, &swapchain_image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
        } else if (result == VK_SUBOPTIMAL_KHR) {
            // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
        } else {
            VULKAN_CHECK(result);
        }
        VkFramebuffer framebuffer = framebuffers[swapchain_image_index];

        // Draw! (record secondary command buffer)
        VkCommandBufferBeginInfo cmdBufDrawBeginInfo = {};
        cmdBufDrawBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufDrawBeginInfo.pNext = NULL;
        cmdBufDrawBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmdBufDrawBeginInfo.pInheritanceInfo = NULL;
        VULKAN_CHECK( vkBeginCommandBuffer(commandBuffer, &cmdBufDrawBeginInfo) );
        vkCmdResetQueryPool(commandBuffer, timestamp_query_pool, 0, TIMESTAMP_ID_RANGE_SIZE);
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestamp_query_pool, TIMESTAMP_ID_BEGIN_FRAME);

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
        renderPassBeginInfo.framebuffer = framebuffer;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width  = kWindowWidthDefault;
        renderPassBeginInfo.renderArea.extent.height = kWindowHeightDefault;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;
        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineGraphics);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,1,&descriptorSet, 0,NULL);
        pushConstants.time = mathfu::vec4(seconds_elapsed, 0, 0, 0);
        pushConstants.eye = mathfu::vec4(
            0.0f,
            2.0f,
            6.0f,
            0);
        mathfu::mat4 w2v = mathfu::mat4::LookAt(
            mathfu::vec3(0,0,0), // target
            mathfu::vec4(pushConstants.eye).xyz(),
            mathfu::vec3(0,1,0), // up
            1.0f); // right-handed
        pushConstants.viewproj = clip_fixup * mathfu::mat4::Perspective(
            (float)M_PI_4,
            (float)kWindowWidthDefault/(float)kWindowHeightDefault,
            0.01f, 100.0f) * w2v;
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
        const uint32_t instance_count = mesh_count;
        vkCmdDrawIndexed(commandBuffer, indexCount, instance_count, 0,0,0);

        vkCmdEndRenderPass(commandBuffer);
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamp_query_pool, TIMESTAMP_ID_END_FRAME);
        VULKAN_CHECK( vkEndCommandBuffer(commandBuffer) );
        const VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = NULL;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &swapchainImageReady;
        submitInfo.pWaitDstStageMask = &pipelineStageFlags;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderingComplete;
        VULKAN_CHECK( vkQueueSubmit(context.graphics_queue, 1, &submitInfo, queue_submitted_fences[vframeIndex]) );
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = NULL;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &context.swapchain;
        presentInfo.pImageIndices = &swapchain_image_index;
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

        // TODO(cort): Now that the CPU and GPU execution is decoupled, we may need to emit a memory barrier to
        // make these query results visible to the host at the end of the submit.
        if (get_timestamps_result == VK_SUCCESS)
        {
            double timestampSeconds[TIMESTAMP_ID_RANGE_SIZE] = {};
            for(int iTS=0; iTS<TIMESTAMP_ID_RANGE_SIZE; ++iTS)
            {
                if (context.graphics_queue_family_properties.timestampValidBits < sizeof(timestamps[0])*8)
                    timestamps[iTS] &= ((1ULL<<context.graphics_queue_family_properties.timestampValidBits)-1);
                timestampSeconds[iTS] = (double)(timestamps[iTS]) * (double)context.physical_device_properties.limits.timestampPeriod / 1e9;
            }
            if ((frameIndex % 100)==0)
                printf("GPU T2B=%10.6f\tT2T=%10.6f\n", (timestampSeconds[TIMESTAMP_ID_END_FRAME] - timestampSeconds[TIMESTAMP_ID_BEGIN_FRAME])*1000.0,
                    (timestampSeconds[TIMESTAMP_ID_BEGIN_FRAME] - timestampSecondsPrevious[TIMESTAMP_ID_BEGIN_FRAME])*1000.0);
            memcpy(timestampSecondsPrevious, timestampSeconds, sizeof(timestampSecondsPrevious));
        }

        glfwPollEvents();
        frameIndex += 1;
        vframeIndex += 1;
        if (vframeIndex == kVframeCount) {
            vframeIndex = 0;
        }
    }

    // NOTE: vkDeviceWaitIdle() only waits for all queue operations to complete; swapchain images may still be
    // queued for presentation when this function returns. To avoid a race condition when freeing them and
    // their associated semaphores, the host needs to...acquire each swapchain image, passing a VkFence and
    // immediately waiting on it? Is that sufficient? I don't think so; no guarantee of acquiring all images.
    // It looks like this is still under discussion (Khronos Vulkan issue #243, #245)
    vkDeviceWaitIdle(context.device);

    for(uint32_t iPool=0; iPool<kVframeCount; ++iPool) {
        stbvk_destroy_query_pool(&context, timestamp_query_pools[iPool]);
    }
    free(timestamp_query_pools);

    for(uint32_t iFence=0; iFence<kVframeCount; ++iFence) {
        stbvk_destroy_fence(&context, queue_submitted_fences[iFence]);
    }
    free(queue_submitted_fences);

    stbvk_destroy_semaphore(&context, swapchainImageReady);
    stbvk_destroy_semaphore(&context, renderingComplete);

    for(uint32_t iFB=0; iFB<context.swapchain_image_count; iFB+=1) {
        stbvk_destroy_framebuffer(&context, framebuffers[iFB]);
    }
    free(framebuffers);

    stbvk_free_device_memory(&context, device_arena, depth_image_mem, depth_image_mem_offset);
    stbvk_destroy_image_view(&context, depth_image_view);
    stbvk_destroy_image(&context, depth_image);

    stbvk_free_device_memory(&context, device_arena, o2w_buffer_mem, o2w_buffer_mem_offset);
    stbvk_destroy_buffer(&context, o2w_buffer);
    stbvk_free_device_memory(&context, device_arena, bufferIndicesMem, bufferIndicesMemOffset);
    stbvk_destroy_buffer(&context, bufferIndices);
    stbvk_free_device_memory(&context, device_arena, bufferVerticesMem, bufferVerticesMemOffset);
    stbvk_destroy_buffer(&context, bufferVertices);

    stbvk_destroy_descriptor_set_layout(&context, descriptorSetLayout);
    stbvk_destroy_descriptor_pool(&context, descriptorPool);

    stbvk_destroy_render_pass(&context, renderPass);

    stbvk_destroy_shader(&context, vertexShaderModule);
    stbvk_destroy_shader(&context, fragmentShaderModule);


    stbvk_free_device_memory(&context, device_arena, texture_image_mem, texture_image_mem_offset);
    stbvk_destroy_image_view(&context, texture_image_view);
    stbvk_destroy_image(&context, texture_image);
    stbvk_destroy_sampler(&context, sampler);

    stbvk_destroy_pipeline_layout(&context, pipelineLayout);
    stbvk_destroy_pipeline(&context, pipelineGraphics);

    free(commandBuffers);
    stbvk_destroy_command_pool(&context, command_pool);

    glfwTerminate();
    stbvk_destroy_context(&context);
    return 0;
}
