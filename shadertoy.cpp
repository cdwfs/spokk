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
// This block of code is inserted in front of every shadertoy fragment shader; it defines the
// uniform variables and invokes the shader's mainImage() function on every pixel.
const std::string frag_shader_preamble = R"glsl(#version 450
#pragma shader_stage(fragment)
layout (location = 0) out vec4 out_fragColor;
in vec4 gl_FragCoord;

// input channel.
layout (set = 0, binding = 0) uniform sampler%s iChannel0;
layout (set = 0, binding = 1) uniform sampler%s iChannel1;
layout (set = 0, binding = 2) uniform sampler%s iChannel2;
layout (set = 0, binding = 3) uniform sampler%s iChannel3;
layout (set = 0, binding = 4) uniform ShaderToyUniforms {
  vec3      iResolution;           // viewport resolution (in pixels)
  float     iChannelTime[4];       // channel playback time (in seconds)
  vec3      iChannelResolution[4]; // channel resolution (in pixels)
  vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
  vec4      iDate;                 // (year, month, day, time in seconds)
  float     iGlobalTime;           // shader playback time (in seconds)
  float     iTimeDelta;            // render time (in seconds)
  int       iFrame;                // shader playback frame
  float     iSampleRate;           // sound sample rate (i.e., 44100)
};

void mainImage(out vec4 fragColor, in vec2 fragCoord);
void main() {
  mainImage(out_fragColor, gl_FragCoord.xy);
  out_fragColor.w = 1.0;
}

////////////////////////////////////////////////////////////////////////////////////////////////

)glsl";

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

mathfu::vec2 click_pos(0,0);
void MyGlfwMouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) {
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    double mouse_x, mouse_y;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);
    click_pos = mathfu::vec2( (float)mouse_x, (float)mouse_y );
  }
}

}  // namespace

