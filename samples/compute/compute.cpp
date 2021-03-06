#include <spokk.h>
using namespace spokk;

#include <array>
#include <cstdio>

namespace {
constexpr uint32_t BUXEL_COUNT = 8192;
}

class ComputeApp : public spokk::Application {
public:
  explicit ComputeApp(Application::CreateInfo &ci) : Application(ci) {
    // Find a compute queue
    const DeviceQueue *compute_queue = device_.FindQueue(VK_QUEUE_COMPUTE_BIT);
    SPOKK_VK_CHECK(device_.SetObjectName(compute_queue->handle, "compute queue"));

    // Allocate command buffers
    VkCommandPoolCreateInfo cpool_ci = {};
    cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpool_ci.queueFamilyIndex = compute_queue->family;
    VkCommandPool cpool = VK_NULL_HANDLE;
    SPOKK_VK_CHECK(vkCreateCommandPool(device_, &cpool_ci, host_allocator_, &cpool));
    SPOKK_VK_CHECK(device_.SetObjectName(cpool, "Primary command pool"));
    VkCommandBufferAllocateInfo cb_allocate_info = {};
    cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_allocate_info.commandPool = cpool;
    cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_allocate_info.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    SPOKK_VK_CHECK(vkAllocateCommandBuffers(device_, &cb_allocate_info, &cb));
    SPOKK_VK_CHECK(device_.SetObjectName(cb, "Primary command buffer"));

    std::array<int32_t, BUXEL_COUNT> in_data, out_ref;
    for (size_t iBuxel = 0; iBuxel < BUXEL_COUNT; ++iBuxel) {
      in_data[iBuxel] = (int32_t)iBuxel;
      out_ref[iBuxel] = (int32_t)iBuxel * 2;
    }
    VkBufferCreateInfo buffer_ci = {};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = BUXEL_COUNT * sizeof(int32_t);
    buffer_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    Buffer in_buffer = {}, out_buffer = {};
    SPOKK_VK_CHECK(in_buffer.Create(device_, buffer_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    SPOKK_VK_CHECK(device_.SetObjectName(in_buffer.Handle(), "input buffer"));
    SPOKK_VK_CHECK(in_buffer.Load(
        device_, THSVS_ACCESS_NONE, THSVS_ACCESS_COMPUTE_SHADER_READ_OTHER, in_data.data(), buffer_ci.size));
    buffer_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    // TODO(cort): until I have buffer.unload, the output buffer must be host-visible.
    SPOKK_VK_CHECK(out_buffer.Create(device_, buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    SPOKK_VK_CHECK(device_.SetObjectName(out_buffer.Handle(), "output buffer"));

    // Load shaders
    Shader double_ints_cs = {};
    ShaderProgram compute_shader_program = {};
    SPOKK_VK_CHECK(double_ints_cs.CreateAndLoadSpirvFile(device_, "data/compute/double_ints.comp.spv"));
    SPOKK_VK_CHECK(compute_shader_program.AddShader(&double_ints_cs));
    SPOKK_VK_CHECK(compute_shader_program.Finalize(device_));

    ComputePipeline compute_pipeline = {};
    compute_pipeline.Init(&compute_shader_program);
    SPOKK_VK_CHECK(compute_pipeline.Finalize(device_));
    SPOKK_VK_CHECK(device_.SetObjectName(compute_pipeline.handle, "integer-doubling pipeline"));

    DescriptorPool dpool = {};
    dpool.Add((uint32_t)compute_shader_program.dset_layout_cis.size(), compute_shader_program.dset_layout_cis.data());
    SPOKK_VK_CHECK(dpool.Finalize(device_));
    VkDescriptorSet dset = VK_NULL_HANDLE;
    dset = dpool.AllocateSet(device_, compute_shader_program.dset_layouts[0]);
    DescriptorSetWriter dset_writer(compute_shader_program.dset_layout_cis[0]);
    dset_writer.BindBuffer(in_buffer.Handle(), double_ints_cs.GetDescriptorBindPoint("innie").binding);
    dset_writer.BindBuffer(out_buffer.Handle(), double_ints_cs.GetDescriptorBindPoint("outie").binding);
    dset_writer.WriteAll(device_, dset);

    VkFenceCreateInfo fence_ci = {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence compute_done_fence = VK_NULL_HANDLE;
    SPOKK_VK_CHECK(vkCreateFence(device_, &fence_ci, host_allocator_, &compute_done_fence));
    SPOKK_VK_CHECK(device_.SetObjectName(compute_done_fence, "compute done fence"));

    VkCommandBufferBeginInfo cb_begin_info = {};
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    SPOKK_VK_CHECK(vkBeginCommandBuffer(cb, &cb_begin_info));

    // These barriers are *probably* unnecessary; spec section 6.9 guarantees host write ordering across a call to
    // vkQueueSubmit(). But, enh.
    VkMemoryBarrier barriers[2] = {};
    VkPipelineStageFlags barrier_src_stages = 0, barrier_dst_stages = 0;
    // barrier for input buffer
    spokk::BuildVkMemoryBarrier(THSVS_ACCESS_HOST_WRITE, THSVS_ACCESS_COMPUTE_SHADER_READ_OTHER, &barrier_src_stages,
        &barrier_dst_stages, &barriers[0]);
    // barrier for output buffer
    spokk::BuildVkMemoryBarrier(
        THSVS_ACCESS_NONE, THSVS_ACCESS_COMPUTE_SHADER_WRITE, &barrier_src_stages, &barrier_dst_stages, &barriers[1]);
    vkCmdPipelineBarrier(cb, barrier_src_stages, barrier_dst_stages, 0, 2, barriers, 0, nullptr, 0, nullptr);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline.handle);
    vkCmdBindDescriptorSets(
        cb, VK_PIPELINE_BIND_POINT_COMPUTE, compute_shader_program.pipeline_layout, 0, 1, &dset, 0, nullptr);
    const float dispatch_label_color[4] = {1, 1, 0, 1};
    device_.DebugLabelInsert(cb, "double those ints!", dispatch_label_color);
    vkCmdDispatch(cb, BUXEL_COUNT, 1, 1);

    // barrier to read from output buffer. This barrier *is necessary; see the notes for vkWaitForFences() in the spec.
    barrier_src_stages = 0;
    barrier_dst_stages = 0;
    spokk::BuildVkMemoryBarrier(THSVS_ACCESS_COMPUTE_SHADER_WRITE, THSVS_ACCESS_HOST_READ, &barrier_src_stages,
        &barrier_dst_stages, &barriers[1]);
    vkCmdPipelineBarrier(cb, barrier_src_stages, barrier_dst_stages, 0, 1, &barriers[1], 0, nullptr, 0, nullptr);

    SPOKK_VK_CHECK(vkEndCommandBuffer(cb));
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cb;
    SPOKK_VK_CHECK(vkQueueSubmit(compute_queue->handle, 1, &submit_info, compute_done_fence));

    SPOKK_VK_CHECK(vkWaitForFences(device_, 1, &compute_done_fence, VK_TRUE, UINT64_MAX));
    SPOKK_VK_CHECK(out_buffer.InvalidateHostCache(device_));

    const int32_t *out_data = (const int32_t *)out_buffer.Mapped();
    bool valid = true;
    for (uint32_t iBuxel = 0; iBuxel < (uint32_t)out_ref.size(); ++iBuxel) {
      if (out_data[iBuxel] != out_ref[iBuxel]) {
        fprintf(stderr, "ERROR: in[%4d]=%4d, out[%4d]]%4d, ref[%4d]=%4d\n", iBuxel, in_data[iBuxel], iBuxel,
            out_data[iBuxel], iBuxel, out_ref[iBuxel]);
        valid = false;
      }
    }
    if (valid) {
      fprintf(stderr, "Results validated successfully! Woohoo!\n");
    }

    // Cleanup
    dpool.Destroy(device_);
    in_buffer.Destroy(device_);
    out_buffer.Destroy(device_);
    compute_pipeline.Destroy(device_);
    compute_shader_program.Destroy(device_);
    double_ints_cs.Destroy(device_);
    vkDestroyFence(device_, compute_done_fence, host_allocator_);
    vkDestroyCommandPool(device_, cpool, host_allocator_);

    force_exit_ = true;
  }
  virtual ~ComputeApp() {}

  ComputeApp(const ComputeApp &) = delete;
  const ComputeApp &operator=(const ComputeApp &) = delete;

  void Update(double) override {
    // nothing to do in a compute sample
  }
  void Render(VkCommandBuffer, uint32_t) override {
    // nothing to do in a compute sample
  }
};

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  std::vector<Application::QueueFamilyRequest> queue_requests = {
      {(VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT), true, 1, 0.0f}};
  Application::CreateInfo app_ci = {};
  app_ci.queue_family_requests = queue_requests;
  app_ci.enable_graphics = false;

  ComputeApp app(app_ci);
  int run_error = app.Run();

  return run_error;
}
