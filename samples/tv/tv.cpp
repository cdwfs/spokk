#include <spokk.h>
using namespace spokk;

#include <common/camera.h>
#include <common/cube_mesh.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {
struct SceneUniforms {
  glm::vec4 res_and_time;  // xy: viewport resolution in pixels, z: unused, w: elapsed seconds
  glm::vec4 eye;  // xyz: eye position
  glm::mat4 viewproj;
};
struct TvUniforms {
  glm::vec4 film_params;  // x: noiseIntensity, y: scanlineIntensity, z: sCount, w: output_grayscale
  glm::vec4 snow_params;  // x: snowAmount, y: snowSize, zw: unused
  glm::vec4 rgb_shift_params;  // x: rgbShiftAmount, y: rgbShiftAngle, zw: unused
  glm::vec4 distort_params;  // x: distortionCoarse, y: distortionFine, z: distortionSpeed, w: rollSpeed
};
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

class TvApp : public spokk::Application {
public:
  explicit TvApp(Application::CreateInfo& ci);
  virtual ~TvApp();
  TvApp(const TvApp&) = delete;
  const TvApp& operator=(const TvApp&) = delete;

  void Update(double dt) override;
  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override;

protected:
  void HandleWindowResize(VkExtent2D new_window_extent) override;

private:
  void CreateRenderBuffers(VkExtent2D extent);

  double seconds_elapsed_;

  Image depth_image_;
  Image color_target_;
  const VkFormat color_target_format_ = VK_FORMAT_R16G16B16A16_SFLOAT;

  RenderPass scene_render_pass_;
  VkFramebuffer scene_framebuffer_;
  RenderPass post_render_pass_;
  std::vector<VkFramebuffer> post_framebuffers_;

  Image albedo_tex_;
  VkSampler sampler_;

  Shader pillar_vs_, pillar_fs_;
  ShaderProgram pillar_shader_program_;
  GraphicsPipeline pillar_pipeline_;

  Shader fullscreen_vs_;
  Shader film_fs_;
  ShaderProgram film_shader_program_;
  GraphicsPipeline tv_pipeline_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

  Mesh mesh_;
  PipelinedBuffer scene_uniforms_;
  PipelinedBuffer tv_uniforms_;
  PipelinedBuffer heightfield_buffer_;
  PipelinedBuffer visible_cells_buffer_;

  TvUniforms tv_params_;

  MeshFormat empty_mesh_format_;

  std::vector<int32_t> visible_cells_;
  std::array<float, HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY> heightfield_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDrone> drone_;
};

