#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>

namespace cdsvk {

struct VertexAttributeInfo
{
  uint32_t id;
  uint32_t offset;
  VkFormat format;
};

constexpr uint32_t MAX_VERTEX_ATTRIBUTE_COUNT = 16;
struct VertexLayout
{
  uint32_t stride;
  std::array<VertexAttributeInfo, MAX_VERTEX_ATTRIBUTE_COUNT> attributes;
};

int convert_vertex_buffer(const void *src_vertices, const VertexLayout& src_layout,
  void *dst_vertices, const VertexLayout &dst_layout, size_t vertex_count);



}  // namespace cdsvk
