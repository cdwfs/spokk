#include <spokk.h>
using namespace spokk;

#include <common/camera.h>
#include <common/cube_mesh.h>

#include <mathfu/glsl_mappings.h>
#include <mathfu/vector.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {
constexpr float FOV_DEGREES = 45.0f;
constexpr float Z_NEAR = 0.01f;
constexpr float Z_FAR = 100.0f;
}  // namespace

#define HEIGHTFIELD_DIMX 256
#define HEIGHTFIELD_DIMY 256
#define XY_TO_CELL(x, y) ((y)*HEIGHTFIELD_DIMX + (x))
#define CELL_X(cell) ((cell) % HEIGHTFIELD_DIMX)
#define CELL_Y(cell) ((cell) / HEIGHTFIELD_DIMX)

#define EFFECT_RADIUS 9
#define VISIBLE_RADIUS ((EFFECT_RADIUS) + 1)

class PillarsApp : public spokk::Application {
public:
  explicit PillarsApp(Application::CreateInfo& ci);
  virtual ~PillarsApp();
  PillarsApp(const PillarsApp&) = delete;
  const PillarsApp& operator=(const PillarsApp&) = delete;

  void Update(double dt) override;
  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override;

protected:
  void HandleWindowResize(VkExtent2D new_window_extent) override;

private:
  void CreateRenderBuffers(VkExtent2D extent);

  double seconds_elapsed_;

  Image depth_image_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  Image albedo_tex_;
  VkSampler sampler_;

  Shader pillar_vs_, pillar_fs_;
  ShaderProgram pillar_shader_program_;
  GraphicsPipeline pillar_pipeline_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

  Mesh mesh_;
  PipelinedBuffer camera_constants_;
  PipelinedBuffer heightfield_buffer_;
  PipelinedBuffer visible_cells_buffer_;

  std::vector<int32_t> visible_cells_;
  std::array<float, HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY> heightfield_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDolly> dolly_;
};

