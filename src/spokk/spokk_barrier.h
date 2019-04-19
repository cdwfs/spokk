#pragma once

// clang-format off
#include <vulkan/vulkan.h>
#include <thsvs_simpler_vulkan_synchronization.h>
// clang-format on

namespace spokk {

// out_src_stages and out_dst_stages are modified with |=, so existing flags are preserved across multiple
// barriers. Important corollary: initialize stages to 0 for the first barrier in a set!
void BuildVkMemoryBarrier(ThsvsAccessType src_access_type, ThsvsAccessType dst_access_type,
    VkPipelineStageFlags* out_src_stages, VkPipelineStageFlags* out_dst_stages, VkMemoryBarrier* out_memory_barrier);

}  // namespace spokk
