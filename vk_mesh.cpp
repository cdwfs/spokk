#include "platform.h"
#include "vk_context.h"
#include "vk_mesh.h"
#include <array>

namespace spokk {

//
// MeshFormat
//
MeshFormat::MeshFormat()
  : vertex_buffer_bindings{},
    vertex_attributes{},
    vertex_input_state_ci{},
    input_assembly_state_ci{} {
}
MeshFormat::MeshFormat(const MeshFormat& rhs) {
  *this = rhs;
}
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
    index_type(VK_INDEX_TYPE_MAX_ENUM),
    index_count(0) {
}

int Mesh::CreateFromFile(const DeviceContext& device_context, const char* mesh_filename) {
  FILE *mesh_file = zomboFopen(mesh_filename, "rb");
  if (mesh_file == nullptr) {
    fprintf(stderr, "Could not open %s for reading\n", mesh_filename);
    return -1;
  }

  MeshFileHeader mesh_header = {};
  size_t read_count = fread(&mesh_header, sizeof(mesh_header), 1, mesh_file);
  if (mesh_header.magic_number != MESH_FILE_MAGIC_NUMBER) {
    fprintf(stderr, "Invalid magic number in %s\n", mesh_filename);
    fclose(mesh_file);
    return -1;
  }

  mesh_format.vertex_buffer_bindings.resize(mesh_header.vertex_buffer_count);
  read_count = fread(mesh_format.vertex_buffer_bindings.data(),
    sizeof(mesh_format.vertex_buffer_bindings[0]), mesh_header.vertex_buffer_count,
    mesh_file);
  mesh_format.vertex_attributes.resize(mesh_header.attribute_count);
  read_count = fread(mesh_format.vertex_attributes.data(),
    sizeof(mesh_format.vertex_attributes[0]), mesh_header.attribute_count,
    mesh_file);

  // Load VB, IB
  std::vector<uint8_t> vertices(mesh_header.vertex_count * mesh_format.vertex_buffer_bindings[0].stride);
  read_count = fread(vertices.data(), mesh_format.vertex_buffer_bindings[0].stride,
    mesh_header.vertex_count, mesh_file);
  std::vector<uint8_t> indices(mesh_header.index_count * mesh_header.bytes_per_index);
  read_count = fread(indices.data(), mesh_header.bytes_per_index, mesh_header.index_count, mesh_file);
  fclose(mesh_file);

  mesh_format.Finalize(mesh_header.topology);
  if (mesh_header.bytes_per_index == 2) {
    index_type = VK_INDEX_TYPE_UINT16;
  } else if (mesh_header.bytes_per_index == 4) {
    index_type = VK_INDEX_TYPE_UINT32;
  } else {
    ZOMBO_ERROR_RETURN(-1, "Invalid index size %u in mesh %s", mesh_header.bytes_per_index, mesh_filename);
  }
  index_count = mesh_header.index_count;
  // create and populate Buffer objects.
  VkBufferCreateInfo index_buffer_ci = {};
  index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  index_buffer_ci.size = indices.size();
  index_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  index_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  index_buffer.Create(device_context, index_buffer_ci);
  index_buffer.Load(device_context, indices.data(), indices.size());
  vertex_buffers.resize(mesh_header.vertex_buffer_count, {});
  for(uint32_t iVB = 0; iVB < mesh_header.vertex_buffer_count; ++iVB) {
    VkBufferCreateInfo vertex_buffer_ci = {};
    vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_ci.size = vertices.size();
    vertex_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertex_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vertex_buffers[iVB].Create(device_context, vertex_buffer_ci);
    vertex_buffers[iVB].Load(device_context, vertices.data(), vertices.size());
  }

  return 0;
}

void Mesh::Destroy(const DeviceContext& device_context) {
  for(auto& vb : vertex_buffers) {
    vb.Destroy(device_context);
  }
  vertex_buffers.clear();
  index_buffer.Destroy(device_context);
  index_count = 0;
}

}  // namespace spokk
