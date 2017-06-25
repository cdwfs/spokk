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
struct SceneUniforms {
  mathfu::vec4_packed time_and_res;  // x: elapsed seconds, yz: viewport resolution in pixels
  mathfu::vec4_packed eye_pos_ws;    // xyz: world-space eye position
  mathfu::vec4_packed eye_dir_wsn;   // xyz: world-space eye direction (normalized)
  mathfu::mat4 viewproj;
  mathfu::mat4 view;
  mathfu::mat4 proj;
  mathfu::mat4 viewproj_inv;
  mathfu::mat4 view_inv;
  mathfu::mat4 proj_inv;
};
constexpr uint32_t MESH_INSTANCE_COUNT = 1024;
constexpr float FOV_DEGREES = 45.0f;
constexpr float Z_NEAR = 0.01f;
constexpr float Z_FAR = 100.0f;
}  // namespace

class CubeSwarmApp : public spokk::Application {
public:
  explicit CubeSwarmApp(Application::CreateInfo &ci) :
      Application(ci) {
    glfwSetInputMode(window_.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    seconds_elapsed_ = 0;

    camera_ = my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, FOV_DEGREES, Z_NEAR, Z_FAR);
    const mathfu::vec3 initial_camera_pos(0, 0, 0);
    const mathfu::vec3 initial_camera_target(0, 0, 0);
    const mathfu::vec3 initial_camera_up(0,1,0);
    camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
    dolly_ = my_make_unique<CameraDolly>(*camera_);

    // Create render pass
    render_pass_.InitFromPreset(RenderPass::Preset::COLOR_DEPTH, swapchain_surface_format_.format);
    SPOKK_VK_CHECK(render_pass_.Finalize(device_context_));
    render_pass_.clear_values[0].color.float32[0] = 0.2f;
    render_pass_.clear_values[0].color.float32[1] = 0.2f;
    render_pass_.clear_values[0].color.float32[2] = 0.3f;
    render_pass_.clear_values[0].color.float32[3] = 0.0f;
    render_pass_.clear_values[1].depthStencil.depth = 1.0f;
    render_pass_.clear_values[1].depthStencil.stencil = 0;

    // Load textures and samplers
    VkSamplerCreateInfo sampler_ci = GetSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    SPOKK_VK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &sampler_));
    const VkDeviceSize blit_buffer_nbytes = 4*1024*1024;
    SPOKK_VK_CHECK(blitter_.Create(device_context_, PFRAME_COUNT, blit_buffer_nbytes));
    albedo_tex_.CreateFromFile(device_context_, blitter_, graphics_and_present_queue_, "data/redf.ktx");

    // Load shader pipelines
    SPOKK_VK_CHECK(mesh_vs_.CreateAndLoadSpirvFile(device_context_, "rigid_mesh.vert.spv"));
    SPOKK_VK_CHECK(mesh_fs_.CreateAndLoadSpirvFile(device_context_, "rigid_mesh.frag.spv"));
    SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_vs_));
    SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_fs_));
    SPOKK_VK_CHECK(mesh_shader_program_.Finalize(device_context_));

    // Populate Mesh object
    mesh_.index_type = (sizeof(cube_indices[0]) == sizeof(uint32_t)) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
    mesh_.index_count = cube_index_count;

    VkBufferCreateInfo index_buffer_ci = {};
    index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buffer_ci.size = cube_index_count * sizeof(cube_indices[0]);
    index_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    index_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(mesh_.index_buffer.Create(device_context_, index_buffer_ci));
    SPOKK_VK_CHECK(mesh_.index_buffer.Load(device_context_, cube_indices, index_buffer_ci.size));

    // Describe the mesh format.
    mesh_format_.vertex_buffer_bindings = {
      {0, 4+4+4, VK_VERTEX_INPUT_RATE_VERTEX},
    };
    mesh_format_.vertex_attributes = {
      {0, 0, VK_FORMAT_R8G8B8A8_SNORM, 0},
      {1, 0, VK_FORMAT_R8G8B8A8_SNORM, 4},
      {2, 0, VK_FORMAT_R16G16_SFLOAT, 8},
    };
    mesh_format_.Finalize(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    mesh_.mesh_format = &mesh_format_;

    VkBufferCreateInfo vertex_buffer_ci = {};
    vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_ci.size = cube_vertex_count * mesh_format_.vertex_buffer_bindings[0].stride;
    vertex_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertex_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    mesh_.vertex_buffers.resize(1);
    SPOKK_VK_CHECK(mesh_.vertex_buffers[0].Create(device_context_, vertex_buffer_ci));
    // Convert the vertex data from its original uncompressed format to its final format.
    // In a real application, this conversion would happen at asset build time.
    const VertexLayout src_vertex_layout = {
      {0, VK_FORMAT_R32G32B32_SFLOAT, 0},
      {1, VK_FORMAT_R32G32B32_SFLOAT, 12},
      {2, VK_FORMAT_R32G32_SFLOAT, 24},
    };
    const VertexLayout final_vertex_layout(mesh_format_, 0);
    std::vector<uint8_t> final_mesh_vertices(vertex_buffer_ci.size);
    int convert_error = ConvertVertexBuffer(cube_vertices, src_vertex_layout,
      final_mesh_vertices.data(), final_vertex_layout, cube_vertex_count);
    assert(convert_error == 0);
    (void)convert_error;
    SPOKK_VK_CHECK(mesh_.vertex_buffers[0].Load(device_context_, final_mesh_vertices.data(), vertex_buffer_ci.size));

    // Create pipelined buffer of per-mesh object-to-world matrices.
    VkBufferCreateInfo o2w_buffer_ci = {};
    o2w_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    o2w_buffer_ci.size = MESH_INSTANCE_COUNT * sizeof(mathfu::mat4);
    o2w_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    o2w_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(mesh_uniforms_.Create(device_context_, PFRAME_COUNT, o2w_buffer_ci));

    // Create pipelined buffer of shader uniforms
    VkBufferCreateInfo scene_uniforms_ci = {};
    scene_uniforms_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    scene_uniforms_ci.size = sizeof(SceneUniforms);
    scene_uniforms_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    scene_uniforms_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(scene_uniforms_.Create(device_context_, PFRAME_COUNT, scene_uniforms_ci,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    mesh_pipeline_.Init(mesh_.mesh_format, &mesh_shader_program_, &render_pass_, 0);
    SPOKK_VK_CHECK(mesh_pipeline_.Finalize(device_context_));

    for(const auto& dset_layout_ci : mesh_shader_program_.dset_layout_cis) {
      dpool_.Add(dset_layout_ci, PFRAME_COUNT);
    }
    SPOKK_VK_CHECK(dpool_.Finalize(device_context_));
    for(uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) { 
      // TODO(cort): allocate_pipelined_set()?
      dsets_[pframe] = dpool_.AllocateSet(device_context_, mesh_shader_program_.dset_layouts[0]);
    }
    DescriptorSetWriter dset_writer(mesh_shader_program_.dset_layout_cis[0]);
    dset_writer.BindImage(albedo_tex_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      mesh_fs_.GetDescriptorBindPoint("tex").binding);
    dset_writer.BindSampler(sampler_, mesh_fs_.GetDescriptorBindPoint("samp").binding);
    for(uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
      dset_writer.BindBuffer(scene_uniforms_.Handle(pframe),
        mesh_vs_.GetDescriptorBindPoint("scene_consts").binding);
      dset_writer.BindBuffer(mesh_uniforms_.Handle(pframe),
        mesh_vs_.GetDescriptorBindPoint("mesh_consts").binding);
      dset_writer.WriteAll(device_context_, dsets_[pframe]);
    }

    // Create swapchain-sized buffers
    CreateRenderBuffers(swapchain_extent_);
  }
  virtual ~CubeSwarmApp() {
    if (device_) {
      vkDeviceWaitIdle(device_);

      dpool_.Destroy(device_context_);

      mesh_uniforms_.Destroy(device_context_);
      scene_uniforms_.Destroy(device_context_);

      // TODO(cort): automate!
      mesh_.index_buffer.Destroy(device_context_);
      mesh_.vertex_buffers[0].Destroy(device_context_);

      mesh_vs_.Destroy(device_context_);
      mesh_fs_.Destroy(device_context_);
      mesh_shader_program_.Destroy(device_context_);
      mesh_pipeline_.Destroy(device_context_);

      vkDestroySampler(device_, sampler_, host_allocator_);
      albedo_tex_.Destroy(device_context_);
      blitter_.Destroy(device_context_);

      for(const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
      render_pass_.Destroy(device_context_);

      depth_image_.Destroy(device_context_);
    }
  }

  CubeSwarmApp(const CubeSwarmApp&) = delete;
  const CubeSwarmApp& operator=(const CubeSwarmApp&) = delete;

  virtual void Update(double dt) override {
    Application::Update(dt);
    seconds_elapsed_ += dt;

    // Update camera
    mathfu::vec3 impulse(0,0,0);
    const float MOVE_SPEED = 0.5f, TURN_SPEED = 0.001f;
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_UP)) {
      impulse += camera_->getViewDirection() * MOVE_SPEED;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_LEFT)) {
      mathfu::vec3 viewRight = camera_->getOrientation() * mathfu::vec3(1,0,0);
      impulse -= viewRight * MOVE_SPEED;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_DOWN)) {
      impulse -= camera_->getViewDirection() * MOVE_SPEED;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_RIGHT)) {
      mathfu::vec3 viewRight = camera_->getOrientation() * mathfu::vec3(1,0,0);
      impulse += viewRight * MOVE_SPEED;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_RPAD_LEFT)) {
      mathfu::vec3 viewUp = camera_->getOrientation() * mathfu::vec3(0,1,0);
      impulse -= viewUp * MOVE_SPEED;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_RPAD_DOWN)) {
      mathfu::vec3 viewUp = camera_->getOrientation() * mathfu::vec3(0,1,0);
      impulse += viewUp * MOVE_SPEED;
    }

    // Update camera based on mouse delta
    mathfu::vec3 camera_eulers = camera_->getEulersYPR() + mathfu::vec3(
      -TURN_SPEED * input_state_.GetAnalogDelta(InputState::ANALOG_MOUSE_Y),
      -TURN_SPEED * input_state_.GetAnalogDelta(InputState::ANALOG_MOUSE_X),
      0);
    if (camera_eulers[0] >= float(M_PI_2 - 0.01f)) {
      camera_eulers[0] = float(M_PI_2 - 0.01f);
    } else if (camera_eulers[0] <= float(-M_PI_2 + 0.01f)) {
      camera_eulers[0] = float(-M_PI_2 + 0.01f);
    }
    camera_eulers[2] = 0; // disallow roll
    camera_->setOrientation(mathfu::quat::FromEulerAngles(camera_eulers));
    dolly_->Impulse(impulse);
    dolly_->Update((float)dt);

    // Update uniforms
    SceneUniforms* uniforms = (SceneUniforms*)scene_uniforms_.Mapped(pframe_index_);
    uniforms->time_and_res = mathfu::vec4(
      (float)seconds_elapsed_, (float)swapchain_extent_.width, (float)swapchain_extent_.height, 0);
    uniforms->eye_pos_ws  = mathfu::vec4(camera_->getEyePoint(), 1.0f);
    uniforms->eye_dir_wsn = mathfu::vec4(camera_->getViewDirection().Normalized(), 1.0f);
    const mathfu::mat4 view = camera_->getViewMatrix();
    const mathfu::mat4 proj = camera_->getProjectionMatrix();
    const mathfu::mat4 clip_fixup(
      +1.0f, +0.0f, +0.0f, +0.0f,
      +0.0f, -1.0f, +0.0f, +0.0f,
      +0.0f, +0.0f, +0.5f, +0.5f,
      +0.0f, +0.0f, +0.0f, +1.0f);
    const mathfu::mat4 viewproj = (clip_fixup * proj) * view;
    uniforms->viewproj = viewproj;
    uniforms->view = view;
    uniforms->proj = clip_fixup * proj;
    uniforms->viewproj_inv = viewproj.Inverse();
    uniforms->view_inv = view.Inverse();
    uniforms->proj_inv = (clip_fixup * proj).Inverse();
    scene_uniforms_.FlushPframeHostCache(pframe_index_);

    // Update object-to-world matrices.
    const float secs = (float)seconds_elapsed_;
    std::array<mathfu::mat4, MESH_INSTANCE_COUNT*4> o2w_matrices;
    const mathfu::vec3 swarm_center(0, 0, 0);
    for(int iMesh=0; iMesh<MESH_INSTANCE_COUNT; ++iMesh) {
      mathfu::quat q = mathfu::quat::FromAngleAxis(secs + (float)iMesh, mathfu::vec3(1,2,3).Normalized());
      mathfu::mat4 o2w = mathfu::mat4::Identity()
        * mathfu::mat4::FromTranslationVector(mathfu::vec3(
          60.0f * cosf(0.2f * secs + float(9*iMesh) + 0.4f) + swarm_center[0],
          41.0f * sinf(0.3f * secs + float(11*iMesh) + 5.0f) + swarm_center[1],
          60.0f * sinf(0.5f * secs + float(13*iMesh) + 2.0f) + swarm_center[2]
        ))
        * q.ToMatrix4()
        * mathfu::mat4::FromScaleVector( mathfu::vec3(1.0f, 1.0f, 1.0f) )
        ;
      o2w_matrices[iMesh] = o2w;
    }
    mesh_uniforms_.Load(device_context_, pframe_index_, o2w_matrices.data(), MESH_INSTANCE_COUNT * sizeof(mathfu::mat4),
      0, 0);
  }

  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override {
    blitter_.NextPframe();

    VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
    render_pass_.begin_info.framebuffer = framebuffer;
    render_pass_.begin_info.renderArea.extent = swapchain_extent_;
    vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.handle);
    VkRect2D scissor_rect = render_pass_.begin_info.renderArea;
    VkViewport viewport = Rect2DToViewport(scissor_rect);
    vkCmdSetViewport(primary_cb, 0,1, &viewport);
    vkCmdSetScissor(primary_cb, 0,1, &scissor_rect);
    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
      mesh_pipeline_.shader_program->pipeline_layout,
      0, 1, &dsets_[pframe_index_], 0, nullptr);
    const VkDeviceSize vertex_buffer_offsets[1] = {}; // TODO(cort): mesh::bind()
    VkBuffer vertex_buffer = mesh_.vertex_buffers[0].Handle();
    vkCmdBindVertexBuffers(primary_cb, 0,1, &vertex_buffer, vertex_buffer_offsets);
    const VkDeviceSize index_buffer_offset = 0;
    vkCmdBindIndexBuffer(primary_cb, mesh_.index_buffer.Handle(), index_buffer_offset, mesh_.index_type);
    vkCmdDrawIndexed(primary_cb, mesh_.index_count, MESH_INSTANCE_COUNT, 0,0,0);
    vkCmdEndRenderPass(primary_cb);
  }