class ShaderToyApp : public spokk::Application {
public:
  explicit ShaderToyApp(Application::CreateInfo &ci) :
      Application(ci) {
    seconds_elapsed_ = 0;

    mouse_pos_ = mathfu::vec2(0,0);
    glfwSetMouseButtonCallback(window_.get(), MyGlfwMouseButtonCallback);

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
    const VkDeviceSize blit_buffer_nbytes = 16*1024*1024;
    SPOKK_VK_CHECK(blitter_.Create(device_context_, PFRAME_COUNT, blit_buffer_nbytes));
    for(size_t i=0; i<textures_.size(); ++i) {
      char filename[17];
      zomboSnprintf(filename, 17, "data/tex%02u.ktx", (uint32_t)i);
      ZOMBO_ASSERT(0 == textures_[i].CreateFromFile(device_context_, blitter_, graphics_and_present_queue_, filename, VK_FALSE),
        "Failed to load %s", filename);
    }
    for(size_t i=0; i<cubemaps_.size(); ++i) {
      char filename[18];
      zomboSnprintf(filename, 18, "data/cube%02u.ktx", (uint32_t)i);
      ZOMBO_ASSERT(0 == cubemaps_[i].CreateFromFile(device_context_, blitter_, graphics_and_present_queue_, filename, VK_FALSE),
        "Failed to load %s", filename);
    }
    active_images_[0] = &textures_[15];
    active_images_[1] = &cubemaps_[2];
    active_images_[2] = &textures_[2];
    active_images_[3] = &textures_[3];

    // Load shader pipelines
    SPOKK_VK_CHECK(fullscreen_tri_vs_.CreateAndLoadSpirvFile(device_context_, "fullscreen.vert.spv"));

    active_pipeline_index_ = 0;
    ReloadShader();  // force a reload of of the shadertoy shader into slot 1
    active_pipeline_index_ = 1 - active_pipeline_index_;

    // Create uniform buffer
    VkBufferCreateInfo uniform_buffer_ci = {};
    uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    uniform_buffer_ci.size = sizeof(ShaderToyUniforms);
    uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(uniform_buffer_.Create(device_context_, PFRAME_COUNT, uniform_buffer_ci));

    for(const auto& dset_layout_ci : shader_programs_[active_pipeline_index_].dset_layout_cis) {
      dpool_.Add(dset_layout_ci, PFRAME_COUNT);
    }
    SPOKK_VK_CHECK(dpool_.Finalize(device_context_));
    
    // Create swapchain-sized resources.
    CreateRenderBuffers(swapchain_extent_);
    
    DescriptorSetWriter dset_writer(shader_programs_[active_pipeline_index_].dset_layout_cis[0]);
    for(size_t iTex = 0; iTex < active_images_.size(); ++iTex) {
      dset_writer.BindCombinedImageSampler(active_images_[iTex]->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        samplers_[iTex], (uint32_t)iTex);
    }
    for(uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) {
      dsets_[pframe] = dpool_.AllocateSet(device_context_, shader_programs_[active_pipeline_index_].dset_layouts[0]);
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
      shader_reloader_thread_.detach();  // TODO(https://github.com/cdwfs/spokk/issues/15) Getting occasional crahes here; graceful exit?

      vkDeviceWaitIdle(device_);

      dpool_.Destroy(device_context_);

      uniform_buffer_.Destroy(device_context_);

      pipelines_[0].Destroy(device_context_);
      pipelines_[1].Destroy(device_context_);

      shader_programs_[0].Destroy(device_context_);
      shader_programs_[1].Destroy(device_context_);
      fullscreen_tri_vs_.Destroy(device_context_);
      fragment_shaders_[0].Destroy(device_context_);
      fragment_shaders_[1].Destroy(device_context_);

      for(const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
      render_pass_.Destroy(device_context_);

      for(auto& image : textures_) {
        image.Destroy(device_context_);
      }
      for(auto& cube : cubemaps_) {
        cube.Destroy(device_context_);
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
      pipelines_[active_pipeline_index_].Destroy(device_context_);
      shader_programs_[active_pipeline_index_].Destroy(device_context_);
      fragment_shaders_[active_pipeline_index_].Destroy(device_context_);
      active_pipeline_index_ = 1 - active_pipeline_index_;
      swap_shader_.store(false);
    }

    // Update uniforms
    double mouse_x = 0, mouse_y = 0;
    glfwGetCursorPos(window_.get(), &mouse_x, &mouse_y);
    if (glfwGetMouseButton(window_.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
      mouse_pos_ = mathfu::vec2((float)mouse_x, (float)mouse_y);
    }

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
    uniforms_.iChannelResolution[0] = mathfu::vec4(
      (float)active_images_[0]->image_ci.extent.width,
      (float)active_images_[0]->image_ci.extent.height,
      (float)active_images_[0]->image_ci.extent.depth, 0.0f);
    uniforms_.iChannelResolution[1] = mathfu::vec4(
      (float)active_images_[1]->image_ci.extent.width,
      (float)active_images_[1]->image_ci.extent.height,
      (float)active_images_[1]->image_ci.extent.depth, 0.0f);
    uniforms_.iChannelResolution[2] = mathfu::vec4(
      (float)active_images_[2]->image_ci.extent.width,
      (float)active_images_[2]->image_ci.extent.height,
      (float)active_images_[2]->image_ci.extent.depth, 0.0f);
    uniforms_.iChannelResolution[3] = mathfu::vec4(
      (float)active_images_[3]->image_ci.extent.width,
      (float)active_images_[3]->image_ci.extent.height,
      (float)active_images_[3]->image_ci.extent.depth, 0.0f);
    uniforms_.iGlobalTime = (float)seconds_elapsed_;
    uniforms_.iTimeDelta = (float)dt;
    uniforms_.iFrame = frame_index_;
    uniforms_.iMouse = mathfu::vec4(mouse_pos_.x, mouse_pos_.y, click_pos.x, click_pos.y);
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
    vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_[active_pipeline_index_].handle);
    vkCmdSetViewport(primary_cb, 0,1, &viewport_);
    vkCmdSetScissor(primary_cb, 0,1, &scissor_rect_);
    vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelines_[active_pipeline_index_].shader_program->pipeline_layout,
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
    const char* image_types[4] = {
      (active_images_[0]->image_ci.arrayLayers == 1) ? "2D" : "Cube",
      (active_images_[1]->image_ci.arrayLayers == 1) ? "2D" : "Cube",
      (active_images_[2]->image_ci.arrayLayers == 1) ? "2D" : "Cube",
      (active_images_[3]->image_ci.arrayLayers == 1) ? "2D" : "Cube",
    };
    int preamble_len = snprintf(nullptr, 0, frag_shader_preamble.c_str(),
      image_types[0], image_types[1], image_types[2], image_types[3]);
    FILE *frag_file = zomboFopen(frag_shader_path.c_str(), "rb");
    if (!frag_file) {
      // Load failed -- try again in a moment?
      zomboSleepMsec(1);
      return;
    }
    fseek(frag_file, 0, SEEK_END);
    size_t frag_file_bytes = ftell(frag_file);
    // TODO(https://github.com/cdwfs/spokk/issues/15): potential race condition here, if the file is modified between ftell and fread()
    std::vector<char> final_frag_source(preamble_len + frag_file_bytes);
    zomboSnprintf(final_frag_source.data(), preamble_len, frag_shader_preamble.c_str(),
      image_types[0], image_types[1], image_types[2], image_types[3]);
    fseek(frag_file, 0, SEEK_SET);
    size_t bytes_read = fread(final_frag_source.data() + preamble_len - 1, 1, frag_file_bytes, frag_file);
    fclose(frag_file);
    if (bytes_read != frag_file_bytes) {
      // let's assume this means the file has changed
      printf("Shader file changed in mid-reload; save again to be safe.\n");
      return;
    }
    final_frag_source[final_frag_source.size()-1] = 0;
    shaderc::SpvCompilationResult compile_result = shader_compiler_.CompileGlslString(final_frag_source.data(),
      frag_shader_path, "main", VK_SHADER_STAGE_FRAGMENT_BIT);
    if (compile_result.GetCompilationStatus() == shaderc_compilation_status_success) {
      Shader& new_fs = fragment_shaders_[1 - active_pipeline_index_];
      SPOKK_VK_CHECK(new_fs.CreateAndLoadCompileResult(device_context_, compile_result));
      ShaderProgram& new_shader_program = shader_programs_[1 - active_pipeline_index_];
      SPOKK_VK_CHECK(new_shader_program.AddShader(&fullscreen_tri_vs_));
      SPOKK_VK_CHECK(new_shader_program.AddShader(&new_fs));
      SPOKK_VK_CHECK(new_shader_program.Finalize(device_context_));
      GraphicsPipeline& new_pipeline = pipelines_[1 - active_pipeline_index_];
      new_pipeline.Init(MeshFormat::GetEmpty(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
        &new_shader_program, &render_pass_, 0);
      SPOKK_VK_CHECK(new_pipeline.Finalize(device_context_));
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
      // TODO(https://github.com/cdwfs/spokk/issues/15): Potential race condition here.
      // Many text editors will "modify" a file as a write to a temp file (IN_MODIFY), followed by
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
  std::array<Image, 16> textures_;
  std::array<Image, 6> cubemaps_;
  std::array<Image*, 4> active_images_;
  std::array<VkSampler,4> samplers_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  Shader fullscreen_tri_vs_;

  // Two copies of all this stuff, so we can ping-pong between them during reloads.
  uint32_t active_pipeline_index_;
  std::array<Shader, 2> fragment_shaders_;
  std::array<ShaderProgram, 2> shader_programs_;
  std::array<GraphicsPipeline, 2> pipelines_;

  VkViewport viewport_;
  VkRect2D scissor_rect_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

  ShaderToyUniforms uniforms_;
  mathfu::vec2 mouse_pos_;
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
