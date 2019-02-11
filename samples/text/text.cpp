#include <spokk.h>
using namespace spokk;

#include <common/camera.h>

#include <array>
#include <cstdio>
#include <cstring>

namespace {
const std::string string_text("Watson, come here. I need you.");

template <typename T>
T my_clamp(T x, T xmin, T xmax) {
  return (x < xmin) ? xmin : ((x > xmax) ? xmax : x);
}
uint16_t F32toU16N(float f) { return (uint16_t)(my_clamp(f, 0.0f, 1.0f) * (float)UINT16_MAX + 0.5f); }
int16_t F32toS16(float f) { return (int16_t)my_clamp(f, (float)INT16_MIN, (float)INT16_MAX); }

struct SceneUniforms {
  glm::vec4 time_and_res;  // x: elapsed seconds, yz: viewport resolution in pixels
  glm::vec4 eye;  // xyz: eye position
  glm::mat4 viewproj;
};
constexpr float FOV_DEGREES = 45.0f;
constexpr float Z_NEAR = 0.01f;
constexpr float Z_FAR = 100.0f;
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
  TextRenderer texter_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDrone> drone_;
};

TextApp::TextApp(Application::CreateInfo &ci) : Application(ci) {
  seconds_elapsed_ = 0;

  camera_ = my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, FOV_DEGREES, Z_NEAR, Z_FAR);
  const glm::vec3 initial_camera_pos(0, 0, 10);
  const glm::vec3 initial_camera_target(0, 0, 0);
  const glm::vec3 initial_camera_up(0, 1, 0);
  camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
  drone_ = my_make_unique<CameraDrone>(*camera_);

  // Create render pass
  render_pass_.InitFromPreset(RenderPass::Preset::COLOR_DEPTH, swapchain_surface_format_.format);
  SPOKK_VK_CHECK(render_pass_.Finalize(device_));
  render_pass_.clear_values[0] = CreateColorClearValue(1.0f, 1.0f, 1.0f, 1.0f);
  render_pass_.clear_values[1] = CreateDepthClearValue(1.0f, 0);
  device_.SetObjectName(render_pass_.handle, "Primary Render Pass");

  // Load font
  int font_create_error = font_.Create("data/text/SourceCodePro-Semibold.ttf");
  ZOMBO_ASSERT(font_create_error == 0, "Font loading error: %d", font_create_error);

  // Create font atlas
  FontAtlasCreateInfo atlas_ci = {};
  atlas_ci.font = &font_;
  atlas_ci.font_size = 36.0f;
  atlas_ci.image_oversample_x = atlas_ci.image_oversample_y = 2;
  atlas_ci.image_width = 512;
  atlas_ci.image_height = 512;
  atlas_ci.codepoint_first = 32;
  atlas_ci.codepoint_count = 96;
  int atlas_create_err = font_atlas_.Create(device_, atlas_ci);
  ZOMBO_ASSERT(atlas_create_err == 0, "Font atlas creation error: %d", atlas_create_err);

  // Create text renderer
  TextRenderer::CreateInfo texter_ci = {};
  texter_ci.font_atlases.push_back(&font_atlas_);
  texter_ci.render_pass = &render_pass_;
  texter_ci.subpass = 0;
  texter_ci.target_color_attachment_index = 0;
  texter_ci.pframe_count = PFRAME_COUNT;
  texter_ci.max_binds_per_pframe = 16;
  texter_ci.max_glyphs_per_pframe = 1024;
  int texter_create_err = texter_.Create(device_, texter_ci);
  ZOMBO_ASSERT(texter_create_err == 0, "Texter create error: %d", texter_create_err);

  // Create swapchain-sized resources.
  CreateRenderBuffers(swapchain_extent_);
}

TextApp::~TextApp() {
  if (device_) {
    vkDeviceWaitIdle(device_);

    texter_.Destroy(device_);
    font_atlas_.Destroy(device_);
    font_.Destroy();

    for (const auto fb : framebuffers_) {
      vkDestroyFramebuffer(device_, fb, host_allocator_);
    }
    render_pass_.Destroy(device_);

    depth_image_.Destroy(device_);
  }
}

void TextApp::Update(double dt) {
  seconds_elapsed_ += dt;

  drone_->Update(input_state_, (float)dt);
}

void TextApp::Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) {
  VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
  render_pass_.begin_info.framebuffer = framebuffer;
  render_pass_.begin_info.renderArea.extent = swapchain_extent_;
  vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
  VkRect2D scissor_rect = render_pass_.begin_info.renderArea;
  VkViewport viewport = Rect2DToViewport(scissor_rect);
  vkCmdSetViewport(primary_cb, 0, 1, &viewport);
  vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect);

  TextRenderer::State text_state = {};
  text_state.pframe_index = pframe_index_;
  text_state.spacing = 0.0f;
  text_state.scale = 1.0f;
  text_state.color[0] = 0.0f;
  text_state.color[1] = 0.0f;
  text_state.color[2] = 0.0f;
  text_state.color[3] = 0.0f;
  text_state.viewport = viewport;
  text_state.font_atlas = &font_atlas_;
  texter_.BindDrawState(device_, primary_cb, text_state);
  float str_x = 100.0f, str_y = 100.0f;
  texter_.Printf(device_, primary_cb, &str_x, &str_y, "Vulkan is %d winners %c render with!", 4, '2');
  vkCmdEndRenderPass(primary_cb);
}

void TextApp::HandleWindowResize(VkExtent2D new_window_extent) {
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

void TextApp::CreateRenderBuffers(VkExtent2D extent) {
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
