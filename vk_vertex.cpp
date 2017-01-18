#include "vk_vertex.h"

#include <assert.h>
#include <math.h>
#include <array>

namespace {
template <typename T>
T my_clamp(T x, T xmin, T xmax) {
  return (x < xmin) ? xmin : ((x > xmax) ? xmax : x);
}
template <typename T>
T my_max(T x, T y) {
  return (x > y) ? x : y;
}

struct AttributeFormatInfo {
  VkFormat format;
  uint32_t components;
  uint32_t size;
};
AttributeFormatInfo GetAttributeFormatInfo(VkFormat format) {
  // clang-format off
    static const std::array<AttributeFormatInfo, 49> format_info_lut = {{
      { VK_FORMAT_UNDEFINED           , 0,  0, },
      { VK_FORMAT_R8_UNORM            , 1,  1, },
      { VK_FORMAT_R8_SNORM            , 1,  1, },
      { VK_FORMAT_R8_UINT             , 1,  1, },
      { VK_FORMAT_R8_SINT             , 1,  1, },
      { VK_FORMAT_R8G8_UNORM          , 2,  2, },
      { VK_FORMAT_R8G8_SNORM          , 2,  2, },
      { VK_FORMAT_R8G8_UINT           , 2,  2, },
      { VK_FORMAT_R8G8_SINT           , 2,  2, },
      { VK_FORMAT_R8G8B8_UNORM        , 3,  3, },
      { VK_FORMAT_R8G8B8_SNORM        , 3,  3, },
      { VK_FORMAT_R8G8B8_UINT         , 3,  3, },
      { VK_FORMAT_R8G8B8_SINT         , 3,  3, },
      { VK_FORMAT_R8G8B8A8_UNORM      , 4,  4, },
      { VK_FORMAT_R8G8B8A8_SNORM      , 4,  4, },
      { VK_FORMAT_R8G8B8A8_UINT       , 4,  4, },
      { VK_FORMAT_R8G8B8A8_SINT       , 4,  4, },
      // TODO(cort): 2-10-10-10 formats go here
      { VK_FORMAT_R16_UNORM           , 1,  2, },
      { VK_FORMAT_R16_SNORM           , 1,  2, },
      { VK_FORMAT_R16_UINT            , 1,  2, },
      { VK_FORMAT_R16_SINT            , 1,  2, },
      { VK_FORMAT_R16_SFLOAT          , 1,  2, },
      { VK_FORMAT_R16G16_UNORM        , 2,  4, },
      { VK_FORMAT_R16G16_SNORM        , 2,  4, },
      { VK_FORMAT_R16G16_UINT         , 2,  4, },
      { VK_FORMAT_R16G16_SINT         , 2,  4, },
      { VK_FORMAT_R16G16_SFLOAT       , 2,  4, },
      { VK_FORMAT_R16G16B16_UNORM     , 3,  6, },
      { VK_FORMAT_R16G16B16_SNORM     , 3,  6, },
      { VK_FORMAT_R16G16B16_UINT      , 3,  6, },
      { VK_FORMAT_R16G16B16_SINT      , 3,  6, },
      { VK_FORMAT_R16G16B16_SFLOAT    , 3,  6, },
      { VK_FORMAT_R16G16B16A16_UNORM  , 4,  8, },
      { VK_FORMAT_R16G16B16A16_SNORM  , 4,  8, },
      { VK_FORMAT_R16G16B16A16_UINT   , 4,  8, },
      { VK_FORMAT_R16G16B16A16_SINT   , 4,  8, },
      { VK_FORMAT_R16G16B16A16_SFLOAT , 4,  8, },
      { VK_FORMAT_R32_UINT            , 1,  4, },
      { VK_FORMAT_R32_SINT            , 1,  4, },
      { VK_FORMAT_R32_SFLOAT          , 1,  4, },
      { VK_FORMAT_R32G32_UINT         , 2,  8, },
      { VK_FORMAT_R32G32_SINT         , 2,  8, },
      { VK_FORMAT_R32G32_SFLOAT       , 2,  8, },
      { VK_FORMAT_R32G32B32_UINT      , 3, 12, },
      { VK_FORMAT_R32G32B32_SINT      , 3, 12, },
      { VK_FORMAT_R32G32B32_SFLOAT    , 3, 12, },
      { VK_FORMAT_R32G32B32A32_UINT   , 4, 16, },
      { VK_FORMAT_R32G32B32A32_SINT   , 4, 16, },
      { VK_FORMAT_R32G32B32A32_SFLOAT , 4, 16, },
    }};
  // clang-format on
  size_t low = 0, high = format_info_lut.size() - 1;
  for (;;) {
    size_t mid = (low + high) / 2;
    assert(format_info_lut[low].format < format_info_lut[mid].format || low == mid);
    assert(format_info_lut[mid].format < format_info_lut[high].format || mid == high);
    if (format == format_info_lut[mid].format) {
      return format_info_lut[mid];  // found!
    } else if (low == high) {
      break;  // not found
    } else if (format < format_info_lut[mid].format) {
      high = mid - 1;
    } else if (format > format_info_lut[mid].format) {
      low = mid + 1;
    }
  }
  // Not found! Return the UNDEFINED entry.
  assert(format_info_lut[0].format == VK_FORMAT_UNDEFINED);
  return format_info_lut[0];
}
bool IsValidAttributeFormat(VkFormat format) { return GetAttributeFormatInfo(format).format != VK_FORMAT_UNDEFINED; }

typedef struct u8x4 { uint8_t elem[4]; } u8x4;
typedef struct s8x4 { int8_t elem[4]; } s8x4;
typedef struct u16x4 { uint16_t elem[4]; } u16x4;
typedef struct s16x4 { int16_t elem[4]; } s16x4;
typedef struct u32x4 { uint32_t elem[4]; } u32x4;
typedef struct s32x4 { int32_t elem[4]; } s32x4;
typedef struct f32x4 { float elem[4]; } f32x4;

typedef union f32 {
  float asFloat;
  uint32_t asInt;
  struct {
    uint32_t mantissa : 23;
    uint32_t exponent : 8;
    uint32_t sign : 1;
  } parts;
} f32;

typedef union f16 {
  uint16_t asInt;
  struct {
    uint32_t mantissa : 10;
    uint32_t exponent : 5;
    uint32_t sign : 1;
  } parts;
} f16;

float Convert1_F16toF32(uint16_t in) {
  f16 h;
  h.asInt = in;
  f32 f;
  f.asInt = 0;
  f.parts.sign = h.parts.sign;
  if (h.parts.exponent == 0) { /* denormalized input */
    if (h.parts.mantissa == 0) {
      return f.asFloat; /* -0 or +0 */
    }
    f.asFloat = (float)h.parts.mantissa / (float)(1 << 10); /* mantissa as fixed-point 0.xxx fraction */
    assert(f.parts.exponent >= 14);
    f.parts.exponent -= 14; /* apply f16 exponent bias. should be += (1-15), but you know how it is. */
    f.parts.sign = h.parts.sign; /* re-copy sign */
    return f.asFloat;
  }
  f.parts.exponent = (h.parts.exponent == 0x1F) ? 0xFF /* infinity -> infinity */
                                                : (h.parts.exponent - 15) + 127; /* remove f16 bias, add f32 bias */
  f.parts.mantissa = h.parts.mantissa << (23 - 10); /* shift mantissa into high bits */
  return f.asFloat;
}
uint16_t Convert1_F32toF16(float in) {
  f32 f;
  f.asFloat = in;
  f16 h;
  h.parts.sign = f.parts.sign;
  if (f.parts.exponent == 0xFF) { /* infinity */
    h.parts.exponent = 0x1F;
    h.parts.mantissa = 0;
    if (f.parts.mantissa != 0) {
      /* QNaN -> high bit of mantissa is 1. SNaN -> high bit of mantissa is 0, low bits are nonzero. */
      h.parts.mantissa = (f.parts.mantissa & (1 << 22)) ? (1 << 9) : (1 << 9) - 1;
    }
  } else {
    const float min_norm = 6.103515625e-05f; /* = 1.0 * 2**(1-15) */
    const float min_denorm = 5.960464477539063e-08f; /* = 1/(2**10) * 2**(1-15) */
    const float max_norm = 65504.0f; /* = (1.0 + ((1<<10)-1)/(1<<10)) * 2**15 */
    const float af = fabsf(f.asFloat);
    if (af < min_denorm) { /* input too small to represent; return +/-0 */
      h.parts.mantissa = 0;
      h.parts.exponent = 0;
    } else if (af < min_norm) { /* convert normalized input to denormalized output */
      h.parts.exponent = 0;
      uint32_t unbiased_exp = f.parts.exponent - 127;
      assert(
          -24 <= unbiased_exp && unbiased_exp <= -15); /* range of exponents that map to non-zero denorm f16 values */
      uint32_t new_mantissa = f.parts.mantissa | (1 << 23);
      h.parts.mantissa = new_mantissa >> (-1 - unbiased_exp);
    } else {
      uint32_t new_exponent = (f.parts.exponent - 127) + 15;
      if (new_exponent >= 31 || af > max_norm) { /* too large to represent */
        h.parts.exponent = 31;
        h.parts.mantissa = 0;
      } else {
        h.parts.exponent = new_exponent;
        h.parts.mantissa = f.parts.mantissa >> (23 - 10);
      }
    }
  }
  return h.asInt;
}

f32x4 Convert4_U8NtoF32(const u8x4 in) {
  // clang-format off
  return {{
      (float)(in.elem[0]) / 255.0f,
      (float)(in.elem[1]) / 255.0f,
      (float)(in.elem[2]) / 255.0f,
      (float)(in.elem[3]) / 255.0f,
  }};
  // clang-format on
}
f32x4 Convert4_S8NtoF32(const s8x4 in) {
  return {{
      (in.elem[0] == -128) ? -1.0f : ((float)(in.elem[0]) / 127.0f),
      (in.elem[1] == -128) ? -1.0f : ((float)(in.elem[1]) / 127.0f),
      (in.elem[2] == -128) ? -1.0f : ((float)(in.elem[2]) / 127.0f),
      (in.elem[3] == -128) ? -1.0f : ((float)(in.elem[3]) / 127.0f),
  }};
}
f32x4 Convert4_U8toF32(const u8x4 in) {
  // clang-format off
  return {{
      (float)(in.elem[0]),
      (float)(in.elem[1]),
      (float)(in.elem[2]),
      (float)(in.elem[3]),
  }};
  // clang-format on
}
f32x4 Convert4_S8toF32(const s8x4 in) {
  // clang-format off
  return {{
      (float)(in.elem[0]),
      (float)(in.elem[1]),
      (float)(in.elem[2]),
      (float)(in.elem[3]),
  }};
  // clang-format on
}
f32x4 Convert4_U16NtoF32(const u16x4 in) {
  // clang-format off
  return {{
      (float)(in.elem[0]) / 65535.0f,
      (float)(in.elem[1]) / 65535.0f,
      (float)(in.elem[2]) / 65535.0f,
      (float)(in.elem[3]) / 65535.0f,
  }};
  // clang-format on
}
f32x4 Convert4_S16NtoF32(const s16x4 in) {
  return {{
      (in.elem[0] == -32768) ? 0.0f : ((float)(in.elem[0]) / 32767.0f),
      (in.elem[1] == -32768) ? 0.0f : ((float)(in.elem[1]) / 32767.0f),
      (in.elem[2] == -32768) ? 0.0f : ((float)(in.elem[2]) / 32767.0f),
      (in.elem[3] == -32768) ? 0.0f : ((float)(in.elem[3]) / 32767.0f),
  }};
}
f32x4 Convert4_U16toF32(const u16x4 in) {
  // clang-format off
  return {{
      (float)(in.elem[0]),
      (float)(in.elem[1]),
      (float)(in.elem[2]),
      (float)(in.elem[3]),
  }};
  // clang-format on
}
f32x4 Convert4_S16toF32(const s16x4 in) {
  // clang-format off
  return {{
      (float)(in.elem[0]),
      (float)(in.elem[1]),
      (float)(in.elem[2]),
      (float)(in.elem[3]),
  }};
  // clang-format on
}
f32x4 Convert4_F16toF32(const u16x4 in) {
  // clang-format off
  return {{
      Convert1_F16toF32(in.elem[0]),
      Convert1_F16toF32(in.elem[1]),
      Convert1_F16toF32(in.elem[2]),
      Convert1_F16toF32(in.elem[3]),
  }};
  // clang-format on
}
f32x4 Convert4_U32toF32(const u32x4 in) {
  // clang-format off
  return {{
      (float)(in.elem[0]),
      (float)(in.elem[1]),
      (float)(in.elem[2]),
      (float)(in.elem[3]),
  }};
  // clang-format on
}
f32x4 Convert4_S32toF32(const s32x4 in) {
  // clang-format off
  return {{
      (float)(in.elem[0]),
      (float)(in.elem[1]),
      (float)(in.elem[2]),
      (float)(in.elem[3]),
  }};
  // clang-format on
}

u8x4 Convert4_F32toU8N(const f32x4 in) {
  return {{
      (uint8_t)(my_clamp(in.elem[0], 0.0f, 1.0f) * 255.0f + 0.5f),
      (uint8_t)(my_clamp(in.elem[1], 0.0f, 1.0f) * 255.0f + 0.5f),
      (uint8_t)(my_clamp(in.elem[2], 0.0f, 1.0f) * 255.0f + 0.5f),
      (uint8_t)(my_clamp(in.elem[3], 0.0f, 1.0f) * 255.0f + 0.5f),
  }};
}
s8x4 Convert4_F32toS8N(const f32x4 in) {
  return {{
      (int8_t)floorf(my_clamp(in.elem[0], -1.0f, 1.0f) * 127.0f + ((in.elem[0] >= 0) ? 0.5f : -0.5f)),
      (int8_t)floorf(my_clamp(in.elem[1], -1.0f, 1.0f) * 127.0f + ((in.elem[1] >= 0) ? 0.5f : -0.5f)),
      (int8_t)floorf(my_clamp(in.elem[2], -1.0f, 1.0f) * 127.0f + ((in.elem[2] >= 0) ? 0.5f : -0.5f)),
      (int8_t)floorf(my_clamp(in.elem[3], -1.0f, 1.0f) * 127.0f + ((in.elem[3] >= 0) ? 0.5f : -0.5f)),
  }};
}
u8x4 Convert4_F32toU8(const f32x4 in) {
  // clang-format off
  return {{
      (uint8_t)(my_clamp(in.elem[0], 0.0f, (float)UINT8_MAX)),
      (uint8_t)(my_clamp(in.elem[1], 0.0f, (float)UINT8_MAX)),
      (uint8_t)(my_clamp(in.elem[2], 0.0f, (float)UINT8_MAX)),
      (uint8_t)(my_clamp(in.elem[3], 0.0f, (float)UINT8_MAX)),
  }};
  // clang-format on
}
s8x4 Convert4_F32toS8(const f32x4 in) {
  return {{
      (int8_t)my_clamp(in.elem[0], (float)INT8_MIN, (float)INT8_MAX),
      (int8_t)my_clamp(in.elem[1], (float)INT8_MIN, (float)INT8_MAX),
      (int8_t)my_clamp(in.elem[2], (float)INT8_MIN, (float)INT8_MAX),
      (int8_t)my_clamp(in.elem[3], (float)INT8_MIN, (float)INT8_MAX),
  }};
}
u16x4 Convert4_F32toU16N(const f32x4 in) {
  return {{
      (uint16_t)(my_clamp(in.elem[0], 0.0f, 1.0f) * (float)UINT16_MAX + 0.5f),
      (uint16_t)(my_clamp(in.elem[1], 0.0f, 1.0f) * (float)UINT16_MAX + 0.5f),
      (uint16_t)(my_clamp(in.elem[2], 0.0f, 1.0f) * (float)UINT16_MAX + 0.5f),
      (uint16_t)(my_clamp(in.elem[3], 0.0f, 1.0f) * (float)UINT16_MAX + 0.5f),
  }};
}
s16x4 Convert4_F32toS16N(const f32x4 in) {
  return {{
      (int16_t)floorf(my_clamp(in.elem[0], -1.0f, 1.0f) * (float)INT16_MAX + ((in.elem[0] >= 0) ? 0.5f : -0.5f)),
      (int16_t)floorf(my_clamp(in.elem[1], -1.0f, 1.0f) * (float)INT16_MAX + ((in.elem[1] >= 0) ? 0.5f : -0.5f)),
      (int16_t)floorf(my_clamp(in.elem[2], -1.0f, 1.0f) * (float)INT16_MAX + ((in.elem[2] >= 0) ? 0.5f : -0.5f)),
      (int16_t)floorf(my_clamp(in.elem[3], -1.0f, 1.0f) * (float)INT16_MAX + ((in.elem[3] >= 0) ? 0.5f : -0.5f)),
  }};
}
u16x4 Convert4_F32toU16(const f32x4 in) {
  return {{
      (uint16_t)(my_clamp(in.elem[0], 0.0f, (float)UINT16_MAX)),
      (uint16_t)(my_clamp(in.elem[1], 0.0f, (float)UINT16_MAX)),
      (uint16_t)(my_clamp(in.elem[2], 0.0f, (float)UINT16_MAX)),
      (uint16_t)(my_clamp(in.elem[3], 0.0f, (float)UINT16_MAX)),
  }};
}
s16x4 Convert4_F32toS16(const f32x4 in) {
  return {{
      (int16_t)my_clamp(in.elem[0], (float)INT16_MIN, (float)INT16_MAX),
      (int16_t)my_clamp(in.elem[1], (float)INT16_MIN, (float)INT16_MAX),
      (int16_t)my_clamp(in.elem[2], (float)INT16_MIN, (float)INT16_MAX),
      (int16_t)my_clamp(in.elem[3], (float)INT16_MIN, (float)INT16_MAX),
  }};
}
u16x4 Convert4_F32toF16(const f32x4 in) {
  // clang-format off
  return {{
      Convert1_F32toF16(in.elem[0]),
      Convert1_F32toF16(in.elem[1]),
      Convert1_F32toF16(in.elem[2]),
      Convert1_F32toF16(in.elem[3]),
  }};
  // clang-format on
}
u32x4 Convert4_F32toU32(const f32x4 in) {
  return {{
      (uint32_t)(my_clamp(in.elem[0], 0.0f, (float)UINT32_MAX)),
      (uint32_t)(my_clamp(in.elem[1], 0.0f, (float)UINT32_MAX)),
      (uint32_t)(my_clamp(in.elem[2], 0.0f, (float)UINT32_MAX)),
      (uint32_t)(my_clamp(in.elem[3], 0.0f, (float)UINT32_MAX)),
  }};
}
s32x4 Convert4_F32toS32(const f32x4 in) {
  return {{
      (int32_t)my_clamp(in.elem[0], (float)INT32_MIN, (float)INT32_MAX),
      (int32_t)my_clamp(in.elem[1], (float)INT32_MIN, (float)INT32_MAX),
      (int32_t)my_clamp(in.elem[2], (float)INT32_MIN, (float)INT32_MAX),
      (int32_t)my_clamp(in.elem[3], (float)INT32_MIN, (float)INT32_MAX),
  }};
}

void ConvertAttribute(const void *in, VkFormat in_format, void *out, VkFormat out_format) {
  /* TODO(cort): special case for in_format == out_format that's just a small memcpy? */

  /* Precondition: formats have already been validated. */
  const AttributeFormatInfo in_format_info = GetAttributeFormatInfo(in_format);
  const AttributeFormatInfo out_format_info = GetAttributeFormatInfo(out_format);
  uint32_t in_comp = in_format_info.components;
  uint32_t out_comp = out_format_info.components;

  /* Load/decompress input data into an f32x4 */
  f32x4 temp_f32 = {{0, 0, 0, 0}};
  switch (in_format) {
  case VK_FORMAT_UNDEFINED:
    break;
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8A8_UNORM: {
    u8x4 temp_u8 = {{0, 0, 0, 0}};
    const uint8_t *in_u8 = (const uint8_t *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_u8.elem[i] = in_u8[i];
    }
    temp_f32 = Convert4_U8NtoF32(temp_u8);
    break;
  }
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8B8_SNORM:
  case VK_FORMAT_R8G8B8A8_SNORM: {
    s8x4 temp_s8 = {{0, 0, 0, 0}};
    const int8_t *in_s8 = (const int8_t *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_s8.elem[i] = in_s8[i];
    }
    temp_f32 = Convert4_S8NtoF32(temp_s8);
    break;
  }
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_R8G8B8A8_UINT: {
    u8x4 temp_u8 = {{0, 0, 0, 0}};
    const uint8_t *in_u8 = (const uint8_t *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_u8.elem[i] = in_u8[i];
    }
    temp_f32 = Convert4_U8toF32(temp_u8);
    break;
  }
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8B8_SINT:
  case VK_FORMAT_R8G8B8A8_SINT: {
    s8x4 temp_s8 = {{0, 0, 0, 0}};
    const int8_t *in_s8 = (const int8_t *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_s8.elem[i] = in_s8[i];
    }
    temp_f32 = Convert4_S8toF32(temp_s8);
    break;
  }
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16A16_UNORM: {
    u16x4 temp_u16 = {{0, 0, 0, 0}};
    const uint16_t *in_u16 = (const uint16_t *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_u16.elem[i] = in_u16[i];
    }
    temp_f32 = Convert4_U16NtoF32(temp_u16);
    break;
  }
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16A16_SNORM: {
    s16x4 temp_s16 = {{0, 0, 0, 0}};
    const int16_t *in_s16 = (const int16_t *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_s16.elem[i] = in_s16[i];
    }
    temp_f32 = Convert4_S16NtoF32(temp_s16);
    break;
  }
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16A16_UINT: {
    u16x4 temp_u16 = {{0, 0, 0, 0}};
    const uint16_t *in_u16 = (const uint16_t *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_u16.elem[i] = in_u16[i];
    }
    temp_f32 = Convert4_U16toF32(temp_u16);
    break;
  }
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16A16_SINT: {
    s16x4 temp_s16 = {{0, 0, 0, 0}};
    const int16_t *in_s16 = (const int16_t *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_s16.elem[i] = in_s16[i];
    }
    temp_f32 = Convert4_S16toF32(temp_s16);
    break;
  }
  case VK_FORMAT_R16_SFLOAT:
  case VK_FORMAT_R16G16_SFLOAT:
  case VK_FORMAT_R16G16B16_SFLOAT:
  case VK_FORMAT_R16G16B16A16_SFLOAT: {
    u16x4 temp_u16 = {{0, 0, 0, 0}};
    const uint16_t *in_u16 = (const uint16_t *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_u16.elem[i] = in_u16[i];
    }
    temp_f32 = Convert4_F16toF32(temp_u16);
    break;
  }
  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32A32_UINT: {
    u32x4 temp_u32 = {{0, 0, 0, 0}};
    const uint32_t *in_u32 = (const uint32_t *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_u32.elem[i] = in_u32[i];
    }
    temp_f32 = Convert4_U32toF32(temp_u32);
    break;
  }
  case VK_FORMAT_R32_SINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32B32A32_SINT: {
    s32x4 temp_s32 = {{0, 0, 0, 0}};
    const int32_t *in_s32 = (const int32_t *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_s32.elem[i] = in_s32[i];
    }
    temp_f32 = Convert4_S32toF32(temp_s32);
    break;
  }
  case VK_FORMAT_R32_SFLOAT:
  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R32G32B32_SFLOAT:
  case VK_FORMAT_R32G32B32A32_SFLOAT: {
    const float *in_f32 = (const float *)in;
    for (uint32_t i = 0; i < in_comp; ++i) {
      temp_f32.elem[i] = in_f32[i];
    }
    break;
  }
  }

  /* Convert temp f32 to output format. */
  switch (out_format) {
  case VK_FORMAT_UNDEFINED:
    break;
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8A8_UNORM: {
    u8x4 temp_u8 = {{0, 0, 0, 0}};
    temp_u8 = Convert4_F32toU8N(temp_f32);
    uint8_t *out_u8 = (uint8_t *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_u8[i] = temp_u8.elem[i];
    }
    break;
  }
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8B8_SNORM:
  case VK_FORMAT_R8G8B8A8_SNORM: {
    s8x4 temp_s8 = {{0, 0, 0, 0}};
    temp_s8 = Convert4_F32toS8N(temp_f32);
    int8_t *out_s8 = (int8_t *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_s8[i] = temp_s8.elem[i];
    }
    break;
  }
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_R8G8B8A8_UINT: {
    u8x4 temp_u8 = {{0, 0, 0, 0}};
    temp_u8 = Convert4_F32toU8(temp_f32);
    uint8_t *out_u8 = (uint8_t *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_u8[i] = temp_u8.elem[i];
    }
    break;
  }
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8B8_SINT:
  case VK_FORMAT_R8G8B8A8_SINT: {
    s8x4 temp_s8 = {{0, 0, 0, 0}};
    temp_s8 = Convert4_F32toS8(temp_f32);
    int8_t *out_s8 = (int8_t *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_s8[i] = temp_s8.elem[i];
    }
    break;
  }
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16A16_UNORM: {
    u16x4 temp_u16 = {{0, 0, 0, 0}};
    temp_u16 = Convert4_F32toU16N(temp_f32);
    uint16_t *out_u16 = (uint16_t *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_u16[i] = temp_u16.elem[i];
    }
    break;
  }
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16A16_SNORM: {
    s16x4 temp_s16 = {{0, 0, 0, 0}};
    temp_s16 = Convert4_F32toS16N(temp_f32);
    int16_t *out_s16 = (int16_t *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_s16[i] = temp_s16.elem[i];
    }
    break;
  }
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16A16_UINT: {
    u16x4 temp_u16 = {{0, 0, 0, 0}};
    temp_u16 = Convert4_F32toU16(temp_f32);
    uint16_t *out_u16 = (uint16_t *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_u16[i] = temp_u16.elem[i];
    }
    break;
  }
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16A16_SINT: {
    s16x4 temp_s16 = {{0, 0, 0, 0}};
    temp_s16 = Convert4_F32toS16(temp_f32);
    int16_t *out_s16 = (int16_t *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_s16[i] = temp_s16.elem[i];
    }
    break;
  }
  case VK_FORMAT_R16_SFLOAT:
  case VK_FORMAT_R16G16_SFLOAT:
  case VK_FORMAT_R16G16B16_SFLOAT:
  case VK_FORMAT_R16G16B16A16_SFLOAT: {
    u16x4 temp_u16 = {{0, 0, 0, 0}};
    temp_u16 = Convert4_F32toF16(temp_f32);
    uint16_t *out_u16 = (uint16_t *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_u16[i] = temp_u16.elem[i];
    }
    break;
  }
  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32A32_UINT: {
    u32x4 temp_u32 = {{0, 0, 0, 0}};
    temp_u32 = Convert4_F32toU32(temp_f32);
    uint32_t *out_u32 = (uint32_t *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_u32[i] = temp_u32.elem[i];
    }
    break;
  }
  case VK_FORMAT_R32_SINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32B32A32_SINT: {
    s32x4 temp_s32 = {{0, 0, 0, 0}};
    temp_s32 = Convert4_F32toS32(temp_f32);
    int32_t *out_s32 = (int32_t *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_s32[i] = temp_s32.elem[i];
    }
    break;
  }
  case VK_FORMAT_R32_SFLOAT:
  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R32G32B32_SFLOAT:
  case VK_FORMAT_R32G32B32A32_SFLOAT: {
    float *out_f32 = (float *)out;
    for (uint32_t i = 0; i < out_comp; ++i) {
      out_f32[i] = temp_f32.elem[i];
    }
    break;
  }
  }
}
}  // namespace

