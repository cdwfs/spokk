#if !defined(VK_PIPELINE_H)
#define VK_PIPELINE_H

#include "vk_context.h"
#include "vk_mesh.h"
#include "vk_renderpass.h"
#include "vk_shader.h"

namespace spokk {

struct ComputePipeline {
  ComputePipeline();
  ComputePipeline(const ComputePipeline& rhs) = delete;
  ComputePipeline& operator=(const ComputePipeline& rhs) = delete;

  VkResult Create(const DeviceContext& device_context, const ShaderPipeline *shader_pipeline, bool defer_pipeline_creation = false);
  void Destroy(const DeviceContext& device_context);

  VkPipeline handle;
  const ShaderPipeline *shader_pipeline;
  VkComputePipelineCreateInfo ci;
};

struct GraphicsPipeline {
  GraphicsPipeline();
  GraphicsPipeline(const GraphicsPipeline& rhs) = delete;
  GraphicsPipeline& operator=(const GraphicsPipeline& rhs) = delete;

  VkResult Create(const DeviceContext& device_context, const MeshFormat *mesh_format, const ShaderPipeline *shader_pipeline, const RenderPass *render_pass, uint32_t subpass,
    const std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT},
    const VkViewport viewport = {}, const VkRect2D scissor_rect = {}, bool defer_pipeline_creation = false);
  void Destroy(const DeviceContext& device_context);

  VkPipeline handle;

  const MeshFormat *mesh_format;
  const ShaderPipeline *shader_pipeline;
  const RenderPass *render_pass;
  uint32_t subpass;
  std::vector<VkDynamicState> dynamic_states;

  VkGraphicsPipelineCreateInfo ci;
  VkPipelineTessellationStateCreateInfo tessellation_state_ci;
  VkPipelineViewportStateCreateInfo viewport_state_ci;
  std::vector<VkViewport> viewports;
  std::vector<VkRect2D> scissor_rects;
  VkPipelineRasterizationStateCreateInfo rasterization_state_ci;
  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_ci;
  VkPipelineColorBlendStateCreateInfo color_blend_state_ci;
  std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachment_states;
  VkPipelineDynamicStateCreateInfo dynamic_state_ci;
};

}  // namespace spokk

#endif  // VK_PIPELINE_H
