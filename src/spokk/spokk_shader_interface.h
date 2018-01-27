#pragma once

// clang-format off
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION     0
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL       1
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_TANGENT      2
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_BITANGENT    3
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_BONE_INDEX   4
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_BONE_WEIGHT  5
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_COLOR0       6
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_COLOR1       7
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_COLOR2       8
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_COLOR3       9
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD0   10
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD1   11
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD2   12
#define SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD3   13
// clang-format on

#ifdef __cplusplus
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#define SPOKK_MAT4(var_name) glm::mat4 var_name
#define SPOKK_VEC4(var_name) glm::vec4 var_name
#else
#define SPOKK_MAT4(var_name) mat4 var_name
#define SPOKK_VEC4(var_name) vec4 var_name
#endif

#ifdef __cplusplus
namespace spokk {
struct
#else
layout (set = 0, binding = 0) uniform
#endif
  CameraConstants {
  SPOKK_VEC4(time_and_res);  // x:seconds since launch. yz: viewport resolution in pixels. TODO(cort): game-world time (H:M:S), real-world time (H:M:S, D/M/Y)
  SPOKK_VEC4(eye_pos_ws);    // xyz: world-space eye position
  SPOKK_VEC4(eye_dir_wsn);   // xyz: world-space eye direction (normalized)
  SPOKK_MAT4(view_proj);
  SPOKK_MAT4(view);
  SPOKK_MAT4(proj);
#ifdef __cplusplus
};
}
#else
} camera;
#endif

#ifdef __cplusplus
namespace spokk {
struct
#else
layout (set = 2, binding = 0) uniform
#endif
InstanceTransforms {
  SPOKK_MAT4(world);
  SPOKK_MAT4(padding1);
  SPOKK_MAT4(padding2);
  SPOKK_MAT4(padding3);
#ifdef __cplusplus
};
}
#else
} transforms;
#endif


#undef SPOKK_MAT4
#undef SPOKK_VEC4
