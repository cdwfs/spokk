#include "vk_application.h"
#include "vk_debug.h"
using namespace spokk;

#include "platform.h"

#include <mathfu/vector.h>
#include <mathfu/glsl_mappings.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <ctime>
#include <memory>
#include <thread>

namespace {

const std::string frag_shader_path = "../shadertoy.frag";

struct ShaderToyUniforms {
  mathfu::vec3_packed iResolution; // viewport resolution (in pixels)
  float     iGlobalTime;           // shader playback time (in seconds)
  float     iTimeDelta;            // render time (in seconds)
  int       iFrame;                // shader playback frame
  float     iChannelTime[4];       // channel playback time (in seconds)
  mathfu::vec3_packed iChannelResolution[4]; // channel resolution (in pixels)
  mathfu::vec4_packed iMouse;      // mouse pixel coords. xy: current (if MLB down), zw: click
  mathfu::vec4_packed iDate;       // (year, month, day, time in seconds)
  float     iSampleRate;           // sound sample rate (i.e., 44100
  uint32_t padding[1+32];          // TODO(cort): dynamic uniform buffer offsets must be a multiple of minUniformBufferOffsetAlignment
};

}  // namespace

class ShaderToyApp : public spokk::Application {
public:
  explicit ShaderToyApp(Application::CreateInfo &ci) :
      Application(ci) {
    seconds_elapsed_ = 0;

    // Create render pass
    render_pass_.init_from_preset(RenderPass::Preset::COLOR, swapchain_surface_format_.format);
    // Customize
    render_pass_.attachment_descs[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    SPOKK_VK_CHECK(render_pass_.finalize_and_create(device_context_));

    // Create VkFramebuffers
    std::vector<VkImageView> attachment_views = {
      VK_NULL_HANDLE, // filled in below
    };
    VkFramebufferCreateInfo framebuffer_ci = render_pass_.get_framebuffer_ci(swapchain_extent_);
    framebuffer_ci.pAttachments = attachment_views.data();
    framebuffers_.resize(swapchain_image_views_.size());
    for(size_t i=0; i<swapchain_image_views_.size(); ++i) {
      attachment_views[0] = swapchain_image_views_[i];
      SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &framebuffers_[i]));
    }

