#pragma once

#include "spokk_buffer.h"

#include <vector>

namespace spokk {

class Device;

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
  int CreateFromFile(const Device& device, const char* mesh_filename);
  void Destroy(const Device& device);

  // Helper to bind all vertex buffers and index buffers, and draw all indices
  void BindBuffersAndDraw(VkCommandBuffer cb, uint32_t index_cnt, uint32_t instance_cnt = 1, uint32_t first_index = 0,
      uint32_t vertex_offset = 0, uint32_t first_instance = 0) const;

  std::vector<Buffer> vertex_buffers;
  MeshFormat mesh_format;
  Buffer index_buffer;
  uint32_t vertex_count;
  uint32_t index_count;
  VkIndexType index_type;

  // Handy arrays of buffer offsets, to avoid allocating them for every bind call
  std::vector<VkDeviceSize> vertex_buffer_byte_offsets;
  VkDeviceSize index_buffer_byte_offset;

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
