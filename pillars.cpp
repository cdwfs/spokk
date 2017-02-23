#include "vk_application.h"
#include "vk_debug.h"
using namespace spokk;

#include "camera.h"
#include "cube_mesh.h"

#include <mathfu/vector.h>
#include <mathfu/glsl_mappings.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {
struct MeshUniforms {
  mathfu::vec4_packed time_and_res;  // x: elapsed seconds, yz: viewport resolution in pixels
  mathfu::vec4_packed eye;  // xyz: eye position
  mathfu::mat4 viewproj;
};
}  // namespace

#define HEIGHTFIELD_DIMX 256
#define HEIGHTFIELD_DIMY 256
#define XY_TO_CELL(x,y) ((y)*HEIGHTFIELD_DIMX + (x))
#define CELL_X(cell) ((cell) % HEIGHTFIELD_DIMX)
#define CELL_Y(cell) ((cell) / HEIGHTFIELD_DIMX)

#define EFFECT_RADIUS 9
#define VISIBLE_RADIUS ((EFFECT_RADIUS) + 1)

class PillarsApp : public spokk::Application {
public:
  explicit PillarsApp(Application::CreateInfo &ci);
  virtual ~PillarsApp();
  PillarsApp(const PillarsApp&) = delete;
  const PillarsApp& operator=(const PillarsApp&) = delete;

  void Update(double dt) override;
  void Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) override;

private:
  double seconds_elapsed_;

  Image depth_image_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  ImageBlitter blitter_;
  Image albedo_tex_;
  VkSampler sampler_;

  Shader pillar_vs_, pillar_fs_;
  ShaderPipeline pillar_shader_pipeline_;
  GraphicsPipeline pillar_pipeline_;

  DescriptorPool dpool_;
  std::array<VkDescriptorSet, PFRAME_COUNT> dsets_;

  MeshFormat mesh_format_;
  Mesh mesh_;
  PipelinedBuffer mesh_uniforms_;
  PipelinedBuffer heightfield_buffer_;
  PipelinedBuffer visible_cells_buffer_;

  std::vector<int32_t> visible_cells_;
  std::array<float, HEIGHTFIELD_DIMX*HEIGHTFIELD_DIMY> heightfield_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDolly> dolly_;
};

