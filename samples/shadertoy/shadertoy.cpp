#include <spokk.h>
using namespace spokk;

#include "shadertoyinfo.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdio>
#include <ctime>

namespace {

// NOTE: declaraction order is different from shadertoy due to packing rules
struct ShaderToyUniforms {
  glm::vec4 iResolution;  // xyz: viewport resolution (in pixels), w: unused
  glm::vec4 iChannelTime[4];  // x: channel playback time (in seconds), yzw: unused
  glm::vec4 iChannelResolution[4];  // xyz: channel resolution (in pixels)
  glm::vec4 iMouse;  // mouse pixel coords. xy: current (if MLB down), zw: click
  glm::vec4 iDate;  // (year, month, day, time in seconds)
  float iTime;  // shader playback time (in seconds)
  float iTimeDelta;  // render time (in seconds)
  int iFrame;  // shader playback frame
  float iSampleRate;  // sound sample rate (i.e., 44100
};

glm::vec2 click_pos(0, 0);
void MyGlfwMouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) {
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    double mouse_x, mouse_y;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);
    click_pos = glm::vec2((float)mouse_x, (float)mouse_y);
  }
}

}  // namespace

class ShaderToyApp : public spokk::Application {
public:
  explicit ShaderToyApp(Application::CreateInfo& ci) : Application(ci) {
    seconds_elapsed_ = 0;

    mouse_pos_ = glm::vec2(0, 0);
    glfwSetMouseButtonCallback(window_.get(), MyGlfwMouseButtonCallback);

    // Create render pass
    render_pass_.InitFromPreset(RenderPass::Preset::COLOR, swapchain_surface_format_.format);
    // Customize
    render_pass_.attachment_descs[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    SPOKK_VK_CHECK(render_pass_.Finalize(device_));
    SPOKK_VK_CHECK(device_.SetObjectName(render_pass_.handle, "main color pass"));

    // Load textures and samplers
    VkSamplerCreateInfo sampler_ci =
        GetSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    for (size_t i = 0; i < samplers_.size(); ++i) {
      SPOKK_VK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &samplers_[i]));
      SPOKK_VK_CHECK(device_.SetObjectName(
          samplers_[i], std::string("basic linear+wrap sampler ") + std::to_string(i)));  // TODO(cort): absl::StrCat
    }
    for (size_t i = 0; i < textures_.size(); ++i) {
      char filename[17];
      zomboSnprintf(filename, 17, "data/tex%02u.ktx", (uint32_t)i);
      ZOMBO_ASSERT(0 ==
              textures_[i].CreateFromFile(device_, graphics_and_present_queue_, filename, VK_FALSE,
                  THSVS_ACCESS_FRAGMENT_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER),
          "Failed to load %s", filename);
    }
    for (size_t i = 0; i < cubemaps_.size(); ++i) {
      char filename[18];
      zomboSnprintf(filename, 18, "data/cube%02u.ktx", (uint32_t)i);
      ZOMBO_ASSERT(0 ==
              cubemaps_[i].CreateFromFile(device_, graphics_and_present_queue_, filename, VK_FALSE,
                  THSVS_ACCESS_FRAGMENT_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER),
          "Failed to load %s", filename);
    }
    active_images_[0] = &textures_[15];
    active_images_[1] = &cubemaps_[2];
    active_images_[2] = &textures_[2];
    active_images_[3] = &textures_[3];

    // Load shader pipelines
    SPOKK_VK_CHECK(fullscreen_tri_vs_.CreateAndLoadSpirvFile(device_, "data/shadertoy/fullscreen.vert.spv"));
    SPOKK_VK_CHECK(fragment_shader_.CreateAndLoadSpirvFile(device_, "data/shadertoy/shadertoy.frag.spv"));
    SPOKK_VK_CHECK(shader_program_.AddShader(&fullscreen_tri_vs_));
    SPOKK_VK_CHECK(shader_program_.AddShader(&fragment_shader_));
    SPOKK_VK_CHECK(shader_program_.Finalize(device_));

    pipeline_.Init(&empty_mesh_format_, &shader_program_, &render_pass_, 0);
    SPOKK_VK_CHECK(pipeline_.Finalize(device_));
    SPOKK_VK_CHECK(device_.SetObjectName(pipeline_.handle, "Shadertoy pipeline"));

    for (const auto& dset_layout_ci : shader_program_.dset_layout_cis) {
      dpool_.Add(dset_layout_ci, PFRAME_COUNT);
    }
    SPOKK_VK_CHECK(dpool_.Finalize(device_));

