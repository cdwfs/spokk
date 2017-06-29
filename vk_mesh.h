#if !defined(VK_MESH_H)
#define VK_MESH_H

#include "vk_buffer.h"
#include <vector>

namespace spokk {

class DeviceContext;

struct MeshFormat {
  // Fill in these arrays manually...
  std::vector<VkVertexInputBindingDescription> vertex_buffer_bindings;
  std::vector<VkVertexInputAttributeDescription> vertex_attributes;
  // ...and all me after filling in attributes and bindings.
  void Finalize(VkPrimitiveTopology topology, VkBool32 enable_primitive_restart = VK_FALSE);
  // Or use one of these functions to get a pre-populated, pre-Finalized format with common settings.
  static const MeshFormat* GetEmpty(VkPrimitiveTopology topology, VkBool32 enable_primitive_restart = VK_FALSE);

  // These are filled in during Finalize(), and should not be modified manually.
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

int LoadMeshFromFile(const DeviceContext& device_context, const char* mesh_filename,
  Mesh* out_mesh, MeshFormat* out_mesh_format);

}  // namespace spokk

#endif // !defined(VK_MESH_H)