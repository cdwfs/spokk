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

#define CDS_MESH_IMPLEMENTATION
#include "cds_mesh.h"

#define CDS_VULKAN_IMPLEMENTATION
#include "cds_vulkan.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <mathfu/vector.h>
#include <mathfu/glsl_mappings.h>

#include <assert.h>

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
    const uint32_t kVframeCount = 2U;
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

    // Allocate command buffers
    vk::CommandPoolCreateInfo command_pool_ci(
        vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        context->graphics_queue_family_index());
    vk::CommandPool command_pool = context->create_command_pool(command_pool_ci, "Command Pool");
    vk::CommandBufferAllocateInfo cb_allocate_info(command_pool, vk::CommandBufferLevel::ePrimary, kVframeCount);
    auto command_buffers = context->device().allocateCommandBuffers(cb_allocate_info);

    // Create depth buffer
    // TODO(cort): use actual swapchain extent instead of window dimensions
    vk::ImageCreateInfo depth_image_ci(vk::ImageCreateFlags(), vk::ImageType::e2D, vk::Format::eUndefined,
        vk::Extent3D(kWindowWidthDefault, kWindowHeightDefault, 1), 1, 1, vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::SharingMode::eExclusive,
        0,nullptr, vk::ImageLayout::eUndefined);
    const vk::Format depth_format_candidates[] = {
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint,
        vk::Format::eD16UnormS8Uint,
    };
    for(auto format : depth_format_candidates) {
        vk::FormatProperties format_properties = context->physical_device().getFormatProperties(format);
        if (format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            depth_image_ci.format = format;
            break;
        }
    }
    assert(depth_image_ci.format != vk::Format::eUndefined);
    vk::Image depth_image = context->create_image(depth_image_ci, vk::ImageLayout::eUndefined,
        vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        "depth buffer image");
    vk::DeviceMemory depth_image_mem = VK_NULL_HANDLE;
    vk::DeviceSize depth_image_mem_offset = 0;
    context->allocate_and_bind_image_memory(depth_image, vk::MemoryPropertyFlagBits::eDeviceLocal,
        &depth_image_mem, &depth_image_mem_offset);
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
    vk::PrimitiveTopology primitive_topology = (vk::PrimitiveTopology)VK_PRIMITIVE_TOPOLOGY_RANGE_SIZE;
    if (mesh_metadata.primitive_type == CDSM_PRIMITIVE_TYPE_TRIANGLE_LIST) {
        primitive_topology = vk::PrimitiveTopology::eTriangleList;
    } else if (mesh_metadata.primitive_type == CDSM_PRIMITIVE_TYPE_LINE_LIST) {
        primitive_topology = vk::PrimitiveTopology::eLineList;
    } else {
        assert(0); // unknown primitive topology
    }

    // Create index buffer
    vk::IndexType index_type = (sizeof(cdsm_index_t) == sizeof(uint32_t)) ? vk::IndexType::eUint32 : vk::IndexType::eUint16;
    vk::BufferCreateInfo index_buffer_ci(vk::BufferCreateFlags(), mesh_indices_size,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    vk::Buffer index_buffer = context->create_buffer(index_buffer_ci, "index buffer");
    vk::DeviceMemory index_buffer_mem = VK_NULL_HANDLE;
    vk::DeviceSize index_buffer_mem_offset = 0;
    VULKAN_CHECK(context->allocate_and_bind_buffer_memory(index_buffer, vk::MemoryPropertyFlagBits::eDeviceLocal,
        &index_buffer_mem, &index_buffer_mem_offset));

    // Create vertex buffer
    // TODO(cort): Automate cdsm_vertex_layout_t to cdsvk::VertexBufferLayout conversion
    cdsvk::VertexBufferLayout vertex_buffer_layout = {
        vertex_layout.stride,
        vk::VertexInputRate::eVertex,
        {
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, vertex_layout.attributes[0].offset),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR16G16B16Snorm, vertex_layout.attributes[1].offset),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR16G16Sfloat, vertex_layout.attributes[2].offset),
        },
    };
    vk::BufferCreateInfo vertex_buffer_ci(vk::BufferCreateFlags(), mesh_vertices_size,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
    vk::Buffer vertex_buffer = context->create_buffer(vertex_buffer_ci, "vertex buffer");
    vk::DeviceMemory vertex_buffer_mem = VK_NULL_HANDLE;
    vk::DeviceSize vertex_buffer_mem_offset = 0;
    VULKAN_CHECK(context->allocate_and_bind_buffer_memory(vertex_buffer, vk::MemoryPropertyFlagBits::eDeviceLocal,
        &vertex_buffer_mem, &vertex_buffer_mem_offset));

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
            0, index_buffer_contents.data(), mesh_indices_size, vk::AccessFlagBits::eIndexRead));
        VULKAN_CHECK(context->load_buffer_contents(vertex_buffer, vertex_buffer_ci,
            0, vertex_buffer_contents.data(), mesh_vertices_size, vk::AccessFlagBits::eVertexAttributeRead));
    }

    const uint32_t kMeshCount = 1024;
    // Create buffer of per-mesh object-to-world matrices.
    // TODO(cort): Make this DEVICE_LOCAL & upload every frame?
    vk::BufferCreateInfo o2w_buffer_ci(vk::BufferCreateFlags(),
        kMeshCount * sizeof(mathfu::mat4) * kVframeCount,
        vk::BufferUsageFlagBits::eUniformBuffer);
    VkBuffer o2w_buffer = context->create_buffer(o2w_buffer_ci, "o2w buffer");
    vk::DeviceMemory o2w_buffer_mem = VK_NULL_HANDLE;
    vk::DeviceSize o2w_buffer_mem_offset = 0;
    VULKAN_CHECK(context->allocate_and_bind_buffer_memory(o2w_buffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        &o2w_buffer_mem, &o2w_buffer_mem_offset));

    // Create push constants.
    // TODO(cort): this should be a per-vframe uniform buffer.
    struct {
        mathfu::vec4_packed time; // .x=seconds, .yzw=???
        mathfu::vec4_packed eye;  // .xyz=world-space eye position, .w=???
        mathfu::mat4 viewproj;
    } push_constants = {};
    std::array<vk::PushConstantRange, 1> push_constant_ranges = {
        vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
            0, sizeof(push_constants))
    };
    // Create Vulkan descriptor layout & pipeline layout
    std::array<vk::DescriptorSetLayoutBinding, 2> dset_layout_bindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBufferDynamic, 1, vk::ShaderStageFlagBits::eVertex),
    };
    vk::DescriptorSetLayoutCreateInfo dset_layout_ci(vk::DescriptorSetLayoutCreateFlags(), 
        (uint32_t)dset_layout_bindings.size(), dset_layout_bindings.data());
    vk::DescriptorSetLayout dset_layout = context->create_descriptor_set_layout(dset_layout_ci, "descriptor set layout");
    vk::PipelineLayoutCreateInfo pipeline_layout_ci(vk::PipelineLayoutCreateFlags(),
        1, &dset_layout, (uint32_t)push_constant_ranges.size(), push_constant_ranges.data());
    vk::PipelineLayout pipeline_layout = context->create_pipeline_layout(pipeline_layout_ci, "pipeline layout");
    
    // Load shaders
    vk::ShaderModule vertex_shader = context->load_shader("tri.vert.spv");
    vk::ShaderModule fragment_shader = context->load_shader("tri.frag.spv");

    // Load textures, create sampler and image view
    vk::SamplerCreateInfo sampler_ci = vk::SamplerCreateInfo{}
        .setMinFilter(vk::Filter::eLinear)
        .setMagFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setAnisotropyEnable(VK_TRUE)
        .setMaxAnisotropy(16)
        .setMinLod(0.0f)
        .setMaxLod(99.0f);
    vk::Sampler sampler= context->create_sampler(sampler_ci, "default sampler");

    const std::string& texture_filename = "trevor/redf.ktx";