PillarsApp::PillarsApp(Application::CreateInfo &ci) :
    Application(ci) {
  glfwSetInputMode(window_.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  seconds_elapsed_ = 0;

  const float fovDegrees = 45.0f;
  const float zNear = 0.01f;
  const float zFar = 100.0f;
  camera_ = my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, fovDegrees, zNear, zFar);
  const mathfu::vec3 initial_camera_pos(HEIGHTFIELD_DIMX/2, 2.0f, HEIGHTFIELD_DIMY/2);
  const mathfu::vec3 initial_camera_target(0, 0, 0);
  const mathfu::vec3 initial_camera_up(0,1,0);
  camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
  dolly_ = my_make_unique<CameraDolly>(*camera_);
  dolly_->SetBounds(mathfu::vec3(VISIBLE_RADIUS, 1, VISIBLE_RADIUS),
    mathfu::vec3(HEIGHTFIELD_DIMX-VISIBLE_RADIUS-1, 30, HEIGHTFIELD_DIMY-VISIBLE_RADIUS-1));

  // Create render pass
  render_pass_.InitFromPreset(RenderPass::Preset::COLOR_DEPTH, swapchain_surface_format_.format);
  SPOKK_VK_CHECK(render_pass_.Finalize(device_context_));

  // Create depth buffer
  VkImageCreateInfo depth_image_ci = render_pass_.GetAttachmentImageCreateInfo(1, swapchain_extent_);
  depth_image_ = {};
  SPOKK_VK_CHECK(depth_image_.Create(device_context_, depth_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    DEVICE_ALLOCATION_SCOPE_DEVICE));

  // Create VkFramebuffers
  std::vector<VkImageView> attachment_views = {
    VK_NULL_HANDLE, // filled in below
    depth_image_.view,
  };
  VkFramebufferCreateInfo framebuffer_ci = render_pass_.GetFramebufferCreateInfo(swapchain_extent_);
  framebuffer_ci.pAttachments = attachment_views.data();
  framebuffers_.resize(swapchain_image_views_.size());
  for(size_t i=0; i<swapchain_image_views_.size(); ++i) {
    attachment_views.at(0) = swapchain_image_views_[i];
    SPOKK_VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &framebuffers_[i]));
  }

  // Load textures and samplers
  VkSamplerCreateInfo sampler_ci = GetSamplerCreateInfo(VK_FILTER_LINEAR,
    VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
  SPOKK_VK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &sampler_));
  const VkDeviceSize blit_buffer_nbytes = 4*1024*1024;
  SPOKK_VK_CHECK(blitter_.Create(device_context_, PFRAME_COUNT, blit_buffer_nbytes));
  albedo_tex_.CreateFromFile(device_context_, blitter_, graphics_and_present_queue_, "trevor/redf.ktx");

  // Load shader pipelines
  SPOKK_VK_CHECK(pillar_vs_.CreateAndLoadSpirvFile(device_context_, "pillar.vert.spv"));
  SPOKK_VK_CHECK(pillar_fs_.CreateAndLoadSpirvFile(device_context_, "pillar.frag.spv"));
  SPOKK_VK_CHECK(pillar_shader_pipeline_.AddShader(&pillar_vs_));
  SPOKK_VK_CHECK(pillar_shader_pipeline_.AddShader(&pillar_fs_));
  SPOKK_VK_CHECK(pillar_shader_pipeline_.Finalize(device_context_));

  // Describe the mesh format.
  mesh_format_.vertex_buffer_bindings = {
    {0, 3+3+2, VK_VERTEX_INPUT_RATE_VERTEX},
  };
  mesh_format_.vertex_attributes = {
    {0, 0, VK_FORMAT_R8G8B8_SNORM, 0},
    {1, 0, VK_FORMAT_R8G8B8_SNORM, 3},
    {2, 0, VK_FORMAT_R8G8_UNORM, 6},
  };
  mesh_format_.Finalize(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  mesh_.mesh_format = &mesh_format_;

  // Create graphics pipelines
  SPOKK_VK_CHECK(pillar_pipeline_.Create(device_context_, mesh_.mesh_format, &pillar_shader_pipeline_, &render_pass_, 0));

  // Populate Mesh object
  mesh_.index_type = (sizeof(cube_indices[0]) == sizeof(uint32_t))
    ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
  mesh_.index_count = cube_index_count;
  // index buffer
  VkBufferCreateInfo index_buffer_ci = {};
  index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  index_buffer_ci.size = cube_index_count * sizeof(cube_indices[0]);
  index_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  index_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(mesh_.index_buffer.Create(device_context_, index_buffer_ci));
  SPOKK_VK_CHECK(mesh_.index_buffer.Load(device_context_, cube_indices, index_buffer_ci.size));
  // vertex buffer
  VkBufferCreateInfo vertex_buffer_ci = {};
  vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vertex_buffer_ci.size = cube_vertex_count * mesh_format_.vertex_buffer_bindings[0].stride;
  vertex_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  vertex_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  mesh_.vertex_buffers.resize(1);
  SPOKK_VK_CHECK(mesh_.vertex_buffers[0].Create(device_context_, vertex_buffer_ci));
  // Convert the vertex data from its original uncompressed format to its final format.
  // In a real application, this conversion would happen at asset build time.
  const VertexLayout src_vertex_layout = {
    {0, VK_FORMAT_R32G32B32_SFLOAT, 0},
    {1, VK_FORMAT_R32G32B32_SFLOAT, 12},
    {2, VK_FORMAT_R32G32_SFLOAT, 24},
  };
  const VertexLayout final_vertex_layout(mesh_format_, 0);
  std::vector<uint8_t> final_mesh_vertices(vertex_buffer_ci.size);
  int convert_error = ConvertVertexBuffer(cube_vertices, src_vertex_layout,
    final_mesh_vertices.data(), final_vertex_layout, cube_vertex_count);
  assert(convert_error == 0);
  (void)convert_error;
  SPOKK_VK_CHECK(mesh_.vertex_buffers[0].Load(device_context_,
    final_mesh_vertices.data(), vertex_buffer_ci.size));

  // Create pipelined buffer of shader uniforms
  VkBufferCreateInfo uniform_buffer_ci = {};
  uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  uniform_buffer_ci.size = sizeof(MeshUniforms);
  uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(mesh_uniforms_.Create(device_context_, PFRAME_COUNT, uniform_buffer_ci,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

  // Create buffer of per-cell "height" values
  VkBufferCreateInfo heightfield_buffer_ci = {};
  heightfield_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  heightfield_buffer_ci.size = HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY * sizeof(float);
  heightfield_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  heightfield_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(heightfield_buffer_.Create(device_context_, PFRAME_COUNT, heightfield_buffer_ci,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
  SPOKK_VK_CHECK(heightfield_buffer_.CreateViews(device_context_, VK_FORMAT_R32_SFLOAT));
  for(int32_t iY=0; iY<HEIGHTFIELD_DIMY; ++iY) {
    for(int32_t iX=0; iX<HEIGHTFIELD_DIMX; ++iX) {
      heightfield_.at(XY_TO_CELL(iX,iY)) = -1.0f;  // non-visible cells have negative heights
    }
  }

  // Create lookup table from instance index [0..visible_cell_count_] to cell index.
  visible_cells_.reserve(HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY);
  VkBufferCreateInfo visible_cells_buffer_ci = {};
  visible_cells_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  visible_cells_buffer_ci.size = HEIGHTFIELD_DIMX * HEIGHTFIELD_DIMY * sizeof(uint32_t);
  visible_cells_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  visible_cells_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(visible_cells_buffer_.Create(device_context_, PFRAME_COUNT, visible_cells_buffer_ci,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
  SPOKK_VK_CHECK(visible_cells_buffer_.CreateViews(device_context_, VK_FORMAT_R32_SINT));

  for(const auto& dset_layout_ci : pillar_shader_pipeline_.dset_layout_cis) {
    dpool_.Add(dset_layout_ci, PFRAME_COUNT);
  }
  SPOKK_VK_CHECK(dpool_.Finalize(device_context_));

  DescriptorSetWriter dset_writer(pillar_shader_pipeline_.dset_layout_cis[0]);
  dset_writer.BindImage(albedo_tex_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler_, 1);
  for(uint32_t pframe = 0; pframe < PFRAME_COUNT; ++pframe) { 
    // TODO(cort): allocate_pipelined_set()?
    dsets_[pframe] = dpool_.AllocateSet(device_context_, pillar_shader_pipeline_.dset_layouts[0]);
    dset_writer.BindBuffer(mesh_uniforms_.Handle(pframe), 0);
    dset_writer.BindTexelBuffer(visible_cells_buffer_.View(pframe), 2);
    dset_writer.BindTexelBuffer(heightfield_buffer_.View(pframe), 3);
    dset_writer.WriteAll(device_context_, dsets_[pframe]);
  }
}

PillarsApp::~PillarsApp() {
  if (device_) {
    vkDeviceWaitIdle(device_);

    dpool_.Destroy(device_context_);

    mesh_uniforms_.Destroy(device_context_);
    visible_cells_buffer_.Destroy(device_context_);
    heightfield_buffer_.Destroy(device_context_);

    // TODO(cort): automate!
    mesh_.index_buffer.Destroy(device_context_);
    mesh_.vertex_buffers[0].Destroy(device_context_);

    pillar_vs_.Destroy(device_context_);
    pillar_fs_.Destroy(device_context_);
    pillar_shader_pipeline_.Destroy(device_context_);
    pillar_pipeline_.Destroy(device_context_);

    vkDestroySampler(device_, sampler_, host_allocator_);
    albedo_tex_.Destroy(device_context_);
    blitter_.Destroy(device_context_);

    for(const auto fb : framebuffers_) {
      vkDestroyFramebuffer(device_, fb, host_allocator_);
    }
    render_pass_.Destroy(device_context_);

    depth_image_.Destroy(device_context_);
  }
}


void PillarsApp::Update(double dt) {
  Application::Update(dt);
  seconds_elapsed_ += dt;

  // Update camera
  mathfu::vec3 impulse(0,0,0);
  const float MOVE_SPEED = 0.3f, TURN_SPEED = 0.001f;
  if (input_state_.GetDigital(InputState::DIGITAL_LPAD_UP)) {
    impulse += camera_->getViewDirection() * MOVE_SPEED;
  }
  if (input_state_.GetDigital(InputState::DIGITAL_LPAD_LEFT)) {
    mathfu::vec3 viewRight = camera_->getOrientation() * mathfu::vec3(1,0,0);
    impulse -= viewRight * MOVE_SPEED;
  }
  if (input_state_.GetDigital(InputState::DIGITAL_LPAD_DOWN)) {
    impulse -= camera_->getViewDirection() * MOVE_SPEED;
  }
  if (input_state_.GetDigital(InputState::DIGITAL_LPAD_RIGHT)) {
    mathfu::vec3 viewRight = camera_->getOrientation() * mathfu::vec3(1,0,0);
    impulse += viewRight * MOVE_SPEED;
  }
  if (input_state_.GetDigital(InputState::DIGITAL_RPAD_DOWN)) {
    mathfu::vec3 viewUp = camera_->getOrientation() * mathfu::vec3(0,1,0);
    impulse += viewUp * MOVE_SPEED;
  }

  // Update camera based on mouse delta
  mathfu::vec3 camera_eulers = camera_->getEulersYPR() + mathfu::vec3(
    -TURN_SPEED * input_state_.GetAnalogDelta(InputState::ANALOG_MOUSE_Y),
    -TURN_SPEED * input_state_.GetAnalogDelta(InputState::ANALOG_MOUSE_X),
    0);
  if (camera_eulers[0] >= float(M_PI_2 - 0.01f)) {
    camera_eulers[0] = float(M_PI_2 - 0.01f);
  } else if (camera_eulers[0] <= float(-M_PI_2 + 0.01f)) {
    camera_eulers[0] = float(-M_PI_2 + 0.01f);
  }
  camera_eulers[2] = 0; // disallow roll
  camera_->setOrientation(mathfu::quat::FromEulerAngles(camera_eulers));
  dolly_->Impulse(impulse);
  dolly_->Update((float)dt);

  // Update uniforms
  MeshUniforms* uniforms = (MeshUniforms*)mesh_uniforms_.Mapped(pframe_index_);
  uniforms->time_and_res = mathfu::vec4(
    (float)seconds_elapsed_, (float)swapchain_extent_.width, (float)swapchain_extent_.height, 0);
  uniforms->eye = mathfu::vec4(camera_->getEyePoint(), 1.0f);
  mathfu::mat4 w2v = camera_->getViewMatrix();
  const mathfu::mat4 proj = camera_->getProjectionMatrix();
  const mathfu::mat4 clip_fixup(
    +1.0f, +0.0f, +0.0f, +0.0f,
    +0.0f, -1.0f, +0.0f, +0.0f,
    +0.0f, +0.0f, +0.5f, +0.5f,
    +0.0f, +0.0f, +0.0f, +1.0f);
  uniforms->viewproj = clip_fixup * proj * w2v;
  mesh_uniforms_.FlushPframeHostCache(pframe_index_);

  // Update visible cells
  // - Add a cell as visible the first time it gets within N units of the camera.
  // - Adjust a cell's height depending on its distance from the camera. There must
  //   be a barrier at least one cell thick that is close enough to be visible but
  //   too far away to be height-adjusted.
  float eye_x = camera_->getEyePoint().x();
  float eye_y = camera_->getEyePoint().z();
  int32_t cell_x = uint32_t(eye_x);
  int32_t cell_y = uint32_t(eye_y);
  int32_t min_x = my_max(0, cell_x-VISIBLE_RADIUS);
  int32_t max_x = my_min(HEIGHTFIELD_DIMX-1, cell_x+VISIBLE_RADIUS);
  int32_t min_y = my_max(0, cell_y-VISIBLE_RADIUS);
  int32_t max_y = my_min(HEIGHTFIELD_DIMY-1, cell_y+VISIBLE_RADIUS);
  for(int32_t iY=min_y; iY<=max_y; ++iY) {
    float fY = float(iY);
    for(int32_t iX=min_x; iX<=max_x; ++iX) {
      int32_t cell = XY_TO_CELL(iX,iY);
      if (heightfield_.at(cell) < 0) {
        // First time we're close enough to draw this cell; add it to the visible list
        visible_cells_.push_back(cell);
        heightfield_.at(cell) = 10.0f;
      }
      if (abs(iX-cell_x) <= EFFECT_RADIUS && abs(iY-cell_y) <= EFFECT_RADIUS) {
        float fX = float(iX);
        float dx = 1.0f * my_max(fabsf(fX-eye_x) - 3.0f, 0.0f);
        float dy = 1.0f * my_max(fabsf(fY-eye_y) - 3.0f, 0.0f);
        heightfield_.at(cell) = my_min(heightfield_.at(cell), 1.6f * sqrtf(dx*dx + dy*dy)
        );
      }
    }
  }
  memcpy(visible_cells_buffer_.Mapped(pframe_index_), visible_cells_.data(), visible_cells_.size() * sizeof(int32_t));
  visible_cells_buffer_.FlushPframeHostCache(pframe_index_);
  memcpy(heightfield_buffer_.Mapped(pframe_index_), heightfield_.data(), heightfield_.size() * sizeof(float));
  heightfield_buffer_.FlushPframeHostCache(pframe_index_);
}

void PillarsApp::Render(VkCommandBuffer primary_cb, uint32_t swapchain_image_index) {
  blitter_.NextPframe();

  VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];
  // TODO(cort): spurious validation warning for unused clear value? Is that
  // actually bad? What if attachments 0 and 2 must be cleared but not 1?
  // Easy enough to work around in this case, just hard-code the array size.
  //std::vector<VkClearValue> clear_values(render_pass_.attachment_descs.size());
  std::vector<VkClearValue> clear_values(2);
  clear_values[0].color.float32[0] = 0.2f;
  clear_values[0].color.float32[1] = 0.2f;
  clear_values[0].color.float32[2] = 0.3f;
  clear_values[0].color.float32[3] = 0.0f;
  clear_values[1].depthStencil.depth = 1.0f;
  clear_values[1].depthStencil.stencil = 0;
  VkRenderPassBeginInfo render_pass_begin_info = {};
  render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_begin_info.renderPass = render_pass_.handle;
  render_pass_begin_info.framebuffer = framebuffer;
  render_pass_begin_info.renderArea.offset.x = 0;
  render_pass_begin_info.renderArea.offset.y = 0;
  render_pass_begin_info.renderArea.extent = swapchain_extent_;
  render_pass_begin_info.clearValueCount = (uint32_t)clear_values.size();
  render_pass_begin_info.pClearValues = clear_values.data();

  vkCmdBeginRenderPass(primary_cb, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pillar_pipeline_.handle);
  VkRect2D scissor_rect = render_pass_begin_info.renderArea;
  VkViewport viewport = Rect2DToViewport(scissor_rect);
  vkCmdSetViewport(primary_cb, 0,1, &viewport);
  vkCmdSetScissor(primary_cb, 0,1, &scissor_rect);
  // TODO(cort): leaving these unbound did not trigger a validation warning...
  vkCmdBindDescriptorSets(primary_cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
    pillar_pipeline_.shader_pipeline->pipeline_layout,
    0, 1, &dsets_[pframe_index_], 0, nullptr);
  const VkDeviceSize vertex_buffer_offsets[1] = {}; // TODO(cort): mesh::bind()
  VkBuffer vertex_buffer = mesh_.vertex_buffers[0].Handle();
  vkCmdBindVertexBuffers(primary_cb, 0,1, &vertex_buffer, vertex_buffer_offsets);
  const VkDeviceSize index_buffer_offset = 0;
  vkCmdBindIndexBuffer(primary_cb, mesh_.index_buffer.Handle(), index_buffer_offset, mesh_.index_type);
  vkCmdDrawIndexed(primary_cb, mesh_.index_count, (uint32_t)visible_cells_.size(), 0,0,0);
  vkCmdEndRenderPass(primary_cb);
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
    {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}
  };
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;

  PillarsApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
