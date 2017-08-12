#include <spokk.h>
using namespace spokk;

#include <common/camera.h>
#include <common/cube_mesh.h>

#include <mathfu/glsl_mappings.h>
#include <mathfu/vector.h>

#include <array>
#include <cstdio>
#include <cstring>

namespace {
constexpr float FOV_DEGREES = 45.0f;
constexpr float Z_NEAR = 0.01f;
constexpr float Z_FAR = 100.0f;

const std::string string_text("Watson, come here. I need you.");

struct GlyphVertex {
#if 0
  int16_t pos_x0, pos_y0;
  uint16_t tex_x0, tex_y0;
#else
  float pos_x0, pos_y0;
  float tex_x0, tex_y0;
#endif
};

template <typename T>
T my_clamp(T x, T xmin, T xmax) {
  return (x < xmin) ? xmin : ((x > xmax) ? xmax : x);
}
uint16_t F32toU16N(float f) { return (uint16_t)(my_clamp(f, 0.0f, 1.0f) * (float)UINT16_MAX + 0.5f); }
int16_t F32toS16(float f) { return (int16_t)my_clamp(f, (float)INT16_MIN, (float)INT16_MAX); }

struct SceneUniforms {
  mathfu::vec4_packed time_and_res;  // x: elapsed seconds, yz: viewport resolution in pixels
  mathfu::vec4_packed eye;  // xyz: eye position
  mathfu::mat4 viewproj;
};

struct StringUniforms {
  mathfu::mat4 o2w;
};

}  // namespace

class TextApp : public spokk::Application {
public:
  explicit TextApp(Application::CreateInfo &ci);
  virtual ~TextApp();
  TextApp(const TextApp &) = delete;
  const TextApp &operator=(const TextApp &) = delete;

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

  Font font_;
  FontAtlas font_atlas_;
  Image font_atlas_image_;
  VkSampler sampler_;

  Shader textmesh_vs_, textmesh_fs_;
  ShaderProgram textmesh_shader_program_;
  GraphicsPipeline textmesh_pipeline_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

  PipelinedBuffer scene_uniforms_;
  PipelinedBuffer string_uniforms_;
  Buffer string_vb_;
  uint32_t string_quad_count_;
  MeshFormat string_mesh_format_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDolly> dolly_;
};