#if 0
    vk::Image texture_image = VK_NULL_HANDLE;
    vk::ImageCreateInfo texture_image_ci = {};
    vkDeviceMemory texture_image_mem = VK_NULL_HANDLE;
    VkDeviceSize texture_image_mem_offset = 0;
    int texture_load_error = load_vkimage_from_file(&texture_image, &texture_image_create_info,
        &texture_image_mem, &texture_image_mem_offset, &context, texture_filename, VK_TRUE,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);
    assert(!texture_load_error); (void)texture_load_error;
    vk::ImageView texture_image_view = cdsvk_create_image_view_from_image(&context, texture_image,
        &texture_image_create_info, "texture image view");
#else
    vk::ImageView texture_image_view = VK_NULL_HANDLE;
#endif

    // Create render pass
    enum {
        kColorAttachmentIndex = 0,
        kDepthAttachmentIndex = 1,
        kAttachmentCount
    };
    std::array<vk::AttachmentDescription, kAttachmentCount> attachment_descs = {
        vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), context->swapchain_format(),
            vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR),
        vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), depth_image_ci.format,
            depth_image_ci.samples, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal),
    };
    std::array<vk::AttachmentReference, kAttachmentCount> attachment_refs = {
        vk::AttachmentReference(kColorAttachmentIndex, vk::ImageLayout::eColorAttachmentOptimal),
        vk::AttachmentReference(kDepthAttachmentIndex, vk::ImageLayout::eDepthStencilAttachmentOptimal),
    };
    vk::SubpassDescription subpass_desc(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics,
        0,nullptr, // input attachments
        1,&attachment_refs[kColorAttachmentIndex], // color attachments
        nullptr, // resolve attachment
        &attachment_refs[kDepthAttachmentIndex], // depth attachment
        0,nullptr); // preserve attachments
    std::array<vk::SubpassDependency, 2> subpass_dependencies = {
        vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 0,
            vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
            vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
            vk::DependencyFlagBits::eByRegion),
        vk::SubpassDependency(0, VK_SUBPASS_EXTERNAL,
            vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
            vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
            vk::DependencyFlagBits::eByRegion),
    };
    vk::RenderPassCreateInfo render_pass_ci(vk::RenderPassCreateFlags(),
        (uint32_t)attachment_descs.size(), attachment_descs.data(), 1,&subpass_desc,
        (uint32_t)subpass_dependencies.size(), subpass_dependencies.data());
    vk::RenderPass render_pass = context->create_render_pass(render_pass_ci, "default render pass");

    // Create VkFramebuffers
    std::array<vk::ImageView, kAttachmentCount> attachment_views = {};
    attachment_views[kColorAttachmentIndex] = VK_NULL_HANDLE; // filled in below;
    attachment_views[kDepthAttachmentIndex] = depth_image_view;
    // TODO(cort): use actual target extents instead of kWindow* constants
    vk::FramebufferCreateInfo framebuffer_ci(vk::FramebufferCreateFlags(), render_pass,
        (uint32_t)attachment_views.size(), attachment_views.data(),
        kWindowWidthDefault, kWindowHeightDefault, 1);
    std::vector<vk::Framebuffer> framebuffers;
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
        graphics_pipeline_ci.rasterization_state_ci.frontFace = vk::FrontFace::eClockwise;
    }
    vk::Pipeline graphics_pipeline = context->create_graphics_pipeline(graphics_pipeline_ci,
        "default graphics pipeline");

    // Create Vulkan descriptor pool and descriptor set.
    // TODO(cort): the current descriptors are constant; we'd need a set per-vframe if it was going to change
    // per-frame.
    vk::DescriptorPool dpool = context->create_descriptor_pool(dset_layout_ci, 1,
        vk::DescriptorPoolCreateFlags(), "Descriptor pool");
    vk::DescriptorSetAllocateInfo dset_alloc_info(dpool, 1, &dset_layout);
    std::vector<vk::DescriptorSet> dsets = context->device().allocateDescriptorSets(dset_alloc_info);
    VULKAN_CHECK(context->set_debug_name(dsets[0], "default descriptor set"));
    std::vector<vk::DescriptorImageInfo> image_infos = {
        vk::DescriptorImageInfo(sampler, texture_image_view, vk::ImageLayout::eShaderReadOnlyOptimal),
    };
    std::vector<vk::DescriptorBufferInfo> buffer_infos = {
        vk::DescriptorBufferInfo(o2w_buffer, 0, VK_WHOLE_SIZE),
    };
    std::vector<vk::WriteDescriptorSet> write_dsets = {
    //    vk::WriteDescriptorSet(dsets[0], 0, 0, (uint32_t)image_infos.size(),
    //        vk::DescriptorType::eCombinedImageSampler, image_infos.data(), nullptr, nullptr),
        vk::WriteDescriptorSet(dsets[0], 1, 0, (uint32_t)buffer_infos.size(),
            vk::DescriptorType::eUniformBufferDynamic, nullptr, buffer_infos.data(), nullptr),
    };
    context->device().updateDescriptorSets(write_dsets, nullptr);

    // Create the semaphores used to synchronize access to swapchain images
    vk::SemaphoreCreateInfo semaphore_ci = {};
    vk::Semaphore swapchain_image_ready_sem = context->create_semaphore(semaphore_ci, "image ready semaphore");
    vk::Semaphore render_complete_sem = context->create_semaphore(semaphore_ci, "rendering complete semaphore");

    // Create the fences used to wait for each swapchain image's command buffer to be submitted.
    // This prevents re-writing the command buffer contents before it's been submitted and processed.
    vk::FenceCreateInfo fence_ci(vk::FenceCreateFlagBits::eSignaled);
    std::vector<vk::Fence> submission_complete_fences(kVframeCount);
    for(auto &fence : submission_complete_fences) {
        fence = context->create_fence(fence_ci, "queue submitted fence");
    }

    uint32_t vframeIndex = 0;
    while(!glfwWindowShouldClose(window)) {
        // Wait for the command buffer previously used to generate this swapchain image to be submitted.
        // TODO(cort): this does not guarantee memory accesses from this submission will be visible on the host;
        // there'd need to be a memory barrier for that.
        context->device().waitForFences(submission_complete_fences[vframeIndex], VK_TRUE, UINT64_MAX);
        context->device().resetFences(submission_complete_fences[vframeIndex]);

        // The host can now safely reset and rebuild this command buffer, even if the GPU hasn't finished presenting the
        // resulting frame yet.
        vk::CommandBuffer cb = command_buffers[vframeIndex];

        // Retrieve the index of the next available swapchain index
        uint32_t swapchain_image_index = 0;
        vk::Result result = context->device().acquireNextImageKHR(context->swapchain(), UINT64_MAX,
            swapchain_image_ready_sem, VK_NULL_HANDLE, &swapchain_image_index);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
        } else if (result == vk::Result::eSuboptimalKHR) {
            // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
        } else {
            VULKAN_CHECK(result);
        }
        vk::Framebuffer framebuffer = framebuffers[swapchain_image_index];

        vk::CommandBufferBeginInfo cb_begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        cb.begin(cb_begin_info);
        std::array<vk::ClearValue, kAttachmentCount> clear_values = {
            vk::ClearColorValue(std::array<float,4>{{0,0,0.3f,1}}),
            vk::ClearDepthStencilValue(1.0f, 0)
        };
        vk::RenderPassBeginInfo render_pass_begin_info(render_pass, framebuffer,
            vk::Rect2D({0,0}, {framebuffer_ci.width, framebuffer_ci.height}),
            (uint32_t)clear_values.size(), clear_values.data());
        cb.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
        cb.endRenderPass();

        VULKAN_CHECK(vkEndCommandBuffer(cb));
        vk::PipelineStageFlags submit_wait_stage = vk::PipelineStageFlagBits::eAllCommands;
        vk::SubmitInfo submit_info(1, &swapchain_image_ready_sem, &submit_wait_stage, 1, &cb, 1, &render_complete_sem);
        context->graphics_queue().submit(submit_info, submission_complete_fences[vframeIndex]);

        // Present
        vk::SwapchainKHR swapchain = context->swapchain();
        vk::PresentInfoKHR present_info(1,&render_complete_sem, 1,&swapchain, &swapchain_image_index);
        result = context->present_queue().presentKHR(present_info);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
        } else if (result == vk::Result::eSuboptimalKHR) {
            // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
        } else {
            VULKAN_CHECK(result);
        }

        glfwPollEvents();
        vframeIndex += 1;
        if (vframeIndex == kVframeCount) {
            vframeIndex = 0;
        }
    }

    context->device().waitIdle();
    // Cleanup
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
    context->destroy_command_pool(command_pool);

    glfwTerminate();
    delete context;
    return 0;
}