PillarsApp::PillarsApp(Application::CreateInfo& ci) : Application(ci) {
  glfwSetInputMode(window_.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  seconds_elapsed_ = 0;

  camera_ = my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, FOV_DEGREES, Z_NEAR, Z_FAR);
  const mathfu::vec3 initial_camera_pos(HEIGHTFIELD_DIMX / 2, 2.0f, HEIGHTFIELD_DIMY / 2);
  const mathfu::vec3 initial_camera_target(0, 0, 0);
  const mathfu::vec3 initial_camera_up(0, 1, 0);
  camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
  dolly_ = my_make_unique<CameraDolly>(*camera_);
  dolly_->SetBounds(mathfu::vec3(VISIBLE_RADIUS, 1, VISIBLE_RADIUS),
      mathfu::vec3(HEIGHTFIELD_DIMX - VISIBLE_RADIUS - 1, 30, HEIGHTFIELD_DIMY - VISIBLE_RADIUS - 1));

  // Create render pass
  render_pass_.InitFromPreset(RenderPass::Preset::COLOR_DEPTH, swapchain_surface_format_.format);
  SPOKK_VK_CHECK(render_pass_.Finalize(device_));
  render_pass_.clear_values[0] = CreateColorClearValue(0.2f, 0.2f, 0.3f);
  render_pass_.clear_values[1] = CreateDepthClearValue(1.0f, 0);

  // Load textures and samplers
  VkSamplerCreateInfo sampler_ci =
      GetSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
  SPOKK_VK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &sampler_));
  albedo_tex_.CreateFromFile(device_, graphics_and_present_queue_, "data/redf.ktx");

  // Load shader pipelines
  SPOKK_VK_CHECK(pillar_vs_.CreateAndLoadSpirvFile(device_, "data/pillar.vert.spv"));
  SPOKK_VK_CHECK(pillar_fs_.CreateAndLoadSpirvFile(device_, "data/pillar.frag.spv"));
  SPOKK_VK_CHECK(pillar_shader_program_.AddShader(&pillar_vs_));
  SPOKK_VK_CHECK(pillar_shader_program_.AddShader(&pillar_fs_));
  SPOKK_VK_CHECK(pillar_shader_program_.Finalize(device_));

  // Describe the mesh format.
  // clang-format off
  mesh_.mesh_format.vertex_buffer_bindings = {
    {0, 4+4+2, VK_VERTEX_INPUT_RATE_VERTEX},
  };
  mesh_.mesh_format.vertex_attributes = {
    {0, 0, VK_FORMAT_R8G8B8A8_SNORM, 0},
    {1, 0, VK_FORMAT_R8G8B8A8_SNORM, 4},
    {2, 0, VK_FORMAT_R8G8_UNORM, 8},
  };
  // clang-format on
  mesh_.mesh_format.Finalize(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  // Populate Mesh object
  mesh_.index_type = (sizeof(cube_indices[0]) == sizeof(uint32_t)) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
  mesh_.index_count = cube_index_count;
  // index buffer
  VkBufferCreateInfo index_buffer_ci = {};
  index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  index_buffer_ci.size = cube_index_count * sizeof(cube_indices[0]);
  index_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  index_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(mesh_.index_buffer.Create(device_, index_buffer_ci));
  SPOKK_VK_CHECK(mesh_.index_buffer.Load(device_, cube_indices, index_buffer_ci.size));
  // vertex buffer
  VkBufferCreateInfo vertex_buffer_ci = {};
  vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vertex_buffer_ci.size = cube_vertex_count * mesh_.mesh_format.vertex_buffer_bindings[0].stride;
  vertex_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  vertex_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  mesh_.vertex_buffers.resize(1);
  mesh_.vertex_buffer_byte_offsets.resize(1, 0);
  mesh_.index_buffer_byte_offset = 0;
  SPOKK_VK_CHECK(mesh_.vertex_buffers[0].Create(device_, vertex_buffer_ci));
  // Convert the vertex data from its original uncompressed format to its final format.
  // In a real application, this conversion would happen at asset build time.
  // clang-format off
  const VertexLayout src_vertex_layout = {
      {0, VK_FORMAT_R32G32B32_SFLOAT, 0},
      {1, VK_FORMAT_R32G32B32_SFLOAT, 12},
      {2, VK_FORMAT_R32G32_SFLOAT, 24},
  };
  // clang-format on
  const VertexLayout final_vertex_layout(mesh_.mesh_format, 0);
  std::vector<uint8_t> final_mesh_vertices(vertex_buffer_ci.size);
  int convert_error = ConvertVertexBuffer(
      cube_vertices, src_vertex_layout, final_mesh_vertices.data(), final_vertex_layout, cube_vertex_count);
  assert(convert_error == 0);
  (void)convert_error;
  SPOKK_VK_CHECK(mesh_.vertex_buffers[0].Load(device_, final_mesh_vertices.data(), vertex_buffer_ci.size));

  // Create graphics pipelines
  pillar_pipeline_.Init(&(mesh_.mesh_format), &pillar_shader_program_, &render_pass_, 0);
  SPOKK_VK_CHECK(pillar_pipeline_.Finalize(device_));

  // Create pipelined buffer of shader uniforms
  VkBufferCreateInfo uniform_buffer_ci = {};
  uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  uniform_buffer_ci.size = sizeof(CameraConstants);
  uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(camera_constants_.Create(device_, PFRAME_COUNT, uniform_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

  // Create buffer of per-cell "height" values
  VkBufferCreateInfo heightfield_buffer_ci = {};
  heightfield_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  heightfield_buffer_ci.size = HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY * sizeof(float);
  heightfield_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
  heightfield_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(
      heightfield_buffer_.Create(device_, PFRAME_COUNT, heightfield_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
  SPOKK_VK_CHECK(heightfield_buffer_.CreateViews(device_, VK_FORMAT_R32_SFLOAT));
  for (int32_t iY = 0; iY < HEIGHTFIELD_DIMY; ++iY) {
    for (int32_t iX = 0; iX < HEIGHTFIELD_DIMX; ++iX) {
      heightfield_.at(XY_TO_CELL(iX, iY)) = -1.0f;  // non-visible cells have negative heights
    }
  }

  // Create lookup table from instance index [0..visible_cell_count_] to cell index.
  visible_cells_.reserve(HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY);
  VkBufferCreateInfo visible_cells_buffer_ci = {};
  visible_cells_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  visible_cells_buffer_ci.size = HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY * sizeof(uint32_t);
  visible_cells_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  visible_cells_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(visible_cells_buffer_.Create(
      device_, PFRAME_COUNT, visible_cells_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
  SPOKK_VK_CHECK(visible_cells_buffer_.CreateViews(device_, VK_FORMAT_R32_SINT));

  for (const auto& dset_layout_ci : pillar_shader_program_.dset_layout_cis) {
    dpool_.Add(dset_layout_ci, PFRAME_COUNT);
  }
  SPOKK_VK_CHECK(dpool_.Finalize(device_));

  // Create swapchain-sized buffers
  CreateRenderBuffers(swapchain_extent_);

  DescriptorSetWriter dset_writer(pillar_shader_program_.dset_layout_cis[0]);
  dset_writer.BindImage(
      albedo_tex_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, pillar_fs_.GetDescriptorBindPoint("tex").binding);
  dset_writer.BindSampler(sampler_, pillar_fs_.GetDescriptorBindPoint("samp").binding);
  for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
    // TODO(cort): allocate_pipelined_set()?
    dsets_[pframe] = dpool_.AllocateSet(device_, pillar_shader_program_.dset_layouts[0]);
    dset_writer.BindBuffer(camera_constants_.Handle(pframe), pillar_vs_.GetDescriptorBindPoint("camera").binding);
    dset_writer.BindTexelBuffer(
        visible_cells_buffer_.View(pframe), pillar_vs_.GetDescriptorBindPoint("visible_cells").binding);
    dset_writer.BindTexelBuffer(
        heightfield_buffer_.View(pframe), pillar_vs_.GetDescriptorBindPoint("cell_heights").binding);
    dset_writer.WriteAll(device_, dsets_[pframe]);
  }
}

PillarsApp::~PillarsApp() {
  if (device_) {
    vkDeviceWaitIdle(device_);

    dpool_.Destroy(device_);

    camera_constants_.Destroy(device_);
    visible_cells_buffer_.Destroy(device_);
    heightfield_buffer_.Destroy(device_);

    mesh_.Destroy(device_);

    pillar_vs_.Destroy(device_);
    pillar_fs_.Destroy(device_);
    pillar_shader_program_.Destroy(device_);
    pillar_pipeline_.Destroy(device_);

    vkDestroySampler(device_, sampler_, host_allocator_);
    albedo_tex_.Destroy(device_);

    for (const auto fb : framebuffers_) {
      vkDestroyFramebuffer(device_, fb, host_allocator_);
    }
    render_pass_.Destroy(device_);

    depth_image_.Destroy(device_);
  }
}

void PillarsApp::Update(double dt) {
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

  // Update uniforms
  CameraConstants* camera_consts = (CameraConstants*)camera_constants_.Mapped(pframe_index_);
  camera_consts->time_and_res = { (float)seconds_elapsed_, (float)swapchain_extent_.width, (float)swapchain_extent_.height, 0 };
  camera_consts->eye_pos_ws = mathfu::vec4(camera_->getEyePoint(), 1.0f);
  mathfu::mat4 w2v = camera_->getViewMatrix();
  const mathfu::mat4 proj = camera_->getProjectionMatrix();
  camera_consts->viewproj = proj * w2v;
  camera_constants_.FlushPframeHostCache(pframe_index_);

  // Update visible cells
  // - Add a cell as visible the first time it gets within N units of the camera.
  // - Adjust a cell's height depending on its distance from the camera. There must
  //   be a barrier at least one cell thick that is close enough to be visible but
  //   too far away to be height-adjusted.
  float eye_x = camera_->getEyePoint().x;
  float eye_y = camera_->getEyePoint().z;
  int32_t cell_x = uint32_t(eye_x);
  int32_t cell_y = uint32_t(eye_y);
  int32_t min_x = my_max(0, cell_x - VISIBLE_RADIUS);
  int32_t max_x = my_min(HEIGHTFIELD_DIMX - 1, cell_x + VISIBLE_RADIUS);
  int32_t min_y = my_max(0, cell_y - VISIBLE_RADIUS);
  int32_t max_y = my_min(HEIGHTFIELD_DIMY - 1, cell_y + VISIBLE_RADIUS);
  for (int32_t iY = min_y; iY <= max_y; ++iY) {
    float fY = float(iY);
    for (int32_t iX = min_x; iX <= max_x; ++iX) {
      int32_t cell = XY_TO_CELL(iX, iY);
      if (heightfield_.at(cell) < 0) {
        // First time we're close enough to draw this cell; add it to the visible list
        visible_cells_.push_back(cell);
        heightfield_.at(cell) = 10.0f;
      }
      if (abs(iX - cell_x) <= EFFECT_RADIUS && abs(iY - cell_y) <= EFFECT_RADIUS) {
        float fX = float(iX);
        float dx = 1.0f * my_max(fabsf(fX - eye_x) - 3.0f, 0.0f);
        float dy = 1.0f * my_max(fabsf(fY - eye_y) - 3.0f, 0.0f);
        heightfield_.at(cell) = my_min(heightfield_.at(cell), 1.6f * sqrtf(dx * dx + dy * dy));
      }
    }
  }
  memcpy(visible_cells_buffer_.Mapped(pframe_index_), visible_cells_.data(), visible_cells_.size() * sizeof(int32_t));
  visible_cells_buffer_.FlushPframeHostCache(pframe_index_);
  memcpy(heightfield_buffer_.Mapped(pframe_index_), heightfield_.data(), heightfield_.size() * sizeof(float));
  heightfield_buffer_.FlushPframeHostCache(pframe_index_);
}

void PillarsApp::Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) {
  VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
  render_pass_.begin_info.framebuffer = framebuffer;
  render_pass_.begin_info.renderArea.extent = swapchain_extent_;
  vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pillar_pipeline_.handle);
  VkRect2D scissor_rect = render_pass_.begin_info.renderArea;
  VkViewport viewport = Rect2DToViewport(scissor_rect);
  vkCmdSetViewport(primary_cb, 0, 1, &viewport);
  vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);
  vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pillar_pipeline_.shader_program->pipeline_layout,
      0, 1, &dsets_[pframe_index_], 0, nullptr);
  mesh_.BindBuffers(primary_cb);
  vkCmdDrawIndexed(primary_cb, mesh_.index_count, (uint32_t)visible_cells_.size(), 0, 0, 0);
  vkCmdEndRenderPass(primary_cb);
}

void PillarsApp::HandleWindowResize(VkExtent2D new_window_extent) {
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

void PillarsApp::CreateRenderBuffers(VkExtent2D extent) {
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
    attachment_views.at(0) = swapchain_image_views_[i];
    SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &framebuffers_[i]));
  }
}

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
      {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}};
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;
  app_ci.pfn_set_device_features = EnableMinimumDeviceFeatures;

  PillarsApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
