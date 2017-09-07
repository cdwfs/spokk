#include "spokk_pipeline.h"

#include <cassert>

namespace spokk {

//
// ComputePipeline
//
ComputePipeline::ComputePipeline() : handle(VK_NULL_HANDLE), shader_program(nullptr), ci{} {}
void ComputePipeline::Init(const ShaderProgram* shader_program_in) {
  this->shader_program = shader_program_in;
  assert(shader_program->shader_stage_cis.size() == 1);
  assert(shader_program->shader_stage_cis[0].stage == VK_SHADER_STAGE_COMPUTE_BIT);

  ci = {};
  ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  ci.flags = 0;  // TODO(cort): pass these in?
  ci.stage = shader_program->shader_stage_cis[0];
  ci.layout = shader_program->pipeline_layout;
  ci.basePipelineHandle = VK_NULL_HANDLE;
  ci.basePipelineIndex = 0;
}
VkResult ComputePipeline::Finalize(const Device& device) {
  return vkCreateComputePipelines(device, device.PipelineCache(), 1, &ci, device.HostAllocator(), &handle);
}
void ComputePipeline::Destroy(const Device& device) {
  if (handle != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, handle, device.HostAllocator());
    handle = VK_NULL_HANDLE;
  }
  shader_program = nullptr;
}

//
// GraphicsPipeline
//
GraphicsPipeline::GraphicsPipeline()
  : handle(VK_NULL_HANDLE),
    mesh_format(nullptr),
    shader_program(nullptr),
    render_pass(nullptr),
    subpass(0),
    dynamic_states{},
    ci{},
    tessellation_state_ci{},
    viewport_state_ci{},
    viewports{},
    scissor_rects{},
    rasterization_state_ci{},
    depth_stencil_state_ci{},
    color_blend_state_ci{},
    color_blend_attachment_states{},
    dynamic_state_ci{} {}
void GraphicsPipeline::Init(const MeshFormat* mesh_format_in, const ShaderProgram* shader_program_in,
    const RenderPass* render_pass_in, uint32_t subpass_in, const std::vector<VkDynamicState> dynamic_states_in,
    const VkViewport viewport, const VkRect2D scissor_rect) {
  this->mesh_format = mesh_format_in;
  this->shader_program = shader_program_in;
  this->render_pass = render_pass_in;
  this->subpass = subpass_in;
  this->dynamic_states = std::move(dynamic_states_in);

  tessellation_state_ci = {};
  tessellation_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

  viewports.resize(1);
  viewports[0] = viewport;
  scissor_rects.resize(1);
  scissor_rects[0] = scissor_rect;
  viewport_state_ci = {};
  viewport_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state_ci.viewportCount = (uint32_t)viewports.size();
  viewport_state_ci.pViewports = viewports.data();
  viewport_state_ci.scissorCount = (uint32_t)scissor_rects.size();
  viewport_state_ci.pScissors = scissor_rects.data();

  rasterization_state_ci = {};
  rasterization_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization_state_ci.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization_state_ci.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterization_state_ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterization_state_ci.lineWidth = 1.0f;

  VkBool32 subpass_has_depth_attachment =
      (render_pass->subpass_descs[subpass].pDepthStencilAttachment != nullptr) ? VK_TRUE : VK_FALSE;
  depth_stencil_state_ci = {};
  depth_stencil_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_state_ci.depthTestEnable = subpass_has_depth_attachment;
  depth_stencil_state_ci.depthWriteEnable = subpass_has_depth_attachment &&
      render_pass->subpass_descs[subpass].pDepthStencilAttachment->layout ==
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  depth_stencil_state_ci.depthCompareOp = VK_COMPARE_OP_LESS;

  color_blend_attachment_states.resize(render_pass->subpass_descs[subpass].colorAttachmentCount);
  for (auto& attachment : color_blend_attachment_states) {
    attachment = {};
    attachment.blendEnable = VK_FALSE;
    attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  }
  color_blend_state_ci = {};
  color_blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend_state_ci.attachmentCount = (uint32_t)color_blend_attachment_states.size();
  color_blend_state_ci.pAttachments = color_blend_attachment_states.data();

  dynamic_state_ci = {};
  dynamic_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state_ci.dynamicStateCount = (uint32_t)dynamic_states.size();
  dynamic_state_ci.pDynamicStates = dynamic_states.data();

  ci = {};
  ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  ci.flags = 0;  // TODO(cort): pass these in?
  ci.stageCount = (uint32_t)shader_program->shader_stage_cis.size();
  ci.pStages = shader_program->shader_stage_cis.data();
  ci.pVertexInputState = &(mesh_format->vertex_input_state_ci);
  ci.pInputAssemblyState = &(mesh_format->input_assembly_state_ci);
  ci.pTessellationState = (shader_program->active_stages &
                              (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
      ? &tessellation_state_ci
      : nullptr;
  ci.pViewportState = &viewport_state_ci;
  ci.pRasterizationState = &rasterization_state_ci;
  ci.pMultisampleState = &(render_pass->subpass_multisample_state_cis[subpass]);
  ci.pDepthStencilState = &depth_stencil_state_ci;
  ci.pColorBlendState = &color_blend_state_ci;
  ci.pDynamicState = (dynamic_state_ci.dynamicStateCount > 0) ? &dynamic_state_ci : nullptr;
  ci.layout = shader_program->pipeline_layout;
  ci.renderPass = render_pass->handle;
  ci.subpass = subpass;
  ci.basePipelineHandle = VK_NULL_HANDLE;
  ci.basePipelineIndex = 0;
}
VkResult GraphicsPipeline::Finalize(const Device& device) {
  return vkCreateGraphicsPipelines(device, device.PipelineCache(), 1, &ci, device.HostAllocator(), &handle);
}
void GraphicsPipeline::Destroy(const Device& device) {
  if (handle != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, handle, device.HostAllocator());
    handle = VK_NULL_HANDLE;
  }
  dynamic_states.clear();
  viewports.clear();
  scissor_rects.clear();
  mesh_format = nullptr;
  shader_program = nullptr;
  render_pass = nullptr;
  subpass = 0;
}

}  // namespace spokk