TvApp::TvApp(Application::CreateInfo& ci) : Application(ci) {
  seconds_elapsed_ = 0;

  camera_ = my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, FOV_DEGREES, Z_NEAR, Z_FAR);
  const glm::vec3 initial_camera_pos(HEIGHTFIELD_DIMX / 2, 2.0f, HEIGHTFIELD_DIMY / 2);
  const glm::vec3 initial_camera_target(0, 0, 0);
  const glm::vec3 initial_camera_up(0, 1, 0);
  camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
  drone_ = my_make_unique<CameraDrone>(*camera_);
  drone_->SetBounds(glm::vec3(VISIBLE_RADIUS, 1, VISIBLE_RADIUS),
      glm::vec3(HEIGHTFIELD_DIMX - VISIBLE_RADIUS - 1, 30, HEIGHTFIELD_DIMY - VISIBLE_RADIUS - 1));

  // Create render passes
  scene_render_pass_.InitFromPreset(RenderPass::Preset::COLOR_DEPTH_OFFSCREEN, color_target_format_);
  SPOKK_VK_CHECK(scene_render_pass_.Finalize(device_));
  SPOKK_VK_CHECK(device_.SetObjectName(scene_render_pass_.handle, "main offscreen color/depth pass"));
  scene_render_pass_.clear_values[0] = CreateColorClearValue(0.2f, 0.2f, 0.3f);
  scene_render_pass_.clear_values[1] = CreateDepthClearValue(1.0f, 0);
  post_render_pass_.InitFromPreset(RenderPass::Preset::POST, swapchain_surface_format_.format);
  SPOKK_VK_CHECK(post_render_pass_.Finalize(device_));
  SPOKK_VK_CHECK(device_.SetObjectName(post_render_pass_.handle, "post-processing pass"));

  // Load textures and samplers
  VkSamplerCreateInfo sampler_ci =
      GetSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
  SPOKK_VK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &sampler_));
  SPOKK_VK_CHECK(device_.SetObjectName(sampler_, "basic linear+repeat sampler"));
  albedo_tex_.CreateFromFile(device_, graphics_and_present_queue_, "data/redf.ktx", VK_FALSE,
      THSVS_ACCESS_FRAGMENT_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER);

  // Load shader pipelines
  SPOKK_VK_CHECK(pillar_vs_.CreateAndLoadSpirvFile(device_, "data/pillars/pillar.vert.spv"));
  SPOKK_VK_CHECK(pillar_fs_.CreateAndLoadSpirvFile(device_, "data/pillars/pillar.frag.spv"));
  SPOKK_VK_CHECK(pillar_shader_program_.AddShader(&pillar_vs_));
  SPOKK_VK_CHECK(pillar_shader_program_.AddShader(&pillar_fs_));

  SPOKK_VK_CHECK(fullscreen_vs_.CreateAndLoadSpirvFile(device_, "data/tv/fullscreen.vert.spv"));
  SPOKK_VK_CHECK(film_fs_.CreateAndLoadSpirvFile(device_, "data/tv/film.frag.spv"));
  SPOKK_VK_CHECK(film_shader_program_.AddShader(&fullscreen_vs_));
  SPOKK_VK_CHECK(film_shader_program_.AddShader(&film_fs_));

  SPOKK_VK_CHECK(ShaderProgram::ForceCompatibleLayoutsAndFinalize(device_,
      {
          &pillar_shader_program_,
          &film_shader_program_,
      }));

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

  // Populate Mesh object
  mesh_.index_type = (sizeof(cube_indices[0]) == sizeof(uint32_t)) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
  mesh_.index_count = cube_index_count;
  mesh_.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  // index buffer
  VkBufferCreateInfo index_buffer_ci = {};
  index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  index_buffer_ci.size = cube_index_count * sizeof(cube_indices[0]);
  index_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  index_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(mesh_.index_buffer.Create(device_, index_buffer_ci));
  SPOKK_VK_CHECK(device_.SetObjectName(mesh_.index_buffer.Handle(), "mesh index buffer"));
  SPOKK_VK_CHECK(mesh_.index_buffer.Load(
      device_, THSVS_ACCESS_NONE, THSVS_ACCESS_INDEX_BUFFER, cube_indices, index_buffer_ci.size));
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
  SPOKK_VK_CHECK(device_.SetObjectName(mesh_.vertex_buffers[0].Handle(), "mesh vertex buffer 0"));
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
  SPOKK_VK_CHECK(mesh_.vertex_buffers[0].Load(
      device_, THSVS_ACCESS_NONE, THSVS_ACCESS_VERTEX_BUFFER, final_mesh_vertices.data(), vertex_buffer_ci.size));

  // Create graphics pipelines
  pillar_pipeline_.Init(&(mesh_.mesh_format), &pillar_shader_program_, &scene_render_pass_, 0);
  SPOKK_VK_CHECK(pillar_pipeline_.Finalize(device_));
  SPOKK_VK_CHECK(device_.SetObjectName(pillar_pipeline_.handle, "pillar pipeline"));
  tv_pipeline_.Init(&empty_mesh_format_, &film_shader_program_, &post_render_pass_, 0);
  SPOKK_VK_CHECK(tv_pipeline_.Finalize(device_));
  SPOKK_VK_CHECK(device_.SetObjectName(tv_pipeline_.handle, "TV pipeline"));

  // Look up the appropriate memory flags for uniform buffers on this platform
  VkMemoryPropertyFlags uniform_buffer_memory_flags =
      device_.MemoryFlagsForAccessPattern(DEVICE_MEMORY_ACCESS_PATTERN_CPU_TO_GPU_DYNAMIC);

  // Create pipelined buffer of shader uniforms
  VkBufferCreateInfo scene_uniform_buffer_ci = {};
  scene_uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  scene_uniform_buffer_ci.size = sizeof(SceneUniforms);
  scene_uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  scene_uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(scene_uniforms_.Create(device_, PFRAME_COUNT, scene_uniform_buffer_ci, uniform_buffer_memory_flags));
  VkBufferCreateInfo tv_uniform_buffer_ci = {};
  tv_uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  tv_uniform_buffer_ci.size = sizeof(TvUniforms);
  tv_uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  tv_uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(tv_uniforms_.Create(device_, PFRAME_COUNT, tv_uniform_buffer_ci, uniform_buffer_memory_flags));

  // Default TV settings
  // clang-format off
  tv_params_.film_params = glm::vec4(
    0.4f,  // noise intensity
    0.9f,  // scanline intensity
    800.0f,  // scanline count,
    0.0f);  // convert to grayscale?
  tv_params_.snow_params = glm::vec4(
    0.1f,  // snow amount
    4.0f,  // snow size
    0,
    0);
  tv_params_.rgb_shift_params = glm::vec4(
    0.0067f,  // rgb shift amount
    (float)M_PI,  // rgb shift angle
    0,
    0);
  tv_params_.distort_params = glm::vec4(
    3.0f,  // distortionCoarse
    5.0f,  // distortionFine
    0.2f,  // distortionSpeed
    0.1f);  // rollSpeed
  // clang-format on

  // Create buffer of per-cell "height" values
  VkBufferCreateInfo heightfield_buffer_ci = {};
  heightfield_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  heightfield_buffer_ci.size = HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY * sizeof(float);
  heightfield_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
  heightfield_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(heightfield_buffer_.Create(device_, PFRAME_COUNT, heightfield_buffer_ci, uniform_buffer_memory_flags));
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
  SPOKK_VK_CHECK(
      visible_cells_buffer_.Create(device_, PFRAME_COUNT, visible_cells_buffer_ci, uniform_buffer_memory_flags));
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
  dset_writer.BindImage(
      color_target_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, film_fs_.GetDescriptorBindPoint("fbColor").binding);
  dset_writer.BindImage(
      depth_image_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, film_fs_.GetDescriptorBindPoint("fbDepth").binding);
  dset_writer.BindSampler(sampler_, film_fs_.GetDescriptorBindPoint("fbSamp").binding);
  for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
    // TODO(cort): allocate_pipelined_set()?
    dsets_[pframe] = dpool_.AllocateSet(device_, pillar_shader_program_.dset_layouts[0]);
    dset_writer.BindBuffer(scene_uniforms_.Handle(pframe), pillar_vs_.GetDescriptorBindPoint("scene_consts").binding);
    dset_writer.BindTexelBuffer(
        visible_cells_buffer_.View(pframe), pillar_vs_.GetDescriptorBindPoint("visible_cells").binding);
    dset_writer.BindTexelBuffer(
        heightfield_buffer_.View(pframe), pillar_vs_.GetDescriptorBindPoint("cell_heights").binding);
    dset_writer.BindBuffer(tv_uniforms_.Handle(pframe), film_fs_.GetDescriptorBindPoint("tv_consts").binding);

    dset_writer.WriteAll(device_, dsets_[pframe]);
  }
}