    // Load textures and samplers
    VkSamplerCreateInfo sampler_ci = get_sampler_ci(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    for(auto& sampler : samplers_) {
      SPOKK_VK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &sampler));
    }
    image_loader_ = my_make_unique<ImageLoader>(device_context_);
    SPOKK_VK_CHECK(textures_[0].create_and_load(device_context_, *image_loader_.get(), "trevor/noise.dds"));
    SPOKK_VK_CHECK(textures_[1].create_and_load(device_context_, *image_loader_.get(), "trevor/redf.ktx"));
    SPOKK_VK_CHECK(textures_[2].create_and_load(device_context_, *image_loader_.get(), "trevor/redf.ktx"));
    SPOKK_VK_CHECK(textures_[3].create_and_load(device_context_, *image_loader_.get(), "trevor/redf.ktx"));

    // Load shader pipelines
    SPOKK_VK_CHECK(fullscreen_tri_vs_.create_and_load_spv_file(device_context_, "fullscreen.vert.spv"));
    SPOKK_VK_CHECK(shadertoy_fs_.create_and_load_spv_file(device_context_, "shadertoy.frag.spv"));
    SPOKK_VK_CHECK(shader_pipeline_.add_shader(&fullscreen_tri_vs_));
    SPOKK_VK_CHECK(shader_pipeline_.add_shader(&shadertoy_fs_));
    SPOKK_VK_CHECK(shader_pipeline_.finalize(device_context_));

    // Create uniform buffer
    VkBufferCreateInfo uniform_buffer_ci = {};
    uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    uniform_buffer_ci.size = sizeof(ShaderToyUniforms);
    uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(uniform_buffer_.create(device_context_, VFRAME_COUNT, uniform_buffer_ci));

    SPOKK_VK_CHECK(pipeline_.create(device_context_,
      MeshFormat::get_empty(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
      &shader_pipeline_, &render_pass_, 0));
    // TODO(cort): add() needs a better name
    for(const auto& dset_layout_ci : shader_pipeline_.dset_layout_cis) {
      dpool_.add(dset_layout_ci, VFRAME_COUNT);
    }
    SPOKK_VK_CHECK(dpool_.finalize(device_context_));
    DescriptorSetWriter dset_writer(shader_pipeline_.dset_layout_cis[0]);
    for(uint32_t iTex = 0; iTex < (uint32_t)textures_.size(); ++iTex) {
      dset_writer.bind_image(textures_[iTex].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, samplers_[iTex], iTex);
    }
    for(uint32_t pframe = 0; pframe < VFRAME_COUNT; ++pframe) {
      dsets_[pframe] = dpool_.allocate_set(device_context_, shader_pipeline_.dset_layouts[0]);
      dset_writer.bind_buffer(uniform_buffer_.handle(pframe), 0, VK_WHOLE_SIZE, 4);
      dset_writer.write_all_to_dset(device_context_, dsets_[pframe]);
    }

    // Spawn the shader-watcher thread, to set a shared bool whenever the contents of the shader
    // directory change.
    swap_shader_.store(false);

    shader_reloader_thread_ = std::thread(&ShaderToyApp::watch_shader_dir, this, "..");
  }
  virtual ~ShaderToyApp() {
    if (device_) {
      shader_reloader_thread_.detach();  // TODO(cort): graceful exit; I'm getting occasional crashes in here

      vkDeviceWaitIdle(device_);

      dpool_.destroy(device_context_);

      uniform_buffer_.destroy(device_context_);

      pipeline_.destroy(device_context_);

      shader_pipeline_.destroy(device_context_);
      fullscreen_tri_vs_.destroy(device_context_);
      shadertoy_fs_.destroy(device_context_);

      for(const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
      render_pass_.destroy(device_context_);

      for(auto& image : textures_) {
        image.destroy(device_context_);
      }
      for(auto sampler : samplers_) {
        vkDestroySampler(device_, sampler, host_allocator_);
      }
      image_loader_.reset();
    }
  }

  ShaderToyApp(const ShaderToyApp&) = delete;
  const ShaderToyApp& operator=(const ShaderToyApp&) = delete;

  void update(double dt) override {
    Application::update(dt);
    seconds_elapsed_ += dt;

    // Reload shaders, if necessary
    bool reload = false;
    swap_shader_.compare_exchange_strong(reload, false);
    if (reload) {
      // If we get this far, it's time to replace the existing pipeline
      vkDeviceWaitIdle(device_);
      pipeline_.destroy(device_context_);
      shader_pipeline_.destroy(device_context_);
      shadertoy_fs_.destroy(device_context_);
      shadertoy_fs_ = staging_fs_;
      shader_pipeline_ = staging_shader_pipeline_;
      pipeline_ = staging_pipeline_;
      pipeline_.shader_pipeline = &shader_pipeline_;

      swap_shader_.store(false);
    }

    // Update uniforms
    // TODO(cort): track mouse events. Update position while button is down, and keep track of most recent
    // click position. If button is up, keep the most recent values.
    double mouse_x = 0, mouse_y = 0;
    glfwGetCursorPos(window_.get(), &mouse_x, &mouse_y);

    std::time_t now = std::time(nullptr);
    const std::tm* cal = std::localtime(&now);
    float year = (float)cal->tm_year;
    float month = (float)cal->tm_mon;
    float mday = (float)cal->tm_mday;
    float dsec = (float)(cal->tm_hour * 3600 + cal->tm_min * 60 + cal->tm_sec);

    viewport_ = vk_extent_to_viewport(swapchain_extent_);
    scissor_rect_ = vk_extent_to_rect2d(swapchain_extent_);
    uniforms_.iResolution = mathfu::vec3(viewport_.width, viewport_.height, 1.0f);
    uniforms_.iGlobalTime = (float)seconds_elapsed_;
    uniforms_.iTimeDelta = (float)dt;
    uniforms_.iFrame = frame_index_;
    uniforms_.iChannelTime[0] = 0.0f;  // TODO(cort): audio/video channels are TBI
    uniforms_.iChannelTime[1] = 0.0f;
    uniforms_.iChannelTime[2] = 0.0f;
    uniforms_.iChannelTime[3] = 0.0f;
    uniforms_.iChannelResolution[0] = mathfu::vec3(1.0f, 1.0f, 1.0f);
    uniforms_.iChannelResolution[1] = mathfu::vec3(1.0f, 1.0f, 1.0f);
    uniforms_.iChannelResolution[2] = mathfu::vec3(1.0f, 1.0f, 1.0f);
    uniforms_.iChannelResolution[3] = mathfu::vec3(1.0f, 1.0f, 1.0f);
    uniforms_.iMouse = mathfu::vec4((float)mouse_x, (float)mouse_y, 0.0f, 0.0f);  // TODO(cort): mouse click tracking is TBI
    uniforms_.iDate = mathfu::vec4(year, month, mday, dsec);
    uniforms_.iSampleRate = 44100.0f;
    uniform_buffer_.load(device_context_, vframe_index_, &uniforms_, sizeof(uniforms_));
  }

  void render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override {
    VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass_.handle;
    render_pass_begin_info.framebuffer = framebuffer;
    render_pass_begin_info.renderArea.offset.x = 0;
    render_pass_begin_info.renderArea.offset.y = 0;
    render_pass_begin_info.renderArea.extent = swapchain_extent_;

    vkCmdBeginRenderPass(primary_cb, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.handle);
    vkCmdSetViewport(primary_cb, 0,1, &viewport_);
    vkCmdSetScissor(primary_cb, 0,1, &scissor_rect_);
    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipeline_.shader_pipeline->pipeline_layout,
      0, 1, &dsets_[vframe_index_], 0, nullptr);
    vkCmdDraw(primary_cb, 3, 1,0,0);
    vkCmdEndRenderPass(primary_cb);
  }

