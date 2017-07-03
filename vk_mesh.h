#if !defined(VK_MESH_H)
#define VK_MESH_H

#include "vk_buffer.h"
#include <vector>

namespace spokk {

class DeviceContext;

struct MeshFormat {
  MeshFormat();
  MeshFormat(const MeshFormat& rhs);
  MeshFormat& operator=(const MeshFormat& rhs);

  // Fill in these arrays manually...
  std::vector<VkVertexInputBindingDescription> vertex_buffer_bindings;
  std::vector<VkVertexInputAttributeDescription> vertex_attributes;
  // ...and all me after filling in attributes and bindings.
  void Finalize(VkPrimitiveTopology topology, VkBool32 enable_primitive_restart = VK_FALSE);

  // These are filled in during Finalize(), and should not be modified manually.
  VkPipelineVertexInputStateCreateInfo vertex_input_state_ci;  // used for graphics pipeline creation
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_ci;  // used for graphics pipeline creation
};

struct Mesh {
  Mesh();
  int CreateFromFile(const DeviceContext& device_context, const char* mesh_filename);
  void Destroy(const DeviceContext& device_context);

  std::vector<Buffer> vertex_buffers;
  MeshFormat mesh_format;
  Buffer index_buffer;
  VkIndexType index_type;
  uint32_t index_count;
private:
  Mesh(const Mesh& rhs) = delete;
  Mesh& operator=(const Mesh& rhs) = delete;
};

// These don't belong here; need a place for shared runtime/tools declarations.
constexpr uint32_t MESH_FILE_MAGIC_NUMBER = 0x4853454D;
struct MeshFileHeader {
  uint32_t magic_number;
  uint32_t vertex_buffer_count;
  uint32_t attribute_count;
  uint32_t bytes_per_index;
  uint32_t vertex_count;
  uint32_t index_count;
  VkPrimitiveTopology topology;
  float aabb_min[3];
  float aabb_max[3];
};

}  // namespace spokk

#endif // !defined(VK_MESH_H)