protected:
  void HandleWindowResize(VkExtent2D new_window_extent) override {
    Application::HandleWindowResize(new_window_extent);

    // Destroy existing objects before re-creating them.
    for(auto fb : framebuffers_) {
      if (fb != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
    }
    framebuffers_.clear();
    depth_image_.Destroy(device_context_);

    float aspect_ratio = (float)new_window_extent.width / (float)new_window_extent.height;
    camera_->setPerspective(FOV_DEGREES, aspect_ratio, Z_NEAR, Z_FAR);

    CreateRenderBuffers(new_window_extent);
  }

private:
  void CreateRenderBuffers(VkExtent2D extent) {
    // Create depth buffer
    VkImageCreateInfo depth_image_ci = render_pass_.GetAttachmentImageCreateInfo(1, extent);
    depth_image_ = {};
    SPOKK_VK_CHECK(depth_image_.Create(device_context_, depth_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DEVICE_ALLOCATION_SCOPE_DEVICE));

    // Create VkFramebuffers
    std::vector<VkImageView> attachment_views = {
      VK_NULL_HANDLE, // filled in below
      depth_image_.view,
    };
    VkFramebufferCreateInfo framebuffer_ci = render_pass_.GetFramebufferCreateInfo(extent);
    framebuffer_ci.pAttachments = attachment_views.data();
    framebuffers_.resize(swapchain_image_views_.size());
    for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
      attachment_views[0] = swapchain_image_views_[i];
      SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &framebuffers_[i]));
    }
  }

  double seconds_elapsed_;

  Image depth_image_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  ImageBlitter blitter_;
  Image albedo_tex_;
  VkSampler sampler_;

  Shader mesh_vs_, mesh_fs_;
  ShaderProgram mesh_shader_program_;
  GraphicsPipeline mesh_pipeline_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

  MeshFormat mesh_format_;
  Mesh mesh_;
  PipelinedBuffer mesh_uniforms_;
  PipelinedBuffer scene_uniforms_;

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
  app_ci.pfn_set_device_features = EnableMinimumDeviceFeatures;

  CubeSwarmApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
