#include "spokk_mesh.h"
#include "spokk_debug.h"
#include "spokk_device.h"
#include "spokk_platform.h"
#include "spokk_shader_interface.h"

#include <array>

namespace spokk {

//
// MeshFormat
//
MeshFormat::MeshFormat()
  : vertex_buffer_bindings{}, vertex_attributes{}, vertex_input_state_ci{}, input_assembly_state_ci{} {}
MeshFormat::MeshFormat(const MeshFormat& rhs) { *this = rhs; }
MeshFormat& MeshFormat::operator=(const MeshFormat& rhs) {
  vertex_buffer_bindings = rhs.vertex_buffer_bindings;
  vertex_attributes = rhs.vertex_attributes;
  vertex_input_state_ci = rhs.vertex_input_state_ci;
  vertex_input_state_ci.pVertexAttributeDescriptions = vertex_attributes.data();
  vertex_input_state_ci.pVertexBindingDescriptions = vertex_buffer_bindings.data();
  input_assembly_state_ci = rhs.input_assembly_state_ci;
  return *this;
}

void MeshFormat::Finalize(VkPrimitiveTopology topology, VkBool32 enable_primitive_restart) {
  vertex_input_state_ci = {};
  vertex_input_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_state_ci.vertexBindingDescriptionCount = (uint32_t)vertex_buffer_bindings.size();
  vertex_input_state_ci.pVertexBindingDescriptions = vertex_buffer_bindings.data();
  vertex_input_state_ci.vertexAttributeDescriptionCount = (uint32_t)vertex_attributes.size();
  vertex_input_state_ci.pVertexAttributeDescriptions = vertex_attributes.data();
  input_assembly_state_ci = {};
  input_assembly_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_state_ci.topology = topology;
  input_assembly_state_ci.primitiveRestartEnable = enable_primitive_restart;
}

//
// Mesh
//
Mesh::Mesh()
  : vertex_buffers{},
    mesh_format{},
    index_buffer{},
    vertex_count(0),
    index_count(0),
    index_type(VK_INDEX_TYPE_MAX_ENUM) {}

int Mesh::CreateFromFile(const Device& device, const char* mesh_filename) {
  FILE* mesh_file = zomboFopen(mesh_filename, "rb");
  if (mesh_file == nullptr) {
    fprintf(stderr, "Could not open %s for reading\n", mesh_filename);
    return -1;
  }

  MeshFileHeader mesh_header = {};
  size_t read_count = fread(&mesh_header, sizeof(mesh_header), 1, mesh_file);
  if (read_count != 1) {
    fprintf(stderr, "I/O error while reading %s\n", mesh_filename);
    fclose(mesh_file);
    return -1;
  }
  if (mesh_header.magic_number != MESH_FILE_MAGIC_NUMBER) {
    fprintf(stderr, "Invalid magic number in %s\n", mesh_filename);
    fclose(mesh_file);
    return -1;
  }

  mesh_format.vertex_buffer_bindings.resize(mesh_header.vertex_buffer_count);
  read_count = fread(mesh_format.vertex_buffer_bindings.data(), sizeof(mesh_format.vertex_buffer_bindings[0]),
      mesh_header.vertex_buffer_count, mesh_file);
  mesh_format.vertex_attributes.resize(mesh_header.attribute_count);
  read_count = fread(mesh_format.vertex_attributes.data(), sizeof(mesh_format.vertex_attributes[0]),
      mesh_header.attribute_count, mesh_file);
  ZOMBO_ASSERT(read_count == mesh_header.attribute_count, "I/O error while reading %s", mesh_filename);

  // Load VB, IB
  std::vector<uint8_t> vertices(mesh_header.vertex_count * mesh_format.vertex_buffer_bindings[0].stride);
  read_count =
      fread(vertices.data(), mesh_format.vertex_buffer_bindings[0].stride, mesh_header.vertex_count, mesh_file);
  ZOMBO_ASSERT(read_count == mesh_header.vertex_count, "I/O error while reading %s", mesh_filename);
  std::vector<uint8_t> indices(mesh_header.index_count * mesh_header.bytes_per_index);
  read_count = fread(indices.data(), mesh_header.bytes_per_index, mesh_header.index_count, mesh_file);
  ZOMBO_ASSERT(read_count == mesh_header.index_count, "I/O error while reading %s", mesh_filename);
  fclose(mesh_file);

  mesh_format.Finalize(mesh_header.topology);
  if (mesh_header.bytes_per_index == 2) {
    index_type = VK_INDEX_TYPE_UINT16;
  } else if (mesh_header.bytes_per_index == 4) {
    index_type = VK_INDEX_TYPE_UINT32;
  } else {
    ZOMBO_ERROR_RETURN(-1, "Invalid index size %u in mesh %s", mesh_header.bytes_per_index, mesh_filename);
  }
  vertex_count = mesh_header.vertex_count;
  index_count = mesh_header.index_count;
  // create and populate Buffer objects.
  VkBufferCreateInfo index_buffer_ci = {};
  index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  index_buffer_ci.size = indices.size();
  index_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  index_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(index_buffer.Create(device, index_buffer_ci));
  SPOKK_VK_CHECK(index_buffer.Load(device, indices.data(), indices.size()));
  vertex_buffers.resize(mesh_header.vertex_buffer_count, {});
  for (uint32_t iVB = 0; iVB < mesh_header.vertex_buffer_count; ++iVB) {
    VkBufferCreateInfo vertex_buffer_ci = {};
    vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_ci.size = vertices.size();
    vertex_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertex_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SPOKK_VK_CHECK(vertex_buffers[iVB].Create(device, vertex_buffer_ci));
    SPOKK_VK_CHECK(vertex_buffers[iVB].Load(device, vertices.data(), vertices.size()));
  }

  // Populate buffer offsets
  vertex_buffer_byte_offsets.resize(vertex_buffers.size());
  for (size_t i = 0; i < vertex_buffers.size(); ++i) {
    vertex_buffer_byte_offsets[i] = 0;
  }
  index_buffer_byte_offset = 0;

  return 0;
}

void Mesh::Destroy(const Device& device) {
  for (auto& vb : vertex_buffers) {
    vb.Destroy(device);
  }
  vertex_buffers.clear();
  index_buffer.Destroy(device);
  index_count = 0;
}

void Mesh::BindBuffers(VkCommandBuffer cb) const {
  ZOMBO_ASSERT((uint32_t)vertex_buffers.size() == mesh_format.vertex_input_state_ci.vertexBindingDescriptionCount,
      "Mesh's vertex buffer count (%u) does not match count in MeshFormat (%u)", (uint32_t)vertex_buffers.size(),
      mesh_format.vertex_input_state_ci.vertexBindingDescriptionCount);
  for (uint32_t i = 0; i < mesh_format.vertex_input_state_ci.vertexBindingDescriptionCount; ++i) {
    VkBuffer handle = vertex_buffers[i].Handle();
    vkCmdBindVertexBuffers(
        cb, mesh_format.vertex_buffer_bindings[i].binding, 1, &handle, &vertex_buffer_byte_offsets[i]);
  }
  vkCmdBindIndexBuffer(cb, index_buffer.Handle(), index_buffer_byte_offset, index_type);
}

////////////////////////////

struct DebugMeshVertex {
  float px, py, pz;
  float nx, ny, nz;
  float tu, tv;
};

void GenerateMeshBox(const Device& device, Mesh* out_mesh, const float min_extent[3], const float max_extent[3]) {
  out_mesh->mesh_format.vertex_attributes = {
      // clang-format off
    {SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION,  0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(DebugMeshVertex, px) },
    {SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL,    0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(DebugMeshVertex, nx) },
    {SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD0, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(DebugMeshVertex, tu) },
      // clang-format on
  };
  out_mesh->mesh_format.vertex_buffer_bindings.resize(1);
  out_mesh->mesh_format.vertex_buffer_bindings[0].binding = 0;
  out_mesh->mesh_format.vertex_buffer_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  out_mesh->mesh_format.vertex_buffer_bindings[0].stride = sizeof(DebugMeshVertex);
  out_mesh->mesh_format.Finalize(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  const std::vector<DebugMeshVertex> vertices = {
      // clang-format off
    {min_extent[0], min_extent[1], min_extent[2],   -1, 0, 0,   0,0},  // -X
    {min_extent[0], min_extent[1], max_extent[2],   -1, 0, 0,   1,0},
    {min_extent[0], max_extent[1], min_extent[2],   -1, 0, 0,   0,1},
    {min_extent[0], max_extent[1], max_extent[2],   -1, 0, 0,   1,1},
    {max_extent[0], min_extent[1], max_extent[2],   +1, 0, 0,   0,0},  // +X
    {max_extent[0], min_extent[1], min_extent[2],   +1, 0, 0,   1,0},
    {max_extent[0], max_extent[1], max_extent[2],   +1, 0, 0,   0,1},
    {max_extent[0], max_extent[1], min_extent[2],   +1, 0, 0,   1,1},
    {min_extent[0], min_extent[1], min_extent[2],    0,-1, 0,   0,0},  // -Y
    {max_extent[0], min_extent[1], min_extent[2],    0,-1, 0,   1,0},
    {min_extent[0], min_extent[1], max_extent[2],    0,-1, 0,   0,1},
    {max_extent[0], min_extent[1], max_extent[2],    0,-1, 0,   1,1},
    {min_extent[0], max_extent[1], max_extent[2],    0,+1, 0,   0,0},  // +Y
    {max_extent[0], max_extent[1], max_extent[2],    0,+1, 0,   1,0},
    {min_extent[0], max_extent[1], min_extent[2],    0,+1, 0,   0,1},
    {max_extent[0], max_extent[1], min_extent[2],    0,+1, 0,   1,1},
    {max_extent[0], min_extent[1], min_extent[2],    0, 0,-1,   0,0},  // -Z
    {min_extent[0], min_extent[1], min_extent[2],    0, 0,-1,   1,0},
    {max_extent[0], max_extent[1], min_extent[2],    0, 0,-1,   0,1},
    {min_extent[0], max_extent[1], min_extent[2],    0, 0,-1,   1,1},
    {min_extent[0], min_extent[1], max_extent[2],    0, 0,+1,   0,0},  // +Z
    {max_extent[0], min_extent[1], max_extent[2],    0, 0,+1,   1,0},
    {min_extent[0], max_extent[1], max_extent[2],    0, 0,+1,   0,1},
    {max_extent[0], max_extent[1], max_extent[2],    0, 0,+1,   1,1},
      // clang-format on
  };
  VkBufferCreateInfo vb_ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  vb_ci.size = vertices.size() * sizeof(vertices[0]);
  vb_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  vb_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  out_mesh->vertex_buffers.resize(1);
  SPOKK_VK_CHECK(out_mesh->vertex_buffers[0].Create(device, vb_ci));
  SPOKK_VK_CHECK(out_mesh->vertex_buffers[0].Load(device, vertices.data(), vb_ci.size));
  out_mesh->vertex_buffer_byte_offsets = {0};
  out_mesh->vertex_count = (uint32_t)vertices.size();

  const std::vector<uint16_t> indices = {
      // clang-format off
     0, 1, 2,   2, 1, 3,
     4, 5, 6,   6, 5, 7,
     8, 9,10,  10, 9,11,
    12,13,14,  14,13,15,
    16,17,18,  18,17,19,
    20,21,22,  22,21,23,
    // clang-format off
  };
  VkBufferCreateInfo ib_ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  ib_ci.size = indices.size() * sizeof(indices[0]);
  ib_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  ib_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(out_mesh->index_buffer.Create(device, ib_ci));
  SPOKK_VK_CHECK(out_mesh->index_buffer.Load(device, indices.data(), ib_ci.size));
  out_mesh->index_buffer_byte_offset = 0;
  out_mesh->index_count = (uint32_t)indices.size();
  out_mesh->index_type = VK_INDEX_TYPE_UINT16;
}


}  // namespace spokk