TvApp::~TvApp() {
  if (device_) {
    vkDeviceWaitIdle(device_);

    dpool_.Destroy(device_);

    scene_uniforms_.Destroy(device_);
    visible_cells_buffer_.Destroy(device_);
    heightfield_buffer_.Destroy(device_);
    tv_uniforms_.Destroy(device_);

    mesh_.Destroy(device_);

    pillar_vs_.Destroy(device_);
    pillar_fs_.Destroy(device_);
    pillar_shader_program_.Destroy(device_);
    pillar_pipeline_.Destroy(device_);

    fullscreen_vs_.Destroy(device_);
    film_fs_.Destroy(device_);
    film_shader_program_.Destroy(device_);
    tv_pipeline_.Destroy(device_);

    vkDestroySampler(device_, sampler_, host_allocator_);
    albedo_tex_.Destroy(device_);

    vkDestroyFramebuffer(device_, scene_framebuffer_, host_allocator_);
    scene_render_pass_.Destroy(device_);

    for (const auto fb : post_framebuffers_) {
      vkDestroyFramebuffer(device_, fb, host_allocator_);
    }
    post_render_pass_.Destroy(device_);

    depth_image_.Destroy(device_);
    color_target_.Destroy(device_);
  }
}

void TvApp::Update(double dt) {
  seconds_elapsed_ += dt;
  drone_->Update(input_state_, (float)dt);

  // Update visible cells
  // - Add a cell as visible the first time it gets within N units of the camera.
  // - Adjust a cell's height depending on its distance from the camera. There must
  //   be a barrier at least one cell thick that is close enough to be visible but
  //   too far away to be height-adjusted.
  float eye_x = camera_->getEyePoint().x;
  float eye_y = camera_->getEyePoint().z;
  int32_t cell_x = uint32_t(eye_x);
  int32_t cell_y = uint32_t(eye_y);
  int32_t min_x = std::max(0, cell_x - VISIBLE_RADIUS);
  int32_t max_x = std::min(HEIGHTFIELD_DIMX - 1, cell_x + VISIBLE_RADIUS);
  int32_t min_y = std::max(0, cell_y - VISIBLE_RADIUS);
  int32_t max_y = std::min(HEIGHTFIELD_DIMY - 1, cell_y + VISIBLE_RADIUS);
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
        float dx = 1.0f * std::max(fabsf(fX - eye_x) - 3.0f, 0.0f);
        float dy = 1.0f * std::max(fabsf(fY - eye_y) - 3.0f, 0.0f);
        heightfield_.at(cell) = std::min(heightfield_.at(cell), 1.6f * sqrtf(dx * dx + dy * dy));
      }
    }
  }
}