namespace spokk {

VertexLayout::VertexLayout(std::initializer_list<AttributeInfo> attr_infos) : stride(0), attributes(attr_infos) {
  if (attributes.size() > 0) {
    AttributeInfo last_attr = attributes[0];
    for (const auto &attr : attributes) {
      if (attr.offset > last_attr.offset) {
        last_attr = attr;
      }
    }
    AttributeFormatInfo format_info = GetAttributeFormatInfo(last_attr.format);
    if (format_info.format == last_attr.format) {
      stride = last_attr.offset + format_info.size;
    }
  }
}
VertexLayout::VertexLayout(const MeshFormat &mesh_format, uint32_t binding) : stride(0) {
  for (const auto &binding_desc : mesh_format.vertex_buffer_bindings) {
    if (binding_desc.binding == binding) {
      stride = binding_desc.stride;
      break;
    }
  }
  for (const auto &attr_desc : mesh_format.vertex_attributes) {
    if (attr_desc.binding == binding) {
      attributes.push_back({attr_desc.location, attr_desc.format, attr_desc.offset});
    }
  }
}

int ConvertVertexBuffer(const void *src_vertices, const VertexLayout &src_layout, void *dst_vertices,
    const VertexLayout &dst_layout, size_t vertex_count) {
  for (const auto &attr : src_layout.attributes) {
    if (!IsValidAttributeFormat(attr.format)) {
      return -1;
    }
  }
  for (const auto &attr : dst_layout.attributes) {
    if (!IsValidAttributeFormat(attr.format)) {
      return -1;
    }
  }

  const uint8_t *src_bytes = (const uint8_t *)src_vertices;
  uint8_t *dst_bytes = (uint8_t *)dst_vertices;
  for (size_t v = 0; v < vertex_count; ++v) {
    for (const auto &src_attr : src_layout.attributes) {
      // Find attribute in dstLayout with the same location.
      const VertexLayout::AttributeInfo *dst_attr = nullptr;
      for (const auto &attr : dst_layout.attributes) {
        if (attr.location == src_attr.location) {
          dst_attr = &attr;
          break;
        }
      }
      if (dst_attr == nullptr) {
        continue;  // couldn't find dst attribute with same location. For now, just skip it. Could be an error, though.
      }
      ConvertAttribute(src_bytes + src_attr.offset, src_attr.format, dst_bytes + dst_attr->offset, dst_attr->format);
    }
    src_bytes += src_layout.stride;
    dst_bytes += dst_layout.stride;
  }
  return 0;
}

}  // namespace spokk