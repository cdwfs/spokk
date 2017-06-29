#include "platform.h"
#include "vk_context.h"
#include "vk_mesh.h"
#include <array>

namespace spokk {

namespace {
const std::array<MeshFormat, VK_PRIMITIVE_TOPOLOGY_RANGE_SIZE * 2> g_empty_mesh_formats = {{
    // Primitive restart disabled
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, VK_FALSE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_FALSE} },
  // Primitive restart enabled
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, VK_TRUE} },
  { {}, {}, {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,nullptr,0, 0,nullptr, 0,nullptr}, {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,nullptr,0, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_TRUE} },
  }};
}  // namespace

//
// MeshFormat
//
const MeshFormat* MeshFormat::GetEmpty(VkPrimitiveTopology topology, VkBool32 enable_primitive_restart) {
  uint32_t index = topology;
  if (enable_primitive_restart) {
    index += VK_PRIMITIVE_TOPOLOGY_RANGE_SIZE;
  }
  return &g_empty_mesh_formats[index];
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

int LoadMeshFromFile(const DeviceContext& device_context, const char* mesh_filename,
    Mesh* out_mesh, MeshFormat* out_mesh_format) {
  FILE *mesh_file = zomboFopen(mesh_filename, "rb");
  if (mesh_file == nullptr) {
    fprintf(stderr, "Could not open %s for reading\n", mesh_filename);
    return -1;
  }

  struct {
    uint32_t magic_number;
    uint32_t vertex_buffer_count;
    uint32_t attribute_count;
    uint32_t bytes_per_index;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t topology; // currently ignored, assume triangles
  } mesh_header = {};
  size_t read_count = fread(&mesh_header, sizeof(mesh_header), 1, mesh_file);
  if (mesh_header.magic_number != 0x12345678) {
    fprintf(stderr, "Invalid magic number in %s\n", mesh_filename);
    fclose(mesh_file);
    return -1;
  }
  ZOMBO_ASSERT(mesh_header.topology == 1, "topology is currently hard-coded to tri lists");

  *out_mesh_format = {};
  out_mesh_format->vertex_buffer_bindings.resize(mesh_header.vertex_buffer_count);
  read_count = fread(out_mesh_format->vertex_buffer_bindings.data(),
    sizeof(out_mesh_format->vertex_buffer_bindings[0]), mesh_header.vertex_buffer_count,
    mesh_file);
  out_mesh_format->vertex_attributes.resize(mesh_header.attribute_count);
  read_count = fread(out_mesh_format->vertex_attributes.data(),
    sizeof(out_mesh_format->vertex_attributes[0]), mesh_header.attribute_count,
    mesh_file);

  // Load VB, IB
  std::vector<uint8_t> vertices(mesh_header.vertex_count * out_mesh_format->vertex_buffer_bindings[0].stride);
  read_count = fread(vertices.data(), out_mesh_format->vertex_buffer_bindings[0].stride,
    mesh_header.vertex_count, mesh_file);
  std::vector<uint8_t> indices(mesh_header.index_count * mesh_header.bytes_per_index);
  read_count = fread(indices.data(), mesh_header.bytes_per_index, mesh_header.index_count, mesh_file);
  fclose(mesh_file);

  out_mesh_format->Finalize(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  *out_mesh = {};
  out_mesh->mesh_format = out_mesh_format;
  if (mesh_header.bytes_per_index == 2) {
    out_mesh->index_type = VK_INDEX_TYPE_UINT16;
  } else if (mesh_header.bytes_per_index == 4) {
    out_mesh->index_type = VK_INDEX_TYPE_UINT32;
  } else {
    ZOMBO_ERROR_RETURN(-1, "Invalid index size %u in mesh %s", mesh_header.bytes_per_index, mesh_filename);
  }
  out_mesh->index_count = mesh_header.index_count;
  // create and populate Buffer objects.
  VkBufferCreateInfo index_buffer_ci = {};
  index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  index_buffer_ci.size = indices.size();
  index_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  index_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  out_mesh->index_buffer.Create(device_context, index_buffer_ci);
  out_mesh->index_buffer.Load(device_context, indices.data(), indices.size());
  out_mesh->vertex_buffers.resize(mesh_header.vertex_buffer_count, {});
  for(uint32_t iVB = 0; iVB < mesh_header.vertex_buffer_count; ++iVB) {
    VkBufferCreateInfo vertex_buffer_ci = {};
    vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertex_buffer_ci.size = vertices.size();
    vertex_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vertex_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    out_mesh->vertex_buffers[iVB].Create(device_context, vertex_buffer_ci);
    out_mesh->vertex_buffers[iVB].Load(device_context, vertices.data(), vertices.size());
  }

  return 0;
}


}  // namespace spokk
