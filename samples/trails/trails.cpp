/*
Each trail is a particle and a contrail.

A particle has a position, velocity.

Each frame, a new position is computed for each particle and appended to a buffer of float4s
This buffer is used as a texel buffer when rendering the particle,
and a vertex buffer when rendering contrails.

Contrails are drawn as lines. ring buffer.
First N frames, just increment the index count up to the vertex buffer size.
  p0            0
  p0 p1         0 1
  p0 p1 p2      0 1 2
  p0 p1 p2 p3   0 1 2 3
Thereafter, new positions overwrite old ones.
  p4 p1 p2 p3   1 2 3 0
Okay, so index buffer is repeated:
  0 1 2 3 0 1 2 3
And each frame uses an index offset to get the appropriate range
  0 1 2 3
    1 2 3 0
      2 3 0 1
        3 0 1 2
Final index is never used, but w/e.

-----------
SIMULATION

Assume 64-byte cache line, that's 4 positions per cache line.
Maybe the shader should just generate 4 positions per time step.
Better integration that way anyway.

64 threads will process 64 particles
input:
  64x float4 particle pos (256 words = 1024 bytes)
  4x loads:
  px00 px16 px32 px48   py00 py16 py32 py48   pz00 pz16 pz32 pz48   pw00 pw16 pw32 pw48

  px00 py00 pz00 pw00   px01 py01 pz01 pw01   px02 py02 pz02 pw02   px03 py03 pz03 pw03
  64x float4 particle velocity (256 words = 1024 bytes)
    vx00 vy00 vz00 vw00   vx01 vy01 vz01 vw01   vx02 vy02 vz02 vw02   vx03 vy03 vz03 vw03

output:
  16x float4 final particle pos
  16x float4 final particle velocity
  4x float4 incremental particle pos to trail buffer per particle

-----------
RENDERING

Each particle is identified by an zero-based index P
Each particle has a range of N (a power of 2) contrail positions in a contiguous chunk of texel buffer.
Each particle tracks its current contrail length L, in the range 0...N
Each particle tracks its current start index S within its buffer region.
To draw a contrail, prepare a VkDrawIndirectCommand cmd:
  cmd.vertexCount = L;
  cmd.instanceCount = 1;
  cmd.firstVertex = S // gl_BaseVertex
  cmd.firstInstance = either P or L; the other can be looked up through gl_DrawID // gl_BaseInstance
global uniforms:
  N
  matrices
per-particle uniforms (indexed by P):
  L
  AgeOffset; // 0..N
  TrailColor
in vertex shader:
  S = gl_BaseVertex;
  I = ((S + gl_VertexIndex) & N) + P*N
  pos = positions.Load(I);
  age = (gl_VertexIndex + AgeOffset) / L;
  opacity = Opacity(age);
  color = Color(TrailColor, age, whatever);

-------------------

*/

#include <spokk.h>
using namespace spokk;

#include <common/camera.h>

#include <array>
#include <cstdio>
#include <memory>

namespace {
struct SceneUniforms {
  glm::vec4 time_and_res;  // x: elapsed seconds, yz: viewport resolution in pixels
  glm::vec4 eye;  // xyz: eye position
  glm::mat4 viewproj;
  glm::ivec4 trail_params;  // x: MAX_PARTICLE_LENGTH, yzw: unused
};
constexpr float FOV_DEGREES = 45.0f;
constexpr float Z_NEAR = 0.01f;
constexpr float Z_FAR = 100.0f;

constexpr int32_t MAX_PARTICLE_COUNT = 256;
constexpr int32_t MAX_PARTICLE_LENGTH = 64;
}  // namespace

class TrailsApp : public spokk::Application {
public:
  explicit TrailsApp(Application::CreateInfo& ci);
  virtual ~TrailsApp();

  TrailsApp(const TrailsApp&) = delete;
  const TrailsApp& operator=(const TrailsApp&) = delete;

