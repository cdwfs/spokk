#include "midi.h"

#include <spokk.h>
using namespace spokk;

#include <common/camera.h>
#include <common/cube_mesh.h>

#include <imgui.h>

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
  struct FrameData {
    VkDescriptorSet dset;
    Buffer scene_ubo;
    Buffer tv_ubo;
    Buffer heightfield_buffer;
    Buffer visible_cells_buffer;
  };
  std::array<FrameData, PFRAME_COUNT> frame_data_;

  Mesh mesh_;

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

  for (const auto& dset_layout_ci : pillar_shader_program_.dset_layout_cis) {
    dpool_.Add(dset_layout_ci, PFRAME_COUNT);
  }
  SPOKK_VK_CHECK(dpool_.Finalize(device_));

  // Create swapchain-sized buffers. This must happen before dset writing, since the render buffer image views
  // are referenced.
  CreateRenderBuffers(swapchain_extent_);

  // Look up the appropriate memory flags for uniform buffers on this platform
  VkMemoryPropertyFlags uniform_buffer_memory_flags =
      device_.MemoryFlagsForAccessPattern(DEVICE_MEMORY_ACCESS_PATTERN_CPU_TO_GPU_DYNAMIC);

  DescriptorSetWriter dset_writer(pillar_shader_program_.dset_layout_cis[0]);
  dset_writer.BindImage(
      albedo_tex_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, pillar_fs_.GetDescriptorBindPoint("tex").binding);
  dset_writer.BindSampler(sampler_, pillar_fs_.GetDescriptorBindPoint("samp").binding);
  dset_writer.BindImage(
      color_target_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, film_fs_.GetDescriptorBindPoint("fbColor").binding);
  dset_writer.BindImage(
      depth_image_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, film_fs_.GetDescriptorBindPoint("fbDepth").binding);
  dset_writer.BindSampler(sampler_, film_fs_.GetDescriptorBindPoint("fbSamp").binding);
  for (auto& frame_data : frame_data_) {
    // Create pipelined buffer of shader uniforms
    VkBufferCreateInfo scene_uniform_buffer_ci = {};
    scene_uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    scene_uniform_buffer_ci.size = sizeof(SceneUniforms);
    scene_uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    scene_uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(frame_data.scene_ubo.Create(device_, scene_uniform_buffer_ci, uniform_buffer_memory_flags));
    dset_writer.BindBuffer(frame_data.scene_ubo.Handle(), pillar_vs_.GetDescriptorBindPoint("scene_consts").binding);

    VkBufferCreateInfo tv_uniform_buffer_ci = {};
    tv_uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    tv_uniform_buffer_ci.size = sizeof(TvUniforms);
    tv_uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    tv_uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(frame_data.tv_ubo.Create(device_, tv_uniform_buffer_ci, uniform_buffer_memory_flags));
    dset_writer.BindBuffer(frame_data.tv_ubo.Handle(), film_fs_.GetDescriptorBindPoint("tv_consts").binding);

    // Create buffer of per-cell "height" values
    VkBufferCreateInfo heightfield_buffer_ci = {};
    heightfield_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    heightfield_buffer_ci.size = HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY * sizeof(float);
    heightfield_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    heightfield_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(frame_data.heightfield_buffer.Create(device_, heightfield_buffer_ci, uniform_buffer_memory_flags));
    SPOKK_VK_CHECK(frame_data.heightfield_buffer.CreateView(device_, VK_FORMAT_R32_SFLOAT));
    dset_writer.BindTexelBuffer(
        frame_data.heightfield_buffer.View(), pillar_vs_.GetDescriptorBindPoint("cell_heights").binding);

    // Create lookup table from instance index [0..visible_cell_count_] to cell index.
    VkBufferCreateInfo visible_cells_buffer_ci = {};
    visible_cells_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    visible_cells_buffer_ci.size = HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY * sizeof(uint32_t);
    visible_cells_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    visible_cells_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(
        frame_data.visible_cells_buffer.Create(device_, visible_cells_buffer_ci, uniform_buffer_memory_flags));
    SPOKK_VK_CHECK(frame_data.visible_cells_buffer.CreateView(device_, VK_FORMAT_R32_SINT));
    dset_writer.BindTexelBuffer(
        frame_data.visible_cells_buffer.View(), pillar_vs_.GetDescriptorBindPoint("visible_cells").binding);

    frame_data.dset = dpool_.AllocateSet(device_, pillar_shader_program_.dset_layouts[0]);
    dset_writer.WriteAll(device_, frame_data.dset);
  }

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

  // initialize heightfield and visible cells
  for (int32_t iY = 0; iY < HEIGHTFIELD_DIMY; ++iY) {
    for (int32_t iX = 0; iX < HEIGHTFIELD_DIMX; ++iX) {
      heightfield_.at(XY_TO_CELL(iX, iY)) = -1.0f;  // non-visible cells have negative heights
    }
  }
  visible_cells_.reserve(HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY);
}

