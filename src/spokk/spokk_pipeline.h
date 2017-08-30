#pragma once

#include "spokk_device.h"
#include "spokk_mesh.h"
#include "spokk_renderpass.h"
#include "spokk_shader.h"

namespace spokk {

// TODO(https://github.com/cdwfs/spokk/issues/26): refactor as a factory
struct ComputePipeline {
  ComputePipeline();
  ComputePipeline(const ComputePipeline& rhs) = delete;
  ComputePipeline& operator=(const ComputePipeline& rhs) = delete;

  void Init(const ShaderProgram* shader_program);
  VkResult Finalize(const Device& device);
  void Destroy(const Device& device);

  VkPipeline handle;
  const ShaderProgram* shader_program;
  VkComputePipelineCreateInfo ci;
};

// TODO(https://github.com/cdwfs/spokk/issues/26): refactor as a factory
struct GraphicsPipeline {
  GraphicsPipeline();
  GraphicsPipeline(const GraphicsPipeline& rhs) = delete;
  GraphicsPipeline& operator=(const GraphicsPipeline& rhs) = delete;

  void Init(const MeshFormat* mesh_format, const ShaderProgram* shader_program, const RenderPass* render_pass,
      uint32_t subpass,
      const std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT},
      const VkViewport viewport = {}, const VkRect2D scissor_rect = {});
  VkResult Finalize(const Device& device);
  void Destroy(const Device& device);

  VkPipeline handle;

  const MeshFormat* mesh_format;
  const ShaderProgram* shader_program;
  const RenderPass* render_pass;
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