void TvApp::Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) {
  // Update uniforms
  SceneUniforms* scene_consts = (SceneUniforms*)scene_uniforms_.Mapped(pframe_index_);
  scene_consts->res_and_time =
    glm::vec4((float)swapchain_extent_.width, (float)swapchain_extent_.height, 0, (float)seconds_elapsed_);
  scene_consts->eye = glm::vec4(camera_->getEyePoint(), 1.0f);
  glm::mat4 w2v = camera_->getViewMatrix();
  const glm::mat4 proj = camera_->getProjectionMatrix();
  scene_consts->viewproj = proj * w2v;
  scene_uniforms_.FlushPframeHostCache(device_, pframe_index_);

  TvUniforms* tv_consts = (TvUniforms*)tv_uniforms_.Mapped(pframe_index_);
  *tv_consts = tv_params_;
  tv_uniforms_.FlushPframeHostCache(device_, pframe_index_);

  memcpy(visible_cells_buffer_.Mapped(pframe_index_), visible_cells_.data(), visible_cells_.size() * sizeof(int32_t));
  SPOKK_VK_CHECK(visible_cells_buffer_.FlushPframeHostCache(device_, pframe_index_));
  memcpy(heightfield_buffer_.Mapped(pframe_index_), heightfield_.data(), heightfield_.size() * sizeof(float));
  SPOKK_VK_CHECK(heightfield_buffer_.FlushPframeHostCache(device_, pframe_index_));

  // Write command buffer

  // offscreen pass
  {
    scene_render_pass_.begin_info.framebuffer = scene_framebuffer_;
    scene_render_pass_.begin_info.renderArea.extent = swapchain_extent_;
    vkCmdBeginRenderPass(primary_cb, &scene_render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pillar_pipeline_.handle);
    VkRect2D scissor_rect = scene_render_pass_.begin_info.renderArea;
    VkViewport viewport = Rect2DToViewport(scissor_rect);
    vkCmdSetViewport(primary_cb, 0, 1, &viewport);
    vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);
    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pillar_pipeline_.shader_program->pipeline_layout, 0, 1, &dsets_[pframe_index_], 0, nullptr);
    mesh_.BindBuffers(primary_cb);
    vkCmdDrawIndexed(primary_cb, mesh_.index_count, (uint32_t)visible_cells_.size(), 0, 0, 0);
    vkCmdEndRenderPass(primary_cb);
  }

  // post-processing pass
  {
    post_render_pass_.begin_info.framebuffer = post_framebuffers_[swapchain_image_index];
    post_render_pass_.begin_info.renderArea.extent = swapchain_extent_;
    vkCmdBeginRenderPass(primary_cb, &post_render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, tv_pipeline_.handle);
    VkRect2D scissor_rect = post_render_pass_.begin_info.renderArea;
    VkViewport viewport = Rect2DToViewport(scissor_rect);
    vkCmdSetViewport(primary_cb, 0, 1, &viewport);
    vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);
    vkCmdDraw(primary_cb, 3, 1, 0, 0);
    vkCmdEndRenderPass(primary_cb);
  }
}