TvApp::~TvApp() {
  if (device_) {
    vkDeviceWaitIdle(device_);

    dpool_.Destroy(device_);

    for (auto& frame_data : frame_data_) {
      frame_data.scene_ubo.Destroy(device_);
      frame_data.tv_ubo.Destroy(device_);
      frame_data.visible_cells_buffer.Destroy(device_);
      frame_data.heightfield_buffer.Destroy(device_);
    }

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

struct MidiDeviceInfo {
  uint32_t id;
  std::string name;
};
struct MidiMessageLog {
  ImGuiTextBuffer Buf;
  ImGuiTextFilter Filter;
  ImVector<int> LineOffsets;  // Index to lines offset. We maintain this with AddLog() calls, allowing us to have a
                              // random access on lines
  bool ScrollToBottom;

  void Clear() {
    Buf.clear();
    LineOffsets.clear();
    LineOffsets.push_back(0);
  }

  void AddLog(const char* fmt, ...) IM_FMTARGS(2) {
    int old_size = Buf.size();
    va_list args;
    va_start(args, fmt);
    Buf.appendfv(fmt, args);
    va_end(args);
    for (int new_size = Buf.size(); old_size < new_size; old_size++)
      if (Buf[old_size] == '\n') LineOffsets.push_back(old_size + 1);
    ScrollToBottom = true;
  }

  void Draw() {
    if (ImGui::Button("Clear")) Clear();
    ImGui::SameLine();
    bool copy = ImGui::Button("Copy");
    ImGui::SameLine();
    Filter.Draw("Filter", -100.0f);
    ImGui::Separator();
    ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    if (copy) ImGui::LogToClipboard();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char* buf = Buf.begin();
    const char* buf_end = Buf.end();
    if (Filter.IsActive()) {
      for (int line_no = 0; line_no < LineOffsets.Size; line_no++) {
        const char* line_start = buf + LineOffsets[line_no];
        const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
        if (Filter.PassFilter(line_start, line_end)) ImGui::TextUnformatted(line_start, line_end);
      }
    } else {
      ImGuiListClipper clipper;
      clipper.Begin(LineOffsets.Size);
      while (clipper.Step()) {
        for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++) {
          const char* line_start = buf + LineOffsets[line_no];
          const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
          ImGui::TextUnformatted(line_start, line_end);
        }
      }
      clipper.End();
    }
    ImGui::PopStyleVar();

    if (ScrollToBottom) ImGui::SetScrollHereY(1.0f);
    ScrollToBottom = false;
    ImGui::EndChild();
  }
};

struct MidiTweakable {
  std::string name;
  int channel;
  uint8_t raw_value;
};

void TvApp::Update(double dt) {
  seconds_elapsed_ += dt;
  drone_->Update(input_state_, (float)dt);

  ImGui::Begin("MIDI");
  static bool first_Time = true;
  static std::vector<MidiDeviceInfo> midi_device_info;
  static std::vector<MidiTweakable> midi_tweakables = {
      {"Coarse Distortion", 0, 0},
      {"Fine Distortion", 0, 0},
      {"Distortion Speed", 0, 0},
      {"Roll Speed", 0, 0},
      {"RGB Shift Amount", 0, 0},
  };
  static MidiTweakable* detecting_tweakable = nullptr;
  static MidiMessageLog midi_log;

  if (ImGui::TreeNode("Devices")) {
    for (const auto& device : midi_device_info) {
      ImGui::Text("%08X: %s", device.id, device.name.c_str());
    }
    if (ImGui::Button("Refresh")) {
      MidiJackRefreshEndpoints();
      int midi_device_count = MidiJackCountEndpoints();
      midi_device_info.resize(midi_device_count);
      for (int i_device = 0; i_device < midi_device_count; ++i_device) {
        uint32_t device_id = MidiJackGetEndpointIDAtIndex(i_device);
        midi_device_info[i_device].id = device_id;
        midi_device_info[i_device].name = MidiJackGetEndpointName(device_id);
      }
    }
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Messages")) {
    midi_log.Draw();
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Channel Map")) {
    for (auto& tweakable : midi_tweakables) {
      ImGui::Text(tweakable.name.c_str());
      ImGui::SameLine();
      ImGui::PushItemWidth(200);
      ImGui::InputInt("Channel", &tweakable.channel);
      ImGui::PopItemWidth();
      ImGui::SameLine();
      if (detecting_tweakable != nullptr) {
        if (detecting_tweakable == &tweakable) {
          // In detecting mode; change the "Detect" button to "Cancel" to escape
          if (ImGui::Button("Cancel")) {
            detecting_tweakable = nullptr;
          }
        } else {
          // Disable detect of other items while something is already being detected
          ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
          ImGui::Button((std::string("Detect##") + tweakable.name).c_str());
          ImGui::PopStyleVar();
        }
      } else {
        if (ImGui::Button((std::string("Detect##") + tweakable.name).c_str())) {
          ZOMBO_ASSERT(detecting_tweakable == nullptr, "Something's already being detected");
          detecting_tweakable = &tweakable;
        }
      }
    }
    ImGui::TreePop();
  }
  ImGui::End();

  // Process MIDI messages for this "frame"
  do {
    uint64_t msg = MidiJackDequeueIncomingData();
    if (msg == 0) {
      break;  // no more data to dequeue
    }
    // process msg here
    uint32_t source = (uint32_t)((msg >> 0) & 0xFFFFFFFF);
    uint8_t status = (uint8_t)((msg >> 32) & 0xFF);
    uint8_t data1 = (uint8_t)((msg >> 40) & 0xFF);
    uint8_t data2 = (uint8_t)((msg >> 48) & 0xFF);
    midi_log.AddLog("%08X: %02X %02X %02X\n", source, status, data1, data2);
    if (status == 0xB0) {
      uint8_t channel = data1;
      if (detecting_tweakable) {
        detecting_tweakable->channel = channel;
        detecting_tweakable = nullptr;
      }

      float value01 = (float)data2 / 127.0f;
      if (channel == midi_tweakables[4].channel) {
        tv_params_.rgb_shift_params.x = value01;
      } else if (channel == 0x10) {
        tv_params_.rgb_shift_params.y = value01;
      } else if (channel == 0x20) {
        tv_params_.film_params.w = value01;
      }

      if (channel == midi_tweakables[0].channel) {
        tv_params_.distort_params.x = 20.0f * value01;
      } else if (channel == midi_tweakables[1].channel) {
        tv_params_.distort_params.y = 20.0f * value01;
      } else if (channel == midi_tweakables[2].channel) {
        tv_params_.distort_params.z = (float)(2.0 * M_PI * value01);
      } else if (channel == midi_tweakables[3].channel) {
        tv_params_.distort_params.w = value01;
      }

      if (channel == 0x02) {
        tv_params_.film_params.x = 2.0f * value01;
      } else if (channel == 0x03) {
        tv_params_.film_params.y = 2.0f * value01;
      }
    }
  } while (true);

  // Tweakable Bad TV effects settings
  const ImGuiColorEditFlags default_color_edit_flags = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel;
  if (ImGui::TreeNode("Bad TV")) {
    ImGui::Text("Distortion:");
    ImGui::SliderFloat("Coarse##Distortion", &tv_params_.distort_params.x, 0.0f, 20.0f);
    ImGui::SliderFloat("Fine##Distortion", &tv_params_.distort_params.y, 0.0f, 20.0f);
    ImGui::SliderFloat("Distortion Speed##Distortion", &tv_params_.distort_params.z, 0.0f, 1.0f);
    ImGui::SliderFloat("Roll Speed##Distortion", &tv_params_.distort_params.w, 0.0f, 1.0f);

    ImGui::Separator();
    ImGui::Text("RGB Shift:");
    ImGui::SliderFloat("Amount##RgbShift", &tv_params_.rgb_shift_params.x, 0.0f, 1.0f);
    ImGui::SliderAngle("Angle##RgbShift", &tv_params_.rgb_shift_params.y, 0, 360.0f, "%.2f deg");

    int scanline_count = (int)tv_params_.film_params.z;
    bool enable_grayscale = tv_params_.film_params.w != 0.0f;
    ImGui::Separator();
    ImGui::Text("Scanlines:");
    ImGui::SliderFloat("Noise Intensity##Scanlines", &tv_params_.film_params.x, 0.0f, 2.0f);
    ImGui::SliderFloat("Scanline Intensity##Scanlines", &tv_params_.film_params.y, 0.0f, 2.0f);
    ImGui::SliderInt("Scanline Count##Scanlines", &scanline_count, 50, 1000);
    ImGui::Checkbox("Convert to B+W?", &enable_grayscale);
    tv_params_.film_params.z = (float)scanline_count;
    tv_params_.film_params.w = enable_grayscale ? 1.0f : 0.0f;

    ImGui::Separator();
    ImGui::Text("Snow:");
    ImGui::SliderFloat("Amount##Snow", &tv_params_.snow_params.x, 0.0f, 1.0f);
    ImGui::SliderFloat("Size##Snow", &tv_params_.snow_params.y, 0.0f, 100.0f);
    ImGui::TreePop();
  }

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
  const auto& frame_data = frame_data_[pframe_index_];
  // Update uniforms
  SceneUniforms* scene_consts = (SceneUniforms*)frame_data.scene_ubo.Mapped();
  scene_consts->res_and_time =
      glm::vec4((float)swapchain_extent_.width, (float)swapchain_extent_.height, 0, (float)seconds_elapsed_);
  scene_consts->eye = glm::vec4(camera_->getEyePoint(), 1.0f);
  glm::mat4 w2v = camera_->getViewMatrix();
  const glm::mat4 proj = camera_->getProjectionMatrix();
  scene_consts->viewproj = proj * w2v;
  SPOKK_VK_CHECK(frame_data.scene_ubo.FlushHostCache(device_));

  TvUniforms* tv_consts = (TvUniforms*)frame_data.tv_ubo.Mapped();
  *tv_consts = tv_params_;
  SPOKK_VK_CHECK(frame_data.tv_ubo.FlushHostCache(device_));

  memcpy(frame_data.visible_cells_buffer.Mapped(), visible_cells_.data(), visible_cells_.size() * sizeof(int32_t));
  SPOKK_VK_CHECK(frame_data.visible_cells_buffer.FlushHostCache(device_));
  memcpy(frame_data.heightfield_buffer.Mapped(), heightfield_.data(), heightfield_.size() * sizeof(float));
  SPOKK_VK_CHECK(frame_data.heightfield_buffer.FlushHostCache(device_));

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
        pillar_pipeline_.shader_program->pipeline_layout, 0, 1, &frame_data.dset, 0, nullptr);
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
  color_target_image_ci.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;  // This image will be sampled by post_render_pass_
  color_target_ = {};
  SPOKK_VK_CHECK(color_target_.Create(
      device_, color_target_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DEVICE_ALLOCATION_SCOPE_DEVICE));
  SPOKK_VK_CHECK(device_.SetObjectName(color_target_.handle, "color target image"));
  SPOKK_VK_CHECK(device_.SetObjectName(color_target_.view, "color target image view"));

  // Create depth buffer
  VkImageCreateInfo depth_image_ci = scene_render_pass_.GetAttachmentImageCreateInfo(1, extent);
  depth_image_ci.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;  // This image will be sampled by post_render_pass_
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

  MidiJackRefreshEndpoints();
  int endpointCount = MidiJackCountEndpoints();
  printf("Detected %d endpoints:\n", endpointCount);
  for (int i = 0; i < endpointCount; ++i) {
    uint32_t id = MidiJackGetEndpointIDAtIndex(i);
    const std::string name = MidiJackGetEndpointName(id);
    printf("- %3d: 0x%016X %s\n", i, id, name.c_str());
  }

  std::vector<Application::QueueFamilyRequest> queue_requests = {
      {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}};
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;
  app_ci.pfn_set_device_features = EnableMinimumDeviceFeatures;

  TvApp app(app_ci);
  int run_error = app.Run();

  MidiJackShutdown();

  return run_error;
}
