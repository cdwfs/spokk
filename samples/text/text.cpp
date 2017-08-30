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
const std::string string_text("Watson, come here. I need you.");

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
  SPOKK_VK_CHECK(render_pass_.Finalize(device_));
  render_pass_.clear_values[0] = CreateColorClearValue(0.2f, 0.2f, 0.3f);
  render_pass_.clear_values[1] = CreateDepthClearValue(1.0f, 0);

  // Load font
  int font_create_error = font_.Create("data/SourceCodePro-Semibold.ttf");
  ZOMBO_ASSERT(font_create_error == 0, "Font loading error: %d", font_create_error);
#if 0
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
#endif

  // Create font atlas
  FontAtlasCreateInfo atlas_ci = {};
  atlas_ci.font = &font_;
  atlas_ci.font_size = 48.0f;
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
  mathfu::vec3 camera_accel =
      (camera_accel_dir.LengthSquared() > 0) ? camera_accel_dir.Normalized() * CAMERA_ACCEL_MAG : mathfu::vec3(0, 0, 0);

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
  text_state.color[0] = 1.0f;
  text_state.color[1] = 1.0f;
  text_state.color[2] = 0.0f;
  text_state.color[3] = 1.0f;
  text_state.viewport = viewport;
  text_state.font_atlas = &font_atlas_;
  texter_.BindDrawState(primary_cb, text_state);
  float str_x = 100.0f, str_y = 100.0f;
  texter_.Printf(primary_cb, &str_x, &str_y, "Vulkan is %d winners %c render with!", 4, '2');
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

///////
const float sdf_size = 128.0;  // the larger this is, the better large font sizes look
const float pixel_dist_scale = 64.0;  // trades off precision w/ ability to handle *smaller* sizes
const int onedge_value = 128;
const int padding = 3;  // not used in shader

typedef struct {
  float advance;
  signed char xoff;
  signed char yoff;
  unsigned char w, h;
  unsigned char *data;
} fontchar;
fontchar fdata[128];

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

void DumpSdfGlyphs() {
  FILE *font_file = fopen("c:/windows/fonts/times.ttf", "rb");
  fseek(font_file, 0, SEEK_END);
  size_t font_nbytes = ftell(font_file);
  fseek(font_file, 0, SEEK_SET);
  std::vector<uint8_t> data(font_nbytes);
  fread(data.data(), 1, font_nbytes, font_file);
  fclose(font_file);

  int ch;
  float scale;
  stbtt_fontinfo font;
  stbtt_InitFont(&font, data.data(), 0);
  scale = stbtt_ScaleForPixelHeight(&font, sdf_size);
  for (ch = 33; ch < 127; ++ch) {
    fontchar fc;
    int xoff, yoff, w, h, advance;
    fc.data = stbtt_GetCodepointSDF(&font, scale, ch, padding, onedge_value, pixel_dist_scale, &w, &h, &xoff, &yoff);
    fc.xoff = (char)xoff;
    fc.yoff = (char)yoff;
    fc.w = (unsigned char)w;
    fc.h = (unsigned char)h;
    stbtt_GetCodepointHMetrics(&font, ch, &advance, NULL);
    fc.advance = advance * scale;
    fdata[ch] = fc;
    char png_name[32] = {};
    sprintf(png_name, "%03d.png", ch);
    stbi_write_png(png_name, w, h, 1, fc.data, 0);
  }
}
//////

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
