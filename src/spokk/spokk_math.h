#pragma once

#include <math.h>

// Prevent clashes with math.h definitions, if enabled
#if !defined(M_E)
#define M_E        2.71828182845904523536   // e
#endif
#if !defined(M_LOG2E)
#define M_LOG2E    1.44269504088896340736   // log2(e)
#endif
#if !defined(M_LOG10E)
#define M_LOG10E   0.434294481903251827651  // log10(e)
#endif
#if !defined(M_LN2)
#define M_LN2      0.693147180559945309417  // ln(2)
#endif
#if !defined(M_LN10)
#define M_LN10     2.30258509299404568402   // ln(10)
#endif
#if !defined(M_PI)
#define M_PI       3.14159265358979323846   // pi
#endif
#if !defined(M_PI_2)
#define M_PI_2     1.57079632679489661923   // pi/2
#endif
#if !defined(M_PI_4)
#define M_PI_4     0.785398163397448309616  // pi/4
#endif
#if !defined(M_1_PI)
#define M_1_PI     0.318309886183790671538  // 1/pi
#endif
#if !defined(M_2_PI)
#define M_2_PI     0.636619772367581343076  // 2/pi
#endif
#if !defined(M_2_SQRTPI)
#define M_2_SQRTPI 1.12837916709551257390   // 2/sqrt(pi)
#endif
#if !defined(M_SQRT2)
#define M_SQRT2    1.41421356237309504880   // sqrt(2)
#endif
#if !defined(M_SQRT1_2)
#define M_SQRT1_2  0.707106781186547524401  // 1/sqrt(2)
#endif
#if !defined(_MATH_DEFINES_DEFINED)
// tell subsequent includes of math.h that we already defined these constants
#define _MATH_DEFINES_DEFINED
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4701)  // potentially uninitialized return value
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace spokk {

// Construct a transformation matrix from the supplied operations, suitable for M*v.
glm::mat4 ComposeTransform(const glm::vec3 translation, const glm::quat rotation, const float uniform_scale);

}  // namespace spokk