private:
  void reload_shader() {
    shaderc::SpvCompilationResult compile_result = shader_compiler_.compile_glsl_file(frag_shader_path);
    if (compile_result.GetCompilationStatus() == shaderc_compilation_status_success) {
      Shader new_fs;
      SPOKK_VK_CHECK(new_fs.create_and_load_compile_result(device_context_, compile_result));
      ShaderPipeline new_shader_pipeline;
      SPOKK_VK_CHECK(new_shader_pipeline.add_shader(&fullscreen_tri_vs_));
      SPOKK_VK_CHECK(new_shader_pipeline.add_shader(&new_fs));
      SPOKK_VK_CHECK(new_shader_pipeline.finalize(device_context_));
      GraphicsPipeline new_pipeline;
      SPOKK_VK_CHECK(new_pipeline.create(device_context_,
        MeshFormat::get_empty(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
        &new_shader_pipeline, &render_pass_, 0));
      // Success!
      staging_fs_ = new_fs;
      staging_shader_pipeline_ = new_shader_pipeline;
      staging_pipeline_ = new_pipeline;
      staging_pipeline_.shader_pipeline = &staging_shader_pipeline_;
      swap_shader_.store(true);
    } else {
      printf("%s\n", compile_result.GetErrorMessage().c_str());
    }
  }
  void watch_shader_dir(const std::string dir_path) {
#ifdef _MSC_VER // Detect changes using Windows change notification API
    std::wstring wpath(dir_path.begin(), dir_path.end()); // only works for ASCII input, but I'm okay with that.
    HANDLE dwChangeHandle = FindFirstChangeNotification(wpath.c_str(), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
    assert(dwChangeHandle != INVALID_HANDLE_VALUE);
    int lastUpdateSeconds = -1;
    for(;;) {
      DWORD dwWaitStatus = WaitForSingleObject(dwChangeHandle, INFINITE);
      SYSTEMTIME localTime;
      GetLocalTime(&localTime);
      // Only reload at most once per second
      if (!swap_shader_ && dwWaitStatus == WAIT_OBJECT_0 && localTime.wSecond != lastUpdateSeconds) {
        zomboSleepMsec(20); // Reloading immediately doesn't work; fopen() fails. Give it a little bit to resolve itself.
                           // Attempt to rebuild the shader and pipeline first
        reload_shader();
        // Don't reload more than once a second
        lastUpdateSeconds = localTime.wSecond;
      }
      FindNextChangeNotification(dwChangeHandle);
    }
#else
#error Unsupported platform! Find your platform's equivalent of inotify!
#endif
  }

  double seconds_elapsed_;

  std::atomic_bool swap_shader_;
  std::thread shader_reloader_thread_;
  ShaderCompiler shader_compiler_;
  shaderc::CompileOptions compiler_options_;

  std::unique_ptr<ImageLoader> image_loader_;
  std::array<Image,4> textures_;
  std::array<VkSampler,4> samplers_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  Shader fullscreen_tri_vs_, shadertoy_fs_;
  ShaderPipeline shader_pipeline_;
  GraphicsPipeline pipeline_;

  Shader staging_fs_;
  ShaderPipeline staging_shader_pipeline_;
  GraphicsPipeline staging_pipeline_;

  VkViewport viewport_;
  VkRect2D scissor_rect_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, VFRAME_COUNT> dsets_;

  ShaderToyUniforms uniforms_;
  PipelinedBuffer uniform_buffer_;
};

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
    {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}
  };
  Application::CreateInfo app_ci = {};
  // TODO(cort): re-enable performance warnings once Tobin's fix for unused VB bindings goes in
  //app_ci.debug_report_flags |= VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
  app_ci.queue_family_requests = queue_requests;

  ShaderToyApp app(app_ci);
  int run_error = app.run();

  return run_error;
}