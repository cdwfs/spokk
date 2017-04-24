#include "vk_application.h"
#include "vk_debug.h"
using namespace spokk;

#include "platform.h"

#include <mathfu/vector.h>
#include <mathfu/glsl_mappings.h>

#if defined(__linux__)
# include <sys/inotify.h>
# include <limits.h>
#endif  // defined(__linux__)

#include <array>
#include <atomic>
#include <cstdio>
#include <ctime>
#include <memory>
#include <thread>

namespace {

const std::string frag_shader_path = "../shadertoy.frag";

// TODO(cort): finish supporting all uniforms
struct ShaderToyUniforms {
  mathfu::vec4_packed iResolution; // xyz: viewport resolution (in pixels), w: unused
  mathfu::vec4_packed iChannelTime[4];       // x: channel playback time (in seconds), yzw: unused
  mathfu::vec4_packed iChannelResolution[4]; // xyz: channel resolution (in pixels)
  mathfu::vec4_packed iMouse;      // mouse pixel coords. xy: current (if MLB down), zw: click
  mathfu::vec4_packed iDate;       // (year, month, day, time in seconds)
  float     iGlobalTime;           // shader playback time (in seconds)
  float     iTimeDelta;            // render time (in seconds)
  int       iFrame;                // shader playback frame
  float     iSampleRate;           // sound sample rate (i.e., 44100
};

}  // namespace

class ShaderToyApp : public spokk::Application {
public:
  explicit ShaderToyApp(Application::CreateInfo &ci) :
      Application(ci) {
    seconds_elapsed_ = 0;

    // Create render pass
    render_pass_.InitFromPreset(RenderPass::Preset::COLOR, swapchain_surface_format_.format);
    // Customize
    render_pass_.attachment_descs[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    SPOKK_VK_CHECK(render_pass_.Finalize(device_context_));

    // Load textures and samplers
    VkSamplerCreateInfo sampler_ci = GetSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    for(auto& sampler : samplers_) {
      SPOKK_VK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &sampler));
    }
    const VkDeviceSize blit_buffer_nbytes = 4*1024*1024;
    SPOKK_VK_CHECK(blitter_.Create(device_context_, PFRAME_COUNT, blit_buffer_nbytes));
    // TODO(cort): replace with some actual ShaderToy textures
    textures_[0].CreateFromFile(device_context_, blitter_, graphics_and_present_queue_, "trevor/noise.dds");
    textures_[1].CreateFromFile(device_context_, blitter_, graphics_and_present_queue_, "trevor/redf.ktx");
    textures_[2].CreateFromFile(device_context_, blitter_, graphics_and_present_queue_, "trevor/redf.ktx");
    textures_[3].CreateFromFile(device_context_, blitter_, graphics_and_present_queue_, "trevor/redf.ktx");

    // Load shader pipelines
    SPOKK_VK_CHECK(fullscreen_tri_vs_.CreateAndLoadSpirvFile(device_context_, "fullscreen.vert.spv"));
    SPOKK_VK_CHECK(shadertoy_fs_.CreateAndLoadSpirvFile(device_context_, "shadertoy.frag.spv"));
    SPOKK_VK_CHECK(shader_pipeline_.AddShader(&fullscreen_tri_vs_));
    SPOKK_VK_CHECK(shader_pipeline_.AddShader(&shadertoy_fs_));
    SPOKK_VK_CHECK(shader_pipeline_.Finalize(device_context_));

    // Create uniform buffer
    VkBufferCreateInfo uniform_buffer_ci = {};
    uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    uniform_buffer_ci.size = sizeof(ShaderToyUniforms);
    uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(uniform_buffer_.Create(device_context_, PFRAME_COUNT, uniform_buffer_ci));