    // Look up the appropriate memory flags for uniform buffers on this platform
    VkMemoryPropertyFlags uniform_buffer_memory_flags =
        device_.MemoryFlagsForAccessPattern(DEVICE_MEMORY_ACCESS_PATTERN_CPU_TO_GPU_DYNAMIC);

    DescriptorSetWriter dset_writer(shader_program_.dset_layout_cis[0]);
    for (size_t iTex = 0; iTex < active_images_.size(); ++iTex) {
      dset_writer.BindCombinedImageSampler(
          active_images_[iTex]->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, samplers_[iTex], (uint32_t)iTex);
    }
    for (uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
      auto& frame_data = frame_data_[pframe];
      // Create uniform buffer
      VkBufferCreateInfo uniform_buffer_ci = {};
      uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      uniform_buffer_ci.size = sizeof(ShaderToyUniforms);
      uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      SPOKK_VK_CHECK(frame_data.ubo.Create(device_, uniform_buffer_ci, uniform_buffer_memory_flags));
      SPOKK_VK_CHECK(device_.SetObjectName(frame_data.ubo.Handle(),
          "uniform buffer " + std::to_string(pframe)));  // TODO(cort): absl::StrCat
      dset_writer.BindBuffer(frame_data.ubo.Handle(), 4);

      frame_data.dset = dpool_.AllocateSet(device_, shader_program_.dset_layouts[0]);
      SPOKK_VK_CHECK(device_.SetObjectName(frame_data.dset,
          "frame dset " + std::to_string(pframe)));  // TODO(cort): absl::StrCat
      dset_writer.WriteAll(device_, frame_data.dset);
    }

    // Create swapchain-sized resources.
    CreateRenderBuffers(swapchain_extent_);
  }
  virtual ~ShaderToyApp() {
    if (device_) {
      vkDeviceWaitIdle(device_);

      dpool_.Destroy(device_);

      for (auto& frame_data : frame_data_) {
        frame_data.ubo.Destroy(device_);
      }

      pipeline_.Destroy(device_);

      shader_program_.Destroy(device_);
      fullscreen_tri_vs_.Destroy(device_);
      fragment_shader_.Destroy(device_);

      for (const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
      render_pass_.Destroy(device_);

      for (auto& image : textures_) {
        image.Destroy(device_);
      }
      for (auto& cube : cubemaps_) {
        cube.Destroy(device_);
      }
      for (auto sampler : samplers_) {
        vkDestroySampler(device_, sampler, host_allocator_);
      }
    }
  }

  ShaderToyApp(const ShaderToyApp&) = delete;
  const ShaderToyApp& operator=(const ShaderToyApp&) = delete;

  void Update(double dt) override {
    seconds_elapsed_ += dt;
    current_dt_ = (float)dt;
  }

  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override {
    const auto& frame_data = frame_data_[pframe_index_];
    // Update uniforms.
    // Shadertoy's origin is in the lower left.
    double mouse_x = 0, mouse_y = 0;
    glfwGetCursorPos(window_.get(), &mouse_x, &mouse_y);
    if (glfwGetMouseButton(window_.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
      mouse_pos_ = glm::vec2((float)mouse_x, (float)mouse_y);
    }
    std::time_t now = std::time(nullptr);
    const std::tm* cal = std::localtime(&now);
    float year = (float)cal->tm_year;
    float month = (float)cal->tm_mon;
    float mday = (float)cal->tm_mday;
    float dsec = (float)(cal->tm_hour * 3600 + cal->tm_min * 60 + cal->tm_sec);
    viewport_ = ExtentToViewport(swapchain_extent_);
    // Convert viewport back to right-handed (flip Y axis, remove Y offset)
    viewport_.y = 0.0f;
    viewport_.height *= -1;
    scissor_rect_ = ExtentToRect2D(swapchain_extent_);
    ShaderToyUniforms* uniforms = (ShaderToyUniforms*)frame_data.ubo.Mapped();
    uniforms->iResolution = glm::vec4(abs(viewport_.width), abs(viewport_.height), 1.0f, 0.0f);
    uniforms->iChannelTime[0] = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);  // TODO(cort): audio/video channels are TBI
    uniforms->iChannelTime[1] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    uniforms->iChannelTime[2] = glm::vec4(2.0f, 0.0f, 0.0f, 0.0f);
    uniforms->iChannelTime[3] = glm::vec4(3.0f, 0.0f, 0.0f, 0.0f);
    uniforms->iChannelResolution[0] = glm::vec4((float)active_images_[0]->image_ci.extent.width,
        (float)active_images_[0]->image_ci.extent.height, (float)active_images_[0]->image_ci.extent.depth, 0.0f);
    uniforms->iChannelResolution[1] = glm::vec4((float)active_images_[1]->image_ci.extent.width,
        (float)active_images_[1]->image_ci.extent.height, (float)active_images_[1]->image_ci.extent.depth, 0.0f);
    uniforms->iChannelResolution[2] = glm::vec4((float)active_images_[2]->image_ci.extent.width,
        (float)active_images_[2]->image_ci.extent.height, (float)active_images_[2]->image_ci.extent.depth, 0.0f);
    uniforms->iChannelResolution[3] = glm::vec4((float)active_images_[3]->image_ci.extent.width,
        (float)active_images_[3]->image_ci.extent.height, (float)active_images_[3]->image_ci.extent.depth, 0.0f);
    uniforms->iTime = (float)seconds_elapsed_;
    uniforms->iTimeDelta = current_dt_;
    uniforms->iFrame = (int)frame_index_;
    // GLFW mouse coord origin is in the upper left; convert to shadertoy's lower-left origin.
    uniforms->iMouse = glm::vec4(mouse_pos_.x, abs(viewport_.height) - mouse_pos_.y,
      click_pos.x, abs(viewport_.height) - click_pos.y);
    uniforms->iDate = glm::vec4(year, month, mday, dsec);
    uniforms->iSampleRate = 44100.0f;
    SPOKK_VK_CHECK(frame_data.ubo.FlushHostCache(device_));

    // Write command buffer
    VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
    render_pass_.begin_info.framebuffer = framebuffer;
    render_pass_.begin_info.renderArea.extent = swapchain_extent_;
    vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.handle);
    vkCmdSetViewport(primary_cb, 0, 1, &viewport_);
    vkCmdSetScissor(primary_cb, 0, 1, &scissor_rect_);
    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_.shader_program->pipeline_layout, 0, 1, &frame_data.dset, 0, nullptr);
    vkCmdDraw(primary_cb, 3, 1, 0, 0);
    vkCmdEndRenderPass(primary_cb);
  }

