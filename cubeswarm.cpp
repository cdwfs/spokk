#include "vk_application.h"
#include "vk_debug.h"
#include "vk_init.h"
#include "vk_texture.h"
#include "vk_vertex.h"
using namespace cdsvk;

#include "camera.h"
#include "cube_mesh.h"

#include <mathfu/vector.h>
#include <mathfu/glsl_mappings.h>

#include <array>
#include <cstdio>
#include <memory>

namespace {
constexpr uint32_t MESH_INSTANCE_COUNT = 1024;
}  // namespace

class CubeSwarmApp : public cdsvk::Application {
public:
  explicit CubeSwarmApp(Application::CreateInfo &ci) :
      Application(ci) {
    seconds_elapsed_ = 0;

    const float fovDegrees = 45.0f;
    const float zNear = 0.01f;
    const float zFar = 100.0f;
    camera_ = my_make_unique<CameraPersp>(swapchain_extent_.width, swapchain_extent_.height, fovDegrees, zNear, zFar);
    const mathfu::vec3 initial_camera_pos(-1, 0, 6);
    const mathfu::vec3 initial_camera_target(0, 0, 0);
    const mathfu::vec3 initial_camera_up(0,1,0);
    camera_->lookAt(initial_camera_pos, initial_camera_target, initial_camera_up);
    dolly_ = my_make_unique<CameraDolly>(*camera_);

    // Retrieve queue handles
    const DeviceQueueContext *queue_context = device_context_.find_queue_context(VK_QUEUE_GRAPHICS_BIT, surface_);
    graphics_and_present_queue_ = queue_context->queue;

    // Allocate command buffers
    VkCommandPoolCreateInfo cpool_ci = {};
    cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpool_ci.queueFamilyIndex = queue_context->queue_family;
    CDSVK_CHECK(vkCreateCommandPool(device_, &cpool_ci, host_allocator_, &cpool_));
    VkCommandBufferAllocateInfo cb_allocate_info = {};
    cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_allocate_info.commandPool = cpool_;
    cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_allocate_info.commandBufferCount = (uint32_t)command_buffers_.size();
    CDSVK_CHECK(vkAllocateCommandBuffers(device_, &cb_allocate_info, command_buffers_.data()));

    // Create render pass
    render_pass_.init_from_preset(RenderPass::Preset::COLOR_DEPTH_POST, swapchain_surface_format_.format);
    CDSVK_CHECK(render_pass_.finalize_and_create(device_context_));

    // Create depth buffer
    VkImageCreateInfo depth_image_ci = render_pass_.get_attachment_image_ci(1, swapchain_extent_);
    depth_image_ = {};
    CDSVK_CHECK(depth_image_.create(device_context_, depth_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DEVICE_ALLOCATION_SCOPE_DEVICE));

    // Create intermediate color buffer
    VkImageCreateInfo offscreen_image_ci = render_pass_.get_attachment_image_ci(0, swapchain_extent_);
    CDSVK_CHECK(offscreen_image_.create(device_context_, offscreen_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DEVICE_ALLOCATION_SCOPE_DEVICE));

    // Create VkFramebuffers
    std::vector<VkImageView> attachment_views = {
      offscreen_image_.view,
      depth_image_.view,
      VK_NULL_HANDLE, // filled in below
    };
    VkFramebufferCreateInfo framebuffer_ci = render_pass_.get_framebuffer_ci(swapchain_extent_);
    framebuffer_ci.pAttachments = attachment_views.data();
    framebuffers_.resize(swapchain_image_views_.size());
    for(size_t i=0; i<swapchain_image_views_.size(); ++i) {
      attachment_views[2] = swapchain_image_views_[i];
      CDSVK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, host_allocator_, &framebuffers_[i]));
    }

    // Load textures and samplers
    VkSamplerCreateInfo sampler_ci = get_sampler_ci(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    CDSVK_CHECK(vkCreateSampler(device_, &sampler_ci, host_allocator_, &sampler_));
    texture_loader_ = my_make_unique<TextureLoader>(device_context_);
    CDSVK_CHECK(albedo_tex_.create_and_load(device_context_, *texture_loader_.get(), "trevor/redf.ktx"));

    // Load shader pipelines
    CDSVK_CHECK(mesh_vs_.create_and_load_spv_file(device_context_, "tri.vert.spv"));
    CDSVK_CHECK(mesh_fs_.create_and_load_spv_file(device_context_, "tri.frag.spv"));
    // TODO(cort): find a better way to override specific buffers as dynamic in shader pipelines.
    // For now it's safe to just poke the new type into the Shader's layout info, before ShaderPipeline creation.
    mesh_vs_.dset_layout_infos[0].bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    CDSVK_CHECK(mesh_shader_pipeline_.add_shader(&mesh_vs_));
    CDSVK_CHECK(mesh_shader_pipeline_.add_shader(&mesh_fs_));

    CDSVK_CHECK(fullscreen_tri_vs_.create_and_load_spv_file(device_context_, "fullscreen.vert.spv"));
    CDSVK_CHECK(post_filmgrain_fs_.create_and_load_spv_file(device_context_, "subpass_post.frag.spv"));
    CDSVK_CHECK(post_shader_pipeline_.add_shader(&fullscreen_tri_vs_));
    CDSVK_CHECK(post_shader_pipeline_.add_shader(&post_filmgrain_fs_));

    CDSVK_CHECK(ShaderPipeline::force_compatible_layouts_and_finalize(device_context_,
      {&mesh_shader_pipeline_, &post_shader_pipeline_}));

    // Populate Mesh object
    mesh_.index_type = (sizeof(cube_indices[0]) == sizeof(uint32_t)) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
    mesh_.index_count = cube_index_count;

    VkBufferCreateInfo index_buffer_ci = {};
    index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buffer_ci.size = cube_index_count * sizeof(cube_indices[0]);
    index_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    index_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CDSVK_CHECK(mesh_.index_buffer.create(device_context_, index_buffer_ci));
    CDSVK_CHECK(mesh_.index_buffer.load(device_context_, cube_indices, index_buffer_ci.size));

    // Describe the mesh format.
    mesh_format_.vertex_buffer_bindings = {
      {0, 3+3+4, VK_VERTEX_INPUT_RATE_VERTEX},
    };
    mesh_format_.vertex_attributes = {
      {0, 0, VK_FORMAT_R8G8B8_SNORM, 0},
      {1, 0, VK_FORMAT_R8G8B8_SNORM, 3},
      {2, 0, VK_FORMAT_R16G16_SFLOAT, 6},
    };
    mesh_format_.finalize(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    mesh_.mesh_format = &mesh_format_;

    VkBufferCreateInfo vertex_buffer_ci = {};
    vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_ci.size = cube_vertex_count * mesh_format_.vertex_buffer_bindings[0].stride;
    vertex_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertex_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    mesh_.vertex_buffers.resize(1);
    CDSVK_CHECK(mesh_.vertex_buffers[0].create(device_context_, vertex_buffer_ci));
    // Convert the vertex data from its original uncompressed format to its final format.
    // In a real application, this conversion would happen at asset build time.
    const VertexLayout src_vertex_layout = {
      {0, VK_FORMAT_R32G32B32_SFLOAT, 0},
      {1, VK_FORMAT_R32G32B32_SFLOAT, 12},
      {2, VK_FORMAT_R32G32_SFLOAT, 24},
    };
    const VertexLayout final_vertex_layout(mesh_format_, 0);
    std::vector<uint8_t> final_mesh_vertices(vertex_buffer_ci.size);
    int convert_error = convert_vertex_buffer(cube_vertices, src_vertex_layout,
      final_mesh_vertices.data(), final_vertex_layout, cube_vertex_count);
    assert(convert_error == 0);
    (void)convert_error;
    CDSVK_CHECK(mesh_.vertex_buffers[0].load(device_context_, final_mesh_vertices.data(), vertex_buffer_ci.size));

    // Create buffer of per-mesh object-to-world matrices.
    // TODO(cort): It may be worth creating an abstraction for N-buffered resources.
    const VkDeviceSize uniform_buffer_vframe_size = MESH_INSTANCE_COUNT * sizeof(mathfu::mat4);
    VkBufferCreateInfo o2w_buffer_ci = {};
    o2w_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    o2w_buffer_ci.size = uniform_buffer_vframe_size * VFRAME_COUNT;
    o2w_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    o2w_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CDSVK_CHECK(mesh_uniforms_.create(device_context_, o2w_buffer_ci));

    CDSVK_CHECK(mesh_pipeline_.create(device_context_, mesh_.mesh_format, &mesh_shader_pipeline_, &render_pass_, 0));

    CDSVK_CHECK(fullscreen_pipeline_.create(device_context_,
      MeshFormat::get_empty(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
      &post_shader_pipeline_, &render_pass_, 1));
    // because the pipelines use a compatible layout, we can just add room for one full layout.
    dpool_.add((uint32_t)mesh_shader_pipeline_.dset_layout_cis.size(), mesh_shader_pipeline_.dset_layout_cis.data());

    // TODO(cort): hmm, how to deal with multiple shader pipelines having overlapping (but compatible)
    // dset layouts and push constant ranges? For now, just keep them separated.
    CDSVK_CHECK(dpool_.finalize(device_context_));
    dset_ = dpool_.allocate_set(device_context_, mesh_shader_pipeline_.dset_layouts[0]);
    DescriptorSetWriter dset_writer(mesh_shader_pipeline_.dset_layout_cis[0]);
    dset_writer.bind_buffer(mesh_uniforms_.handle, 0, VK_WHOLE_SIZE, 0);
    dset_writer.bind_image(albedo_tex_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler_, 1);
    dset_writer.bind_image(offscreen_image_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_NULL_HANDLE, 2, 0);
    dset_writer.write_all_to_dset(device_context_, dset_);

    viewport_.x = 0;
    viewport_.y = 0;
    viewport_.width  = (float)swapchain_extent_.width;
    viewport_.height = (float)swapchain_extent_.height;
    viewport_.minDepth = 0.0f;
    viewport_.maxDepth = 1.0f;
    scissor_rect_.extent.width  = swapchain_extent_.width;
    scissor_rect_.extent.height = swapchain_extent_.height;
    scissor_rect_.offset.x = 0;
    scissor_rect_.offset.y = 0;

    // Create the semaphores used to synchronize access to swapchain images
    VkSemaphoreCreateInfo semaphore_ci = {};
    semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    CDSVK_CHECK(vkCreateSemaphore(device_, &semaphore_ci, host_allocator_, &swapchain_image_ready_sem_));
    CDSVK_CHECK(vkCreateSemaphore(device_, &semaphore_ci, host_allocator_, &rendering_complete_sem_));

    // Create the fences used to wait for each swapchain image's command buffer to be submitted.
    // This prevents re-writing the command buffer contents before it's been submitted and processed.
    VkFenceCreateInfo fence_ci = {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for(auto &fence : submission_complete_fences_) {
      CDSVK_CHECK(vkCreateFence(device_, &fence_ci, host_allocator_, &fence));
    }
  }
  virtual ~CubeSwarmApp() {
    if (device_) {
      vkDeviceWaitIdle(device_);

      dpool_.destroy(device_context_);

      mesh_uniforms_.destroy(device_context_);

      // TODO(cort): automate!
      mesh_.index_buffer.destroy(device_context_);
      mesh_.vertex_buffers[0].destroy(device_context_);

      fullscreen_pipeline_.destroy(device_context_);

      mesh_vs_.destroy(device_context_);
      mesh_fs_.destroy(device_context_);
      mesh_shader_pipeline_.destroy(device_context_);
      mesh_pipeline_.destroy(device_context_);

      post_shader_pipeline_.destroy(device_context_);
      fullscreen_tri_vs_.destroy(device_context_);
      post_filmgrain_fs_.destroy(device_context_);

      for(auto &fence : submission_complete_fences_) {
        vkDestroyFence(device_, fence, host_allocator_);
      }
      vkDestroySemaphore(device_, swapchain_image_ready_sem_, host_allocator_);
      vkDestroySemaphore(device_, rendering_complete_sem_, host_allocator_);

      vkDestroySampler(device_, sampler_, host_allocator_);
      albedo_tex_.destroy(device_context_);
      texture_loader_.reset();

      for(const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, host_allocator_);
      }
      render_pass_.destroy(device_context_);

      offscreen_image_.destroy(device_context_);
      depth_image_.destroy(device_context_);

      vkDestroyCommandPool(device_, cpool_, host_allocator_);
    }
  }

  CubeSwarmApp(const CubeSwarmApp&) = delete;
  const CubeSwarmApp& operator=(const CubeSwarmApp&) = delete;

  virtual void update(double dt) override {
    Application::update(dt);
    seconds_elapsed_ += dt;

    // Update camera
    mathfu::vec3 impulse(0,0,0);
    const float MOVE_SPEED = 0.5f, TURN_SPEED = 0.001f;
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_UP)) {
      impulse += camera_->getViewDirection() * MOVE_SPEED;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_LEFT)) {
      impulse -= cross(camera_->getViewDirection(), camera_->getWorldUp()) * MOVE_SPEED;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_DOWN)) {
      impulse -= camera_->getViewDirection() * MOVE_SPEED;
    }
    if (input_state_.GetDigital(InputState::DIGITAL_LPAD_RIGHT)) {
      impulse += cross(camera_->getViewDirection(), camera_->getWorldUp()) * MOVE_SPEED;
    }

    camera_->setOrientation(mathfu::quat::FromEulerAngles(mathfu::vec3(
      -TURN_SPEED * input_state_.GetAnalog(InputState::ANALOG_MOUSE_Y),
      -TURN_SPEED * input_state_.GetAnalog(InputState::ANALOG_MOUSE_X),
      0)));
    dolly_->Impulse(impulse);
    dolly_->Update((float)dt);

    // Update object-to-world matrices.
    const float secs = (float)seconds_elapsed_;
    std::array<mathfu::mat4, MESH_INSTANCE_COUNT*4> o2w_matrices;
    const mathfu::vec3 swarm_center(0, 0, -2);
    for(int iMesh=0; iMesh<MESH_INSTANCE_COUNT; ++iMesh) {
      mathfu::quat q = mathfu::quat::FromAngleAxis(secs + (float)iMesh, mathfu::vec3(0,1,0));
      mathfu::mat4 o2w = mathfu::mat4::Identity()
        * mathfu::mat4::FromTranslationVector(mathfu::vec3(
          40.0f * cosf((1.0f+0.001f*iMesh) * 0.2f * secs + float(149*iMesh) + 0.0f) + swarm_center[0],
          20.5f * sinf(0.3f * secs + float(13*iMesh) + 5.0f) + swarm_center[1],
          30.0f * sinf(0.05f * secs + float(51*iMesh) + 2.0f) + swarm_center[2]
        ))
        * q.ToMatrix4()
        * mathfu::mat4::FromScaleVector( mathfu::vec3(1.0f, 1.0f, 1.0f) )
        ;
      o2w_matrices[iMesh] = o2w;
    }
    // TODO(cort): lean this up when I figure out a good abstraction for N-buffered resources.
    // Specifically, dynamic uniform buffers are currently an issue.
    mesh_uniforms_.load(device_context_, o2w_matrices.data(), MESH_INSTANCE_COUNT * sizeof(mathfu::mat4),
      0, MESH_INSTANCE_COUNT * sizeof(mathfu::mat4) * vframe_index_);
  }

  virtual void render() override {
    // Wait for the command buffer previously used to generate this swapchain image to be submitted.
    // TODO(cort): this does not guarantee memory accesses from this submission will be visible on the host;
    // there'd need to be a memory barrier for that.
    vkWaitForFences(device_, 1, &submission_complete_fences_[vframe_index_], VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &submission_complete_fences_[vframe_index_]);

    // The host can now safely reset and rebuild this command buffer, even if the GPU hasn't finished presenting the
    // resulting frame yet.
    VkCommandBuffer cb = command_buffers_[vframe_index_];

    // Retrieve the index of the next available swapchain index
    uint32_t swapchain_image_index = UINT32_MAX;
    VkFence image_acquired_fence = VK_NULL_HANDLE; // currently unused, but if you want the CPU to wait for an image to be acquired...
    VkResult acquire_result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, swapchain_image_ready_sem_,
      image_acquired_fence, &swapchain_image_index);
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
      assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
    } else if (acquire_result == VK_SUBOPTIMAL_KHR) {
      // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
    } else {
      CDSVK_CHECK(acquire_result);
    }
    VkFramebuffer framebuffer = framebuffers_[swapchain_image_index];

    VkCommandBufferBeginInfo cb_begin_info = {};
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CDSVK_CHECK(vkBeginCommandBuffer(cb, &cb_begin_info) );

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

    vkCmdBeginRenderPass(cb, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh_pipeline_.handle);
    vkCmdSetViewport(cb, 0,1, &viewport_);
    vkCmdSetScissor(cb, 0,1, &scissor_rect_);
    uint32_t dynamic_uniform_offset = MESH_INSTANCE_COUNT * sizeof(mathfu::mat4) * vframe_index_;
    // TODO(cort): leaving these unbound did not trigger a validation warning...
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
      mesh_pipeline_.shader_pipeline->pipeline_layout,
      0, 1, &dset_, 1, &dynamic_uniform_offset);
    struct {
      mathfu::vec4_packed time_and_res;
      mathfu::vec4_packed eye;
      mathfu::mat4 viewproj;
    } push_constants = {};
    push_constants.time_and_res = mathfu::vec4((float)seconds_elapsed_,
      viewport_.width, viewport_.height, 0);
    push_constants.eye = mathfu::vec4(camera_->getEyePoint(), 1.0f);
    mathfu::mat4 w2v = camera_->getViewMatrix();
    const mathfu::mat4 proj = camera_->getProjectionMatrix();
    const mathfu::mat4 clip_fixup(
      +1.0f, +0.0f, +0.0f, +0.0f,
      +0.0f, -1.0f, +0.0f, +0.0f,
      +0.0f, +0.0f, +0.5f, +0.5f,
      +0.0f, +0.0f, +0.0f, +1.0f);
    const mathfu::mat4 viewproj = clip_fixup * proj * w2v;
    push_constants.viewproj = viewproj;
    vkCmdPushConstants(cb, mesh_pipeline_.shader_pipeline->pipeline_layout,
      mesh_pipeline_.shader_pipeline->push_constant_ranges[0].stageFlags,
      mesh_pipeline_.shader_pipeline->push_constant_ranges[0].offset,
      mesh_pipeline_.shader_pipeline->push_constant_ranges[0].size,
      &push_constants);
    const VkDeviceSize vertex_buffer_offsets[1] = {};
    vkCmdBindVertexBuffers(cb, 0,1, &mesh_.vertex_buffers[0].handle, vertex_buffer_offsets);
    const VkDeviceSize index_buffer_offset = 0;
    vkCmdBindIndexBuffer(cb, mesh_.index_buffer.handle, index_buffer_offset, mesh_.index_type);
    vkCmdDrawIndexed(cb, mesh_.index_count, MESH_INSTANCE_COUNT, 0,0,0);

    vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreen_pipeline_.handle);
    vkCmdSetViewport(cb, 0,1, &viewport_);
    vkCmdSetScissor(cb, 0,1, &scissor_rect_);
    vkCmdDraw(cb, 3, 1,0,0);
    vkCmdEndRenderPass(cb);

    CDSVK_CHECK( vkEndCommandBuffer(cb) );
    const VkPipelineStageFlags submit_wait_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &swapchain_image_ready_sem_;
    submit_info.pWaitDstStageMask = &submit_wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cb;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &rendering_complete_sem_;
    CDSVK_CHECK( vkQueueSubmit(graphics_and_present_queue_, 1, &submit_info, submission_complete_fences_[vframe_index_]) );
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = NULL;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain_;
    present_info.pImageIndices = &swapchain_image_index;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &rendering_complete_sem_;
    VkResult present_result = vkQueuePresentKHR(graphics_and_present_queue_, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR) {
      assert(0); // TODO(cort): swapchain is out of date (e.g. resized window) and must be recreated.
    } else if (present_result == VK_SUBOPTIMAL_KHR) {
      // TODO(cort): swapchain is not as optimal as it could be, but it'll work. Just an FYI condition.
    } else {
      CDSVK_CHECK(present_result);
    }
  }

