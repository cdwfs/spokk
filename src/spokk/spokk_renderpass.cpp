#include "spokk_renderpass.h"
#include "spokk_utilities.h"

#include <cassert>

namespace spokk {

//
// RenderPass
//
void RenderPass::InitFromPreset(Preset preset, VkFormat output_color_format) {
  if (preset == Preset::COLOR) {
    attachment_descs.resize(1);
    attachment_descs[0].format = output_color_format;
    attachment_descs[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_descs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_descs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment_descs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_descs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    subpass_attachments.resize(1);
    subpass_attachments[0].color_refs.push_back({0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    subpass_dependencies.resize(2);
    subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[0].dstSubpass = 0;
    subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpass_dependencies[1].srcSubpass = 0;
    subpass_dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[1].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[1].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  } else if (preset == Preset::COLOR_DEPTH) {
    attachment_descs.resize(2);
    attachment_descs[0].format = output_color_format;
    attachment_descs[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_descs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_descs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment_descs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_descs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachment_descs[1].format = VK_FORMAT_D32_SFLOAT;
    attachment_descs[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_descs[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_descs[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_descs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    subpass_attachments.resize(1);
    subpass_attachments[0].color_refs.push_back({0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    subpass_attachments[0].depth_stencil_refs.push_back({1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
    subpass_dependencies.resize(2);
    subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[0].dstSubpass = 0;
    subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpass_dependencies[1].srcSubpass = 0;
    subpass_dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[1].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[1].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[1].dstAccessMask = subpass_dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  } else if (preset == Preset::COLOR_POST) {
    attachment_descs.resize(2);
    attachment_descs[0].format = VK_FORMAT_R8G8B8A8_SRGB;
    attachment_descs[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_descs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_descs[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_descs[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment_descs[1].format = output_color_format;
    attachment_descs[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_descs[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment_descs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_descs[1].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    subpass_attachments.resize(2);
    subpass_attachments[0].color_refs.push_back({0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    subpass_attachments[1].input_refs.push_back({0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    subpass_attachments[1].color_refs.push_back({2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    subpass_dependencies.resize(3);
    subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[0].dstSubpass = 0;
    subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpass_dependencies[1].srcSubpass = 0;
    subpass_dependencies[1].dstSubpass = 1;
    subpass_dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subpass_dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpass_dependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    subpass_dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpass_dependencies[2].srcSubpass = 1;
    subpass_dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[2].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[2].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[2].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  } else if (preset == Preset::COLOR_DEPTH_POST) {
    attachment_descs.resize(3);
    attachment_descs[0].format = VK_FORMAT_R8G8B8A8_SRGB;
    attachment_descs[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_descs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_descs[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_descs[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment_descs[1].format = VK_FORMAT_D32_SFLOAT;
    attachment_descs[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_descs[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_descs[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_descs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachment_descs[2].format = output_color_format;
    attachment_descs[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_descs[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment_descs[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_descs[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_descs[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_descs[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    subpass_attachments.resize(2);
    subpass_attachments[0].color_refs.push_back({0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    subpass_attachments[0].depth_stencil_refs.push_back({1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
    subpass_attachments[1].input_refs.push_back({0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    subpass_attachments[1].color_refs.push_back({2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    subpass_dependencies.resize(3);
    subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[0].dstSubpass = 0;
    subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpass_dependencies[1].srcSubpass = 0;
    subpass_dependencies[1].dstSubpass = 1;
    subpass_dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subpass_dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpass_dependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    subpass_dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpass_dependencies[2].srcSubpass = 1;
    subpass_dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependencies[2].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[2].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    subpass_dependencies[2].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    subpass_dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  }
}

VkResult RenderPass::Finalize(const Device& device, VkPipelineBindPoint bind_point, VkSubpassDescriptionFlags flags) {
  subpass_descs.resize(subpass_attachments.size());
  subpass_multisample_state_cis.resize(subpass_attachments.size());
  for (size_t i = 0; i < subpass_attachments.size(); ++i) {
    subpass_descs[i].flags = flags;
    subpass_descs[i].pipelineBindPoint = bind_point;
    subpass_descs[i].inputAttachmentCount = (uint32_t)subpass_attachments[i].input_refs.size();
    subpass_descs[i].pInputAttachments = subpass_attachments[i].input_refs.data();
    subpass_descs[i].colorAttachmentCount = (uint32_t)subpass_attachments[i].color_refs.size();
    subpass_descs[i].pColorAttachments = subpass_attachments[i].color_refs.data();
    assert(subpass_attachments[i].resolve_refs.empty() ||
        subpass_attachments[i].resolve_refs.size() == subpass_attachments[i].color_refs.size());
    subpass_descs[i].pResolveAttachments = subpass_attachments[i].resolve_refs.data();
    assert(subpass_attachments[i].depth_stencil_refs.size() <= 1);
    subpass_descs[i].pDepthStencilAttachment =
        subpass_attachments[i].depth_stencil_refs.empty() ? nullptr : &subpass_attachments[i].depth_stencil_refs[0];
    subpass_descs[i].preserveAttachmentCount = (uint32_t)subpass_attachments[i].preserve_indices.size();
    subpass_descs[i].pPreserveAttachments = subpass_attachments[i].preserve_indices.data();
    // All color and depth/stencil attachments used in a subpass must have the same sample count,
    // as specified by the graphics pipeline.
    subpass_multisample_state_cis[i].sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    if (subpass_descs[i].pDepthStencilAttachment) {
      subpass_multisample_state_cis[i].rasterizationSamples =
          attachment_descs[subpass_descs[i].pDepthStencilAttachment->attachment].samples;
    } else if (subpass_descs[i].colorAttachmentCount > 0) {
      subpass_multisample_state_cis[i].rasterizationSamples =
          attachment_descs[subpass_descs[i].pColorAttachments[0].attachment].samples;
    } else {
      // zero-attachment subpass. /shrug
      subpass_multisample_state_cis[i].rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    }
    // Reasonable defaults for the rest of these
    subpass_multisample_state_cis[i].sampleShadingEnable = VK_FALSE;
    subpass_multisample_state_cis[i].minSampleShading = 1.0f;
    subpass_multisample_state_cis[i].pSampleMask = nullptr;
    subpass_multisample_state_cis[i].alphaToCoverageEnable = VK_FALSE;
    subpass_multisample_state_cis[i].alphaToOneEnable = VK_FALSE;
  }
  VkRenderPassCreateInfo render_pass_ci = {};
  render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_ci.attachmentCount = (uint32_t)attachment_descs.size();
  render_pass_ci.pAttachments = attachment_descs.data();
  render_pass_ci.subpassCount = (uint32_t)subpass_descs.size();
  render_pass_ci.pSubpasses = subpass_descs.data();
  render_pass_ci.dependencyCount = (uint32_t)subpass_dependencies.size();
  render_pass_ci.pDependencies = subpass_dependencies.data();
  VkResult create_result = vkCreateRenderPass(device.Logical(), &render_pass_ci, device.HostAllocator(), &handle);

  // vkBeginRenderPass layers will warn if clearValueCount includes entries that will never be used.
  // So, find the last attachment that's cleared, and only store enough clear values to handle that one.
  clear_values.clear();
  for (size_t i = attachment_descs.size(); i > 0; --i) {
    if (attachment_descs[i - 1].loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR ||
        attachment_descs[i - 1].stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
      clear_values.resize(i);
      break;
    }
  }
  for (size_t i = 0; i < clear_values.size(); ++i) {
    if (IsDepthFormat(attachment_descs[i].format)) {
      clear_values[i] = CreateDepthClearValue(1.0f, 0);
    } else {
      // Technically SINT/UINT formats should use int32/uint32, but clearing the float fields to zero has the same
      // effect either way.
      clear_values[i] = CreateColorClearValue(0, 0, 0, 0);
    }
  }
  begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin_info.renderPass = handle;
  begin_info.framebuffer = VK_NULL_HANDLE;  // must be filled in every frame
  begin_info.renderArea.offset = {0, 0};
  begin_info.renderArea.extent = {0, 0};  // must be filled in every frame
  begin_info.clearValueCount = (uint32_t)clear_values.size();
  begin_info.pClearValues = clear_values.data();

  return create_result;
}

VkImageCreateInfo RenderPass::GetAttachmentImageCreateInfo(uint32_t attachment_index, VkExtent2D render_area) const {
  VkImageCreateInfo ci = {};
  assert(attachment_index < (uint32_t)attachment_descs.size());
  if (handle != VK_NULL_HANDLE && attachment_index < (uint32_t)attachment_descs.size()) {
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = attachment_descs[attachment_index].format;
    ci.extent.width = render_area.width;
    ci.extent.height = render_area.height;
    ci.extent.depth = 1;
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = attachment_descs[attachment_index].samples;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = 0;
    for (const auto& subpass : subpass_attachments) {
      for (const auto& color_ref : subpass.color_refs) {
        if (color_ref.attachment == attachment_index) {
          ci.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
      }
      for (const auto& input_ref : subpass.input_refs) {
        if (input_ref.attachment == attachment_index) {
          ci.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }
      }
      for (const auto& depth_ref : subpass.depth_stencil_refs) {
        if (depth_ref.attachment == attachment_index) {
          ci.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
      }
      for (const auto& resolve_ref : subpass.resolve_refs) {
        if (resolve_ref.attachment == attachment_index) {
          ci.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
      }
    }
    if (attachment_descs[attachment_index].loadOp != VK_ATTACHMENT_LOAD_OP_LOAD &&
        attachment_descs[attachment_index].storeOp != VK_ATTACHMENT_STORE_OP_STORE &&
        attachment_descs[attachment_index].stencilLoadOp != VK_ATTACHMENT_LOAD_OP_LOAD &&
        attachment_descs[attachment_index].stencilStoreOp != VK_ATTACHMENT_STORE_OP_STORE) {
      ci.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = attachment_descs[attachment_index].initialLayout;
  }
  return ci;
}
VkImageViewCreateInfo RenderPass::GetAttachmentImageViewCreateInfo(uint32_t attachment_index, VkImage image) const {
  VkImageViewCreateInfo ci = {};
  if (handle != VK_NULL_HANDLE && attachment_index < (uint32_t)attachment_descs.size()) {
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image = image;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = attachment_descs[attachment_index].format;
    ci.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY};
    ci.subresourceRange.aspectMask = GetImageAspectFlags(ci.format);
  }
  return ci;
}
VkFramebufferCreateInfo RenderPass::GetFramebufferCreateInfo(VkExtent2D render_area) const {
  VkFramebufferCreateInfo ci = {};
  if (handle != VK_NULL_HANDLE) {
    ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass = handle;
    ci.attachmentCount = (uint32_t)attachment_descs.size();
    ci.pAttachments = nullptr;  // Must be filled in by caller
    ci.width = render_area.width;
    ci.height = render_area.height;
    ci.layers = 1;
  }
  return ci;
}

void RenderPass::Destroy(const Device& device) {
  if (handle != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device.Logical(), handle, device.HostAllocator());
    handle = VK_NULL_HANDLE;
  }
  attachment_descs.clear();
  subpass_attachments.clear();
  subpass_dependencies.clear();
  clear_values.clear();
  subpass_descs.clear();
  subpass_multisample_state_cis.clear();
}

}  // namespace spokk