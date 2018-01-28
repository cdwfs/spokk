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
};

struct Mesh {
  Mesh();
  int CreateFromFile(const Device& device, const char* mesh_filename);
  void Destroy(const Device& device);

  // Helper to bind all vertex buffers and index buffers
  void BindBuffers(VkCommandBuffer cb) const;

  std::vector<Buffer> vertex_buffers;
  MeshFormat mesh_format;
  Buffer index_buffer;
  uint32_t total_vertex_count;
  uint32_t total_index_count; // across all segments -- do not use for draw calls!
  VkIndexType index_type;
  VkPrimitiveTopology topology;

  struct Segment {
    VkDrawIndexedIndirectCommand draw_data; // indices and counts to draw each segment
    // TODO(cort): AABB?
  };
  std::vector<Segment> segments;

  // Handy arrays of buffer offsets, to avoid allocating them for every bind call
  std::vector<VkDeviceSize> vertex_buffer_byte_offsets;
  VkDeviceSize index_buffer_byte_offset;

private:
  Mesh(const Mesh& rhs) = delete;
  Mesh& operator=(const Mesh& rhs) = delete;
};

// Handy debug meshes
void GenerateMeshBox(const Device& device, Mesh* out_mesh, const float min_extent[3], const float max_extent[3]);

// These don't belong here; need a place for shared runtime/tools declarations.
constexpr uint32_t MESH_FILE_MAGIC_NUMBER = 0x4853454D;
struct MeshFileHeader {
  uint32_t magic_number;
  uint32_t vertex_buffer_count;
  uint32_t attribute_count;
  uint32_t bytes_per_index;
  uint32_t vertex_count;
  uint32_t index_count;
  uint32_t segment_count;
  VkPrimitiveTopology topology;
};

}  // namespace spokk
