#include "vk_application.h"
#include "vk_debug.h"
#include "vk_init.h"
#include "vk_texture.h"
using namespace cdsvk;

#include <array>
#include <cstdio>

class CubeSwarmApp : public cdsvk::Application {
public:
  explicit CubeSwarmApp(Application::CreateInfo &ci) :
      Application(ci) {
    // Retrieve queue handles
    graphics_and_present_queue_ = device_context_.find_queue_context(VK_QUEUE_GRAPHICS_BIT, surface_)->queue;

    // Allocate command buffers
    VkCommandPoolCreateInfo cpool_ci = {};
    cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpool_ci.queueFamilyIndex = device_context_.find_queue_context(VK_QUEUE_GRAPHICS_BIT)->queue_family;
    CDSVK_CHECK(vkCreateCommandPool(device_, &cpool_ci, allocation_callbacks_, &cpool_));
    VkCommandBufferAllocateInfo cb_allocate_info = {};
    cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_allocate_info.commandPool = cpool_;
    cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_allocate_info.commandBufferCount = (uint32_t)command_buffers_.size();
    CDSVK_CHECK(vkAllocateCommandBuffers(device_, &cb_allocate_info, command_buffers_.data()));

    // Create depth buffer
    VkImageCreateInfo depth_image_ci = {};
    depth_image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depth_image_ci.imageType = VK_IMAGE_TYPE_2D;
    depth_image_ci.format = VK_FORMAT_UNDEFINED; // filled in below
    depth_image_ci.extent = {ci.window_width, ci.window_height, 1}; // TODO(cort): use actual swapchain extent instead of window dimensions
    depth_image_ci.mipLevels = 1;
    depth_image_ci.arrayLayers = 1;
    depth_image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    depth_image_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depth_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    const VkFormat depth_format_candidates[] = {
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM_S8_UINT,
    };
    for(auto format : depth_format_candidates)
    {
      VkFormatProperties format_properties = {};
      vkGetPhysicalDeviceFormatProperties(physical_device_, format, &format_properties);
      if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
      {
        depth_image_ci.format = format;
        break;
      }
    }
    assert(depth_image_ci.format != VK_FORMAT_UNDEFINED);
    depth_image_ = {};
    CDSVK_CHECK(depth_image_.create(device_context_, depth_image_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      DEVICE_ALLOCATION_SCOPE_DEVICE));

#if 0
    // Create intermediate color buffer
    VkImageCreateInfo rt_image_ci = {};
    rt_image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    rt_image_ci.imageType = VK_IMAGE_TYPE_2D;
    rt_image_ci.format = context->swapchain_format();
    rt_image_ci.extent = {kWindowWidthDefault, kWindowHeightDefault, 1}; // TODO(cort): use actual swapchain extent instead of window dimensions
    rt_image_ci.mipLevels = 1;
    rt_image_ci.arrayLayers = 1;
    rt_image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    rt_image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    rt_image_ci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    rt_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    rt_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage rt_image = context->create_image(rt_image_ci, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, "offscreen color buffer image");
    VkDeviceMemory rt_image_mem = VK_NULL_HANDLE;
    VkDeviceSize rt_image_mem_offset = 0;
    VULKAN_CHECK(context->allocate_and_bind_image_memory(rt_image,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &rt_image_mem, &rt_image_mem_offset));
    VkImageView rt_image_view = context->create_image_view(rt_image, rt_image_ci, "offscreen color buffer image view");
#endif

    // Create render pass
    enum {
      kColorAttachmentIndex = 0,
      kDepthAttachmentIndex = 1,
      kAttachmentCount
    };
    render_pass_.attachment_descs.resize(kAttachmentCount);
    render_pass_.attachment_descs[kColorAttachmentIndex].format = swapchain_surface_format_.format;
    render_pass_.attachment_descs[kColorAttachmentIndex].samples = VK_SAMPLE_COUNT_1_BIT;
    render_pass_.attachment_descs[kColorAttachmentIndex].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    render_pass_.attachment_descs[kColorAttachmentIndex].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    render_pass_.attachment_descs[kColorAttachmentIndex].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    render_pass_.attachment_descs[kColorAttachmentIndex].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    render_pass_.attachment_descs[kColorAttachmentIndex].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    render_pass_.attachment_descs[kColorAttachmentIndex].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    render_pass_.attachment_descs[kDepthAttachmentIndex].format = depth_image_ci.format;
    render_pass_.attachment_descs[kDepthAttachmentIndex].samples = depth_image_ci.samples;
    render_pass_.attachment_descs[kDepthAttachmentIndex].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    render_pass_.attachment_descs[kDepthAttachmentIndex].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    render_pass_.attachment_descs[kDepthAttachmentIndex].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    render_pass_.attachment_descs[kDepthAttachmentIndex].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    render_pass_.attachment_descs[kDepthAttachmentIndex].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    render_pass_.attachment_descs[kDepthAttachmentIndex].finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    render_pass_.subpass_attachments.resize(1);
    render_pass_.subpass_attachments[0].color_refs.push_back({kColorAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    render_pass_.subpass_attachments[0].depth_stencil_refs.push_back({kDepthAttachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
    render_pass_.subpass_dependencies.resize(2);
    render_pass_.subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    render_pass_.subpass_dependencies[0].dstSubpass = 0;
    render_pass_.subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    render_pass_.subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    render_pass_.subpass_dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    render_pass_.subpass_dependencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    render_pass_.subpass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    render_pass_.subpass_dependencies[1].srcSubpass = 0;
    render_pass_.subpass_dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    render_pass_.subpass_dependencies[1].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    render_pass_.subpass_dependencies[1].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    render_pass_.subpass_dependencies[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    render_pass_.subpass_dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    render_pass_.subpass_dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    render_pass_.finalize_subpasses();
    VkRenderPassCreateInfo render_pass_ci = {};
    render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_ci.attachmentCount = (uint32_t)render_pass_.attachment_descs.size();
    render_pass_ci.pAttachments = render_pass_.attachment_descs.data();
    render_pass_ci.subpassCount = (uint32_t)render_pass_.subpass_descs.size();
    render_pass_ci.pSubpasses = render_pass_.subpass_descs.data();;
    render_pass_ci.dependencyCount = (uint32_t)render_pass_.subpass_dependencies.size();
    render_pass_ci.pDependencies = render_pass_.subpass_dependencies.data();
    CDSVK_CHECK(vkCreateRenderPass(device_, &render_pass_ci, allocation_callbacks_, &render_pass_.handle));

    // Create VkFramebuffers
    std::array<VkImageView, kAttachmentCount> attachment_views = {};
    attachment_views[kColorAttachmentIndex] = VK_NULL_HANDLE; // filled in below
    attachment_views[kDepthAttachmentIndex] = depth_image_.view;
    VkFramebufferCreateInfo framebuffer_ci = {};
    framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_ci.renderPass = render_pass_.handle;
    framebuffer_ci.attachmentCount = (uint32_t)attachment_views.size();
    framebuffer_ci.pAttachments = attachment_views.data();
    framebuffer_ci.width = swapchain_extent_.width;
    framebuffer_ci.height = swapchain_extent_.height;
    framebuffer_ci.layers = 1;
    framebuffers_.resize(swapchain_image_views_.size());
    for(size_t i=0; i<swapchain_image_views_.size(); ++i) {
      attachment_views[kColorAttachmentIndex] = swapchain_image_views_[i];
      CDSVK_CHECK(vkCreateFramebuffer(device_, &framebuffer_ci, allocation_callbacks_, &framebuffers_[i]));
    }

    // Load shaders
    CDSVK_CHECK(fullscreen_tri_vs_.create_and_load(device_context_, "fullscreen.vert.spv"));
    CDSVK_CHECK(post_filmgrain_fs_.create_and_load(device_context_, "subpass_post.frag.spv"));
    CDSVK_CHECK(post_shader_pipeline_.create(device_context_, {
      {&fullscreen_tri_vs_, "main"},
      {&post_filmgrain_fs_, "main"},
    }));

    // Load textures and samplers
    VkSamplerCreateInfo sampler_ci = get_sampler_ci(VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    CDSVK_CHECK(vkCreateSampler(device_, &sampler_ci, allocation_callbacks_, &sampler_));
    texture_loader_ = my_make_unique<TextureLoader>(device_context_);
    albedo_tex_.create_and_load(device_context_, *texture_loader_.get(), "trevor/redf.ktx");

    // Create the semaphores used to synchronize access to swapchain images
    VkSemaphoreCreateInfo semaphore_ci = {};
    semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    CDSVK_CHECK(vkCreateSemaphore(device_, &semaphore_ci, allocation_callbacks_, &swapchain_image_ready_sem_));
    CDSVK_CHECK(vkCreateSemaphore(device_, &semaphore_ci, allocation_callbacks_, &rendering_complete_sem_));

    // Create the fences used to wait for each swapchain image's command buffer to be submitted.
    // This prevents re-writing the command buffer contents before it's been submitted and processed.
    VkFenceCreateInfo fence_ci = {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for(auto &fence : submission_complete_fences_) {
      CDSVK_CHECK(vkCreateFence(device_, &fence_ci, allocation_callbacks_, &fence));
    }
  }
  virtual ~CubeSwarmApp() {
    if (device_) {
      vkDeviceWaitIdle(device_);

      post_shader_pipeline_.destroy(device_context_);
      fullscreen_tri_vs_.destroy(device_context_);
      post_filmgrain_fs_.destroy(device_context_);

      for(auto &fence : submission_complete_fences_) {
        vkDestroyFence(device_, fence, allocation_callbacks_);
      }
      vkDestroySemaphore(device_, swapchain_image_ready_sem_, allocation_callbacks_);
      vkDestroySemaphore(device_, rendering_complete_sem_, allocation_callbacks_);

      vkDestroySampler(device_, sampler_, allocation_callbacks_);
      albedo_tex_.destroy(device_context_);
      texture_loader_.reset();

      for(const auto fb : framebuffers_) {
        vkDestroyFramebuffer(device_, fb, allocation_callbacks_);
      }
      vkDestroyRenderPass(device_, render_pass_.handle, allocation_callbacks_);

      depth_image_.destroy(device_context_);

      vkDestroyCommandPool(device_, cpool_, allocation_callbacks_);
    }
  }

  CubeSwarmApp(const CubeSwarmApp&) = delete;
  const CubeSwarmApp& operator=(const CubeSwarmApp&) = delete;

  virtual void update(double dt) override {
    Application::update(dt);
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

    std::vector<VkClearValue> clear_values(render_pass_.attachment_descs.size());
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
  VkQueue graphics_and_present_queue_;

  VkCommandPool cpool_;
  std::array<VkCommandBuffer, VFRAME_COUNT> command_buffers_;

  VkSemaphore swapchain_image_ready_sem_, rendering_complete_sem_;
  std::array<VkFence, VFRAME_COUNT> submission_complete_fences_;

  Image depth_image_;

  RenderPass render_pass_;
  std::vector<VkFramebuffer> framebuffers_;

  std::unique_ptr<TextureLoader> texture_loader_;
  Image albedo_tex_;
  VkSampler sampler_;
  Shader fullscreen_tri_vs_, post_filmgrain_fs_;
  ShaderPipeline post_shader_pipeline_;

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