  virtual void Update(double dt) override;
  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override;

protected:
  void HandleWindowResize(VkExtent2D new_window_extent) override;

private:
  void CreateRenderBuffers(VkExtent2D extent);

  double seconds_elapsed_;

  Image msaa_color_image_;
  Image depth_image_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  MeshFormat trail_mesh_format_;
  MeshFormat particle_mesh_format_;

  Shader trail_vs_, trail_fs_;
  ShaderProgram trail_shader_program_;
  GraphicsPipeline trail_pipeline_;

  Shader particle_vs_, particle_fs_;
  ShaderProgram particle_shader_program_;
  GraphicsPipeline particle_pipeline_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

  std::vector<glm::vec3> host_particle_positions_;
  std::vector<glm::vec3> host_particle_velocities_;
  std::vector<int32_t> host_trail_ends_;
  std::vector<int32_t> host_trail_lengths_;

  PipelinedBuffer scene_uniforms_;

  PipelinedBuffer particle_vb_;
  PipelinedBuffer trail_lengths_;
  PipelinedBuffer trail_age_offsets_;
  PipelinedBuffer trail_positions_;

  PipelinedBuffer indirect_draw_commands_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDrone> drone_;
};

TrailsApp::TrailsApp(Application::CreateInfo& ci) : Application(ci) {
  seconds_elapsed_ = 0;

  camera_ = my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, FOV_DEGREES, Z_NEAR, Z_FAR);
  const glm::vec3 initial_camera_pos(-1, 0, 6);
  const glm::vec3 initial_camera_target(0, 0, 0);
  const glm::vec3 initial_camera_up(0, 1, 0);
  camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
  drone_ = my_make_unique<CameraDrone>(*camera_);

  // Create render pass
  // render_pass_.InitFromPreset(RenderPass::Preset::COLOR_DEPTH, swapchain_surface_format_.format);
  // clang-format off
  render_pass_.attachment_descs = {
    {0, swapchain_surface_format_.format, VK_SAMPLE_COUNT_8_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    {0, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_8_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },
    {0, swapchain_surface_format_.format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR },
  };
  render_pass_.subpass_attachments.resize(1);
  render_pass_.subpass_attachments[0].color_refs = { {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL} };
  render_pass_.subpass_attachments[0].depth_stencil_refs = { { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL } };
  render_pass_.subpass_attachments[0].resolve_refs = { { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } };
  // clang-format on
  render_pass_.Finalize(device_);
  render_pass_.clear_values[0] = CreateColorClearValue(0.05f, 0.05f, 0.05f);
  render_pass_.clear_values[1] = CreateDepthClearValue(1.0f, 0);

  // Build shader programs
  SPOKK_VK_CHECK(particle_vs_.CreateAndLoadSpirvFile(device_, "data/trails/particle.vert.spv"));
  SPOKK_VK_CHECK(particle_fs_.CreateAndLoadSpirvFile(device_, "data/trails/particle.frag.spv"));
  SPOKK_VK_CHECK(particle_shader_program_.AddShader(&particle_vs_));
  SPOKK_VK_CHECK(particle_shader_program_.AddShader(&particle_fs_));
  SPOKK_VK_CHECK(trail_vs_.CreateAndLoadSpirvFile(device_, "data/trails/trail.vert.spv"));
  SPOKK_VK_CHECK(trail_fs_.CreateAndLoadSpirvFile(device_, "data/trails/trail.frag.spv"));
  SPOKK_VK_CHECK(trail_shader_program_.AddShader(&trail_vs_));
  SPOKK_VK_CHECK(trail_shader_program_.AddShader(&trail_fs_));
  SPOKK_VK_CHECK(
      ShaderProgram::ForceCompatibleLayoutsAndFinalize(device_, {&particle_shader_program_, &trail_shader_program_}));

  // Look up the appropriate memory flags for cpu/gpu dynamic buffers on this platform
  VkMemoryPropertyFlags cpu_to_gpu_dynamic_memflags =
      device_.MemoryFlagsForAccessPattern(DEVICE_MEMORY_ACCESS_PATTERN_CPU_TO_GPU_DYNAMIC);

  // Create pipelined buffer of scene uniforms
  VkBufferCreateInfo scene_uniforms_ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  scene_uniforms_ci.size = sizeof(SceneUniforms);
  scene_uniforms_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  SPOKK_VK_CHECK(scene_uniforms_.Create(device_, PFRAME_COUNT, scene_uniforms_ci, cpu_to_gpu_dynamic_memflags));

  // Particle attributes, used for host-side simulation.
  host_particle_positions_.resize(MAX_PARTICLE_COUNT, glm::vec3(0, 0, 0));
  host_particle_velocities_.resize(MAX_PARTICLE_COUNT, glm::vec3(0, 0, 0));
  host_trail_ends_.resize(MAX_PARTICLE_COUNT, 0);
  host_trail_lengths_.resize(MAX_PARTICLE_COUNT, 0);
  // TODO: better initialization. For now, just assign random position/velocities
  for (int32_t i_part = 0; i_part < MAX_PARTICLE_COUNT; ++i_part) {
    host_particle_positions_[i_part] = glm::sphericalRand(3.0f);  // glm::ballRand(3.0f);
    host_particle_velocities_[i_part] = glm::sphericalRand(0.01f);
    host_trail_lengths_[i_part] = 0;
  }

  // Manually create mesh format for particle vertex buffer
  particle_mesh_format_.vertex_buffer_bindings = {
      {0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX},
  };
  particle_mesh_format_.vertex_attributes = {
      {SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
  };
  particle_mesh_format_.Finalize(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
  // Create pipelined particle vertex buffer.
  VkBufferCreateInfo particle_vb_ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  particle_vb_ci.size = MAX_PARTICLE_COUNT * particle_mesh_format_.vertex_buffer_bindings[0].stride;
  particle_vb_ci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  SPOKK_VK_CHECK(particle_vb_.Create(device_, PFRAME_COUNT, particle_vb_ci, cpu_to_gpu_dynamic_memflags));
  SPOKK_VK_CHECK(particle_vb_.CreateViews(device_, particle_mesh_format_.vertex_attributes[0].format));

  // Create pipelined trail lengths buffer
  VkBufferCreateInfo trail_lengths_buffer_ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  trail_lengths_buffer_ci.size = MAX_PARTICLE_COUNT * sizeof(int32_t);
  trail_lengths_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
  SPOKK_VK_CHECK(trail_lengths_.Create(device_, PFRAME_COUNT, trail_lengths_buffer_ci, cpu_to_gpu_dynamic_memflags));
  SPOKK_VK_CHECK(trail_lengths_.CreateViews(device_, VK_FORMAT_R32_SINT));

  // Create pipelined trail age offsets buffer
  VkBufferCreateInfo trail_age_offsets_buffer_ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  trail_age_offsets_buffer_ci.size = MAX_PARTICLE_COUNT * sizeof(int32_t);
  trail_age_offsets_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
  SPOKK_VK_CHECK(
      trail_age_offsets_.Create(device_, PFRAME_COUNT, trail_age_offsets_buffer_ci, cpu_to_gpu_dynamic_memflags));
  SPOKK_VK_CHECK(trail_age_offsets_.CreateViews(device_, VK_FORMAT_R32_SINT));

  // Create pipelined trail positions buffer.
  VkBufferCreateInfo trail_positions_buffer_ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  trail_positions_buffer_ci.size = MAX_PARTICLE_COUNT * MAX_PARTICLE_LENGTH * sizeof(glm::vec4);
  trail_positions_buffer_ci.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
  SPOKK_VK_CHECK(
      trail_positions_.Create(device_, PFRAME_COUNT, trail_positions_buffer_ci, cpu_to_gpu_dynamic_memflags));
  SPOKK_VK_CHECK(trail_positions_.CreateViews(device_, VK_FORMAT_R32G32B32A32_SFLOAT));

  // Create pipelined buffer of VkDrawIndrectCommand
  VkBufferCreateInfo indirect_draw_buffer_ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  indirect_draw_buffer_ci.size = MAX_PARTICLE_COUNT * sizeof(VkDrawIndirectCommand);
  indirect_draw_buffer_ci.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  SPOKK_VK_CHECK(
      indirect_draw_commands_.Create(device_, PFRAME_COUNT, indirect_draw_buffer_ci, cpu_to_gpu_dynamic_memflags));

  // We need empty mesh format for the trail pipeline
  trail_mesh_format_.Finalize(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP);

  // Create graphics pipelines
  // particle_pipeline_.Init(&particle_mesh_format_, &particle_shader_program_, &render_pass_, 0);
  // SPOKK_VK_CHECK(particle_pipeline_.Finalize(device_));
  trail_pipeline_.Init(&trail_mesh_format_, &trail_shader_program_, &render_pass_, 0);
  // trail_pipeline_.rasterization_state_ci.lineWidth = 5.0f;
  trail_pipeline_.color_blend_attachment_states[0].blendEnable = VK_TRUE;
  trail_pipeline_.color_blend_attachment_states[0].colorBlendOp = VK_BLEND_OP_ADD;
  trail_pipeline_.color_blend_attachment_states[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  trail_pipeline_.color_blend_attachment_states[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  SPOKK_VK_CHECK(trail_pipeline_.Finalize(device_));

  // Create and populate descriptor sets.
  // All pipelines in this app share a common dset layout, so we only need to add
  // layouts from one shader program.
  for (const auto& dset_layout_ci : trail_shader_program_.dset_layout_cis) {
    dpool_.Add(dset_layout_ci, PFRAME_COUNT);
  }
  SPOKK_VK_CHECK(dpool_.Finalize(device_));
  for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
    // TODO(cort): allocate_pipelined_set()?
    dsets_[pframe] = dpool_.AllocateSet(device_, trail_shader_program_.dset_layouts[0]);
  }
  DescriptorSetWriter dset_writer(trail_shader_program_.dset_layout_cis[0]);
  for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
    dset_writer.BindBuffer(scene_uniforms_.Handle(pframe), trail_vs_.GetDescriptorBindPoint("scene_consts").binding);
    dset_writer.BindTexelBuffer(trail_lengths_.View(pframe), trail_vs_.GetDescriptorBindPoint("trail_lengths").binding);
    dset_writer.BindTexelBuffer(
        trail_age_offsets_.View(pframe), trail_vs_.GetDescriptorBindPoint("trail_age_offsets").binding);
    dset_writer.BindTexelBuffer(
        trail_positions_.View(pframe), trail_vs_.GetDescriptorBindPoint("trail_positions").binding);
    dset_writer.WriteAll(device_, dsets_[pframe]);
  }

  // Create swapchain-sized buffers
  CreateRenderBuffers(swapchain_extent_);
}
TrailsApp::~TrailsApp() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);

    dpool_.Destroy(device_);

    scene_uniforms_.Destroy(device_);

    particle_vb_.Destroy(device_);
    trail_lengths_.Destroy(device_);
    trail_age_offsets_.Destroy(device_);
    trail_positions_.Destroy(device_);

    indirect_draw_commands_.Destroy(device_);

    particle_vs_.Destroy(device_);
    particle_fs_.Destroy(device_);
    particle_shader_program_.Destroy(device_);
    particle_pipeline_.Destroy(device_);
    trail_vs_.Destroy(device_);
    trail_fs_.Destroy(device_);
    trail_shader_program_.Destroy(device_);
    trail_pipeline_.Destroy(device_);

    for (const auto fb : framebuffers_) {
      vkDestroyFramebuffer(device_, fb, host_allocator_);
    }
    render_pass_.Destroy(device_);

    msaa_color_image_.Destroy(device_);
    depth_image_.Destroy(device_);
  }
}

