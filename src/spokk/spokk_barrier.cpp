#define THSVS_SIMPLER_VULKAN_SYNCHRONIZATION_IMPLEMENTATION
#include "spokk_barrier.h"

namespace spokk {

void BuildVkMemoryBarrier(ThsvsAccessType src_access_type, ThsvsAccessType dst_access_type,
    VkPipelineStageFlags* out_src_stages, VkPipelineStageFlags* out_dst_stages, VkMemoryBarrier* out_memory_barrier) {
  VkPipelineStageFlags new_src_stages = 0, new_dst_stages = 0;
  thsvsGetVulkanMemoryBarrier(
      {1, &src_access_type, 1, &dst_access_type}, &new_src_stages, &new_dst_stages, out_memory_barrier);
  *out_src_stages |= new_src_stages;
  *out_dst_stages |= new_dst_stages;
}

}  // namespace spokk