TextApp::TextApp(Application::CreateInfo &ci) : Application(ci) {
  glfwSetInputMode(window_.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  seconds_elapsed_ = 0;

  camera_ = my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, FOV_DEGREES, Z_NEAR, Z_FAR);
  const mathfu::vec3 initial_camera_pos(0, 0, 10);
  const mathfu::vec3 initial_camera_target(0, 0, 0);
  const mathfu::vec3 initial_camera_up(0, 1, 0);
  camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
  dolly_ = my_make_unique<CameraDolly>(*camera_);

  // Create render pass
  render_pass_.InitFromPreset(RenderPass::Preset::COLOR_DEPTH, swapchain_surface_format_.format);
  SPOKK_VK_CHECK(render_pass_.Finalize(device_context_));
  render_pass_.clear_values[0] = CreateColorClearValue(0.2f, 0.2f, 0.3f);
  render_pass_.clear_values[1] = CreateDepthClearValue(1.0f, 0);

  // Create sampler
  VkSamplerCreateInfo sampler_ci =
      GetSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
  SPOKK_VK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &sampler_));

  // Load font
  int font_create_error = font_.Create("data/SourceCodePro-Semibold.ttf");
  ZOMBO_ASSERT(font_create_error == 0, "Font loading error: %d", font_create_error);
  // Test CPU string rastering
  Font::StringRenderInfo string_info = {};
  string_info.font_size = 32;
  string_info.x_start = 64;
  string_info.x_min = 128;
  string_info.x_max = 512;
  string_info.str =
      "This is a long string, just to test out string rendering. It includes one "
      "longer-than-average-string-to-stress-test the wrapping code.";
  uint32_t bmp_w = 0, bmp_h = 0;
  font_.ComputeStringBitmapDimensions(string_info, &bmp_w, &bmp_h);
  std::vector<uint8_t> bmp(bmp_w * bmp_h);
  bmp.assign(bmp.size(), 0);
  int bake_err = font_.RenderStringToBitmap(string_info, bmp_w, bmp_h, bmp.data());
  ZOMBO_ASSERT(bake_err == 0, "bake error: %d", bake_err);

  // Create font atlas
  FontAtlasCreateInfo atlas_ci = {};
  atlas_ci.font = &font_;
  atlas_ci.font_size = 16.0f;
  atlas_ci.image_oversample_x = atlas_ci.image_oversample_y = 2;
  atlas_ci.image_width = 512;
  atlas_ci.image_height = 512;
  atlas_ci.codepoint_first = 0;
  atlas_ci.codepoint_count = 256;
  std::vector<uint8_t> atlas_image_pixels(atlas_ci.image_width * atlas_ci.image_height);
  atlas_image_pixels.assign(atlas_image_pixels.size(), 0);
  int atlas_create_err = font_atlas_.Create(atlas_ci, atlas_image_pixels.data());
  ZOMBO_ASSERT(atlas_create_err == 0, "Font atlas creation error: %d", atlas_create_err);
  // Load atlas image
  VkImageCreateInfo atlas_image_ci = {};
  atlas_image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  atlas_image_ci.imageType = VK_IMAGE_TYPE_2D;
  atlas_image_ci.format = VK_FORMAT_R8_UNORM;
  atlas_image_ci.extent.width = atlas_ci.image_width;
  atlas_image_ci.extent.height = atlas_ci.image_height;
  atlas_image_ci.extent.depth = 1;
  atlas_image_ci.mipLevels = GetMaxMipLevels(atlas_image_ci.extent);
  atlas_image_ci.arrayLayers = 1;
  atlas_image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
  atlas_image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
  atlas_image_ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  atlas_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  atlas_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  SPOKK_VK_CHECK(font_atlas_image_.Create(device_context_, atlas_image_ci));
  VkImageSubresource dst_subresource = {};
  dst_subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  dst_subresource.mipLevel = 0;
  dst_subresource.arrayLayer = 0;
  int atlas_load_err = font_atlas_image_.LoadSubresourceFromMemory(device_context_, graphics_and_present_queue_,
      atlas_image_pixels.data(), atlas_image_pixels.size(), atlas_ci.image_width, atlas_ci.image_height,
      dst_subresource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
  ZOMBO_ASSERT(atlas_load_err == 0, "error (%d) while loading font atlas into memory", atlas_load_err);
  VkImageMemoryBarrier mipmap_barrier = {};
  mipmap_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  mipmap_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // matches final access pass to LoadSubresourceFromMemory
  mipmap_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  mipmap_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // matches final layout to LoadSubresourceFromMemory
  mipmap_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  mipmap_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  mipmap_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  mipmap_barrier.image = font_atlas_image_.handle;
  mipmap_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  mipmap_barrier.subresourceRange.baseMipLevel = 0;
  mipmap_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  mipmap_barrier.subresourceRange.baseArrayLayer = 0;
  mipmap_barrier.subresourceRange.layerCount = 1;
  int mipmap_gen_err =
      font_atlas_image_.GenerateMipmaps(device_context_, graphics_and_present_queue_, mipmap_barrier, 0, 0);
  ZOMBO_ASSERT(mipmap_gen_err == 0, "error (%d) while generating atlas mipmaps", mipmap_gen_err);

  // Generate quads for a string.
  std::vector<FontAtlas::Quad> quads(string_text.size());
  string_quad_count_ = 0;
  font_atlas_.GetStringQuads(string_text.c_str(), string_text.size(), quads.data(), &string_quad_count_);
  // Create vertex buffer
  VkBufferCreateInfo vb_ci = {};
  vb_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vb_ci.size = sizeof(GlyphVertex) * 6 * string_quad_count_;
  vb_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  vb_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(string_vb_.Create(device_context_, vb_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
  // Convert raw quads into a compressed vertex buffer
  GlyphVertex *verts = (GlyphVertex *)string_vb_.Mapped();
  for (uint32_t i = 0; i < string_quad_count_; ++i) {
    const auto &q = quads[i];
#if 0
    verts[6*i+0] = { F32toS16(q.x0), F32toS16(q.y0), F32toU16N(q.s0), F32toU16N(q.t0) };
    verts[6*i+1] = { F32toS16(q.x0), F32toS16(q.y1), F32toU16N(q.s0), F32toU16N(q.t1) };
    verts[6*i+2] = { F32toS16(q.x1), F32toS16(q.y0), F32toU16N(q.s1), F32toU16N(q.t0) };
    verts[6*i+3] = { F32toS16(q.x1), F32toS16(q.y0), F32toU16N(q.s1), F32toU16N(q.t0) };
    verts[6*i+4] = { F32toS16(q.x0), F32toS16(q.y1), F32toU16N(q.s0), F32toU16N(q.t1) };
    verts[6*i+5] = { F32toS16(q.x1), F32toS16(q.y1), F32toU16N(q.s1), F32toU16N(q.t1) };
#else
    verts[6 * i + 0] = {q.x0, q.y0, q.s0, q.t0};
    verts[6 * i + 1] = {q.x0, q.y1, q.s0, q.t1};
    verts[6 * i + 2] = {q.x1, q.y0, q.s1, q.t0};
    verts[6 * i + 3] = {q.x1, q.y0, q.s1, q.t0};
    verts[6 * i + 4] = {q.x0, q.y1, q.s0, q.t1};
    verts[6 * i + 5] = {q.x1, q.y1, q.s1, q.t1};
#endif
  }
  string_vb_.FlushHostCache();
  // Mesh format
  string_mesh_format_.vertex_buffer_bindings = {
      {0, sizeof(GlyphVertex), VK_VERTEX_INPUT_RATE_VERTEX},
  };
  string_mesh_format_.vertex_attributes = {
#if 0
    {0, 0, VK_FORMAT_R16G16_SINT, 0},
    {1, 0, VK_FORMAT_R16G16_UNORM, 4},
#else
    {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
    {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},
#endif
  };
  string_mesh_format_.Finalize(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  // Load shader pipelines
  SPOKK_VK_CHECK(textmesh_vs_.CreateAndLoadSpirvFile(device_context_, "data/textmesh.vert.spv"));
  SPOKK_VK_CHECK(textmesh_fs_.CreateAndLoadSpirvFile(device_context_, "data/textmesh.frag.spv"));
  SPOKK_VK_CHECK(textmesh_shader_program_.AddShader(&textmesh_vs_));
  SPOKK_VK_CHECK(textmesh_shader_program_.AddShader(&textmesh_fs_));
  SPOKK_VK_CHECK(textmesh_shader_program_.Finalize(device_context_));

  // Create graphics pipelines
  textmesh_pipeline_.Init(&string_mesh_format_, &textmesh_shader_program_, &render_pass_, 0);
  textmesh_pipeline_.rasterization_state_ci.cullMode = VK_CULL_MODE_NONE;
  textmesh_pipeline_.depth_stencil_state_ci.depthTestEnable = VK_FALSE;
  textmesh_pipeline_.color_blend_attachment_states[0].blendEnable = VK_TRUE;
  textmesh_pipeline_.color_blend_attachment_states[0].colorBlendOp = VK_BLEND_OP_ADD;
  textmesh_pipeline_.color_blend_attachment_states[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  textmesh_pipeline_.color_blend_attachment_states[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  // pipeline_.color_blend_state_ci.blendConstants
  SPOKK_VK_CHECK(textmesh_pipeline_.Finalize(device_context_));

  // Create pipelined buffer of shader uniforms
  VkBufferCreateInfo uniform_buffer_ci = {};
  uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  uniform_buffer_ci.size = sizeof(SceneUniforms);
  uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(
      scene_uniforms_.Create(device_context_, PFRAME_COUNT, uniform_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
  // ...and for strings
  uniform_buffer_ci.size = sizeof(StringUniforms);
  SPOKK_VK_CHECK(
      string_uniforms_.Create(device_context_, PFRAME_COUNT, uniform_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

  // Descriptor sets
  for (const auto &dset_layout_ci : textmesh_shader_program_.dset_layout_cis) {
    dpool_.Add(dset_layout_ci, PFRAME_COUNT);
  }
  SPOKK_VK_CHECK(dpool_.Finalize(device_context_));

  // Create swapchain-sized resources.
  CreateRenderBuffers(swapchain_extent_);

  DescriptorSetWriter dset_writer(textmesh_shader_program_.dset_layout_cis[0]);
  dset_writer.BindImage(font_atlas_image_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      textmesh_fs_.GetDescriptorBindPoint("atlas_tex").binding);
  dset_writer.BindSampler(sampler_, textmesh_fs_.GetDescriptorBindPoint("samp").binding);
  for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
    // TODO(cort): allocate_pipelined_set()?
    dsets_[pframe] = dpool_.AllocateSet(device_context_, textmesh_shader_program_.dset_layouts[0]);
    dset_writer.BindBuffer(scene_uniforms_.Handle(pframe), textmesh_vs_.GetDescriptorBindPoint("scene_consts").binding);
    dset_writer.BindBuffer(
        string_uniforms_.Handle(pframe), textmesh_vs_.GetDescriptorBindPoint("string_consts").binding);
    dset_writer.WriteAll(device_context_, dsets_[pframe]);
  }
}

TextApp::~TextApp() {
  if (device_) {
    vkDeviceWaitIdle(device_);

    dpool_.Destroy(device_context_);

    scene_uniforms_.Destroy(device_context_);
    string_uniforms_.Destroy(device_context_);
    string_vb_.Destroy(device_context_);

    textmesh_vs_.Destroy(device_context_);
    textmesh_fs_.Destroy(device_context_);
    textmesh_shader_program_.Destroy(device_context_);
    textmesh_pipeline_.Destroy(device_context_);

    vkDestroySampler(device_, sampler_, host_allocator_);
    font_atlas_image_.Destroy(device_context_);
    font_.Destroy();
    font_atlas_.Destroy();

    for (const auto fb : framebuffers_) {
      vkDestroyFramebuffer(device_, fb, host_allocator_);
    }
    render_pass_.Destroy(device_context_);

    depth_image_.Destroy(device_context_);
  }
}

void TextApp::Update(double dt) {
  Application::Update(dt);
  seconds_elapsed_ += dt;

  // Update camera
  mathfu::vec3 impulse(0, 0, 0);
  const float MOVE_SPEED = 0.3f, TURN_SPEED = 0.001f;
  if (input_state_.GetDigital(InputState::DIGITAL_LPAD_UP)) {
    impulse += camera_->getViewDirection() * MOVE_SPEED;
  }
  if (input_state_.GetDigital(InputState::DIGITAL_LPAD_LEFT)) {
    mathfu::vec3 viewRight = camera_->getOrientation() * mathfu::vec3(1, 0, 0);
    impulse -= viewRight * MOVE_SPEED;
  }
  if (input_state_.GetDigital(InputState::DIGITAL_LPAD_DOWN)) {
    impulse -= camera_->getViewDirection() * MOVE_SPEED;
  }
  if (input_state_.GetDigital(InputState::DIGITAL_LPAD_RIGHT)) {
    mathfu::vec3 viewRight = camera_->getOrientation() * mathfu::vec3(1, 0, 0);
    impulse += viewRight * MOVE_SPEED;
  }
  if (input_state_.GetDigital(InputState::DIGITAL_RPAD_LEFT)) {
    mathfu::vec3 viewUp = camera_->getOrientation() * mathfu::vec3(0, 1, 0);
    impulse -= viewUp * MOVE_SPEED;
  }
  if (input_state_.GetDigital(InputState::DIGITAL_RPAD_DOWN)) {
    mathfu::vec3 viewUp = camera_->getOrientation() * mathfu::vec3(0, 1, 0);
    impulse += viewUp * MOVE_SPEED;
  }

  // Update camera based on mouse delta
  mathfu::vec3 camera_eulers = camera_->getEulersYPR() +
      mathfu::vec3(-TURN_SPEED * input_state_.GetAnalogDelta(InputState::ANALOG_MOUSE_Y),
          -TURN_SPEED * input_state_.GetAnalogDelta(InputState::ANALOG_MOUSE_X), 0);
  if (camera_eulers[0] >= float(M_PI_2 - 0.01f)) {
    camera_eulers[0] = float(M_PI_2 - 0.01f);
  } else if (camera_eulers[0] <= float(-M_PI_2 + 0.01f)) {
    camera_eulers[0] = float(-M_PI_2 + 0.01f);
  }
  camera_eulers[2] = 0;  // disallow roll
  camera_->setOrientation(mathfu::quat::FromEulerAngles(camera_eulers));
  dolly_->Impulse(impulse);
  dolly_->Update((float)dt);

  // Update uniforms
  SceneUniforms *uniforms = (SceneUniforms *)scene_uniforms_.Mapped(pframe_index_);
  uniforms->time_and_res =
      mathfu::vec4((float)seconds_elapsed_, (float)swapchain_extent_.width, (float)swapchain_extent_.height, 0);
  uniforms->eye = mathfu::vec4(camera_->getEyePoint(), 1.0f);
  mathfu::mat4 w2v = camera_->getViewMatrix();
  const mathfu::mat4 proj = camera_->getProjectionMatrix();
  // clang-format off
  const mathfu::mat4 clip_fixup(
    +1.0f, +0.0f, +0.0f, +0.0f,
    +0.0f, -1.0f, +0.0f, +0.0f,
    +0.0f, +0.0f, +0.5f, +0.5f,
    +0.0f, +0.0f, +0.0f, +1.0f);
  // clang-format on
  uniforms->viewproj = clip_fixup * proj * w2v;
  scene_uniforms_.FlushPframeHostCache(pframe_index_);

  StringUniforms *string_uniforms = (StringUniforms *)string_uniforms_.Mapped(pframe_index_);
  string_uniforms->o2w = mathfu::mat4::Identity() * 0.001f;
  string_uniforms_.FlushPframeHostCache(pframe_index_);
}

void TextApp::Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) {
  VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
  render_pass_.begin_info.framebuffer = framebuffer;
  render_pass_.begin_info.renderArea.extent = swapchain_extent_;
  vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, textmesh_pipeline_.handle);
  VkRect2D scissor_rect = render_pass_.begin_info.renderArea;
  VkViewport viewport = Rect2DToViewport(scissor_rect);
  vkCmdSetViewport(primary_cb, 0, 1, &viewport);
  vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);
  vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
      textmesh_pipeline_.shader_program->pipeline_layout, 0, 1, &dsets_[pframe_index_], 0, nullptr);
  VkDeviceSize vb_offsets[1] = {0};
  VkBuffer vb_handles[1] = {string_vb_.Handle()};
  vkCmdBindVertexBuffers(primary_cb, 0, 1, vb_handles, vb_offsets);
  vkCmdDraw(primary_cb, 6 * string_quad_count_, 1, 0, 0);

  vkCmdEndRenderPass(primary_cb);
}

void TextApp::HandleWindowResize(VkExtent2D new_window_extent) {
  Application::HandleWindowResize(new_window_extent);

  // Destroy existing objects before re-creating them.
  for (auto fb : framebuffers_) {
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

void TextApp::CreateRenderBuffers(VkExtent2D extent) {
  // Create depth buffer
  VkImageCreateInfo depth_image_ci = render_pass_.GetAttachmentImageCreateInfo(1, extent);
  depth_image_ = {};
  SPOKK_VK_CHECK(depth_image_.Create(
      device_context_, depth_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DEVICE_ALLOCATION_SCOPE_DEVICE));

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

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
      {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}};
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;
  app_ci.pfn_set_device_features = EnableMinimumDeviceFeatures;

  TextApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