protected:
  void HandleWindowResize(VkExtent2D new_window_extent) override {
    for (auto fb : framebuffers_) {
      if (fb != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
    }
    framebuffers_.clear();

    CreateRenderBuffers(new_window_extent);
  }

private:
  void CreateRenderBuffers(VkExtent2D extent) {
    // Create VkFramebuffers
    std::vector<VkImageView> attachment_views = {
        VK_NULL_HANDLE,  // filled in below
    };
    VkFramebufferCreateInfo framebuffer_ci = render_pass_.GetFramebufferCreateInfo(extent);
    framebuffer_ci.pAttachments = attachment_views.data();
    framebuffers_.resize(swapchain_image_views_.size());
    for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
      attachment_views[0] = swapchain_image_views_[i];
      SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &framebuffers_[i]));
      SPOKK_VK_CHECK(device_.SetObjectName(
          framebuffers_[i], std::string("swapchain framebuffer ") + std::to_string(i)));  // TODO(cort): absl::StrCat
    }
  }

  double seconds_elapsed_;
  float current_dt_;

  std::array<Image, 16> textures_;
  std::array<Image, 6> cubemaps_;
  std::array<Image*, 4> active_images_;
  std::array<VkSampler, 4> samplers_;

  MeshFormat empty_mesh_format_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  Shader fullscreen_tri_vs_;

  Shader fragment_shader_;
  ShaderProgram shader_program_;
  GraphicsPipeline pipeline_;

  VkViewport viewport_;
  VkRect2D scissor_rect_;

  DescriptorPool dpool_;
  struct FrameData {
    VkDescriptorSet dset;
    Buffer ubo;
  };
  std::array<FrameData, PFRAME_COUNT> frame_data_;

  glm::vec2 mouse_pos_;
};

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  ShadertoyInfo shader_info;
  ZOMBO_ASSERT_RETURN(shader_info.Load("samples/shadertoy/cache/info/3lsSzf.json") == 0, 1, "Failed to load shader");

  std::vector<Application::QueueFamilyRequest> queue_requests = {
      {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}};
  Application::CreateInfo app_ci = {};
  app_ci.debug_report_flags |= VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
  app_ci.queue_family_requests = queue_requests;
  app_ci.pfn_set_device_features = EnableMinimumDeviceFeatures;

  ShaderToyApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
