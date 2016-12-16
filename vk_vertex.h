#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace cdsvk {

struct VertexAttributeInfo
{
  uint32_t id;
  uint32_t offset;
  VkFormat format;
};

struct VertexLayout
{
  uint32_t stride;
  std::vector<VertexAttributeInfo> attributes;
};

int convert_vertex_buffer(const void *src_vertices, const VertexLayout& src_layout,
  void *dst_vertices, const VertexLayout &dst_layout, size_t vertex_count);



}  // namespace cdsvk