void TrailsApp::Update(double dt) {
  seconds_elapsed_ += dt;
  drone_->Update(input_state_, (float)dt);

  // TODO: simulate particles. Need an acceleration term, probably
  // from a vector field.
  for (int32_t i_part = 0; i_part < MAX_PARTICLE_COUNT; ++i_part) {
    // host_particle_positions_[i_part] += (float)dt * host_particle_velocities_[i_part];
    host_particle_positions_[i_part] =
        glm::vec3(0.1f * float(i_part) * sinf((float)seconds_elapsed_ + 0.1f * (float)i_part),
            2.0f * cosf((float)seconds_elapsed_ + 0.1f * (float)i_part), 0);
    if (host_trail_lengths_[i_part] >= 0 && host_trail_lengths_[i_part] < MAX_PARTICLE_LENGTH) {
      host_trail_lengths_[i_part] += 1;
    }
  }
}

void TrailsApp::Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) {
  // Update uniforms
  SceneUniforms* uniforms = (SceneUniforms*)scene_uniforms_.Mapped(pframe_index_);
  uniforms->time_and_res =
      glm::vec4((float)seconds_elapsed_, (float)swapchain_extent_.width, (float)swapchain_extent_.height, 0);
  uniforms->eye = glm::vec4(camera_->getEyePoint(), 1.0f);
  glm::mat4 w2v = camera_->getViewMatrix();
  const glm::mat4 proj = camera_->getProjectionMatrix();
  uniforms->viewproj = proj * w2v;
  uniforms->trail_params.x = MAX_PARTICLE_LENGTH;
  SPOKK_VK_CHECK(scene_uniforms_.FlushPframeHostCache(device_, pframe_index_));

  // Update particle attribute buffers
  glm::vec3* dst_particle_positions = (glm::vec3*)particle_vb_.Mapped(pframe_index_);
  memcpy(dst_particle_positions, host_particle_positions_.data(), particle_vb_.BytesPerPframe());
  SPOKK_VK_CHECK(particle_vb_.FlushPframeHostCache(device_, pframe_index_));

  glm::vec4* src_trail_positions = (glm::vec4*)trail_positions_.Mapped(1 - pframe_index_);
  glm::vec4* dst_trail_positions = (glm::vec4*)trail_positions_.Mapped(pframe_index_);
  memcpy(dst_trail_positions, src_trail_positions, trail_positions_.BytesPerPframe());  // waaaaaste
  for (int32_t i_part = 0; i_part < MAX_PARTICLE_COUNT; ++i_part) {
    glm::vec4 out_pos(host_particle_positions_[i_part], 1.0f);
    dst_trail_positions[i_part * MAX_PARTICLE_LENGTH + (host_trail_ends_[i_part] % MAX_PARTICLE_LENGTH)] = out_pos;
    host_trail_ends_[i_part] += 1;
  }
  SPOKK_VK_CHECK(trail_positions_.FlushPframeHostCache(device_, pframe_index_));

  float* dst_trail_lengths = (float*)trail_lengths_.Mapped(pframe_index_);
  memcpy(dst_trail_lengths, host_trail_lengths_.data(), trail_lengths_.BytesPerPframe());
  SPOKK_VK_CHECK(trail_lengths_.FlushPframeHostCache(device_, pframe_index_));

  // Update indirect draw commands
  uint32_t draw_count = 0;
  VkDrawIndirectCommand* draw_cmds = (VkDrawIndirectCommand*)indirect_draw_commands_.Mapped(pframe_index_);
  for (int32_t i_part = 0; i_part < MAX_PARTICLE_COUNT; ++i_part) {
    int32_t trail_start = host_trail_ends_[i_part] - host_trail_lengths_[i_part];
    draw_cmds[draw_count].vertexCount = host_trail_lengths_[i_part];
    draw_cmds[draw_count].instanceCount = 1;
    draw_cmds[draw_count].firstVertex = trail_start >= 0 ? (trail_start % MAX_PARTICLE_LENGTH) : 0;  // gl_BaseVertex
    draw_cmds[draw_count].firstInstance = i_part;  // gl_BaseInstance
    ++draw_count;
  }
  SPOKK_VK_CHECK(indirect_draw_commands_.FlushPframeHostCache(device_, pframe_index_));

  // Write command buffer
  VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
  render_pass_.begin_info.framebuffer = framebuffer;
  render_pass_.begin_info.renderArea.extent = swapchain_extent_;
  vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
  VkRect2D scissor_rect = render_pass_.begin_info.renderArea;
  VkViewport viewport = Rect2DToViewport(scissor_rect);
  vkCmdSetViewport(primary_cb, 0, 1, &viewport);
  vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);
  vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, trail_pipeline_.handle);
  vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, trail_pipeline_.shader_program->pipeline_layout,
      0, 1, &dsets_[pframe_index_], 0, nullptr);
  vkCmdDrawIndirect(
      primary_cb, indirect_draw_commands_.Handle(pframe_index_), 0, draw_count, sizeof(VkDrawIndirectCommand));
  vkCmdEndRenderPass(primary_cb);
}

