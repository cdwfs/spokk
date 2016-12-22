#include "vk_application.h"
#include "vk_debug.h"
#include "vk_init.h"
using namespace cdsvk;

#include <mathfu/vector.h>
#include <mathfu/glsl_mappings.h>

#include <array>
#include <cstdio>

namespace {
  constexpr uint32_t BUXEL_COUNT = 8192;
}

class ComputeApp : public cdsvk::Application {
public:
  explicit ComputeApp(Application::CreateInfo &ci) :
      Application(ci) {
    // Retrieve Queue contexts
    const DeviceQueueContext *queue_context = device_context_.find_queue_context(VK_QUEUE_COMPUTE_BIT);
    compute_queue_ = queue_context->queue;

    // Allocate command buffers
    VkCommandPoolCreateInfo cpool_ci = {};
    cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpool_ci.queueFamilyIndex = queue_context->queue_family;
    CDSVK_CHECK(vkCreateCommandPool(device_, &cpool_ci, allocation_callbacks_, &cpool_));
    VkCommandBufferAllocateInfo cb_allocate_info = {};
    cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_allocate_info.commandPool = cpool_;
    cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_allocate_info.commandBufferCount = 1;
    CDSVK_CHECK(vkAllocateCommandBuffers(device_, &cb_allocate_info, &command_buffer_));

    for(size_t iBuxel=0; iBuxel<BUXEL_COUNT; ++iBuxel) {
      in_data_[iBuxel] = (int32_t)iBuxel;
      out_ref_[iBuxel] = (int32_t)iBuxel * 2;
    }
    VkBufferCreateInfo buffer_ci = {};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = BUXEL_COUNT * sizeof(int32_t);
    buffer_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CDSVK_CHECK(in_buffer_.create(device_context_, buffer_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CDSVK_CHECK(in_buffer_.load(device_context_, in_data_.data(), buffer_ci.size));
    buffer_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    // TODO(cort): until I have buffer.unload, the output buffer must be host-visible.
    CDSVK_CHECK(out_buffer_.create(device_context_, buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    // Load shaders
    CDSVK_CHECK(double_ints_cs_.create_and_load_spv_file(device_context_, "double_ints.comp.spv"));
    CDSVK_CHECK(compute_shader_pipeline_.add_shader(&double_ints_cs_, "main"));
    CDSVK_CHECK(compute_shader_pipeline_.finalize(device_context_));

    compute_pipeline_.create(device_context_, &compute_shader_pipeline_);

    dpool_.add((uint32_t)compute_shader_pipeline_.dset_layout_cis.size(), compute_shader_pipeline_.dset_layout_cis.data());
    CDSVK_CHECK(dpool_.finalize(device_context_));
    dset_ = dpool_.allocate_set(device_context_, compute_shader_pipeline_.dset_layouts[0]);
    DescriptorSetWriter dset_writer(compute_shader_pipeline_.dset_layout_cis[0]);
    dset_writer.bind_buffer(in_buffer_.handle, 0, VK_WHOLE_SIZE, 0);
    dset_writer.bind_buffer(out_buffer_.handle, 0, VK_WHOLE_SIZE, 1);
    dset_writer.write_all_to_dset(device_context_, dset_);

    VkFenceCreateInfo fence_ci = {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    CDSVK_CHECK(vkCreateFence(device_, &fence_ci, allocation_callbacks_, &submission_complete_fence_));

    VkCommandBuffer cb = command_buffer_;

    VkCommandBufferBeginInfo cb_begin_info = {};
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CDSVK_CHECK(vkBeginCommandBuffer(cb, &cb_begin_info) );

    VkBufferMemoryBarrier buffer_barriers[2] = {};
    buffer_barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_barriers[0].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    buffer_barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    buffer_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer_barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer_barriers[0].buffer = in_buffer_.handle;
    buffer_barriers[0].offset = 0;
    buffer_barriers[0].size = VK_WHOLE_SIZE;
    buffer_barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_barriers[1].srcAccessMask = 0;
    buffer_barriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    buffer_barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer_barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer_barriers[1].buffer = out_buffer_.handle;
    buffer_barriers[1].offset = 0;
    buffer_barriers[1].size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
      0,nullptr, 2,buffer_barriers, 0,nullptr);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_.handle);
    // TODO(cort): leaving these unbound did not trigger a validation warning...
    vkCmdBindDescriptorSets (cb, VK_PIPELINE_BIND_POINT_COMPUTE, compute_shader_pipeline_.pipeline_layout,
      0, 1, &dset_, 0,nullptr);
    vkCmdDispatch(cb, BUXEL_COUNT,1,1);

    buffer_barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    buffer_barriers[1].srcAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
      0,nullptr, 1,&buffer_barriers[1], 0,nullptr);

    CDSVK_CHECK( vkEndCommandBuffer(cb) );
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cb;
    CDSVK_CHECK( vkQueueSubmit(compute_queue_, 1, &submit_info, submission_complete_fence_) );

    CDSVK_CHECK(vkWaitForFences(device_, 1, &submission_complete_fence_, VK_TRUE, UINT64_MAX));
    out_buffer_.memory.invalidate(device_);

    const int32_t *out_data = (const int32_t*)out_buffer_.memory.mapped();
    bool valid = true;
    for(uint32_t iBuxel=0; iBuxel<(uint32_t)out_ref_.size(); ++iBuxel) {
      if (out_data[iBuxel] != out_ref_[iBuxel]) {
        fprintf(stderr, "ERROR: in[%4d]=%4d, out[%4d]]%4d, ref[%4d]=%4d\n",
          iBuxel, in_data_[iBuxel], iBuxel, out_data[iBuxel], iBuxel, out_ref_[iBuxel]);
        valid = false;
      }
    }
    if (valid) {
      fprintf(stderr, "Results validated successfully! Woohoo!\n");
    }

    force_exit_ = true;
  }
  virtual ~ComputeApp() {
    if (device_) {
      vkDeviceWaitIdle(device_);

      dpool_.destroy(device_context_);

      in_buffer_.destroy(device_context_);
      out_buffer_.destroy(device_context_);

      compute_pipeline_.destroy(device_context_);

      compute_shader_pipeline_.destroy(device_context_);
      double_ints_cs_.destroy(device_context_);

      vkDestroyFence(device_, submission_complete_fence_, allocation_callbacks_);

      vkDestroyCommandPool(device_, cpool_, allocation_callbacks_);
    }
  }

  ComputeApp(const ComputeApp&) = delete;
  const ComputeApp& operator=(const ComputeApp&) = delete;

private:
  VkQueue compute_queue_;

  VkCommandPool cpool_;
  VkCommandBuffer command_buffer_;

  Shader double_ints_cs_;
  ShaderPipeline compute_shader_pipeline_;

  ComputePipeline compute_pipeline_;

  Buffer in_buffer_, out_buffer_;
  std::array<int32_t, BUXEL_COUNT> in_data_, out_ref_;

  DescriptorPool dpool_;
  VkDescriptorSet dset_;

  VkFence submission_complete_fence_;
};

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
    {(VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}
  };
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;
  app_ci.enable_graphics = false;

  ComputeApp app(app_ci);
  int run_error = app.run();

  return run_error;
}