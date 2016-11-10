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

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <mathfu/vector.h>
#include <mathfu/glsl_mappings.h>

#include "vk_texture.h"

#include "camera.h"

#define CDS_MESH_IMPLEMENTATION
#include "cds_mesh.h"

#define CDS_VULKAN_IMPLEMENTATION
#include "cds_vulkan.hpp"

#include <assert.h>

namespace {
    void my_glfw_error_callback(int error, const char *description) {
        fprintf( stderr, "GLFW Error %d: %s\n", error, description);
    }

    class InputState {
    public:
        explicit InputState(const std::shared_ptr<GLFWwindow>& window)
                : window_(window)
                , current_{}
                , prev_{} {
        }
        ~InputState() = default;

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
    void InputState::Update(void) {
        std::shared_ptr<GLFWwindow> w = window_.lock();
        assert(w != nullptr);
        GLFWwindow *pw = w.get();

        prev_ = current_;

        current_.digital[DIGITAL_LPAD_UP] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_W));
        current_.digital[DIGITAL_LPAD_LEFT] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_A));
        current_.digital[DIGITAL_LPAD_RIGHT] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_D));
        current_.digital[DIGITAL_LPAD_DOWN] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_S));

        double mx = 0, my = 0;
        glfwGetCursorPos(pw, &mx, &my);
        current_.analog[ANALOG_MOUSE_X] = (float)mx;
        current_.analog[ANALOG_MOUSE_Y] = (float)my;
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
    const uint32_t kVframeCount = 2U;
}  // namespace

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    const float fovDegrees = 45.0f;
    const float zNear = 0.01f;
    const float zFar = 100.0f;
    CameraPersp camera(kWindowWidthDefault, kWindowHeightDefault, fovDegrees, zNear, zFar);
    const mathfu::vec3 initial_camera_pos(-2, 0, 6);
    const mathfu::vec3 initial_camera_target(0, 0, 0);
    const mathfu::vec3 initial_camera_up(0,1,0);
    camera.lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
    CameraDolly dolly(camera);

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
    auto window = std::shared_ptr<GLFWwindow>(
        glfwCreateWindow(kWindowWidthDefault, kWindowHeightDefault, application_name.c_str(), NULL, NULL),
        [](GLFWwindow *w){ glfwDestroyWindow(w); });
    glfwSetInputMode(window.get(), GLFW_STICKY_KEYS, 1);
    glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwPollEvents(); // dummy poll for first loop iteration

    InputState input_state(window);

    VkApplicationInfo application_info = {};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = application_name.c_str();
    application_info.applicationVersion = 0x1000;
    application_info.pEngineName = engine_name.c_str();
    application_info.engineVersion = 0x1001;
    application_info.apiVersion = VK_MAKE_VERSION(1,0,30);
    cdsvk::ContextCreateInfo context_ci = {};
    context_ci.allocation_callbacks = nullptr;
    context_ci.required_instance_layer_names = {
#if !defined(NDEBUG)
        "VK_LAYER_LUNARG_standard_validation",// TODO: fallback if standard_validation metalayer is not available
#endif
    };
    context_ci.optional_instance_layer_names = {
#if !defined(NDEBUG)
        // Do not explicitly enable! only needed to test VK_EXT_debug_marker support, and may generate other
        // spurious errors. Or set ENABLE_VULKAN_RENDERDOC_CAPTURE=1 in the environment variables.
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
    context_ci.get_vk_surface_userdata = window.get();
    context_ci.application_info = &application_info;
    context_ci.debug_report_callback = my_debug_report_callback;
    context_ci.debug_report_flags = 0
        | VK_DEBUG_REPORT_ERROR_BIT_EXT
        | VK_DEBUG_REPORT_WARNING_BIT_EXT
        | VK_DEBUG_REPORT_INFORMATION_BIT_EXT
        | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
        ;
    cdsvk::Context *context = new cdsvk::Context(context_ci);

    // Allocate command buffers
    VkCommandPoolCreateInfo cpool_ci = {};
    cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpool_ci.queueFamilyIndex = context->graphics_queue_family_index();
    VkCommandPool cpool = context->create_command_pool(cpool_ci, "Command Pool");
    std::array<VkCommandBuffer, kVframeCount> command_buffers = {};
    VkCommandBufferAllocateInfo cb_allocate_info = {};
    cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_allocate_info.commandPool = cpool;
    cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_allocate_info.commandBufferCount = (uint32_t)command_buffers.size();
    VULKAN_CHECK(vkAllocateCommandBuffers(context->device(), &cb_allocate_info, command_buffers.data()));

    // Create depth buffer
    // TODO(cort): use actual swapchain extent instead of window dimensions
    VkImageCreateInfo depth_image_ci = {};
    depth_image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depth_image_ci.imageType = VK_IMAGE_TYPE_2D;
    depth_image_ci.format = VK_FORMAT_UNDEFINED; // filled in below
    depth_image_ci.extent = {kWindowWidthDefault, kWindowHeightDefault, 1};
    depth_image_ci.mipLevels = 1;
    depth_image_ci.arrayLayers = 1;
    depth_image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    depth_image_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depth_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    const VkFormat depth_format_candidates[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
    };
    for(auto format : depth_format_candidates)
    {
        VkFormatProperties format_properties = {};
        vkGetPhysicalDeviceFormatProperties(context->physical_device(), format, &format_properties);
        if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
        {
            depth_image_ci.format = format;
            break;
        }
    }
    assert(depth_image_ci.format != VK_FORMAT_UNDEFINED);
    VkImage depth_image = context->create_image(depth_image_ci, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        "depth buffer image");
    VkDeviceMemory depth_image_mem = VK_NULL_HANDLE;
    VkDeviceSize depth_image_mem_offset = 0;
    VULKAN_CHECK(context->allocate_and_bind_image_memory(depth_image,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depth_image_mem, &depth_image_mem_offset));
    VkImageView depth_image_view = context->create_image_view(depth_image, depth_image_ci, "depth buffer image view");

    // Generate a procedural mesh
    cdsm_metadata_t mesh_metadata = {};
    size_t mesh_vertices_size = 0, mesh_indices_size = 0;
    enum {
        MESH_TYPE_CUBE     = 0,
        MESH_TYPE_SPHERE   = 1,
        MESH_TYPE_AXES     = 3,
        MESH_TYPE_CYLINDER = 2,
    } meshType = MESH_TYPE_CUBE;
    const cdsm_vertex_layout_t vertex_layout = {
        22, 3, {
            {0, 0, CDSM_ATTRIBUTE_FORMAT_R32G32B32_FLOAT},
            {1, 12, CDSM_ATTRIBUTE_FORMAT_R16G16B16_SNORM},
            {2,18, CDSM_ATTRIBUTE_FORMAT_R16G16_FLOAT},
        }
    };
    cdsm_cube_recipe_t cube_recipe = {};
    cube_recipe.vertex_layout = vertex_layout;
    cube_recipe.min_extent = {-0.2f,-0.2f,-0.2f};
    cube_recipe.max_extent = {+0.2f,+0.2f,+0.2f};
    cube_recipe.front_face = CDSM_FRONT_FACE_CCW;
    cdsm_sphere_recipe_t sphere_recipe = {};
    sphere_recipe.vertex_layout = vertex_layout;
    sphere_recipe.latitudinal_segments = 30;
    sphere_recipe.longitudinal_segments = 30;
    sphere_recipe.radius = 0.2f;
    cdsm_cylinder_recipe_t cylinder_recipe = {};
    cylinder_recipe.vertex_layout = vertex_layout;
    cylinder_recipe.length = 0.3f;
    cylinder_recipe.axial_segments = 3;
    cylinder_recipe.radial_segments = 60;
    cylinder_recipe.radius0 = 0.3f;
    cylinder_recipe.radius1 = 0.4f;
    cdsm_axes_recipe_t axes_recipe = {};
    axes_recipe.vertex_layout = vertex_layout;
    axes_recipe.length = 1.0f;
    if (meshType == MESH_TYPE_CUBE) {
        cdsm_create_cube(&mesh_metadata, NULL, &mesh_vertices_size, NULL, &mesh_indices_size, &cube_recipe);
    } else if (meshType == MESH_TYPE_SPHERE) {
        cdsm_create_sphere(&mesh_metadata, NULL, &mesh_vertices_size, NULL, &mesh_indices_size, &sphere_recipe);
    } else if (meshType == MESH_TYPE_AXES) {
        cdsm_create_axes(&mesh_metadata, NULL, &mesh_vertices_size, NULL, &mesh_indices_size, &axes_recipe);
    } else if (meshType == MESH_TYPE_CYLINDER) {
        cdsm_create_cylinder(&mesh_metadata, NULL, &mesh_vertices_size, NULL, &mesh_indices_size, &cylinder_recipe);
    }
    VkPrimitiveTopology primitive_topology = VK_PRIMITIVE_TOPOLOGY_RANGE_SIZE;
    if (mesh_metadata.primitive_type == CDSM_PRIMITIVE_TYPE_TRIANGLE_LIST) {
        primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    } else if (mesh_metadata.primitive_type == CDSM_PRIMITIVE_TYPE_LINE_LIST) {
        primitive_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    } else {
        assert(0); // unknown primitive topology
    }

    // Create index buffer
    VkIndexType index_type = (sizeof(cdsm_index_t) == sizeof(uint32_t)) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
    VkBufferCreateInfo index_buffer_ci = {};
    index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buffer_ci.size = mesh_indices_size;
    index_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    index_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer index_buffer = context->create_buffer(index_buffer_ci, "index buffer");
    VkDeviceMemory index_buffer_mem = VK_NULL_HANDLE;
    VkDeviceSize index_buffer_mem_offset = 0;
    VULKAN_CHECK(context->allocate_and_bind_buffer_memory(index_buffer,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &index_buffer_mem, &index_buffer_mem_offset));

    // Create vertex buffer
    // TODO(cort): Automate cdsm_vertex_layout_t to cdsvk::VertexBufferLayout conversion
    cdsvk::VertexBufferLayout vertex_buffer_layout = {};
    vertex_buffer_layout.stride = vertex_layout.stride;
    vertex_buffer_layout.attributes.resize(vertex_layout.attribute_count);
    vertex_buffer_layout.attributes[0].binding = 0;
    vertex_buffer_layout.attributes[0].location = 0;
    vertex_buffer_layout.attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_buffer_layout.attributes[0].offset = vertex_layout.attributes[0].offset;
    vertex_buffer_layout.attributes[1].binding = 0;
    vertex_buffer_layout.attributes[1].location = 1;
    vertex_buffer_layout.attributes[1].format = VK_FORMAT_R16G16B16_SNORM; // TODO(cort): convert from CDSM_* enum
    vertex_buffer_layout.attributes[1].offset = vertex_layout.attributes[1].offset;
    vertex_buffer_layout.attributes[2].binding = 0;
    vertex_buffer_layout.attributes[2].location = 2;
    vertex_buffer_layout.attributes[2].format = VK_FORMAT_R16G16_SFLOAT;
    vertex_buffer_layout.attributes[2].offset = vertex_layout.attributes[2].offset;
    VkBufferCreateInfo vertex_buffer_ci = {};
    vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_ci.size = mesh_vertices_size;
    vertex_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertex_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer vertex_buffer = context->create_buffer(vertex_buffer_ci, "vertex buffer");
    VkDeviceMemory vertex_buffer_mem = VK_NULL_HANDLE;
    VkDeviceSize vertex_buffer_mem_offset = 0;
    VULKAN_CHECK(context->allocate_and_bind_buffer_memory(vertex_buffer,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertex_buffer_mem, &vertex_buffer_mem_offset));

    // Populate vertex/index buffers
    {
        std::vector<uint8_t> index_buffer_contents(mesh_indices_size);
        std::vector<uint8_t> vertex_buffer_contents(mesh_vertices_size);
        if (meshType == MESH_TYPE_CUBE) {
            cdsm_create_cube(&mesh_metadata, vertex_buffer_contents.data(), &mesh_vertices_size,
                (cdsm_index_t*)index_buffer_contents.data(), &mesh_indices_size, &cube_recipe);
        } else if (meshType == MESH_TYPE_SPHERE) {
            cdsm_create_sphere(&mesh_metadata, vertex_buffer_contents.data(), &mesh_vertices_size,
                (cdsm_index_t*)index_buffer_contents.data(), &mesh_indices_size, &sphere_recipe);
        } else if (meshType == MESH_TYPE_AXES) {
            cdsm_create_axes(&mesh_metadata, vertex_buffer_contents.data(), &mesh_vertices_size,
                (cdsm_index_t*)index_buffer_contents.data(), &mesh_indices_size, &axes_recipe);
        } else if (meshType == MESH_TYPE_CYLINDER) {
            cdsm_create_cylinder(&mesh_metadata, vertex_buffer_contents.data(), &mesh_vertices_size,
                (cdsm_index_t*)index_buffer_contents.data(), &mesh_indices_size, &cylinder_recipe);
        }
        VULKAN_CHECK(context->load_buffer_contents(index_buffer, index_buffer_ci, 
            0, index_buffer_contents.data(), mesh_indices_size, VK_ACCESS_INDEX_READ_BIT));
        VULKAN_CHECK(context->load_buffer_contents(vertex_buffer, vertex_buffer_ci,
            0, vertex_buffer_contents.data(), mesh_vertices_size, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT));
    }

    const uint32_t kMeshCount = 1024;
    // Create buffer of per-mesh object-to-world matrices.
    // TODO(cort): Make this DEVICE_LOCAL & upload every frame?
    const VkDeviceSize uniform_buffer_vframe_size = kMeshCount * sizeof(mathfu::mat4);
    VkBufferCreateInfo o2w_buffer_ci = {};
    o2w_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    o2w_buffer_ci.size = uniform_buffer_vframe_size * kVframeCount;
    o2w_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    o2w_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer o2w_buffer = context->create_buffer(o2w_buffer_ci, "o2w buffer");
    VkDeviceMemory o2w_buffer_mem = VK_NULL_HANDLE;
    VkDeviceSize o2w_buffer_mem_offset = 0;
    VULKAN_CHECK(context->allocate_and_bind_buffer_memory(o2w_buffer,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &o2w_buffer_mem, &o2w_buffer_mem_offset));

    // Create push constants.
    // TODO(cort): this should be a per-vframe uniform buffer.
    struct {
        mathfu::vec4_packed time_and_res; // .x=seconds, .yz=dimensions, w=???
        mathfu::vec4_packed eye;  // .xyz=world-space eye position, .w=???
        mathfu::mat4 viewproj;
    } push_constants = {};
    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(push_constants);

    // Create Vulkan descriptor layout & pipeline layout
    std::array<VkDescriptorSetLayoutBinding, 2> dset_layout_bindings = {};
    dset_layout_bindings[0].binding = 0;
    dset_layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dset_layout_bindings[0].descriptorCount = 1;
    dset_layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dset_layout_bindings[1].binding = 1;
    dset_layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    dset_layout_bindings[1].descriptorCount = 1;
    dset_layout_bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo dset_layout_ci = {};
    dset_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dset_layout_ci.bindingCount = (uint32_t)dset_layout_bindings.size();
    dset_layout_ci.pBindings = dset_layout_bindings.data();
    VkDescriptorSetLayout dset_layout = context->create_descriptor_set_layout(dset_layout_ci, "descriptor set layout");
    VkPipelineLayoutCreateInfo pipeline_layout_ci = {};
    pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_ci.setLayoutCount = 1;
    pipeline_layout_ci.pSetLayouts = &dset_layout;
    pipeline_layout_ci.pushConstantRangeCount = 1;
    pipeline_layout_ci.pPushConstantRanges = &push_constant_range;
    VkPipelineLayout pipeline_layout = context->create_pipeline_layout(pipeline_layout_ci, "pipeline layout");
    
    // Load shaders
    VkShaderModule vertex_shader = context->load_shader("tri.vert.spv");
    VkShaderModule fragment_shader = context->load_shader("tri.frag.spv");

    // Load textures, create sampler and image view
    VkSamplerCreateInfo sampler_ci = {};
    sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_ci.magFilter = VK_FILTER_LINEAR;
    sampler_ci.minFilter = VK_FILTER_LINEAR;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_ci.mipLodBias = 0.0f;
    sampler_ci.anisotropyEnable = VK_TRUE;
    sampler_ci.maxAnisotropy = 16;
    sampler_ci.compareOp = VK_COMPARE_OP_NEVER;
    sampler_ci.minLod = 0.0f;
    sampler_ci.maxLod = FLT_MAX;
    sampler_ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler_ci.unnormalizedCoordinates = VK_FALSE;
    VkSampler sampler= context->create_sampler(sampler_ci, "default sampler");

    const std::string& texture_filename = "trevor/redf.ktx";
    VkImage texture_image = VK_NULL_HANDLE;
    VkImageCreateInfo texture_image_ci = {};
    VkDeviceMemory texture_image_mem = VK_NULL_HANDLE;
    VkDeviceSize texture_image_mem_offset = 0;
    int texture_load_error = load_vkimage_from_file(&texture_image, &texture_image_ci,
        &texture_image_mem, &texture_image_mem_offset, *context, texture_filename, VK_TRUE,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
    assert(!texture_load_error); (void)texture_load_error;
    VkImageView texture_image_view = context->create_image_view(texture_image,
        texture_image_ci, "texture image view");

    // Create render pass
    std::array<VkAttachmentDescription, 2> attachment_descs = {};
    attachment_descs[0].format = context->swapchain_format();
    attachment_descs[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_descs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_descs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment_descs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_descs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachment_descs[1].format = depth_image_ci.format;
    attachment_descs[1].samples = depth_image_ci.samples;
    attachment_descs[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_descs[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_descs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    enum {
        kColorAttachmentIndex = 0,
        kDepthAttachmentIndex = 1,
        kAttachmentCount
    };
    std::array<VkAttachmentReference, kAttachmentCount> attachment_refs = {};
    attachment_refs[kColorAttachmentIndex].attachment = 0;
    attachment_refs[kColorAttachmentIndex].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment_refs[kDepthAttachmentIndex].attachment = kDepthAttachmentIndex;
    attachment_refs[kDepthAttachmentIndex].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass_desc = {};
    subpass_desc.flags = 0;
    subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_desc.colorAttachmentCount = 1;
    subpass_desc.pColorAttachments = &attachment_refs[kColorAttachmentIndex];
    subpass_desc.pDepthStencilAttachment = &attachment_refs[kDepthAttachmentIndex];
    std::array<VkSubpassDependency, 2> subpass_dependencies = {};
    subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[0].dstSubpass = 0;
    subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpass_dependencies[1].srcSubpass = 0;
    subpass_dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[1].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[1].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    VkRenderPassCreateInfo render_pass_ci = {};
    render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_ci.attachmentCount = (uint32_t)attachment_descs.size();
    render_pass_ci.pAttachments = attachment_descs.data();
    render_pass_ci.subpassCount = 1;
    render_pass_ci.pSubpasses = &subpass_desc;
    render_pass_ci.dependencyCount = (uint32_t)subpass_dependencies.size();
    render_pass_ci.pDependencies = subpass_dependencies.data();
    VkRenderPass render_pass = context->create_render_pass(render_pass_ci, "default render pass");

    // Create VkFramebuffers
    std::array<VkImageView, kAttachmentCount> attachment_views = {};
    attachment_views[kColorAttachmentIndex] = VK_NULL_HANDLE; // filled in below;
    attachment_views[kDepthAttachmentIndex] = depth_image_view;
    // TODO(cort): use actual target extents instead of kWindow* constants
    VkFramebufferCreateInfo framebuffer_ci = {};
    framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_ci.renderPass = render_pass;
    framebuffer_ci.attachmentCount = (uint32_t)attachment_views.size();
    framebuffer_ci.pAttachments = attachment_views.data();
    framebuffer_ci.width = kWindowWidthDefault;
    framebuffer_ci.height = kWindowHeightDefault;
    framebuffer_ci.layers = 1;
    std::vector<VkFramebuffer> framebuffers;
    framebuffers.reserve(context->swapchain_image_views().size());
    for(auto view : context->swapchain_image_views()) {
        attachment_views[kColorAttachmentIndex] = view;
        framebuffers.push_back(context->create_framebuffer(framebuffer_ci, "Default framebuffer"));
    }

    // Create VkPipeline
    cdsvk::GraphicsPipelineSettingsVsPs graphics_pipeline_settings = {};
    graphics_pipeline_settings.vertex_buffer_layout = vertex_buffer_layout;
    graphics_pipeline_settings.dynamic_state_mask = 0
        | (1<<VK_DYNAMIC_STATE_VIEWPORT)
        | (1<<VK_DYNAMIC_STATE_SCISSOR)
        ;
    graphics_pipeline_settings.primitive_topology = primitive_topology;
    graphics_pipeline_settings.pipeline_layout = pipeline_layout;
    graphics_pipeline_settings.render_pass = render_pass;
    graphics_pipeline_settings.subpass = 0;
    graphics_pipeline_settings.subpass_color_attachment_count = subpass_desc.colorAttachmentCount;
    graphics_pipeline_settings.vertex_shader = vertex_shader;
    graphics_pipeline_settings.fragment_shader = fragment_shader;
    cdsvk::GraphicsPipelineCreateInfo graphics_pipeline_ci(graphics_pipeline_settings);
    // Fixup default values if necessary
    if (mesh_metadata.front_face == CDSM_FRONT_FACE_CW) {
        graphics_pipeline_ci.rasterization_state_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
    }
    VkPipeline graphics_pipeline = context->create_graphics_pipeline(graphics_pipeline_ci,
        "default graphics pipeline");

    // Create Vulkan descriptor pool and descriptor set.
    // TODO(cort): the current descriptors are constant; we'd need a set per-vframe if it was going to change
    // per-frame.
    VkDescriptorPool dpool = context->create_descriptor_pool(dset_layout_ci, 1,
        VkDescriptorPoolCreateFlags(0), "Descriptor pool");
    VkDescriptorSetAllocateInfo dset_alloc_info = {};
    dset_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dset_alloc_info.descriptorPool = dpool;
    dset_alloc_info.descriptorSetCount = 1;
    dset_alloc_info.pSetLayouts = &dset_layout;
    std::vector<VkDescriptorSet> dsets(dset_alloc_info.descriptorSetCount);
    VULKAN_CHECK(vkAllocateDescriptorSets(context->device(), &dset_alloc_info, dsets.data()));
    VULKAN_CHECK(context->set_debug_name(dsets[0], "default descriptor set"));
    std::vector<VkDescriptorImageInfo> image_infos(1);
    image_infos[0].sampler = sampler;
    image_infos[0].imageView = texture_image_view;
    image_infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    std::vector<VkDescriptorBufferInfo> buffer_infos(1);
    buffer_infos[0].buffer = o2w_buffer;
    buffer_infos[0].offset = 0;
    buffer_infos[0].range = VK_WHOLE_SIZE;
    std::vector<VkWriteDescriptorSet> write_dsets(2);
    write_dsets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_dsets[0].dstSet = dsets[0];
    write_dsets[0].dstBinding = 0;
    write_dsets[0].descriptorCount = (uint32_t)image_infos.size();
    write_dsets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_dsets[0].pImageInfo = image_infos.data();
    write_dsets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_dsets[1].dstSet = dsets[0];
    write_dsets[1].dstBinding = 1;
    write_dsets[1].descriptorCount = (uint32_t)buffer_infos.size();
    write_dsets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    write_dsets[1].pBufferInfo = buffer_infos.data();
    vkUpdateDescriptorSets(context->device(), (uint32_t)write_dsets.size(), write_dsets.data(), 0,nullptr);

    // Create the semaphores used to synchronize access to swapchain images
    VkSemaphoreCreateInfo semaphore_ci = {};
    semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore swapchain_image_ready_sem = context->create_semaphore(semaphore_ci, "image ready semaphore");
    VkSemaphore render_complete_sem = context->create_semaphore(semaphore_ci, "rendering complete semaphore");

    // Create the fences used to wait for each swapchain image's command buffer to be submitted.
    // This prevents re-writing the command buffer contents before it's been submitted and processed.
    VkFenceCreateInfo fence_ci = {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    std::vector<VkFence> submission_complete_fences(kVframeCount);
    for(auto &fence : submission_complete_fences) {
        fence = context->create_fence(fence_ci, "queue submitted fence");
    }

    const mathfu::mat4 clip_fixup(
        +1.0f, +0.0f, +0.0f, +0.0f,
        +0.0f, -1.0f, +0.0f, +0.0f,
        +0.0f, +0.0f, +0.5f, +0.5f,
        +0.0f, +0.0f, +0.0f, +1.0f);

 // Create timestamp query pools.
    typedef enum {
        TIMESTAMP_ID_BEGIN_FRAME = 0,
        TIMESTAMP_ID_END_FRAME = 1,
        TIMESTAMP_ID_RANGE_SIZE
    } TimestampId;
    VkQueryPoolCreateInfo timestamp_query_pool_ci = {};
    timestamp_query_pool_ci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    timestamp_query_pool_ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
    timestamp_query_pool_ci.queryCount = TIMESTAMP_ID_RANGE_SIZE;
    std::array<VkQueryPool, kVframeCount> timestamp_query_pools = {};
    for(auto& pool : timestamp_query_pools) {
        pool = context->create_query_pool(timestamp_query_pool_ci, "timestamp query pool");
    }
    {
        // Submit dummy timestamp queries for the first frame to retrieve
        VkCommandBuffer cb = context->begin_one_shot_command_buffer();
        for(auto pool : timestamp_query_pools) {
            for(uint32_t iTS=0; iTS<TIMESTAMP_ID_RANGE_SIZE; ++iTS) {
                vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pool, iTS);
            }
        }
        context->end_and_submit_one_shot_command_buffer(&cb);
    }

    const uint64_t clock_start = zomboClockTicks();
    std::array<double, TIMESTAMP_ID_RANGE_SIZE> timestamp_seconds_prev = {};
    uint32_t vframe_index = 0;
    uint32_t frame_index = 0;
    while(!glfwWindowShouldClose(window.get())) {
        input_state.Update();
        mathfu::vec3 impulse(0,0,0);
        if (input_state.GetDigital(InputState::DIGITAL_LPAD_UP)) {
            impulse += camera.getViewDirection() * 0.1f;
        }
        if (input_state.GetDigital(InputState::DIGITAL_LPAD_LEFT)) {
            impulse -= cross(camera.getViewDirection(), camera.getWorldUp()) * 0.1f;
        }
        if (input_state.GetDigital(InputState::DIGITAL_LPAD_DOWN)) {
            impulse -= camera.getViewDirection() * 0.1f;
        }
        if (input_state.GetDigital(InputState::DIGITAL_LPAD_RIGHT)) {
            impulse += cross(camera.getViewDirection(), camera.getWorldUp()) * 0.1f;
        }

        camera.setOrientation(mathfu::quat::FromEulerAngles(mathfu::vec3(
            -0.001f * input_state.GetAnalog(InputState::ANALOG_MOUSE_Y),
            -0.001f * input_state.GetAnalog(InputState::ANALOG_MOUSE_X),
            0)));
        dolly.Impulse(impulse);
        dolly.Update(1.0f/60.0f);

        // Wait for the command buffer previously used to generate this swapchain image to be submitted.
        // TODO(cort): this does not guarantee memory accesses from this submission will be visible on the host;
        // there'd need to be a memory barrier for that.
        vkWaitForFences(context->device(), 1, &submission_complete_fences[vframe_index], VK_TRUE, UINT64_MAX);
        vkResetFences(context->device(), 1, &submission_complete_fences[vframe_index]);

        // The host can now safely reset and rebuild this command buffer, even if the GPU hasn't finished presenting the
        // resulting frame yet.
        VkCommandBuffer cb = command_buffers[vframe_index];

        // Read the timestamp query results from the frame that was just presented. We'll process and report them later,
        // but we need to get the old data out before we reuse the query pool for the current frame.
        VkQueryPool timestamp_query_pool = timestamp_query_pools[vframe_index];
        std::array<uint64_t, TIMESTAMP_ID_RANGE_SIZE> timestamps = {};
        VkResult get_timestamps_result = vkGetQueryPoolResults(context->device(), timestamp_query_pool,
            0,TIMESTAMP_ID_RANGE_SIZE, timestamps.size()*sizeof(timestamps[0]), timestamps.data(),
            sizeof(timestamps[0]), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        assert(get_timestamps_result == VK_SUCCESS);

        // Update object-to-world matrices.
        uint32_t uniform_buffer_vframe_offset = (uint32_t)uniform_buffer_vframe_size * vframe_index;
        mathfu::vec4 *mapped_o2w_buffer = nullptr;
        VULKAN_CHECK(vkMapMemory(context->device(), o2w_buffer_mem, (VkDeviceSize)uniform_buffer_vframe_offset,
            uniform_buffer_vframe_size, VkMemoryMapFlags(0), (void**)&mapped_o2w_buffer));
        const float seconds_elapsed = (float)( zomboTicksToSeconds(zomboClockTicks() - clock_start) );
        const mathfu::vec3 swarm_center(0, 0, -2);
        for(int iMesh=0; iMesh<kMeshCount; ++iMesh) {
            mathfu::quat q = mathfu::quat::FromAngleAxis(seconds_elapsed + (float)iMesh, mathfu::vec3(0,1,0));
            mathfu::mat4 o2w = mathfu::mat4::Identity()
                * mathfu::mat4::FromTranslationVector(mathfu::vec3(
                    4.0f * cosf((1.0f+0.001f*iMesh) * seconds_elapsed + float(149*iMesh) + 0.0f) + swarm_center[0],
                    2.5f * sinf(1.5f * seconds_elapsed + float(13*iMesh) + 5.0f) + swarm_center[1],
                    3.0f * sinf(0.25f * seconds_elapsed + float(51*iMesh) + 2.0f) + swarm_center[2]
                    ))
                * q.ToMatrix4()
                //* mathfu::mat4::FromScaleVector( mathfu::vec3(0.1f, 0.1f, 0.1f) )
                ;
            mapped_o2w_buffer[iMesh*4+0] = mathfu::vec4(o2w[ 0], o2w[ 1], o2w[ 2], o2w[ 3]);
            mapped_o2w_buffer[iMesh*4+1] = mathfu::vec4(o2w[ 4], o2w[ 5], o2w[ 6], o2w[ 7]);
            mapped_o2w_buffer[iMesh*4+2] = mathfu::vec4(o2w[ 8], o2w[ 9], o2w[10], o2w[11]);
            mapped_o2w_buffer[iMesh*4+3] = mathfu::vec4(o2w[12], o2w[13], o2w[14], o2w[15]);
        }
        vkUnmapMemory(context->device(), o2w_buffer_mem);

        // Retrieve the index of the next available swapchain index
        uint32_t swapchain_image_index = UINT32_MAX;
        VkFence image_acquired_fence = VK_NULL_HANDLE; // currently unused, but if you want the CPU to wait for an image to be acquired...
        VkResult result = vkAcquireNextImageKHR(context->device(), context->swapchain(), UINT64_MAX, swapchain_image_ready_sem,
            image_acquired_fence, &swapchain_image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
        } else if (result == VK_SUBOPTIMAL_KHR) {
            // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
        } else {
            VULKAN_CHECK(result);
        }
        VkFramebuffer framebuffer = framebuffers[swapchain_image_index];

        VkCommandBufferBeginInfo cb_begin_info = {};
        cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VULKAN_CHECK(vkBeginCommandBuffer(cb, &cb_begin_info) );
        vkCmdResetQueryPool(cb, timestamp_query_pool, 0, TIMESTAMP_ID_RANGE_SIZE);
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestamp_query_pool, TIMESTAMP_ID_BEGIN_FRAME);

        std::array<VkClearValue, kAttachmentCount> clear_values;
        clear_values[0].color.float32[0] = 0.0f;
        clear_values[0].color.float32[1] = 0.0f;
        clear_values[0].color.float32[2] = 0.3f;
        clear_values[0].color.float32[3] = 0.0f;
        clear_values[1].depthStencil.depth = 1.0f;
        clear_values[1].depthStencil.stencil = 0;
        VkRenderPassBeginInfo render_pass_begin_info = {};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = render_pass;
        render_pass_begin_info.framebuffer = framebuffer;
        render_pass_begin_info.renderArea.offset.x = 0;
        render_pass_begin_info.renderArea.offset.y = 0;
        render_pass_begin_info.renderArea.extent.width  = kWindowWidthDefault;
        render_pass_begin_info.renderArea.extent.height = kWindowHeightDefault;
        render_pass_begin_info.clearValueCount = (uint32_t)clear_values.size();
        render_pass_begin_info.pClearValues = clear_values.data();
        vkCmdBeginRenderPass(cb, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_layout, 0, (uint32_t)dsets.size(), dsets.data(), 1,&uniform_buffer_vframe_offset);
        push_constants.time_and_res = mathfu::vec4(seconds_elapsed,
            (float)kWindowWidthDefault, (float)kWindowHeightDefault, 0);
        push_constants.eye = mathfu::vec4(camera.getEyePoint(), 1.0f);
        mathfu::mat4 w2v = camera.getViewMatrix();
        const mathfu::mat4 proj = camera.getProjectionMatrix();
        const mathfu::mat4 viewproj = clip_fixup * proj * w2v;
        push_constants.viewproj = viewproj;
        vkCmdPushConstants(cb, pipeline_layout, push_constant_range.stageFlags,
            push_constant_range.offset, push_constant_range.size, &push_constants);
        VkViewport viewport = {};
        viewport.width  = (float)kWindowWidthDefault;
        viewport.height = (float)kWindowHeightDefault;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cb, 0,1, &viewport);
        VkRect2D scissor_rect = {};
        scissor_rect.extent.width  = kWindowWidthDefault;
        scissor_rect.extent.height = kWindowHeightDefault;
        scissor_rect.offset.x = 0;
        scissor_rect.offset.y = 0;
        vkCmdSetScissor(cb, 0,1, &scissor_rect);
        const VkDeviceSize vertex_buffer_offsets[1] = {};
        vkCmdBindVertexBuffers(cb, 0,1, &vertex_buffer, vertex_buffer_offsets);
        const VkDeviceSize index_buffer_offset = 0;
        vkCmdBindIndexBuffer(cb, index_buffer, index_buffer_offset, index_type);
        const uint32_t instance_count = kMeshCount;
        vkCmdDrawIndexed(cb, mesh_metadata.index_count, instance_count, 0,0,0);
        vkCmdEndRenderPass(cb);
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamp_query_pool, TIMESTAMP_ID_END_FRAME);
        VULKAN_CHECK( vkEndCommandBuffer(cb) );
        const VkPipelineStageFlags submit_wait_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &swapchain_image_ready_sem;
        submit_info.pWaitDstStageMask = &submit_wait_stages;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cb;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &render_complete_sem;
        VULKAN_CHECK( vkQueueSubmit(context->graphics_queue(), 1, &submit_info, submission_complete_fences[vframe_index]) );
        VkSwapchainKHR swapchain = context->swapchain();
        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.pNext = NULL;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain;
        present_info.pImageIndices = &swapchain_image_index;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &render_complete_sem;
        result = vkQueuePresentKHR(context->present_queue(), &present_info); // TODO(cort): concurrent image access required?
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
        } else if (result == VK_SUBOPTIMAL_KHR) {
            // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
        } else {
            VULKAN_CHECK(result);
        }

        // TODO(cort): Now that the CPU and GPU execution is decoupled, we may need to emit a memory barrier to
        // make these query results visible to the host at the end of the submit.
        {
            std::array<double, TIMESTAMP_ID_RANGE_SIZE> timestamp_seconds = {};
            for(int iTS=0; iTS<TIMESTAMP_ID_RANGE_SIZE; ++iTS) {
                if (context->graphics_queue_family_properties().timestampValidBits < sizeof(timestamps[0])*8)
                    timestamps[iTS] &= ((1ULL<<context->graphics_queue_family_properties().timestampValidBits)-1);
                timestamp_seconds[iTS] = (double)(timestamps[iTS])
                    * (double)context->physical_device_properties().limits.timestampPeriod / 1e9;
            }
            if ((frame_index % 100)==0) {
                printf("GPU T2B=%10.6f\tT2T=%10.6f\n",
                    (timestamp_seconds[TIMESTAMP_ID_END_FRAME] - timestamp_seconds[TIMESTAMP_ID_BEGIN_FRAME])*1000.0,
                    (timestamp_seconds[TIMESTAMP_ID_BEGIN_FRAME] - timestamp_seconds_prev[TIMESTAMP_ID_BEGIN_FRAME])*1000.0);
            }
            timestamp_seconds_prev = timestamp_seconds;
        }
        glfwPollEvents();
        frame_index += 1;
        vframe_index += 1;
        if (vframe_index == kVframeCount) {
            vframe_index = 0;
        }
    }

    // NOTE: vkDeviceWaitIdle() only waits for all queue operations to complete; swapchain images may still be
    // queued for presentation when this function returns. To avoid a race condition when freeing them and
    // their associated semaphores, the host needs to...acquire each swapchain image, passing a VkFence and
    // immediately waiting on it? Is that sufficient? I don't think so; no guarantee of acquiring all images.
    // It looks like this is still under discussion (Khronos Vulkan issue #243, #245)
    vkDeviceWaitIdle(context->device());
    // Cleanup
    for(auto pool : timestamp_query_pools) {
        context->destroy_query_pool(pool);
    }
    context->destroy_semaphore(swapchain_image_ready_sem);
    context->destroy_semaphore(render_complete_sem);
    for(auto fence : submission_complete_fences) {
        context->destroy_fence(fence);
    }
    context->destroy_render_pass(render_pass);
    for(auto framebuffer : framebuffers) {
        context->destroy_framebuffer(framebuffer);
    }
    context->destroy_pipeline(graphics_pipeline);
    context->destroy_pipeline_layout(pipeline_layout);
    context->destroy_descriptor_pool(dpool);
    context->destroy_descriptor_set_layout(dset_layout);
    context->free_device_memory(texture_image_mem, texture_image_mem_offset);
    context->destroy_image_view(texture_image_view);
    context->destroy_image(texture_image);
    context->destroy_sampler(sampler);
    context->destroy_shader(vertex_shader);
    context->destroy_shader(fragment_shader);
    context->free_device_memory(o2w_buffer_mem, o2w_buffer_mem_offset);
    context->destroy_buffer(o2w_buffer);
    context->free_device_memory(index_buffer_mem, index_buffer_mem_offset);
    context->destroy_buffer(index_buffer);
    context->free_device_memory(vertex_buffer_mem, vertex_buffer_mem_offset);
    context->destroy_buffer(vertex_buffer);
    context->free_device_memory(depth_image_mem, depth_image_mem_offset);
    context->destroy_image_view(depth_image_view);
    context->destroy_image(depth_image);
    context->destroy_command_pool(cpool);

    window.reset();
    glfwTerminate();
    delete context;
    return 0;
}