void TrailsApp::HandleWindowResize(VkExtent2D new_window_extent) {
  Application::HandleWindowResize(new_window_extent);

  // Destroy existing objects before re-creating them.
  for (auto fb : framebuffers_) {
    if (fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device_, fb, host_allocator_);
    }
  }
  framebuffers_.clear();
  msaa_color_image_.Destroy(device_);
  depth_image_.Destroy(device_);

  float aspect_ratio = (float)new_window_extent.width / (float)new_window_extent.height;
  camera_->setPerspective(FOV_DEGREES, aspect_ratio, Z_NEAR, Z_FAR);

  CreateRenderBuffers(new_window_extent);
}

void TrailsApp::CreateRenderBuffers(VkExtent2D extent) {
  // Create MSAA color buffer
  VkImageCreateInfo msaa_color_image_ci = render_pass_.GetAttachmentImageCreateInfo(0, extent);
  msaa_color_image_ = {};
  SPOKK_VK_CHECK(msaa_color_image_.Create(
      device_, msaa_color_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DEVICE_ALLOCATION_SCOPE_DEVICE));

  // Create depth buffer
  VkImageCreateInfo depth_image_ci = render_pass_.GetAttachmentImageCreateInfo(1, extent);
  depth_image_ = {};
  SPOKK_VK_CHECK(depth_image_.Create(
      device_, depth_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DEVICE_ALLOCATION_SCOPE_DEVICE));

  // Create VkFramebuffers
  std::vector<VkImageView> attachment_views = {
      msaa_color_image_.view, depth_image_.view,
      VK_NULL_HANDLE,  // filled in below
  };
  VkFramebufferCreateInfo framebuffer_ci = render_pass_.GetFramebufferCreateInfo(extent);
  framebuffer_ci.pAttachments = attachment_views.data();
  framebuffers_.resize(swapchain_image_views_.size());
  for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
    attachment_views[2] = swapchain_image_views_[i];
    SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &framebuffers_[i]));
  }
}

static VkBool32 EnableAppDeviceFeatures(
    const VkPhysicalDeviceFeatures& supported_features, VkPhysicalDeviceFeatures* enabled_features) {
  // multiDrawIndirect is required
  if (!supported_features.multiDrawIndirect) {
    return false;
  }
  enabled_features->multiDrawIndirect = VK_TRUE;
  // largePoints is required
  if (!supported_features.largePoints) {
    return false;
  }
  enabled_features->largePoints = VK_TRUE;
  // drawIndirectFirstInstance is required
  if (!supported_features.drawIndirectFirstInstance) {
    return false;
  }
  enabled_features->drawIndirectFirstInstance = VK_TRUE;

  return EnableMinimumDeviceFeatures(supported_features, enabled_features);
}

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
      {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}};
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;
  app_ci.pfn_set_device_features = EnableAppDeviceFeatures;
  app_ci.required_device_extension_names = {VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME};

  TrailsApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
