#include "spokk_math.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace spokk {

glm::mat4 ComposeTransform(const glm::vec3 translation, const glm::quat rotation, const float uniform_scale) {
  float qxx(rotation.x * rotation.x);
  float qyy(rotation.y * rotation.y);
  float qzz(rotation.z * rotation.z);
  float qxz(rotation.x * rotation.z);
  float qxy(rotation.x * rotation.y);
  float qyz(rotation.y * rotation.z);
  float qwx(rotation.w * rotation.x);
  float qwy(rotation.w * rotation.y);
  float qwz(rotation.w * rotation.z);
  glm::mat4 xform = glm::mat4(
      // clang-format off
      (1.0f - 2.0f * (qyy +  qzz)) * uniform_scale,
      (2.0f * (qxy + qwz)) * uniform_scale,
      (2.0f * (qxz - qwy)) * uniform_scale,
      0,

      (2.0f * (qxy - qwz)) * uniform_scale,
      (1.0f - 2.0f * (qxx +  qzz)) * uniform_scale,
      (2.0f * (qyz + qwx)) * uniform_scale,
      0,

      (2.0f * (qxz + qwy)) * uniform_scale,
      (2.0f * (qyz - qwx)) * uniform_scale,
      (1.0f - 2.0f * (qxx +  qyy)) * uniform_scale,
      0,

      translation.x, translation.y, translation.z, 1
      // clang-format on
  );
  return xform;
}

#if defined(ZOMBO_COMPILER_MSVC)
#pragma float_control(precise, on, push)
#endif
glm::vec3 ExtractViewPos(const glm::mat4& view) {
  glm::mat3 view_rot(
      view[0][0], view[0][1], view[0][2], view[1][0], view[1][1], view[1][2], view[2][0], view[2][1], view[2][2]);
  glm::vec3 d(view[3][0], view[3][1], view[3][2]);
  return -d * view_rot;
}
#if defined(ZOMBO_COMPILER_MSVC)
#pragma float_control(pop)
#endif

glm::vec3 ExtractViewDir(const glm::mat4& view) { return glm::vec3(-view[0][2], -view[1][2], -view[2][2]); }

}  // namespace spokk