    SPOKK_VK_CHECK(pipeline_.Create(device_context_,
      MeshFormat::GetEmpty(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
      &shader_pipeline_, &render_pass_, 0));
    for(const auto& dset_layout_ci : shader_pipeline_.dset_layout_cis) {
      dpool_.Add(dset_layout_ci, PFRAME_COUNT);
    }
    SPOKK_VK_CHECK(dpool_.Finalize(device_context_));
    
    // Create swapchain-sized resources.
    CreateRenderBuffers(swapchain_extent_);
    
    DescriptorSetWriter dset_writer(shader_pipeline_.dset_layout_cis[0]);
    for(uint32_t iTex = 0; iTex < (uint32_t)textures_.size(); ++iTex) {
      dset_writer.BindImage(textures_[iTex].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, samplers_[iTex], iTex);
    }
    for(uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
      dsets_[pframe] = dpool_.AllocateSet(device_context_, shader_pipeline_.dset_layouts[0]);
      dset_writer.BindBuffer(uniform_buffer_.Handle(pframe), 4);
      dset_writer.WriteAll(device_context_, dsets_[pframe]);
    }

    // Spawn the shader-watcher thread, to set a shared bool whenever the contents of the shader
    // directory change.
    swap_shader_.store(false);

    shader_reloader_thread_ = std::thread(&ShaderToyApp::WatchShaderDir, this, "..");
  }
  virtual ~ShaderToyApp() {
    if (device_) {
      shader_reloader_thread_.detach();  // TODO(cort): graceful exit; I'm getting occasional crashes in here

      vkDeviceWaitIdle(device_);

      dpool_.Destroy(device_context_);

      uniform_buffer_.Destroy(device_context_);

      pipeline_.Destroy(device_context_);

      shader_pipeline_.Destroy(device_context_);
      fullscreen_tri_vs_.Destroy(device_context_);
      shadertoy_fs_.Destroy(device_context_);

      for(const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
      render_pass_.Destroy(device_context_);

      for(auto& image : textures_) {
        image.Destroy(device_context_);
      }
      for(auto sampler : samplers_) {
        vkDestroySampler(device_, sampler, host_allocator_);
      }
      blitter_.Destroy(device_context_);
    }
  }

  ShaderToyApp(const ShaderToyApp&) = delete;
  const ShaderToyApp& operator=(const ShaderToyApp&) = delete;

  void Update(double dt) override {
    Application::Update(dt);
    seconds_elapsed_ += dt;

    // Reload shaders, if necessary
    bool reload = false;
    swap_shader_.compare_exchange_strong(reload, false);
    if (reload) {
      // If we get this far, it's time to replace the existing pipeline
      vkDeviceWaitIdle(device_);
      pipeline_.Destroy(device_context_);
      shader_pipeline_.Destroy(device_context_);
      shadertoy_fs_.Destroy(device_context_);
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

    viewport_ = ExtentToViewport(swapchain_extent_);
    scissor_rect_ = ExtentToRect2D(swapchain_extent_);
    uniforms_.iResolution = mathfu::vec4(viewport_.width, viewport_.height, 1.0f, 0.0f);
    uniforms_.iChannelTime[0] = mathfu::vec4(0.0f, 0.0f, 0.0f, 0.0f);  // TODO(cort): audio/video channels are TBI
    uniforms_.iChannelTime[1] = mathfu::vec4(1.0f, 0.0f, 0.0f, 0.0f);
    uniforms_.iChannelTime[2] = mathfu::vec4(2.0f, 0.0f, 0.0f, 0.0f);
    uniforms_.iChannelTime[3] = mathfu::vec4(3.0f, 0.0f, 0.0f, 0.0f);
    uniforms_.iChannelResolution[0] = mathfu::vec4(1.1f, 1.0f, 1.0f, 0.0f);  // TODO(cort): insert texture dimensions
    uniforms_.iChannelResolution[1] = mathfu::vec4(2.2f, 1.0f, 1.0f, 0.0f);
    uniforms_.iChannelResolution[2] = mathfu::vec4(3.3f, 1.0f, 1.0f, 0.0f);
    uniforms_.iChannelResolution[3] = mathfu::vec4(4.4f, 1.0f, 1.0f, 0.0f);
    uniforms_.iGlobalTime = (float)seconds_elapsed_;
    uniforms_.iTimeDelta = (float)dt;
    uniforms_.iFrame = frame_index_;
    uniforms_.iMouse = mathfu::vec4((float)mouse_x, (float)mouse_y, 0.0f, 0.0f);  // TODO(cort): mouse click tracking is TBI
    uniforms_.iDate = mathfu::vec4(year, month, mday, dsec);
    uniforms_.iSampleRate = 44100.0f;
    uniform_buffer_.Load(device_context_, pframe_index_, &uniforms_, sizeof(uniforms_));
  }

  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override {
    blitter_.NextPframe();
    VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
    render_pass_.begin_info.framebuffer = framebuffer;
    render_pass_.begin_info.renderArea.extent = swapchain_extent_;
    vkCmdBeginRenderPass(primary_cb, &render_pass_.begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.handle);
    vkCmdSetViewport(primary_cb, 0,1, &viewport_);
    vkCmdSetScissor(primary_cb, 0,1, &scissor_rect_);
    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipeline_.shader_pipeline->pipeline_layout,
      0, 1, &dsets_[pframe_index_], 0, nullptr);
    vkCmdDraw(primary_cb, 3, 1,0,0);
    vkCmdEndRenderPass(primary_cb);
  }

protected:
  void HandleWindowResize(VkExtent2D new_window_extent) override {
    Application::HandleWindowResize(new_window_extent);

    for(auto fb : framebuffers_) {
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
      VK_NULL_HANDLE, // filled in below
    };
    VkFramebufferCreateInfo framebuffer_ci = render_pass_.GetFramebufferCreateInfo(extent);
    framebuffer_ci.pAttachments = attachment_views.data();
    framebuffers_.resize(swapchain_image_views_.size());
    for(size_t i=0; i<swapchain_image_views_.size(); ++i) {
      attachment_views[0] = swapchain_image_views_[i];
      SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &framebuffers_[i]));
    }
  }

  void ReloadShader() {
    shaderc::SpvCompilationResult compile_result = shader_compiler_.CompileGlslFile(frag_shader_path);
    if (compile_result.GetCompilationStatus() == shaderc_compilation_status_success) {
      Shader new_fs;
      SPOKK_VK_CHECK(new_fs.CreateAndLoadCompileResult(device_context_, compile_result));
      ShaderPipeline new_shader_pipeline;
      SPOKK_VK_CHECK(new_shader_pipeline.AddShader(&fullscreen_tri_vs_));
      SPOKK_VK_CHECK(new_shader_pipeline.AddShader(&new_fs));
      SPOKK_VK_CHECK(new_shader_pipeline.Finalize(device_context_));
      GraphicsPipeline new_pipeline;
      SPOKK_VK_CHECK(new_pipeline.Create(device_context_,
        MeshFormat::GetEmpty(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
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
  void WatchShaderDir(const std::string dir_path) {
#ifdef _MSC_VER  // Detect changes using Windows change notification API
    std::wstring wpath(dir_path.begin(), dir_path.end()); // only works for ASCII input, but I'm okay with that.
    HANDLE dwChangeHandle = FindFirstChangeNotification(wpath.c_str(), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
    ZOMBO_ASSERT(dwChangeHandle != INVALID_HANDLE_VALUE, "FindFirstChangeNotification() returned invalid handle");
    int lastUpdateSeconds = -1;
    for(;;) {
      DWORD dwWaitStatus = WaitForSingleObject(dwChangeHandle, INFINITE);
      SYSTEMTIME localTime;
      GetLocalTime(&localTime);
      // Only reload at most once per second
      if (!swap_shader_ && dwWaitStatus == WAIT_OBJECT_0 && localTime.wSecond != lastUpdateSeconds) {
        zomboSleepMsec(20); // Reloading immediately doesn't work; fopen() fails. Give it a little bit to resolve itself.
                           // Attempt to rebuild the shader and pipeline first
        ReloadShader();
        // Don't reload more than once a second
        lastUpdateSeconds = localTime.wSecond;
      }
      FindNextChangeNotification(dwChangeHandle);
    }
#elif defined(__linux__)  // Detect changes using inotify
    int fd = inotify_init();
    ZOMBO_ASSERT(fd != -1, "inotify_init() failed (errno=%d)", errno);
    int wd = inotify_add_watch(fd, dir_path.c_str(), IN_MODIFY | IN_MOVED_TO);
    ZOMBO_ASSERT(wd != -1, "inotify_add_watch() failed (errno=%d)", errno);
    for(;;) {
      // TODO(cort): Many text editors will "modify" a file as a write to a temp file (IN_MODIFY), followed by
      // a rename to the original file (IN_MOVED_TO). We shouldn't reload a shader until the rename. I think this
      // is currently handled by the zomboSleepMsec() below, but it seems janky.
      uint8_t event_buffer[sizeof(inotify_event) + NAME_MAX + 1] = {};
      ssize_t event_bytes = read(fd, event_buffer, sizeof(inotify_event) + NAME_MAX + 1);
      ZOMBO_ASSERT(event_bytes >= sizeof(inotify_event), "inotify event read failed (errno=%d)", errno);
      int32_t event_offset = 0;
      while(event_offset < event_bytes) {
        inotify_event *event = reinterpret_cast<inotify_event*>(event_buffer + event_offset);
        if (event->wd != wd) {
          continue;
        }
        // printf("%s: 0x%08X\n", event->name, event->mask);
        if ((event->mask & IN_MODIFY) != 0) {
          ReloadShader();
          sleep(1);  // only process one reload per second.
        } else if ((event->mask & (IN_IGNORED | IN_UNMOUNT | IN_Q_OVERFLOW)) != 0) {
          ZOMBO_ERROR("inotify event mask (0x%08X) indicates something awful is afoot!", event->mask);
        }
        event_offset += sizeof(inotify_event) + event->len;
      }
    }
    inotify_rm_watch(fd, wd);
    close(fd);
#else
#error Unsupported platform! Find the equivalent of inotify on your platform!
#endif
  }

  double seconds_elapsed_;

  std::atomic_bool swap_shader_;
  std::thread shader_reloader_thread_;
  ShaderCompiler shader_compiler_;
  shaderc::CompileOptions compiler_options_;

  ImageBlitter blitter_;
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
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

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
  app_ci.debug_report_flags |= VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
  app_ci.queue_family_requests = queue_requests;
  app_ci.pfn_set_device_features = EnableMinimumDeviceFeatures;

  ShaderToyApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
