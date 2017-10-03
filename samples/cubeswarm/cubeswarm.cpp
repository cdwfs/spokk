#include <spokk.h>
using namespace spokk;

#include <common/camera.h>
#include <common/cube_mesh.h>

#include <mathfu/glsl_mappings.h>
#include <mathfu/vector.h>

#include <array>
#include <cstdio>
#include <memory>

namespace {
constexpr uint32_t MESH_INSTANCE_COUNT = 1024;
constexpr float FOV_DEGREES = 45.0f;
constexpr float Z_NEAR = 0.01f;
constexpr float Z_FAR = 100.0f;
}  // namespace

class CubeSwarmApp : public spokk::Application {
public:
  explicit CubeSwarmApp(Application::CreateInfo& ci) : Application(ci) {
    glfwSetInputMode(window_.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    seconds_elapsed_ = 0;

    camera_ =
        my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, FOV_DEGREES, Z_NEAR, Z_FAR);
    const mathfu::vec3 initial_camera_pos(-1, 0, 6);
    const mathfu::vec3 initial_camera_target(0, 0, 0);
    const mathfu::vec3 initial_camera_up(0, 1, 0);
    camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
    dolly_ = my_make_unique<CameraDolly>(*camera_);

    // Create render pass
    render_pass_.InitFromPreset(RenderPass::Preset::COLOR_DEPTH, swapchain_surface_format_.format);
    SPOKK_VK_CHECK(render_pass_.Finalize(device_));
    render_pass_.clear_values[0] = CreateColorClearValue(0.2f, 0.2f, 0.3f);
    render_pass_.clear_values[1] = CreateDepthClearValue(1.0f, 0);

    // Renderer
    Renderer::CreateInfo renderer_ci = {};
    renderer_ci.pframe_count = PFRAME_COUNT;
    int renderer_create_error = renderer_.Create(device_, renderer_ci);
    ZOMBO_ASSERT(!renderer_create_error, "Renderer create returned %d", renderer_create_error);

    // Load shader pipelines
    SPOKK_VK_CHECK(mesh_vs_.CreateAndLoadSpirvFile(device_, "data/rigid_mesh.vert.spv"));
    SPOKK_VK_CHECK(mesh_fs_.CreateAndLoadSpirvFile(device_, "data/rigid_mesh.frag.spv"));
    SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_vs_));
    SPOKK_VK_CHECK(mesh_shader_program_.AddShader(&mesh_fs_));
    SPOKK_VK_CHECK(mesh_shader_program_.AddRendererDsets(renderer_));
    SPOKK_VK_CHECK(mesh_shader_program_.Finalize(device_));

    // Populate Mesh object
    int mesh_load_error = mesh_.CreateFromFile(device_, "data/teapot.mesh");
    ZOMBO_ASSERT(!mesh_load_error, "load error: %d", mesh_load_error);

    mesh_pipeline_.Init(&mesh_.mesh_format, &mesh_shader_program_, &render_pass_, 0);
    SPOKK_VK_CHECK(mesh_pipeline_.Finalize(device_));

    // Material
    material_.pipeline = &mesh_pipeline_;
    material_.material_dsets.resize(PFRAME_COUNT);

    // Mesh Instance
    for (uint32_t i = 0; i < MESH_INSTANCE_COUNT; ++i) {
      MeshInstance* instance = renderer_.CreateInstance(&mesh_, &material_);
      ZOMBO_ASSERT(instance, "Failed to add mesh instance");
      mesh_instances_.push_back(instance);
    }

    // Create swapchain-sized buffers
    CreateRenderBuffers(swapchain_extent_);
  }
  virtual ~CubeSwarmApp() {
    if (device_ != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device_);

      renderer_.Destroy(device_);

      mesh_.Destroy(device_);

      mesh_vs_.Destroy(device_);
      mesh_fs_.Destroy(device_);
      mesh_shader_program_.Destroy(device_);
      mesh_pipeline_.Destroy(device_);

      for (const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
      render_pass_.Destroy(device_);

      depth_image_.Destroy(device_);
    }
  }

  CubeSwarmApp(const CubeSwarmApp&) = delete;
  const CubeSwarmApp& operator=(const CubeSwarmApp&) = delete;

  virtual void Update(double dt) override {
    Application::Update(dt);
    seconds_elapsed_ += dt;

    // Update camera
    mathfu::vec3 camera_accel_dir(0, 0, 0);
    const float CAMERA_ACCEL_MAG = 100.0f, CAMERA_TURN_SPEED = 0.001f;
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_UP)) {
      camera_accel_dir += camera_->getViewDirection();
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_LEFT)) {
      mathfu::vec3 viewRight = camera_->getOrientation() * mathfu::vec3(1, 0, 0);
      camera_accel_dir -= viewRight;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_DOWN)) {
      camera_accel_dir -= camera_->getViewDirection();
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_RIGHT)) {
      mathfu::vec3 viewRight = camera_->getOrientation() * mathfu::vec3(1, 0, 0);
      camera_accel_dir += viewRight;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_RPAD_LEFT)) {
      mathfu::vec3 viewUp = camera_->getOrientation() * mathfu::vec3(0, 1, 0);
      camera_accel_dir -= viewUp;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_RPAD_DOWN)) {
      mathfu::vec3 viewUp = camera_->getOrientation() * mathfu::vec3(0, 1, 0);
      camera_accel_dir += viewUp;
    }
    mathfu::vec3 camera_accel = (camera_accel_dir.LengthSquared() > 0)
        ? camera_accel_dir.Normalized() * CAMERA_ACCEL_MAG
        : mathfu::vec3(0, 0, 0);

    // Update camera based on acceleration vector and mouse delta
    mathfu::vec3 camera_eulers = camera_->getEulersYPR() +
        mathfu::vec3(-CAMERA_TURN_SPEED * input_state_.GetAnalogDelta(InputState::ANALOG_MOUSE_Y),
            -CAMERA_TURN_SPEED * input_state_.GetAnalogDelta(InputState::ANALOG_MOUSE_X), 0);
    if (camera_eulers[0] >= float(M_PI_2 - 0.01f)) {
      camera_eulers[0] = float(M_PI_2 - 0.01f);
    } else if (camera_eulers[0] <= float(-M_PI_2 + 0.01f)) {
      camera_eulers[0] = float(-M_PI_2 + 0.01f);
    }
    camera_eulers[2] = 0;  // disallow roll
    camera_->setOrientation(mathfu::quat::FromEulerAngles(camera_eulers));
    dolly_->Update(camera_accel, (float)dt);

    // Update transforms
    const float secs = (float)seconds_elapsed_;
    const mathfu::vec3 swarm_center(0, 0, -2);
    for (size_t iMesh = 0; iMesh < mesh_instances_.size(); ++iMesh) {
      mesh_instances_[iMesh]->transform_.orientation =
          mathfu::quat::FromAngleAxis(secs + (float)iMesh, mathfu::vec3(1, 2, 3).Normalized());
      mesh_instances_[iMesh]->transform_.pos = mathfu::vec3(
          // clang-format off
          40.0f * cosf(0.2f * secs + float(9*iMesh) + 0.4f) + swarm_center[0],
          20.5f * sinf(0.3f * secs + float(11*iMesh) + 5.0f) + swarm_center[1],
          30.0f * sinf(0.5f * secs + float(13*iMesh) + 2.0f) + swarm_center[2]
          // clang-format on
      );
      mesh_instances_[iMesh]->transform_.scale = 3.0f;
    }
  }

  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override {
    VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
    render_pass_.begin_info.framebuffer = framebuffer;
    render_pass_.begin_info.renderArea.extent = swapchain_extent_;
    vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
    VkRect2D scissor_rect = render_pass_.begin_info.renderArea;
    VkViewport viewport = Rect2DToViewport(scissor_rect);
    vkCmdSetViewport(primary_cb, 0, 1, &viewport);
    vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);
    renderer_.RenderView(primary_cb, camera_->getViewMatrix(), camera_->getProjectionMatrix(),
        mathfu::vec4((float)seconds_elapsed_, (float)swapchain_extent_.width, (float)swapchain_extent_.height, 0));
    vkCmdEndRenderPass(primary_cb);
  }

