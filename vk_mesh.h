#if !defined(VK_MESH_H)
#define VK_MESH_H

#include "vk_buffer.h"
#include <vector>

namespace spokk {

struct MeshFormat {
  std::vector<VkVertexInputBindingDescription> vertex_buffer_bindings;
  std::vector<VkVertexInputAttributeDescription> vertex_attributes;
  static const MeshFormat* get_empty(VkPrimitiveTopology topology, VkBool32 enable_primitive_restart = VK_FALSE);
  // Call me after filling in attributes and bindings.
  void finalize(VkPrimitiveTopology topology, VkBool32 enable_primitive_restart = VK_FALSE);
  VkPipelineVertexInputStateCreateInfo vertex_input_state_ci;  // used for graphics pipeline creation
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_ci;  // used for graphics pipeline creation
};

// TODO(cort): better abstraction. create/destroy functions?
struct Mesh {
  std::vector<Buffer> vertex_buffers;
  const MeshFormat* mesh_format;
  Buffer index_buffer;
  VkIndexType index_type;
  uint32_t index_count;
};

}  // namespace spokk

#endif // !defined(VK_MESH_H)