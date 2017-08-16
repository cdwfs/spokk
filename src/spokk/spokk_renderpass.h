#pragma once

#include "spokk_device.h"

#include <vector>

namespace spokk {

struct SubpassAttachments {
  std::vector<VkAttachmentReference> input_refs;
  std::vector<VkAttachmentReference> color_refs;
  std::vector<VkAttachmentReference> resolve_refs;
  std::vector<VkAttachmentReference> depth_stencil_refs;  // max length of 1
  std::vector<uint32_t> preserve_indices;
};

struct RenderPass {
  // Fill in these arrays manually...
  std::vector<VkAttachmentDescription> attachment_descs;  // one per attachment,
  std::vector<SubpassAttachments> subpass_attachments;  // one per subpass,
  std::vector<VkSubpassDependency>
      subpass_dependencies;  // one per dependency between subpasses (and/or previous/next render passes).
  // ...or call this to populate the arrays using common presets...
  // TODO(cort): This is not satisfying. Needs limited customizability -- format preferences for various use cases,
  // sample counts, a way to decouple framebuffer and target image creation from renderpass configuration, etc.
  // Maybe a RenderPass can generate its own FramebufferCreateInfos and VkImageCreateInfos
  enum Preset {
    COLOR = 1,  // 1 subpass; color (clear -> store)
    COLOR_DEPTH = 2,  // 1 subpass; color (clear -> store), depth (clear -> dontcare)
    COLOR_POST = 3,  // 2 subpass; color (clear -> dontcare); final color (dontcare -> store)
    COLOR_DEPTH_POST = 4,  // 2 subpass; color (clear -> store), depth (clear -> dontcare); color (dontcare -> store)
  };
  void InitFromPreset(Preset preset, VkFormat output_color_format);
  // ...and call this after populating the previous vectors.
  VkResult Finalize(const Device& device, VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS,
      VkSubpassDescriptionFlags flags = 0);

  VkImageCreateInfo GetAttachmentImageCreateInfo(uint32_t attachment_index, VkExtent2D render_area) const;
  VkImageViewCreateInfo GetAttachmentImageViewCreateInfo(uint32_t attachment_index, VkImage image) const;
  VkFramebufferCreateInfo GetFramebufferCreateInfo(VkExtent2D render_area) const;

  void Destroy(const Device& device);

  // These are created during finalization
  VkRenderPass handle;
  std::vector<VkClearValue> clear_values;
  VkRenderPassBeginInfo begin_info;  // Caller must fill in framebuffer and renderArea.extent!
  std::vector<VkSubpassDescription> subpass_descs;
  std::vector<VkPipelineMultisampleStateCreateInfo> subpass_multisample_state_cis;
};

}  // namespace spokk
