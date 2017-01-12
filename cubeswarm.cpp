#include "vk_application.h"
#include "vk_debug.h"
using namespace spokk;

#include "camera.h"
#include "cube_mesh.h"

#include <mathfu/vector.h>
#include <mathfu/glsl_mappings.h>

#include <array>
#include <cstdio>
#include <memory>

namespace {
constexpr uint32_t MESH_INSTANCE_COUNT = 1024;
}  // namespace

class CubeSwarmApp : public spokk::Application {
public:
  explicit CubeSwarmApp(Application::CreateInfo &ci) :
      Application(ci) {
    glfwSetInputMode(window_.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    seconds_elapsed_ = 0;

    const float fovDegrees = 45.0f;
    const float zNear = 0.01f;
    const float zFar = 100.0f;
    camera_ = my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, fovDegrees, zNear, zFar);
    const mathfu::vec3 initial_camera_pos(-1, 0, 6);
    const mathfu::vec3 initial_camera_target(0, 0, 0);
    const mathfu::vec3 initial_camera_up(0,1,0);
    camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
    dolly_ = my_make_unique<CameraDolly>(*camera_);

    // Create render pass
    render_pass_.init_from_preset(RenderPass::Preset::COLOR_DEPTH_POST, swapchain_surface_format_.format);
    SPOKK_VK_CHECK(render_pass_.finalize_and_create(device_context_));

    // Create depth buffer
    VkImageCreateInfo depth_image_ci = render_pass_.get_attachment_image_ci(1, swapchain_extent_);
    depth_image_ = {};
    SPOKK_VK_CHECK(depth_image_.create(device_context_, depth_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DEVICE_ALLOCATION_SCOPE_DEVICE));

    // Create intermediate color buffer
    VkImageCreateInfo offscreen_image_ci = render_pass_.get_attachment_image_ci(0, swapchain_extent_);
    SPOKK_VK_CHECK(offscreen_image_.create(device_context_, offscreen_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DEVICE_ALLOCATION_SCOPE_DEVICE));

    // Create VkFramebuffers
    std::vector<VkImageView> attachment_views = {
      offscreen_image_.view,
      depth_image_.view,
      VK_NULL_HANDLE, // filled in below
    };
    VkFramebufferCreateInfo framebuffer_ci = render_pass_.get_framebuffer_ci(swapchain_extent_);
    framebuffer_ci.pAttachments = attachment_views.data();
    framebuffers_.resize(swapchain_image_views_.size());
    for(size_t i=0; i<swapchain_image_views_.size(); ++i) {
      attachment_views[2] = swapchain_image_views_[i];
      SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &framebuffers_[i]));
    }

    // Load textures and samplers
    VkSamplerCreateInfo sampler_ci = get_sampler_ci(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    SPOKK_VK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &sampler_));
    image_loader_ = my_make_unique<ImageLoader>(device_context_);
    SPOKK_VK_CHECK(albedo_tex_.create_and_load(device_context_, *image_loader_.get(), "trevor/redf.ktx"));

    // Load shader pipelines
    SPOKK_VK_CHECK(mesh_vs_.create_and_load_spv_file(device_context_, "tri.vert.spv"));
    SPOKK_VK_CHECK(mesh_fs_.create_and_load_spv_file(device_context_, "tri.frag.spv"));
    SPOKK_VK_CHECK(mesh_shader_pipeline_.add_shader(&mesh_vs_));
    SPOKK_VK_CHECK(mesh_shader_pipeline_.add_shader(&mesh_fs_));

    SPOKK_VK_CHECK(fullscreen_tri_vs_.create_and_load_spv_file(device_context_, "fullscreen.vert.spv"));
    SPOKK_VK_CHECK(post_filmgrain_fs_.create_and_load_spv_file(device_context_, "subpass_post.frag.spv"));
    SPOKK_VK_CHECK(post_shader_pipeline_.add_shader(&fullscreen_tri_vs_));
    SPOKK_VK_CHECK(post_shader_pipeline_.add_shader(&post_filmgrain_fs_));

    SPOKK_VK_CHECK(ShaderPipeline::force_compatible_layouts_and_finalize(device_context_,
      {&mesh_shader_pipeline_, &post_shader_pipeline_}));

    // Populate Mesh object
    mesh_.index_type = (sizeof(cube_indices[0]) == sizeof(uint32_t)) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
    mesh_.index_count = cube_index_count;

    VkBufferCreateInfo index_buffer_ci = {};
    index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buffer_ci.size = cube_index_count * sizeof(cube_indices[0]);
    index_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    index_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(mesh_.index_buffer.create(device_context_, index_buffer_ci));
    SPOKK_VK_CHECK(mesh_.index_buffer.load(device_context_, cube_indices, index_buffer_ci.size));

    // Describe the mesh format.
    mesh_format_.vertex_buffer_bindings = {
      {0, 3+3+4, VK_VERTEX_INPUT_RATE_VERTEX},
    };
    mesh_format_.vertex_attributes = {
      {0, 0, VK_FORMAT_R8G8B8_SNORM, 0},
      {1, 0, VK_FORMAT_R8G8B8_SNORM, 3},
      {2, 0, VK_FORMAT_R16G16_SFLOAT, 6},
    };
    mesh_format_.finalize(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    mesh_.mesh_format = &mesh_format_;

    VkBufferCreateInfo vertex_buffer_ci = {};
    vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_ci.size = cube_vertex_count * mesh_format_.vertex_buffer_bindings[0].stride;
    vertex_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertex_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    mesh_.vertex_buffers.resize(1);
    SPOKK_VK_CHECK(mesh_.vertex_buffers[0].create(device_context_, vertex_buffer_ci));
    // Convert the vertex data from its original uncompressed format to its final format.
    // In a real application, this conversion would happen at asset build time.
    const VertexLayout src_vertex_layout = {
      {0, VK_FORMAT_R32G32B32_SFLOAT, 0},
      {1, VK_FORMAT_R32G32B32_SFLOAT, 12},
      {2, VK_FORMAT_R32G32_SFLOAT, 24},
    };
    const VertexLayout final_vertex_layout(mesh_format_, 0);
    std::vector<uint8_t> final_mesh_vertices(vertex_buffer_ci.size);
    int convert_error = convert_vertex_buffer(cube_vertices, src_vertex_layout,
      final_mesh_vertices.data(), final_vertex_layout, cube_vertex_count);
    assert(convert_error == 0);
    (void)convert_error;
    SPOKK_VK_CHECK(mesh_.vertex_buffers[0].load(device_context_, final_mesh_vertices.data(), vertex_buffer_ci.size));

    // Create pipelined buffer of per-mesh object-to-world matrices.
    VkBufferCreateInfo o2w_buffer_ci = {};
    o2w_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    o2w_buffer_ci.size = MESH_INSTANCE_COUNT * sizeof(mathfu::mat4);
    o2w_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    o2w_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(mesh_uniforms_.create(device_context_, VFRAME_COUNT, o2w_buffer_ci));

    SPOKK_VK_CHECK(mesh_pipeline_.create(device_context_, mesh_.mesh_format, &mesh_shader_pipeline_, &render_pass_, 0));

    SPOKK_VK_CHECK(fullscreen_pipeline_.create(device_context_,
      MeshFormat::get_empty(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
      &post_shader_pipeline_, &render_pass_, 1));
    // because the pipelines use a compatible layout, we can just add room for one full layout.
    for(const auto& dset_layout_ci : mesh_shader_pipeline_.dset_layout_cis) {
      dpool_.add(dset_layout_ci, VFRAME_COUNT);
    }
    SPOKK_VK_CHECK(dpool_.finalize(device_context_));

    DescriptorSetWriter dset_writer(mesh_shader_pipeline_.dset_layout_cis[0]);
    dset_writer.bind_image(albedo_tex_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler_, 1);
    dset_writer.bind_image(offscreen_image_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_NULL_HANDLE, 2, 0);
    for(uint32_t pframe = 0; pframe < VFRAME_COUNT; ++pframe) { 
      // TODO(cort): allocate_pipelined_set()?
      dsets_[pframe] = dpool_.allocate_set(device_context_, mesh_shader_pipeline_.dset_layouts[0]);
      dset_writer.bind_buffer(mesh_uniforms_.handle(pframe), 0, VK_WHOLE_SIZE, 0);
      dset_writer.write_all_to_dset(device_context_, dsets_[pframe]);
    }
  }
  virtual ~CubeSwarmApp() {
    if (device_) {
      vkDeviceWaitIdle(device_);

      dpool_.destroy(device_context_);

      mesh_uniforms_.destroy(device_context_);

      // TODO(cort): automate!
      mesh_.index_buffer.destroy(device_context_);
      mesh_.vertex_buffers[0].destroy(device_context_);

      fullscreen_pipeline_.destroy(device_context_);

      mesh_vs_.destroy(device_context_);
      mesh_fs_.destroy(device_context_);
      mesh_shader_pipeline_.destroy(device_context_);
      mesh_pipeline_.destroy(device_context_);

      post_shader_pipeline_.destroy(device_context_);
      fullscreen_tri_vs_.destroy(device_context_);
      post_filmgrain_fs_.destroy(device_context_);

      vkDestroySampler(device_, sampler_, host_allocator_);
      albedo_tex_.destroy(device_context_);
      image_loader_.reset();

      for(const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
      render_pass_.destroy(device_context_);

      offscreen_image_.destroy(device_context_);
      depth_image_.destroy(device_context_);
    }
  }

  CubeSwarmApp(const CubeSwarmApp&) = delete;
  const CubeSwarmApp& operator=(const CubeSwarmApp&) = delete;

  virtual void update(double dt) override {
    Application::update(dt);
    seconds_elapsed_ += dt;

    // Update camera
    mathfu::vec3 impulse(0,0,0);
    const float MOVE_SPEED = 0.5f, TURN_SPEED = 0.001f;
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_UP)) {
      impulse += camera_->getViewDirection() * MOVE_SPEED;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_LEFT)) {
      impulse -= cross(camera_->getViewDirection(), camera_->getWorldUp()) * MOVE_SPEED;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_DOWN)) {
      impulse -= camera_->getViewDirection() * MOVE_SPEED;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_RIGHT)) {
      impulse += cross(camera_->getViewDirection(), camera_->getWorldUp()) * MOVE_SPEED;
    }

    camera_->setOrientation(mathfu::quat::FromEulerAngles(mathfu::vec3(
      -TURN_SPEED * input_state_.GetAnalog(InputState::ANALOG_MOUSE_Y),
      -TURN_SPEED * input_state_.GetAnalog(InputState::ANALOG_MOUSE_X),
      0)));
    dolly_->Impulse(impulse);
    dolly_->Update((float)dt);

    // Update object-to-world matrices.
    const float secs = (float)seconds_elapsed_;
    std::array<mathfu::mat4, MESH_INSTANCE_COUNT*4> o2w_matrices;
    const mathfu::vec3 swarm_center(0, 0, -2);
    for(int iMesh=0; iMesh<MESH_INSTANCE_COUNT; ++iMesh) {
      mathfu::quat q = mathfu::quat::FromAngleAxis(secs + (float)iMesh, mathfu::vec3(0,1,0));
      mathfu::mat4 o2w = mathfu::mat4::Identity()
        * mathfu::mat4::FromTranslationVector(mathfu::vec3(
          40.0f * cosf((1.0f+0.001f*iMesh) * 0.2f * secs + float(149*iMesh) + 0.0f) + swarm_center[0],
          20.5f * sinf(0.3f * secs + float(13*iMesh) + 5.0f) + swarm_center[1],
          30.0f * sinf(0.05f * secs + float(51*iMesh) + 2.0f) + swarm_center[2]
        ))
        * q.ToMatrix4()
        * mathfu::mat4::FromScaleVector( mathfu::vec3(1.0f, 1.0f, 1.0f) )
        ;
      o2w_matrices[iMesh] = o2w;
    }
    mesh_uniforms_.load(device_context_, vframe_index_, o2w_matrices.data(), MESH_INSTANCE_COUNT * sizeof(mathfu::mat4),
      0, 0);
  }

  void render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override {
    VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
    // TODO(cort): spurious validation warning for unused clear value? Is that
    // actually bad? What if attachments 0 and 2 must be cleared but not 1?
    // Easy enough to work around in this case, just hard-code the array size.
    //std::vector<VkClearValue> clear_values(render_pass_.attachment_descs.size());
    std::vector<VkClearValue> clear_values(2);
    clear_values[0].color.float32[0] = 0.2f;
    clear_values[0].color.float32[1] = 0.2f;
    clear_values[0].color.float32[2] = 0.3f;
    clear_values[0].color.float32[3] = 0.0f;
    clear_values[1].depthStencil.depth = 1.0f;
    clear_values[1].depthStencil.stencil = 0;
    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass_.handle;
    render_pass_begin_info.framebuffer = framebuffer;
    render_pass_begin_info.renderArea.offset.x = 0;
    render_pass_begin_info.renderArea.offset.y = 0;
    render_pass_begin_info.renderArea.extent = swapchain_extent_;
    render_pass_begin_info.clearValueCount = (uint32_t)clear_values.size();
    render_pass_begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(primary_cb, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.handle);
    VkRect2D scissor_rect = render_pass_begin_info.renderArea;
    VkViewport viewport = vk_rect2d_to_viewport(scissor_rect);
    vkCmdSetViewport(primary_cb, 0,1, &viewport);
    vkCmdSetScissor(primary_cb, 0,1, &scissor_rect);
    // TODO(cort): leaving these unbound did not trigger a validation warning...
    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
      mesh_pipeline_.shader_pipeline->pipeline_layout,
      0, 1, &dsets_[vframe_index_], 0, nullptr);
    struct {
      mathfu::vec4_packed time_and_res;
      mathfu::vec4_packed eye;
      mathfu::mat4 viewproj;
    } push_constants = {};
    push_constants.time_and_res = mathfu::vec4((float)seconds_elapsed_,
      viewport.width, viewport.height, 0);
    push_constants.eye = mathfu::vec4(camera_->getEyePoint(), 1.0f);
    mathfu::mat4 w2v = camera_->getViewMatrix();
    const mathfu::mat4 proj = camera_->getProjectionMatrix();
    const mathfu::mat4 clip_fixup(
      +1.0f, +0.0f, +0.0f, +0.0f,
      +0.0f, -1.0f, +0.0f, +0.0f,
      +0.0f, +0.0f, +0.5f, +0.5f,
      +0.0f, +0.0f, +0.0f, +1.0f);
    const mathfu::mat4 viewproj = clip_fixup * proj * w2v;
    push_constants.viewproj = viewproj;
    vkCmdPushConstants(primary_cb, mesh_pipeline_.shader_pipeline->pipeline_layout,
      mesh_pipeline_.shader_pipeline->push_constant_ranges[0].stageFlags,
      mesh_pipeline_.shader_pipeline->push_constant_ranges[0].offset,
      mesh_pipeline_.shader_pipeline->push_constant_ranges[0].size,
      &push_constants);
    const VkDeviceSize vertex_buffer_offsets[1] = {}; // TODO(cort): mesh::bind()
    VkBuffer vertex_buffer = mesh_.vertex_buffers[0].handle();
    vkCmdBindVertexBuffers(primary_cb, 0,1, &vertex_buffer, vertex_buffer_offsets);
    const VkDeviceSize index_buffer_offset = 0;
    vkCmdBindIndexBuffer(primary_cb, mesh_.index_buffer.handle(), index_buffer_offset, mesh_.index_type);
    vkCmdDrawIndexed(primary_cb, mesh_.index_count, MESH_INSTANCE_COUNT, 0,0,0);

    // post-processing subpass
    vkCmdNextSubpass(primary_cb, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreen_pipeline_.handle);
    vkCmdSetViewport(primary_cb, 0,1, &viewport);
    vkCmdSetScissor(primary_cb, 0,1, &scissor_rect);
    vkCmdDraw(primary_cb, 3, 1,0,0);
    vkCmdEndRenderPass(primary_cb);
  }

private:
  double seconds_elapsed_;

  Image depth_image_;
  Image offscreen_image_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  std::unique_ptr<ImageLoader> image_loader_;
  Image albedo_tex_;
  VkSampler sampler_;

  Shader mesh_vs_, mesh_fs_;
  ShaderPipeline mesh_shader_pipeline_;
  GraphicsPipeline mesh_pipeline_;

  Shader fullscreen_tri_vs_, post_filmgrain_fs_;
  ShaderPipeline post_shader_pipeline_;
  GraphicsPipeline fullscreen_pipeline_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, VFRAME_COUNT> dsets_;

  MeshFormat mesh_format_;
  Mesh mesh_;
  PipelinedBuffer mesh_uniforms_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDolly> dolly_;
};

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
    {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}
  };
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;

  CubeSwarmApp app(app_ci);
  int run_error = app.run();

  return run_error;
}