protected:
  void HandleWindowResize(VkExtent2D new_window_extent) override {
    Application::HandleWindowResize(new_window_extent);

    // Destroy existing objects before re-creating them.
    for (auto fb : framebuffers_) {
      if (fb != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
    }
    framebuffers_.clear();
    depth_image_.Destroy(device_);

    float aspect_ratio = (float)new_window_extent.width / (float)new_window_extent.height;
    camera_->setPerspective(FOV_DEGREES, aspect_ratio, Z_NEAR, Z_FAR);

    CreateRenderBuffers(new_window_extent);
  }

private:
  void CreateRenderBuffers(VkExtent2D extent) {
    // Create depth buffer
    VkImageCreateInfo depth_image_ci = render_pass_.GetAttachmentImageCreateInfo(1, extent);
    depth_image_ = {};
    SPOKK_VK_CHECK(depth_image_.Create(
        device_, depth_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DEVICE_ALLOCATION_SCOPE_DEVICE));

    // Create VkFramebuffers
    std::vector<VkImageView> attachment_views = {
        VK_NULL_HANDLE,  // filled in below
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

  Shader mesh_vs_, mesh_fs_;
  ShaderProgram mesh_shader_program_;
  GraphicsPipeline mesh_pipeline_;

  Mesh mesh_;
  Material material_;
  std::vector<MeshInstance*> mesh_instances_;
  Renderer renderer_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDolly> dolly_;
};

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
      {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}};
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;
  app_ci.pfn_set_device_features = EnableMinimumDeviceFeatures;

  CubeSwarmApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
