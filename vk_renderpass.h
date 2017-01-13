#if !defined(VK_RENDERPASS_H)
#define VK_RENDERPASS_H

#include "vk_context.h"

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
  std::vector<VkAttachmentDescription> attachment_descs;
  std::vector<SubpassAttachments> subpass_attachments;
  std::vector<VkSubpassDependency> subpass_dependencies;
  // ...or call this to populate the arrays using common presets...
  // TODO(cort): This is not satisfying. Needs limited customizability -- format preferences for various use cases,
  // sample counts, a way to decouple framebuffer and target image creation from renderpass configuration, etc.
  // Maybe a RenderPass can generate its own FramebufferCreateInfos and VkImageCreateInfos
  enum Preset {
    COLOR            = 1,  // 1 subpass; color (clear -> store)
    COLOR_DEPTH      = 2,  // 1 subpass; color (clear -> store), depth (clear -> dontcare)
    COLOR_POST       = 3,  // 2 subpass; color (clear -> dontcare); final color (dontcare -> store)
    COLOR_DEPTH_POST = 4,  // 2 subpass; color (clear -> store), depth (clear -> dontcare); final color (dontcare -> store)
  };
  void InitFromPreset(Preset preset, VkFormat output_color_format);
  // ...and call this after populating the previous vectors.
  VkResult Finalize(const DeviceContext& device_context, VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS,
    VkSubpassDescriptionFlags flags = 0);

  VkImageCreateInfo GetAttachmentImageCreateInfo(uint32_t attachment_index, const VkExtent2D& render_area) const;
  VkImageViewCreateInfo GetAttachmentImageViewCreateInfo(uint32_t attachment_index, VkImage image) const;
  VkFramebufferCreateInfo GetFramebufferCreateInfo(const VkExtent2D& render_area) const;
  void Destroy(const DeviceContext& device_context);

  // These are created during finalization
  VkRenderPass handle;
  std::vector<VkSubpassDescription> subpass_descs;
  std::vector<VkPipelineMultisampleStateCreateInfo> subpass_multisample_state_cis;
};

}  // namespace spokk

#endif  // !defined(VK_RENDERPASS_H)