private:
  double seconds_elapsed_;

  VkQueue graphics_and_present_queue_;

  VkCommandPool cpool_;
  std::array<VkCommandBuffer, VFRAME_COUNT> command_buffers_;

  VkSemaphore swapchain_image_ready_sem_, rendering_complete_sem_;
  std::array<VkFence, VFRAME_COUNT> submission_complete_fences_;

  Image depth_image_;
  Image offscreen_image_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  std::unique_ptr<TextureLoader> texture_loader_;
  Image albedo_tex_;
  VkSampler sampler_;

  Shader mesh_vs_, mesh_fs_;
  ShaderPipeline mesh_shader_pipeline_;
  GraphicsPipeline mesh_pipeline_;

  Shader fullscreen_tri_vs_, post_filmgrain_fs_;
  ShaderPipeline post_shader_pipeline_;
  GraphicsPipeline fullscreen_pipeline_;

  VkViewport viewport_;
  VkRect2D scissor_rect_;

  DescriptorPool dpool_;
  VkDescriptorSet dset_;

  MeshFormat mesh_format_;
  Mesh mesh_;
  Buffer mesh_uniforms_;

  std::unique_ptr<CameraPersp> camera_;
  std::unique_ptr<CameraDolly> dolly_;
};

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
    {(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}
  };
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;

  CubeSwarmApp app(app_ci);
  int run_error = app.run();

  return run_error;
}