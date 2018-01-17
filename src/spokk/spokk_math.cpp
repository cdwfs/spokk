#include "spokk_math.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace spokk {

glm::mat4 ComposeTransform(const glm::vec3 translation, const glm::quat rotation, const float uniform_scale) {
  // clang-format off
  return glm::scale(
    glm::rotate(
      glm::translate(
        glm::mat4(1.0f),
        translation),
      glm::angle(rotation),
      glm::axis(rotation)),
    glm::vec3(uniform_scale));
  // clang-format on
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

glm::vec3 ExtractViewDir(const glm::mat4& view) {
  return glm::vec3(-view[0][2], -view[1][2], -view[2][2]);
}

}  // namespace spokk