void TvApp::HandleWindowResize(VkExtent2D new_window_extent) {
  // Destroy existing objects before re-creating them.
  if (scene_framebuffer_ == VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device_, scene_framebuffer_, host_allocator_);
  }
  for (auto fb : post_framebuffers_) {
    if (fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device_, fb, host_allocator_);
    }
  }
  post_framebuffers_.clear();
  depth_image_.Destroy(device_);
  color_target_.Destroy(device_);

  float aspect_ratio = (float)new_window_extent.width / (float)new_window_extent.height;
  camera_->setPerspective(FOV_DEGREES, aspect_ratio, Z_NEAR, Z_FAR);

  CreateRenderBuffers(new_window_extent);
}

void TvApp::CreateRenderBuffers(VkExtent2D extent) {
  // Create color targets
  VkImageCreateInfo color_target_image_ci = scene_render_pass_.GetAttachmentImageCreateInfo(0, extent);
  // TODO(https://github.com/cdwfs/spokk/issues/34): Combine usage from multiple render passes
  color_target_image_ci.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
  color_target_ = {};
  SPOKK_VK_CHECK(color_target_.Create(
      device_, color_target_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DEVICE_ALLOCATION_SCOPE_DEVICE));
  SPOKK_VK_CHECK(device_.SetObjectName(color_target_.handle, "color target image"));
  SPOKK_VK_CHECK(device_.SetObjectName(color_target_.view, "color target image view"));

  // Create depth buffer
  VkImageCreateInfo depth_image_ci = scene_render_pass_.GetAttachmentImageCreateInfo(1, extent);
  // TODO(https://github.com/cdwfs/spokk/issues/34): Combine usage from multiple render passes
  depth_image_ci.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
  depth_image_ = {};
  SPOKK_VK_CHECK(depth_image_.Create(
      device_, depth_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DEVICE_ALLOCATION_SCOPE_DEVICE));
  SPOKK_VK_CHECK(device_.SetObjectName(depth_image_.handle, "depth image"));
  SPOKK_VK_CHECK(device_.SetObjectName(depth_image_.view, "depth image view"));

  // Create VkFramebuffers
  std::vector<VkImageView> scene_attachment_views = {
      color_target_.view,
      depth_image_.view,
  };
  VkFramebufferCreateInfo scene_framebuffer_ci = scene_render_pass_.GetFramebufferCreateInfo(extent);
  scene_framebuffer_ci.pAttachments = scene_attachment_views.data();
  SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &scene_framebuffer_ci, host_allocator_, &scene_framebuffer_));
  SPOKK_VK_CHECK(device_.SetObjectName(scene_framebuffer_, std::string("scene framebuffer")));

  std::vector<VkImageView> post_attachment_views = {
      VK_NULL_HANDLE,  // filled in below
  };
  VkFramebufferCreateInfo post_framebuffer_ci = post_render_pass_.GetFramebufferCreateInfo(extent);
  post_framebuffer_ci.pAttachments = post_attachment_views.data();
  post_framebuffers_.resize(swapchain_image_views_.size());
  for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
    post_attachment_views.at(0) = swapchain_image_views_[i];
    SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &post_framebuffer_ci, host_allocator_, &post_framebuffers_[i]));
    SPOKK_VK_CHECK(device_.SetObjectName(post_framebuffers_[i],
        std::string("swapchain framebuffer ") + std::to_string(i)));  // TODO(cort): absl::StrCat
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

  TvApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
