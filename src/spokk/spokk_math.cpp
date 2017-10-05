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

}  // namespace spokk
