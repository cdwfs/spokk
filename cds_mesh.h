/* cds_mesh.h -- procedural 3D mesh generation in C89
 *               No warranty implied; use at your own risk.
 *
 * Do this:
 *   #define CDS_MESH_IMPLEMENTATION
 * before including this file in *one* C/C++ file to provide the function
 * implementations.
 *
 * For a unit test on gcc/Clang:
 *   cc -Wall -std=c89 -g -x c -DCDS_MESH_TEST -o test_cds_mesh.exe cds_mesh.h -lm
 *
 * For a unit test on Visual C++:
 *   "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat"
 *   cl -W4 -nologo -TC -DCDS_MESH_TEST /Fetest_cds_mesh.exe cds_mesh.h
 * Debug-mode:
 *   cl -W4 -Od -Z7 -FC -MTd -nologo -TC -DCDS_MESH_TEST /Fetest_cds_mesh.exe cds_mesh.h
 *
 * LICENSE:
 * This software is in the public domain. Where that dedication is not
 * recognized, you are granted a perpetual, irrevocable license to
 * copy, distribute, and modify this file as you see fit.
 */

#if !defined(CDSM_INCLUDE_CDS_MESH_H)
#define CDSM_INCLUDE_CDS_MESH_H

#include <stddef.h>

#if defined(CDSM_STATIC)
#   define CDSM_DEF static
#else
#   define CDSM_DEF extern
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1700)
/* no stdint.h in VS2010 and earlier.
 * LONG and DWORD are guaranteed to be 32 bits forever, though.
 */
typedef INT8  cdsm_s8;
typedef BYTE  cdsm_u8;
typedef LONG  cdsm_s32;
typedef DWORD cdsm_u32;
#else
#   include <stdint.h>
typedef  int8_t  cdsm_s8;
typedef uint8_t  cdsm_u8;
typedef  int32_t cdsm_s32;
typedef uint32_t cdsm_u32;
#endif
typedef cdsm_s32 cdsm_error_t;
typedef cdsm_u32 cdsm_index_t;

#ifdef __cplusplus
extern "C"
{
#endif
    typedef enum
    {
        CDSM_ATTRIBUTE_FORMAT_UNKNOWN            =  0,
        CDSM_ATTRIBUTE_FORMAT_R8_UNORM           =  1,
        CDSM_ATTRIBUTE_FORMAT_R8_SNORM           =  2,
        CDSM_ATTRIBUTE_FORMAT_R8_UINT            =  3,
        CDSM_ATTRIBUTE_FORMAT_R8_SINT            =  4,
        CDSM_ATTRIBUTE_FORMAT_R8G8_UNORM         =  5,
        CDSM_ATTRIBUTE_FORMAT_R8G8_SNORM         =  6,
        CDSM_ATTRIBUTE_FORMAT_R8G8_UINT          =  7,
        CDSM_ATTRIBUTE_FORMAT_R8G8_SINT          =  8,
        CDSM_ATTRIBUTE_FORMAT_R8G8B8_UNORM       =  9,
        CDSM_ATTRIBUTE_FORMAT_R8G8B8_SNORM       = 10,
        CDSM_ATTRIBUTE_FORMAT_R8G8B8_UINT        = 11,
        CDSM_ATTRIBUTE_FORMAT_R8G8B8_SINT        = 12,
        CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_UNORM     = 13,
        CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_SNORM     = 14,
        CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_UINT      = 15,
        CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_SINT      = 16,
        CDSM_ATTRIBUTE_FORMAT_R16_UNORM          = 17,
        CDSM_ATTRIBUTE_FORMAT_R16_SNORM          = 18,
        CDSM_ATTRIBUTE_FORMAT_R16_UINT           = 19,
        CDSM_ATTRIBUTE_FORMAT_R16_SINT           = 20,
        CDSM_ATTRIBUTE_FORMAT_R16_FLOAT          = 21,
        CDSM_ATTRIBUTE_FORMAT_R16G16_UNORM       = 22,
        CDSM_ATTRIBUTE_FORMAT_R16G16_SNORM       = 23,
        CDSM_ATTRIBUTE_FORMAT_R16G16_UINT        = 24,
        CDSM_ATTRIBUTE_FORMAT_R16G16_SINT        = 25,
        CDSM_ATTRIBUTE_FORMAT_R16G16_FLOAT       = 26,
        CDSM_ATTRIBUTE_FORMAT_R16G16B16_UNORM    = 27,
        CDSM_ATTRIBUTE_FORMAT_R16G16B16_SNORM    = 28,
        CDSM_ATTRIBUTE_FORMAT_R16G16B16_UINT     = 29,
        CDSM_ATTRIBUTE_FORMAT_R16G16B16_SINT     = 30,
        CDSM_ATTRIBUTE_FORMAT_R16G16B16_FLOAT    = 31,
        CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_UNORM = 32,
        CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_SNORM = 33,
        CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_UINT  = 34,
        CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_SINT  = 35,
        CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_FLOAT = 36,
        CDSM_ATTRIBUTE_FORMAT_R32_UINT           = 37,
        CDSM_ATTRIBUTE_FORMAT_R32_SINT           = 38,
        CDSM_ATTRIBUTE_FORMAT_R32_FLOAT          = 39,
        CDSM_ATTRIBUTE_FORMAT_R32G32_UINT        = 40,
        CDSM_ATTRIBUTE_FORMAT_R32G32_SINT        = 41,
        CDSM_ATTRIBUTE_FORMAT_R32G32_FLOAT       = 42,
        CDSM_ATTRIBUTE_FORMAT_R32G32B32_UINT     = 43,
        CDSM_ATTRIBUTE_FORMAT_R32G32B32_SINT     = 44,
        CDSM_ATTRIBUTE_FORMAT_R32G32B32_FLOAT    = 45,
        CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_UINT  = 46,
        CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_SINT  = 47,
        CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_FLOAT = 48,
        CDSM_ATTRIBUTE_FORMAT_BEGIN_RANGE        = CDSM_ATTRIBUTE_FORMAT_UNKNOWN,
        CDSM_ATTRIBUTE_FORMAT_END_RANGE          = CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_FLOAT,
        CDSM_ATTRIBUTE_FORMAT_RANGE_SIZE         = (CDSM_ATTRIBUTE_FORMAT_UNKNOWN - CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_FLOAT + 1),
        CDSM_ATTRIBUTE_FORMAT_MAX_ENUM           = 0x7FFFFFFF
    } cdsm_attribute_format_t;

    typedef struct
    {
        uint32_t id;
        uint32_t offset;
        cdsm_attribute_format_t format;
    } cdsm_attribute_inf_t;

    #define CDSM_MAX_VERTEX_ATTRIBUTE_COUNT 16
    typedef struct
    {
        uint32_t stride;
        uint32_t attribute_count;
        cdsm_attribute_inf_t attributes[CDSM_MAX_VERTEX_ATTRIBUTE_COUNT];
    } cdsm_vertex_layout_t;

    CDSM_DEF
    cdsm_error_t cdsm_convert_vertex_buffer(const void *src_vertices, const cdsm_vertex_layout_t *src_layout,
        void *dst_vertices, const cdsm_vertex_layout_t *dst_layout, size_t vertex_count);

    typedef enum {
        CDSM_PRIMITIVE_TYPE_TRIANGLE_LIST = 0,
        CDSM_PRIMITIVE_TYPE_LINE_LIST     = 1,
    } cdsm_primitive_type_t;

    typedef enum {
        CDSM_FRONT_FACE_CCW = 0,
        CDSM_FRONT_FACE_CW  = 1,
    } cdsm_front_face_t;

    typedef struct
    {
        cdsm_vertex_layout_t vertex_layout;
        cdsm_primitive_type_t primitive_type;
        cdsm_front_face_t front_face;
        cdsm_s32 vertex_count;
        cdsm_s32 index_count;
    } cdsm_metadata_t;

    typedef struct
    {
        cdsm_vertex_layout_t vertex_layout;
        struct { float x,y,z; } min_extent;
        struct { float x,y,z; } max_extent;
        cdsm_front_face_t front_face;
    } cdsm_cube_recipe_t;

    CDSM_DEF
    cdsm_error_t cdsm_create_cube(cdsm_metadata_t *out_metadata,
        void *out_vertices, size_t *out_vertices_size,
        cdsm_index_t *out_indices, size_t *out_indices_size,
        const cdsm_cube_recipe_t *recipe);

    typedef struct
    {
        cdsm_vertex_layout_t vertex_layout;
        cdsm_s32 latitudinal_segments;
        cdsm_s32 longitudinal_segments;
        float radius;
    } cdsm_sphere_recipe_t;

    CDSM_DEF
    cdsm_error_t cdsm_create_sphere(cdsm_metadata_t *out_metadata,
        void *out_vertices, size_t *out_vertices_size,
        cdsm_index_t *out_indices, size_t *out_indices_size,
        const cdsm_sphere_recipe_t *recipe);

    typedef struct
    {
        cdsm_vertex_layout_t vertex_layout;
        float length;
    } cdsm_axes_recipe_t;

    CDSM_DEF
    cdsm_error_t cdsm_create_axes(cdsm_metadata_t *out_metadata,
        void *out_vertices, size_t *out_vertices_size,
        cdsm_index_t *out_indices, size_t *out_indices_size,
        const cdsm_axes_recipe_t *recipe);

    typedef struct
    {
        cdsm_vertex_layout_t vertex_layout;
        float length;
        float radius0, radius1;
        cdsm_s32 axial_segments;
        cdsm_s32 radial_segments;
    } cdsm_cylinder_recipe_t;

    CDSM_DEF
    cdsm_error_t cdsm_create_cylinder(cdsm_metadata_t *out_metadata,
        void *out_vertices, size_t *out_vertices_size,
        cdsm_index_t *out_indices, size_t *out_indices_size,
        const cdsm_cylinder_recipe_t *recipe);

#ifdef __cplusplus
}
#endif

#endif /*-------------- end header file ------------------------ */

#if defined(CDS_MESH_TEST)
#   if !defined(CDS_MESH_IMPLEMENTATION)
#       define CDS_MESH_IMPLEMENTATION
#   endif
#endif

#ifdef CDS_MESH_IMPLEMENTATION

#include <assert.h>
#include <math.h>
#include <stdint.h>
#if !defined(CDSM_NO_STDIO)
#include <stdio.h>
#endif

#define CDSM__MIN(a,b) ( (a)<(b) ? (a) : (b) )
#define CDSM__MAX(a,b) ( (a)>(b) ? (a) : (b) )
#define CDSM__CLAMP(x, xmin, xmax) ( ((x)<(xmin)) ? (xmin) : ( ((x)>(xmax)) ? (xmax) : (x) ) )
#define CDSM__UNUSED(x) ((void)x)
static const float CDSM__PI = 3.14159265358979323846f;

typedef struct
{
    float position[3];
    float normal[3];
#if 0
    float tangent[3];
    float bitangent[3];
#endif
    float texcoord[2];
} cdsm__default_vertex_t;

static const cdsm_vertex_layout_t cdsm__default_vertex_layout = {
    sizeof(cdsm__default_vertex_t),
    3,
    {
        {0,  0, CDSM_ATTRIBUTE_FORMAT_R32G32B32_FLOAT},
        {1, 12, CDSM_ATTRIBUTE_FORMAT_R32G32B32_FLOAT},
        {2, 24, CDSM_ATTRIBUTE_FORMAT_R32G32_FLOAT},
    }
};

/* Export functions */
CDSM_DEF
cdsm_error_t cdsm_export_to_header(const char *filename, const char *prefix,
    cdsm_metadata_t const *metadata, cdsm__default_vertex_t const *vertices, cdsm_index_t const *indices);

/*************************** Vertex buffer conversion *********************************/

typedef struct cdsm__attribute_format_info
{
    cdsm_attribute_format_t format;
    uint32_t components;
    uint32_t size;
} cdsm__attribute_format_info;
static const cdsm__attribute_format_info format_info_lut[] =
{
    { CDSM_ATTRIBUTE_FORMAT_UNKNOWN            , 0,  0, },
    { CDSM_ATTRIBUTE_FORMAT_R8_UNORM           , 1,  1, },
    { CDSM_ATTRIBUTE_FORMAT_R8_SNORM           , 1,  1, },
    { CDSM_ATTRIBUTE_FORMAT_R8_UINT            , 1,  1, },
    { CDSM_ATTRIBUTE_FORMAT_R8_SINT            , 1,  1, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8_UNORM         , 2,  2, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8_SNORM         , 2,  2, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8_UINT          , 2,  2, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8_SINT          , 2,  2, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8B8_UNORM       , 3,  3, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8B8_SNORM       , 3,  3, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8B8_UINT        , 3,  3, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8B8_SINT        , 3,  3, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_UNORM     , 4,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_SNORM     , 4,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_UINT      , 4,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_SINT      , 4,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R16_UNORM          , 1,  2, },
    { CDSM_ATTRIBUTE_FORMAT_R16_SNORM          , 1,  2, },
    { CDSM_ATTRIBUTE_FORMAT_R16_UINT           , 1,  2, },
    { CDSM_ATTRIBUTE_FORMAT_R16_SINT           , 1,  2, },
    { CDSM_ATTRIBUTE_FORMAT_R16_FLOAT          , 1,  2, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16_UNORM       , 2,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16_SNORM       , 2,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16_UINT        , 2,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16_SINT        , 2,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16_FLOAT       , 2,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16B16_UNORM    , 3,  6, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16B16_SNORM    , 3,  6, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16B16_UINT     , 3,  6, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16B16_SINT     , 3,  6, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16B16_FLOAT    , 3,  6, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_UNORM , 4,  8, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_SNORM , 4,  8, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_UINT  , 4,  8, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_SINT  , 4,  8, },
    { CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_FLOAT , 4,  8, },
    { CDSM_ATTRIBUTE_FORMAT_R32_UINT           , 1,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R32_SINT           , 1,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R32_FLOAT          , 1,  4, },
    { CDSM_ATTRIBUTE_FORMAT_R32G32_UINT        , 2,  8, },
    { CDSM_ATTRIBUTE_FORMAT_R32G32_SINT        , 2,  8, },
    { CDSM_ATTRIBUTE_FORMAT_R32G32_FLOAT       , 2,  8, },
    { CDSM_ATTRIBUTE_FORMAT_R32G32B32_UINT     , 3, 12, },
    { CDSM_ATTRIBUTE_FORMAT_R32G32B32_SINT     , 3, 12, },
    { CDSM_ATTRIBUTE_FORMAT_R32G32B32_FLOAT    , 3, 12, },
    { CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_UINT  , 4, 16, },
    { CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_SINT  , 4, 16, },
    { CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_FLOAT , 4, 16, },
};

typedef struct cdsm__u8x4
{
    uint8_t elem[4];
} cdsm__u8x4;
typedef struct cdsm__s8x4
{
    int8_t elem[4];
} cdsm__s8x4;
typedef struct cdsm__u16x4
{
    uint16_t elem[4];
} cdsm__u16x4;
typedef struct cdsm__s16x4
{
    int16_t elem[4];
} cdsm__s16x4;
typedef struct cdsm__u32x4
{
    uint32_t elem[4];
} cdsm__u32x4;
typedef struct cdsm__s32x4
{
    int32_t elem[4];
} cdsm__s32x4;
typedef struct cdsm__f32x4
{
    float elem[4];
} cdsm__f32x4;

typedef union cdsm__f32
{
    float asFloat;
    uint32_t asInt;
    struct
    {
        uint32_t mantissa : 23;
        uint32_t exponent :  8;
        uint32_t sign     :  1;
    } parts;
} cdsm__f32;

typedef union cdsm__f16
{
    uint16_t asInt;
    struct
    {
        uint32_t mantissa : 10;
        uint32_t exponent :  5;
        uint32_t sign     :  1;
    } parts;
} cdsm__f16;

static float cdsm__convert1_f16_to_f32(uint16_t in)
{
    cdsm__f16 h;
    h.asInt = in;
    cdsm__f32 f;
    f.asInt = 0;
    f.parts.sign = h.parts.sign;
    if (h.parts.exponent == 0) /* denormalized input */
    {
        if (h.parts.mantissa == 0)
            return f.asFloat; /* -0 or +0 */
        f.asFloat = (float)h.parts.mantissa / (float)(1<<10); /* mantissa as fixed-point 0.xxx fraction */
        assert(f.parts.exponent >= 14);
        f.parts.exponent -= 14; /* apply f16 exponent bias. should be += (1-15), but you know how it is. */
        f.parts.sign = h.parts.sign; /* re-copy sign */
        return f.asFloat;
    }
    f.parts.exponent = (h.parts.exponent == 0x1F)
        ? 0xFF /* infinity -> infinity */
        : (h.parts.exponent - 15) + 127; /* remove f16 bias, add f32 bias */
    f.parts.mantissa = h.parts.mantissa << (23 - 10); /* shift mantissa into high bits */
    return f.asFloat;
}
static uint16_t cdsm__convert1_f32_to_f16(float in)
{
    cdsm__f32 f;
    f.asFloat = in;
    cdsm__f16 h;
    h.parts.sign = f.parts.sign;
    if (f.parts.exponent == 0xFF) /* infinity */
    {
        h.parts.exponent = 0x1F;
        h.parts.mantissa = 0;
        if (f.parts.mantissa != 0)
        {
            /* QNaN -> high bit of mantissa is 1. SNaN -> high bit of mantissa is 0, low bits are nonzero. */
            h.parts.mantissa = (f.parts.mantissa & (1<<22)) ? (1<<9) : (1<<9)-1;
        }
    }
    else
    {
        const float min_norm = 6.103515625e-05f; /* = 1.0 * 2**(1-15) */
        const float min_denorm = 5.960464477539063e-08f; /* = 1/(2**10) * 2**(1-15) */
        const float max_norm = 65504.0f; /* = (1.0 + ((1<<10)-1)/(1<<10)) * 2**15 */
        const float af = fabsf(f.asFloat);
        if (af < min_denorm) /* input too small to represent; return +/-0 */
        {
            h.parts.mantissa = 0;
            h.parts.exponent = 0;
        }
        else if (af < min_norm) /* convert normalized input to denormalized output */
        {
            h.parts.exponent = 0;
            uint32_t unbiased_exp = f.parts.exponent - 127;
            assert(-24 <= unbiased_exp && unbiased_exp <= -15); /* range of exponents that map to non-zero denorm f16 values */
            uint32_t new_mantissa = f.parts.mantissa | (1<<23);
            h.parts.mantissa = new_mantissa >> (-1 - unbiased_exp);
        }
        else
        {
            uint32_t new_exponent = (f.parts.exponent - 127) + 15;
            if (new_exponent >= 31 || af > max_norm) /* too large to represent */
            {
                h.parts.exponent = 31;
                h.parts.mantissa = 0;
            }
            else
            {
                h.parts.exponent = new_exponent;
                h.parts.mantissa = f.parts.mantissa >> (23-10);
            }
        }
    }
    return h.asInt;
}


static cdsm__f32x4 cdsm__convert4_u8n_to_f32(const cdsm__u8x4 in)
{
    return {{
        (float)(in.elem[0]) / 255.0f,
        (float)(in.elem[1]) / 255.0f,
        (float)(in.elem[2]) / 255.0f,
        (float)(in.elem[3]) / 255.0f,
    }};
}
static cdsm__f32x4 cdsm__convert4_s8n_to_f32(const cdsm__s8x4 in)
{
    return {{
        (in.elem[0] == -128) ? -1.0f : ((float)(in.elem[0]) / 127.0f),
        (in.elem[1] == -128) ? -1.0f : ((float)(in.elem[1]) / 127.0f),
        (in.elem[2] == -128) ? -1.0f : ((float)(in.elem[2]) / 127.0f),
        (in.elem[3] == -128) ? -1.0f : ((float)(in.elem[3]) / 127.0f),
    }};
}
static cdsm__f32x4 cdsm__convert4_u8_to_f32(const cdsm__u8x4 in)
{
    return {{
        (float)(in.elem[0]),
        (float)(in.elem[1]),
        (float)(in.elem[2]),
        (float)(in.elem[3]),
    }};
}
static cdsm__f32x4 cdsm__convert4_s8_to_f32(const cdsm__s8x4 in)
{
    return {{
        (float)(in.elem[0]),
        (float)(in.elem[1]),
        (float)(in.elem[2]),
        (float)(in.elem[3]),
    }};
}
static cdsm__f32x4 cdsm__convert4_u16n_to_f32(const cdsm__u16x4 in)
{
    return {{
        (float)(in.elem[0]) / 65535.0f,
        (float)(in.elem[1]) / 65535.0f,
        (float)(in.elem[2]) / 65535.0f,
        (float)(in.elem[3]) / 65535.0f,
    }};
}
static cdsm__f32x4 cdsm__convert4_s16n_to_f32(const cdsm__s16x4 in)
{
    return {{
        (in.elem[0] == -32768) ? 0.0f : ((float)(in.elem[0]) / 32767.0f),
        (in.elem[1] == -32768) ? 0.0f : ((float)(in.elem[1]) / 32767.0f),
        (in.elem[2] == -32768) ? 0.0f : ((float)(in.elem[2]) / 32767.0f),
        (in.elem[3] == -32768) ? 0.0f : ((float)(in.elem[3]) / 32767.0f),
    }};
}
static cdsm__f32x4 cdsm__convert4_u16_to_f32(const cdsm__u16x4 in)
{
    return {{
        (float)(in.elem[0]),
        (float)(in.elem[1]),
        (float)(in.elem[2]),
        (float)(in.elem[3]),
    }};
}
static cdsm__f32x4 cdsm__convert4_s16_to_f32(const cdsm__s16x4 in)
{
    return {{
        (float)(in.elem[0]),
        (float)(in.elem[1]),
        (float)(in.elem[2]),
        (float)(in.elem[3]),
    }};
}
static cdsm__f32x4 cdsm__convert4_f16_to_f32(const cdsm__u16x4 in)
{
    return {{
        cdsm__convert1_f16_to_f32(in.elem[0]),
        cdsm__convert1_f16_to_f32(in.elem[1]),
        cdsm__convert1_f16_to_f32(in.elem[2]),
        cdsm__convert1_f16_to_f32(in.elem[3]),
    }};
}
static cdsm__f32x4 cdsm__convert4_u32_to_f32(const cdsm__u32x4 in)
{
    return {{
        (float)(in.elem[0]),
        (float)(in.elem[1]),
        (float)(in.elem[2]),
        (float)(in.elem[3]),
    }};
}
static cdsm__f32x4 cdsm__convert4_s32_to_f32(const cdsm__s32x4 in)
{
    return {{
        (float)(in.elem[0]),
        (float)(in.elem[1]),
        (float)(in.elem[2]),
        (float)(in.elem[3]),
    }};
}


static cdsm__u8x4 cdsm__convert_f32_to_u8n(const cdsm__f32x4 in)
{
    return {{
        (uint8_t)( CDSM__CLAMP(in.elem[0], 0.0f, 1.0f) * 255.0f + 0.5f),
        (uint8_t)( CDSM__CLAMP(in.elem[1], 0.0f, 1.0f) * 255.0f + 0.5f),
        (uint8_t)( CDSM__CLAMP(in.elem[2], 0.0f, 1.0f) * 255.0f + 0.5f),
        (uint8_t)( CDSM__CLAMP(in.elem[3], 0.0f, 1.0f) * 255.0f + 0.5f),
    }};
}
static cdsm__s8x4 cdsm__convert_f32_to_s8n(const cdsm__f32x4 in)
{
    return {{
        (int8_t)floorf( CDSM__CLAMP(in.elem[0], -1.0f, 1.0f) * 127.0f
            + ((in.elem[0] >= 0) ? 0.5f : -0.5f) ),
        (int8_t)floorf( CDSM__CLAMP(in.elem[1], -1.0f, 1.0f) * 127.0f
            + ((in.elem[1] >= 0) ? 0.5f : -0.5f) ),
        (int8_t)floorf( CDSM__CLAMP(in.elem[2], -1.0f, 1.0f) * 127.0f
            + ((in.elem[2] >= 0) ? 0.5f : -0.5f) ),
        (int8_t)floorf( CDSM__CLAMP(in.elem[3], -1.0f, 1.0f) * 127.0f
            + ((in.elem[3] >= 0) ? 0.5f : -0.5f) ),
    }};
}
static cdsm__u8x4 cdsm__convert_f32_to_u8(const cdsm__f32x4 in)
{
    return {{
        (uint8_t)( CDSM__CLAMP(in.elem[0], 0.0f, 1.0f) + 0.5f ),
        (uint8_t)( CDSM__CLAMP(in.elem[1], 0.0f, 1.0f) + 0.5f ),
        (uint8_t)( CDSM__CLAMP(in.elem[2], 0.0f, 1.0f) + 0.5f ),
        (uint8_t)( CDSM__CLAMP(in.elem[3], 0.0f, 1.0f) + 0.5f ),
    }};
}
static cdsm__s8x4 cdsm__convert_f32_to_s8(const cdsm__f32x4 in)
{
    return {{
        (int8_t)floorf( CDSM__CLAMP(in.elem[0], -1.0f, 1.0f)
            + ((in.elem[0] >= 0) ? 0.5f : -0.5f) ),
        (int8_t)floorf( CDSM__CLAMP(in.elem[1], -1.0f, 1.0f)
            + ((in.elem[1] >= 0) ? 0.5f : -0.5f) ),
        (int8_t)floorf( CDSM__CLAMP(in.elem[2], -1.0f, 1.0f)
            + ((in.elem[2] >= 0) ? 0.5f : -0.5f) ),
        (int8_t)floorf( CDSM__CLAMP(in.elem[3], -1.0f, 1.0f)
            + ((in.elem[3] >= 0) ? 0.5f : -0.5f) ),
    }};
}
static cdsm__u16x4 cdsm__convert_f32_to_u16n(const cdsm__f32x4 in)
{
    return {{
        (uint16_t)( CDSM__CLAMP(in.elem[0], 0.0f, 1.0f) * 65535.0f + 0.5f ),
        (uint16_t)( CDSM__CLAMP(in.elem[1], 0.0f, 1.0f) * 65535.0f + 0.5f ),
        (uint16_t)( CDSM__CLAMP(in.elem[2], 0.0f, 1.0f) * 65535.0f + 0.5f ),
        (uint16_t)( CDSM__CLAMP(in.elem[3], 0.0f, 1.0f) * 65535.0f + 0.5f ),
    }};
}
static cdsm__s16x4 cdsm__convert_f32_to_s16n(const cdsm__f32x4 in)
{
    return {{
        (int16_t)floorf( CDSM__CLAMP(in.elem[0], -1.0f, 1.0f) * 32767.0f
            + ((in.elem[0] >= 0) ? 0.5f : -0.5f) ),
        (int16_t)floorf( CDSM__CLAMP(in.elem[1], -1.0f, 1.0f) * 32767.0f
            + ((in.elem[1] >= 0) ? 0.5f : -0.5f) ),
        (int16_t)floorf( CDSM__CLAMP(in.elem[2], -1.0f, 1.0f) * 32767.0f
            + ((in.elem[2] >= 0) ? 0.5f : -0.5f) ),
        (int16_t)floorf( CDSM__CLAMP(in.elem[3], -1.0f, 1.0f) * 32767.0f
            + ((in.elem[3] >= 0) ? 0.5f : -0.5f) ),
    }};
}
static cdsm__u16x4 cdsm__convert_f32_to_u16(const cdsm__f32x4 in)
{
    return {{
        (uint16_t)( CDSM__CLAMP(in.elem[0], 0.0f, 1.0f) + 0.5f ),
        (uint16_t)( CDSM__CLAMP(in.elem[1], 0.0f, 1.0f) + 0.5f ),
        (uint16_t)( CDSM__CLAMP(in.elem[2], 0.0f, 1.0f) + 0.5f ),
        (uint16_t)( CDSM__CLAMP(in.elem[3], 0.0f, 1.0f) + 0.5f ),
    }};
}
static cdsm__s16x4 cdsm__convert_f32_to_s16(const cdsm__f32x4 in)
{
    return {{
        (int16_t)floorf( CDSM__CLAMP(in.elem[0], -1.0f, 1.0f)
            + ((in.elem[0] >= 0) ? 0.5f : -0.5f) ),
        (int16_t)floorf( CDSM__CLAMP(in.elem[1], -1.0f, 1.0f)
            + ((in.elem[1] >= 0) ? 0.5f : -0.5f) ),
        (int16_t)floorf( CDSM__CLAMP(in.elem[2], -1.0f, 1.0f)
            + ((in.elem[2] >= 0) ? 0.5f : -0.5f) ),
        (int16_t)floorf( CDSM__CLAMP(in.elem[3], -1.0f, 1.0f)
            + ((in.elem[3] >= 0) ? 0.5f : -0.5f) ),
    }};
}
static cdsm__u16x4 cdsm__convert_f32_to_f16(const cdsm__f32x4 in)
{
    return {{
        cdsm__convert1_f32_to_f16(in.elem[0]),
        cdsm__convert1_f32_to_f16(in.elem[1]),
        cdsm__convert1_f32_to_f16(in.elem[2]),
        cdsm__convert1_f32_to_f16(in.elem[3]),
    }};
}
static cdsm__u32x4 cdsm__convert_f32_to_u32(const cdsm__f32x4 in)
{
    return {{
        (uint32_t)( CDSM__MAX(in.elem[0], 0.0f) + 0.5f ),
        (uint32_t)( CDSM__MAX(in.elem[1], 0.0f) + 0.5f ),
        (uint32_t)( CDSM__MAX(in.elem[2], 0.0f) + 0.5f ),
        (uint32_t)( CDSM__MAX(in.elem[3], 0.0f) + 0.5f ),
    }};
}
static cdsm__s32x4 cdsm__convert_f32_to_s32(const cdsm__f32x4 in)
{
    return {{
        (int32_t)floorf( in.elem[0] + ((in.elem[0] >= 0) ? 0.5f : -0.5f) ),
        (int32_t)floorf( in.elem[1] + ((in.elem[1] >= 0) ? 0.5f : -0.5f) ),
        (int32_t)floorf( in.elem[2] + ((in.elem[2] >= 0) ? 0.5f : -0.5f) ),
        (int32_t)floorf( in.elem[3] + ((in.elem[3] >= 0) ? 0.5f : -0.5f) ),
    }};
}

static void cdsm__convert_attribute(const void *in, cdsm_attribute_format_t in_format,
    void *out, cdsm_attribute_format_t out_format)
{
    /* TODO(cort): special case for in_format == out_format that's just a small memcpy? */

    uint32_t in_comp = format_info_lut[in_format].components;
    uint32_t out_comp = format_info_lut[out_format].components;

    /* Load/decompress input data into an f32x4 */
    cdsm__f32x4 temp_f32 = {{0,0,0,0}};
    switch(in_format)
    {
    case CDSM_ATTRIBUTE_FORMAT_UNKNOWN:
        break;
    case CDSM_ATTRIBUTE_FORMAT_R8_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_UNORM:
    {
        cdsm__u8x4 temp_u8 = {{0,0,0,0}};
        const uint8_t *in_u8 = (const uint8_t*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_u8.elem[i] = in_u8[i];
        temp_f32 = cdsm__convert4_u8n_to_f32(temp_u8);
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R8_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_SNORM:
    {
        cdsm__s8x4 temp_s8 = {{0,0,0,0}};
        const int8_t *in_s8 = (const int8_t*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_s8.elem[i] = in_s8[i];
        temp_f32 = cdsm__convert4_s8n_to_f32(temp_s8);
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R8_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_UINT:
    {
        cdsm__u8x4 temp_u8 = {{0,0,0,0}};
        const uint8_t *in_u8 = (const uint8_t*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_u8.elem[i] = in_u8[i];
        temp_f32 = cdsm__convert4_u8_to_f32(temp_u8);
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R8_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_SINT:
    {
        cdsm__s8x4 temp_s8 = {{0,0,0,0}};
        const int8_t *in_s8 = (const int8_t*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_s8.elem[i] = in_s8[i];
        temp_f32 = cdsm__convert4_s8_to_f32(temp_s8);
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R16_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_UNORM:
    {
        cdsm__u16x4 temp_u16 = {{0,0,0,0}};
        const uint16_t *in_u16 = (const uint16_t*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_u16.elem[i] = in_u16[i];
        temp_f32 = cdsm__convert4_u16n_to_f32(temp_u16);
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R16_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_SNORM:
    {
        cdsm__s16x4 temp_s16 = {{0,0,0,0}};
        const int16_t *in_s16 = (const int16_t*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_s16.elem[i] = in_s16[i];
        temp_f32 = cdsm__convert4_s16n_to_f32(temp_s16);
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R16_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_UINT:
    {
        cdsm__u16x4 temp_u16 = {{0,0,0,0}};
        const uint16_t *in_u16 = (const uint16_t*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_u16.elem[i] = in_u16[i];
        temp_f32 = cdsm__convert4_u16_to_f32(temp_u16);
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R16_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_SINT:
    {
        cdsm__s16x4 temp_s16 = {{0,0,0,0}};
        const int16_t *in_s16 = (const int16_t*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_s16.elem[i] = in_s16[i];
        temp_f32 = cdsm__convert4_s16_to_f32(temp_s16);
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R16_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_FLOAT:
    {
        cdsm__u16x4 temp_u16 = {{0,0,0,0}};
        const uint16_t *in_u16 = (const uint16_t*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_u16.elem[i] = in_u16[i];
        temp_f32 = cdsm__convert4_f16_to_f32(temp_u16);
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R32_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_UINT:
    {
        cdsm__u32x4 temp_u32 = {{0,0,0,0}};
        const uint32_t *in_u32 = (const uint32_t*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_u32.elem[i] = in_u32[i];
        temp_f32 = cdsm__convert4_u32_to_f32(temp_u32);
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R32_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_SINT:
    {
        cdsm__s32x4 temp_s32 = {{0,0,0,0}};
        const int32_t *in_s32 = (const int32_t*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_s32.elem[i] = in_s32[i];
        temp_f32 = cdsm__convert4_s32_to_f32(temp_s32);
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R32_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_FLOAT:
    {
        const float *in_f32 = (const float*)in;
        for(uint32_t i=0; i<in_comp; ++i)
            temp_f32.elem[i] = in_f32[i];
        break;
    }
    }

    /* Convert temp f32 to output format. */
    switch(out_format)
    {
    case CDSM_ATTRIBUTE_FORMAT_UNKNOWN:
        break;
    case CDSM_ATTRIBUTE_FORMAT_R8_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_UNORM:
    {
        cdsm__u8x4 temp_u8 = {{0,0,0,0}};
        temp_u8 = cdsm__convert_f32_to_u8n(temp_f32);
        uint8_t *out_u8 = (uint8_t*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_u8[i] = temp_u8.elem[i];
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R8_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_SNORM:
    {
        cdsm__s8x4 temp_s8 = {{0,0,0,0}};
        temp_s8 = cdsm__convert_f32_to_s8n(temp_f32);
        int8_t *out_s8 = (int8_t*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_s8[i] = temp_s8.elem[i];
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R8_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_UINT:
    {
        cdsm__u8x4 temp_u8 = {{0,0,0,0}};
        temp_u8 = cdsm__convert_f32_to_u8(temp_f32);
        uint8_t *out_u8 = (uint8_t*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_u8[i] = temp_u8.elem[i];
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R8_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R8G8B8A8_SINT:
    {
        cdsm__s8x4 temp_s8 = {{0,0,0,0}};
        temp_s8 = cdsm__convert_f32_to_s8(temp_f32);
        int8_t *out_s8 = (int8_t*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_s8[i] = temp_s8.elem[i];
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R16_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16_UNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_UNORM:
    {
        cdsm__u16x4 temp_u16 = {{0,0,0,0}};
        temp_u16 = cdsm__convert_f32_to_u16n(temp_f32);
        uint16_t *out_u16 = (uint16_t*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_u16[i] = temp_u16.elem[i];
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R16_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16_SNORM:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_SNORM:
    {
        cdsm__s16x4 temp_s16 = {{0,0,0,0}};
        temp_s16 = cdsm__convert_f32_to_s16n(temp_f32);
        int16_t *out_s16 = (int16_t*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_s16[i] = temp_s16.elem[i];
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R16_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_UINT:
    {
        cdsm__u16x4 temp_u16 = {{0,0,0,0}};
        temp_u16 = cdsm__convert_f32_to_u16(temp_f32);
        uint16_t *out_u16 = (uint16_t*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_u16[i] = temp_u16.elem[i];
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R16_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_SINT:
    {
        cdsm__s16x4 temp_s16 = {{0,0,0,0}};
        temp_s16 = cdsm__convert_f32_to_s16(temp_f32);
        int16_t *out_s16 = (int16_t*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_s16[i] = temp_s16.elem[i];
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R16_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R16G16B16A16_FLOAT:
    {
        cdsm__u16x4 temp_u16 = {{0,0,0,0}};
        temp_u16 = cdsm__convert_f32_to_f16(temp_f32);
        uint16_t *out_u16 = (uint16_t*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_u16[i] = temp_u16.elem[i];
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R32_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32_UINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_UINT:
    {
        cdsm__u32x4 temp_u32 = {{0,0,0,0}};
        temp_u32 = cdsm__convert_f32_to_u32(temp_f32);
        uint32_t *out_u32 = (uint32_t*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_u32[i] = temp_u32.elem[i];
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R32_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32_SINT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_SINT:
    {
        cdsm__s32x4 temp_s32 = {{0,0,0,0}};
        temp_s32 = cdsm__convert_f32_to_s32(temp_f32);
        int32_t *out_s32 = (int32_t*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_s32[i] = temp_s32.elem[i];
        break;
    }
    case CDSM_ATTRIBUTE_FORMAT_R32_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32_FLOAT:
    case CDSM_ATTRIBUTE_FORMAT_R32G32B32A32_FLOAT:
    {
        float *out_f32 = (float*)out;
        for(uint32_t i=0; i<out_comp; ++i)
            out_f32[i] = temp_f32.elem[i];
        break;
    }
    }
}

CDSM_DEF
cdsm_error_t cdsm_convert_vertex_buffer(const void *src_vertices, const cdsm_vertex_layout_t *src_layout,
    void *dst_vertices, const cdsm_vertex_layout_t *dst_layout, size_t vertex_count)
{
    for(uint32_t a = 0; a < src_layout->attribute_count; ++a)
    {
        if (src_layout->attributes[a].format < CDSM_ATTRIBUTE_FORMAT_BEGIN_RANGE ||
            src_layout->attributes[a].format > CDSM_ATTRIBUTE_FORMAT_END_RANGE)
        {
            return -1;
        }
    }
    for(uint32_t a = 0; a < dst_layout->attribute_count; ++a)
    {
        if (dst_layout->attributes[a].format < CDSM_ATTRIBUTE_FORMAT_BEGIN_RANGE ||
            dst_layout->attributes[a].format > CDSM_ATTRIBUTE_FORMAT_END_RANGE)
        {
            return -1;
        }
    }

    const uint8_t *src_bytes = (const uint8_t*)src_vertices;
    uint8_t *dst_bytes = (uint8_t*)dst_vertices;
    for(size_t v = 0; v < vertex_count; ++v)
    {
        for(uint32_t a = 0; a < src_layout->attribute_count; ++a)
        {
            cdsm__convert_attribute(src_bytes + src_layout->attributes[a].offset, src_layout->attributes[a].format,
                dst_bytes + dst_layout->attributes[a].offset, dst_layout->attributes[a].format);
        }
        src_bytes += src_layout->stride;
        dst_bytes += dst_layout->stride;
    }
    return 0;
}

/*************************** Mesh generation *****************************/

CDSM_DEF
cdsm_error_t cdsm_create_cube(cdsm_metadata_t *out_metadata,
    void *out_vertices, size_t *out_vertices_size,
    cdsm_index_t *out_indices, size_t *out_indices_size,
    const cdsm_cube_recipe_t *recipe)
{
    if (!( (out_vertices && out_indices) || (!out_vertices && !out_indices) ))
    {
        return -1; /* both must be NULL or both must be non-NULL. */
    }
    out_metadata->vertex_layout = recipe->vertex_layout;
    out_metadata->index_count  = 3 * 2 * 6;
    out_metadata->vertex_count = 4 * 6;
    out_metadata->primitive_type = CDSM_PRIMITIVE_TYPE_TRIANGLE_LIST;
    out_metadata->front_face = recipe->front_face;

    const size_t min_vertices_size = out_metadata->vertex_count * recipe->vertex_layout.stride;
    const size_t min_indices_size  = out_metadata->index_count  * sizeof(cdsm_index_t);
    if (out_vertices == 0 || out_indices == 0)
    {
        *out_vertices_size = min_vertices_size;
        *out_indices_size = min_indices_size;
        return 0;
    }
    if (*out_vertices_size < min_vertices_size ||
        *out_indices_size < min_indices_size)
    {
        return -2;
    }

    /* min = 0,1,2,  max = 3,4,5, */
    float face_pos[6];
    face_pos[0] = recipe->min_extent.x;
    face_pos[1] = recipe->min_extent.y;
    face_pos[2] = recipe->min_extent.z;
    face_pos[3] = recipe->max_extent.x;
    face_pos[4] = recipe->max_extent.y;
    face_pos[5] = recipe->max_extent.z;

    const int face_pos_indices[] = {
        3,1,5,  3,1,2,  3,4,5,  3,4,2, /* +X */
        0,1,2,  0,1,5,  0,4,2,  0,4,5, /* -X */
        0,4,5,  3,4,5,  0,4,2,  3,4,2, /* +Y */
        0,1,2,  3,1,2,  0,1,5,  3,1,5, /* -Y */
        0,1,5,  3,1,5,  0,4,5,  3,4,5, /* +Z */
        3,1,2,  0,1,2,  3,4,2,  0,4,2, /* -Z */
    };
    const float face_uvs[] = {
        0,1, 1,1, 0,0, 1,0,
    };
    const float face_normals[] = {
        +1,+0,+0,
        -1,+0,+0,
        +0,+1,+0,
        +0,-1,+0,
        +0,+0,+1,
        +0,+0,-1,
    };
    cdsm_index_t index_offset[2];
    index_offset[0] = (recipe->front_face == CDSM_FRONT_FACE_CCW) ? 1 : 2;
    index_offset[1] = (recipe->front_face == CDSM_FRONT_FACE_CCW) ? 2 : 1;
    uint8_t *next_out_vertex = (uint8_t*)out_vertices;
    for(int iFace=0; iFace<6; ++iFace)
    {
        cdsm__default_vertex_t temp_vertex = {};
        temp_vertex.position[0] = face_pos[ face_pos_indices[12*iFace+ 0] ];
        temp_vertex.position[1] = face_pos[ face_pos_indices[12*iFace+ 1] ];
        temp_vertex.position[2] = face_pos[ face_pos_indices[12*iFace+ 2] ];
        temp_vertex.normal[0] = face_normals[3*iFace+0];
        temp_vertex.normal[1] = face_normals[3*iFace+1];
        temp_vertex.normal[2] = face_normals[3*iFace+2];
        temp_vertex.texcoord[0] = face_uvs[0];
        temp_vertex.texcoord[1] = face_uvs[1];
        cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout,
            next_out_vertex, &recipe->vertex_layout, 1);
        next_out_vertex += recipe->vertex_layout.stride;

        temp_vertex.position[0] = face_pos[ face_pos_indices[12*iFace+ 3] ];
        temp_vertex.position[1] = face_pos[ face_pos_indices[12*iFace+ 4] ];
        temp_vertex.position[2] = face_pos[ face_pos_indices[12*iFace+ 5] ];
        temp_vertex.normal[0] = face_normals[3*iFace+0];
        temp_vertex.normal[1] = face_normals[3*iFace+1];
        temp_vertex.normal[2] = face_normals[3*iFace+2];
        temp_vertex.texcoord[0] = face_uvs[2];
        temp_vertex.texcoord[1] = face_uvs[3];
        cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout,
            next_out_vertex, &recipe->vertex_layout, 1);
        next_out_vertex += recipe->vertex_layout.stride;

        temp_vertex.position[0] = face_pos[ face_pos_indices[12*iFace+ 6] ];
        temp_vertex.position[1] = face_pos[ face_pos_indices[12*iFace+ 7] ];
        temp_vertex.position[2] = face_pos[ face_pos_indices[12*iFace+ 8] ];
        temp_vertex.normal[0] = face_normals[3*iFace+0];
        temp_vertex.normal[1] = face_normals[3*iFace+1];
        temp_vertex.normal[2] = face_normals[3*iFace+2];
        temp_vertex.texcoord[0] = face_uvs[4];
        temp_vertex.texcoord[1] = face_uvs[5];
        cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout,
            next_out_vertex, &recipe->vertex_layout, 1);
        next_out_vertex += recipe->vertex_layout.stride;

        temp_vertex.position[0] = face_pos[ face_pos_indices[12*iFace+ 9] ];
        temp_vertex.position[1] = face_pos[ face_pos_indices[12*iFace+10] ];
        temp_vertex.position[2] = face_pos[ face_pos_indices[12*iFace+11] ];
        temp_vertex.normal[0] = face_normals[3*iFace+0];
        temp_vertex.normal[1] = face_normals[3*iFace+1];
        temp_vertex.normal[2] = face_normals[3*iFace+2];
        temp_vertex.texcoord[0] = face_uvs[6];
        temp_vertex.texcoord[1] = face_uvs[7];
        cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout,
            next_out_vertex, &recipe->vertex_layout, 1);
        next_out_vertex += recipe->vertex_layout.stride;

        out_indices[6*iFace+0] = 4*iFace+0;
        out_indices[6*iFace+1] = 4*iFace+index_offset[0];
        out_indices[6*iFace+2] = 4*iFace+index_offset[1];
        out_indices[6*iFace+3] = 4*iFace+index_offset[1];
        out_indices[6*iFace+4] = 4*iFace+index_offset[0];
        out_indices[6*iFace+5] = 4*iFace+3;
    }
    return 0;
}

CDSM_DEF
cdsm_error_t cdsm_create_sphere(cdsm_metadata_t *out_metadata,
    void *out_vertices, size_t *out_vertices_size,
    cdsm_index_t *out_indices, size_t *out_indices_size,
    const cdsm_sphere_recipe_t *recipe)
{
    if (!( (out_vertices && out_indices) || (!out_vertices && !out_indices) ))
    {
        return -1; /* both must be NULL or both must be non-NULL. */
    }
    if (recipe->latitudinal_segments < 2 || recipe->longitudinal_segments < 3)
    {
        return -3;
    }
    out_metadata->vertex_layout = recipe->vertex_layout;
    // Each longitudinal segment has one triangle for each of the firs two latitudinal segments,
    // and two triangles for each additional latitudinal segment beyond that.
    out_metadata->index_count  = recipe->longitudinal_segments * (1 + 1 + 2*(recipe->latitudinal_segments-2)) * 3;
    // Every latitudinal segment adds one vertex per longitudinal segment.
    out_metadata->vertex_count = (recipe->latitudinal_segments+1) * recipe->longitudinal_segments;
    out_metadata->primitive_type = CDSM_PRIMITIVE_TYPE_TRIANGLE_LIST;
    out_metadata->front_face = CDSM_FRONT_FACE_CCW;

    const size_t min_vertices_size = out_metadata->vertex_count * recipe->vertex_layout.stride;
    const size_t min_indices_size  = out_metadata->index_count  * sizeof(cdsm_index_t);
    if (out_vertices == 0 || out_indices == 0)
    {
        *out_vertices_size = min_vertices_size;
        *out_indices_size = min_indices_size;
        return 0;
    }
    if (*out_vertices_size < min_vertices_size ||
        *out_indices_size < min_indices_size)
    {
        return -2;
    }

    uint8_t *vert = (uint8_t*)out_vertices;
    cdsm__default_vertex_t temp_vertex = {};
    for(int i_ring = 0; i_ring <= recipe->latitudinal_segments; ++i_ring)
    {
        const float phi_lerp = (float)i_ring / (float)recipe->latitudinal_segments; // [0..1]
        const float phi = phi_lerp * CDSM__PI;
        const float z = -recipe->radius * cosf(phi);
        const float ring_radius = recipe->radius * sinf(phi);
        const float normal_z = z / recipe->radius;
        const float normal_xy_scale = ring_radius / recipe->radius;
        const float texcoord_u_offset = (i_ring == 0 || i_ring == recipe->latitudinal_segments) ?
            1.0f / (2.0f * (float)recipe->longitudinal_segments) : 0.0f;
        for(int i_ring_vert = 0; i_ring_vert < recipe->longitudinal_segments; ++i_ring_vert)
        {
            const float radial_lerp = (float)i_ring_vert / (float)recipe->longitudinal_segments; // [0..1)
            const float theta = (2.0f * CDSM__PI) * radial_lerp;
            const float sin_t = sinf(theta);
            const float cos_t = cosf(theta);
            temp_vertex.position[0] = cos_t * ring_radius;
            temp_vertex.position[1] = sin_t * ring_radius;
            temp_vertex.position[2] = z;
            temp_vertex.normal[0] = cos_t * normal_xy_scale;
            temp_vertex.normal[1] = sin_t * normal_xy_scale;
            temp_vertex.normal[2] = normal_z;
            temp_vertex.texcoord[0] = radial_lerp + texcoord_u_offset;
            temp_vertex.texcoord[1] = phi_lerp;
            cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
            vert += recipe->vertex_layout.stride;
        }
    }
    assert(vert == (uint8_t*)out_vertices + out_metadata->vertex_count * recipe->vertex_layout.stride);

    cdsm_index_t *tri = out_indices;
    for(int i_strip = 0; i_strip < recipe->longitudinal_segments; ++i_strip)
    {
        int i_ring = 0;
        tri[0] = (i_ring+0) * recipe->longitudinal_segments + ((i_strip + 0));
        tri[1] = (i_ring+1) * recipe->longitudinal_segments + ((i_strip + 1) % recipe->longitudinal_segments);
        tri[2] = (i_ring+1) * recipe->longitudinal_segments + ((i_strip + 0));
        tri += 3;
        for(i_ring=1; i_ring <= recipe->latitudinal_segments-2; ++i_ring)
        {
            tri[0] = (i_ring+0) * recipe->longitudinal_segments + ((i_strip + 0));
            tri[1] = (i_ring+0) * recipe->longitudinal_segments + ((i_strip + 1) % recipe->longitudinal_segments);
            tri[2] = (i_ring+1) * recipe->longitudinal_segments + ((i_strip + 0));

            tri[3] = (i_ring+1) * recipe->longitudinal_segments + ((i_strip + 0));
            tri[4] = (i_ring+0) * recipe->longitudinal_segments + ((i_strip + 1) % recipe->longitudinal_segments);
            tri[5] = (i_ring+1) * recipe->longitudinal_segments + ((i_strip + 1) % recipe->longitudinal_segments);

            tri += 6;
        }
        i_ring = recipe->latitudinal_segments-1;
        tri[0] = (i_ring+0) * recipe->longitudinal_segments + ((i_strip + 0));
        tri[1] = (i_ring+0) * recipe->longitudinal_segments + ((i_strip + 1) % recipe->longitudinal_segments);
        tri[2] = (i_ring+1) * recipe->longitudinal_segments + ((i_strip + 0));
        tri += 3;
    }
    assert(tri == out_indices + out_metadata->index_count);

    return 0;
}


CDSM_DEF
cdsm_error_t cdsm_create_axes(cdsm_metadata_t *out_metadata,
    void *out_vertices, size_t *out_vertices_size,
    cdsm_index_t *out_indices, size_t *out_indices_size,
    const cdsm_axes_recipe_t *recipe)
{
    if (!( (out_vertices && out_indices) || (!out_vertices && !out_indices) ))
    {
        return -1; /* both must be NULL or both must be non-NULL. */
    }
    out_metadata->vertex_layout = recipe->vertex_layout;
    out_metadata->index_count  = 2 * 3;
    out_metadata->vertex_count = 2 * 3;
    out_metadata->primitive_type = CDSM_PRIMITIVE_TYPE_LINE_LIST;
    out_metadata->front_face = CDSM_FRONT_FACE_CCW;
    
    const size_t min_vertices_size = out_metadata->vertex_count * recipe->vertex_layout.stride;
    const size_t min_indices_size  = out_metadata->index_count  * sizeof(cdsm_index_t);
    if (out_vertices == 0 || out_indices == 0)
    {
        *out_vertices_size = min_vertices_size;
        *out_indices_size = min_indices_size;
        return 0;
    }
    if (*out_vertices_size < min_vertices_size ||
        *out_indices_size < min_indices_size)
    {
        return -2;
    }

    cdsm__default_vertex_t temp_vertex;
    uint8_t *vert = (uint8_t*)out_vertices;

    temp_vertex.position[0] = 0;
    temp_vertex.position[1] = 0;
    temp_vertex.position[2] = 0;
    temp_vertex.normal[0] = 1;
    temp_vertex.normal[1] = 0;
    temp_vertex.normal[2] = 0;
    temp_vertex.texcoord[0] = 0;
    temp_vertex.texcoord[1] = 0;
    cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
    vert += recipe->vertex_layout.stride;
    temp_vertex.position[0] = recipe->length;
    temp_vertex.position[1] = 0;
    temp_vertex.position[2] = 0;
    temp_vertex.normal[0] = 1;
    temp_vertex.normal[1] = 0;
    temp_vertex.normal[2] = 0;
    temp_vertex.texcoord[0] = 1;
    temp_vertex.texcoord[1] = 0;
    cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
    vert += recipe->vertex_layout.stride;

    temp_vertex.position[0] = 0;
    temp_vertex.position[1] = 0;
    temp_vertex.position[2] = 0;
    temp_vertex.normal[0] = 0;
    temp_vertex.normal[1] = 1;
    temp_vertex.normal[2] = 0;
    temp_vertex.texcoord[0] = 0;
    temp_vertex.texcoord[1] = 0;
    cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
    vert += recipe->vertex_layout.stride;
    temp_vertex.position[0] = 0;
    temp_vertex.position[1] = recipe->length;
    temp_vertex.position[2] = 0;
    temp_vertex.normal[0] = 0;
    temp_vertex.normal[1] = 1;
    temp_vertex.normal[2] = 0;
    temp_vertex.texcoord[0] = 1;
    temp_vertex.texcoord[1] = 0;
    cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
    vert += recipe->vertex_layout.stride;

    temp_vertex.position[0] = 0;
    temp_vertex.position[1] = 0;
    temp_vertex.position[2] = 0;
    temp_vertex.normal[0] = 0;
    temp_vertex.normal[1] = 0;
    temp_vertex.normal[2] = 1;
    temp_vertex.texcoord[0] = 0;
    temp_vertex.texcoord[1] = 0;
    cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
    vert += recipe->vertex_layout.stride;
    temp_vertex.position[0] = 0;
    temp_vertex.position[1] = 0;
    temp_vertex.position[2] = recipe->length;
    temp_vertex.normal[0] = 0;
    temp_vertex.normal[1] = 0;
    temp_vertex.normal[2] = 1;
    temp_vertex.texcoord[0] = 1;
    temp_vertex.texcoord[1] = 0;
    cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
    vert += recipe->vertex_layout.stride;

    out_indices[0] = 0;
    out_indices[1] = 1;
    out_indices[2] = 2;
    out_indices[3] = 3;
    out_indices[4] = 4;
    out_indices[5] = 5;
    return 0;
}

CDSM_DEF
cdsm_error_t cdsm_create_cylinder(cdsm_metadata_t *out_metadata,
    void *out_vertices, size_t *out_vertices_size,
    cdsm_index_t *out_indices, size_t *out_indices_size,
    const cdsm_cylinder_recipe_t *recipe)
{
    if (!( (out_vertices && out_indices) || (!out_vertices && !out_indices) ))
    {
        return -1; /* both must be NULL or both must be non-NULL. */
    }
    if (recipe->radial_segments < 3 || recipe->axial_segments < 1)
    {
        return -3;
    }
    out_metadata->vertex_layout = recipe->vertex_layout;
    // Each endcap has radial_segments triangles.
    // Each length segment has radial_segments*2 triangles.
    out_metadata->index_count  = 3 *
        ((2 * recipe->radial_segments) + (2 * recipe->radial_segments * recipe->axial_segments));
    // radial_segments=N -> N vertices per circle.
    // axial_segments=N -> N+1 circles per cylinder.
    // +2 more circles per cylinder (duplicate verts along the endcaps)
    // +2 vertices (one in the center of each endcap)
    out_metadata->vertex_count = 2
        + (recipe->radial_segments * (recipe->axial_segments + 1 + 2));
    out_metadata->primitive_type = CDSM_PRIMITIVE_TYPE_TRIANGLE_LIST;
    out_metadata->front_face = CDSM_FRONT_FACE_CCW;
    
    const size_t min_vertices_size = out_metadata->vertex_count * recipe->vertex_layout.stride;
    const size_t min_indices_size  = out_metadata->index_count  * sizeof(cdsm_index_t);
    if (out_vertices == 0 || out_indices == 0)
    {
        *out_vertices_size = min_vertices_size;
        *out_indices_size = min_indices_size;
        return 0;
    }
    if (*out_vertices_size < min_vertices_size ||
        *out_indices_size < min_indices_size)
    {
        return -2;
    }

    const float d_radius = recipe->radius0 - recipe->radius1;
    const float denominator = 1.0f / sqrtf(d_radius*d_radius + recipe->length*recipe->length);
    const float normal_z = d_radius * denominator * (d_radius>=0 ? -1.0f : 1.0f);
    const float normal_xy_scale = d_radius ? (recipe->length * denominator) : 1.0f;

    cdsm__default_vertex_t temp_vertex = {};
    uint8_t *vert = (uint8_t*)out_vertices;
    int i_ring = 0;
    for(i_ring = 0; i_ring <= recipe->axial_segments; ++i_ring)
    {
        const float axial_lerp = (float)i_ring / (float)recipe->axial_segments; // [0..1]
        const float z = recipe->length * axial_lerp;
        const float ring_radius = recipe->radius0 + axial_lerp * (recipe->radius1 - recipe->radius0);
        for(int i_ring_vert = 0; i_ring_vert < recipe->radial_segments; ++i_ring_vert)
        {
            const float radial_lerp = (float)i_ring_vert / (float)recipe->radial_segments; // [0..1)
            const float theta = 2.0f * CDSM__PI * radial_lerp;
            const float sin_t = sinf(theta);
            const float cos_t = cosf(theta);
            temp_vertex.position[0] = cos_t * ring_radius;
            temp_vertex.position[1] = sin_t * ring_radius;
            temp_vertex.position[2] = z;
            temp_vertex.normal[0] = cos_t * normal_xy_scale;
            temp_vertex.normal[1] = sin_t * normal_xy_scale;
            temp_vertex.normal[2] = normal_z;
            temp_vertex.texcoord[0] = radial_lerp;
            temp_vertex.texcoord[1] = axial_lerp;
            cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
            vert += recipe->vertex_layout.stride;
        }
    }

    i_ring = 0;
    cdsm_index_t cap_start0 = (cdsm_index_t)( (vert - (uint8_t*)out_vertices) / recipe->vertex_layout.stride );
    {
        const float axial_lerp = (float)i_ring / (float)recipe->axial_segments; // [0..1]
        const float z = recipe->length * axial_lerp;
        const float ring_radius = recipe->radius0 + axial_lerp * (recipe->radius1 - recipe->radius0);
        for(int i_ring_vert = 0; i_ring_vert < recipe->radial_segments; ++i_ring_vert)
        {
            const float radial_lerp = (float)i_ring_vert / (float)recipe->radial_segments; // [0..1)
            const float theta = 2.0f * CDSM__PI * radial_lerp;
            const float sin_t = sinf(theta);
            const float cos_t = cosf(theta);
            temp_vertex.position[0] = cos_t * ring_radius;
            temp_vertex.position[1] = sin_t * ring_radius;
            temp_vertex.position[2] = z;
            temp_vertex.normal[0] = 0;
            temp_vertex.normal[1] = 0;
            temp_vertex.normal[2] = -1;
            temp_vertex.texcoord[0] = radial_lerp;
            temp_vertex.texcoord[1] = axial_lerp;
            cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
            vert += recipe->vertex_layout.stride;
        }
    }
    cdsm_index_t cap_center0 = (cdsm_index_t)( (vert - (uint8_t*)out_vertices) / recipe->vertex_layout.stride );
    temp_vertex.position[0] = 0;
    temp_vertex.position[1] = 0;
    temp_vertex.position[2] = 0;
    temp_vertex.normal[0] = 0;
    temp_vertex.normal[1] = 0;
    temp_vertex.normal[2] = -1;
    temp_vertex.texcoord[0] = 0;
    temp_vertex.texcoord[1] = 0;
    cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
    vert += recipe->vertex_layout.stride;

    i_ring = recipe->axial_segments;
    cdsm_index_t cap_start1 = (cdsm_index_t)( (vert - (uint8_t*)out_vertices) / recipe->vertex_layout.stride );
    {
        const float axial_lerp = (float)i_ring / (float)recipe->axial_segments; // [0..1]
        const float z = recipe->length * axial_lerp;
        const float ring_radius = recipe->radius0 + axial_lerp * (recipe->radius1 - recipe->radius0);
        for(int i_ring_vert = 0; i_ring_vert < recipe->radial_segments; ++i_ring_vert)
        {
            const float radial_lerp = (float)i_ring_vert / (float)recipe->radial_segments; // [0..1)
            const float theta = 2.0f * CDSM__PI * radial_lerp;
            const float sin_t = sinf(theta);
            const float cos_t = cosf(theta);
            temp_vertex.position[0] = cos_t * ring_radius;
            temp_vertex.position[1] = sin_t * ring_radius;
            temp_vertex.position[2] = z;
            temp_vertex.normal[0] = 0;
            temp_vertex.normal[1] = 0;
            temp_vertex.normal[2] = 1;
            temp_vertex.texcoord[0] = radial_lerp;
            temp_vertex.texcoord[1] = axial_lerp;
            cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
            vert += recipe->vertex_layout.stride;
        }
    }
    cdsm_index_t cap_center1 = (cdsm_index_t)( (vert - (uint8_t*)out_vertices) / recipe->vertex_layout.stride );
    temp_vertex.position[0] = 0;
    temp_vertex.position[1] = 0;
    temp_vertex.position[2] = recipe->length;
    temp_vertex.normal[0] = 0;
    temp_vertex.normal[1] = 0;
    temp_vertex.normal[2] = 1;
    temp_vertex.texcoord[0] = 0;
    temp_vertex.texcoord[1] = 0;
    cdsm_convert_vertex_buffer(&temp_vertex, &cdsm__default_vertex_layout, vert, &recipe->vertex_layout, 1);
    vert += recipe->vertex_layout.stride;
    assert(vert == (uint8_t*)out_vertices + out_metadata->vertex_count * recipe->vertex_layout.stride);

    cdsm_index_t *tri = out_indices;
    for(i_ring = 0; i_ring < recipe->axial_segments; ++i_ring)
    {
        for(int i_ring_vert = 0; i_ring_vert < recipe->radial_segments; ++i_ring_vert)
        {
            tri[0] = (i_ring+0) * recipe->radial_segments + ((i_ring_vert + 0));
            tri[1] = (i_ring+0) * recipe->radial_segments + ((i_ring_vert + 1) % recipe->radial_segments);
            tri[2] = (i_ring+1) * recipe->radial_segments + ((i_ring_vert + 0));

            tri[3] = (i_ring+1) * recipe->radial_segments + ((i_ring_vert + 0));
            tri[4] = (i_ring+0) * recipe->radial_segments + ((i_ring_vert + 1) % recipe->radial_segments);
            tri[5] = (i_ring+1) * recipe->radial_segments + ((i_ring_vert + 1) % recipe->radial_segments);
            tri += 6;
        }
    }
    for(int i_ring_vert = 0; i_ring_vert < recipe->radial_segments; ++i_ring_vert)
    {
        tri[0] = cap_start0 + ((i_ring_vert + 0));
        tri[1] = cap_center0;
        tri[2] = cap_start0 + ((i_ring_vert + 1) % recipe->radial_segments);
        tri += 3;
    }
    for(int i_ring_vert = 0; i_ring_vert < recipe->radial_segments; ++i_ring_vert)
    {
        tri[0] = cap_start1 + ((i_ring_vert + 0));
        tri[1] = cap_start1 + ((i_ring_vert + 1) % recipe->radial_segments);
        tri[2] = cap_center1;
        tri += 3;
    }
    assert(tri == out_indices + out_metadata->index_count);

    return 0;
}

#ifndef CDSM_NO_STDIO
static FILE *cdsm__fopen(char const *filename, char const *mode)
{
   FILE *f;
#if defined(_MSC_VER) && _MSC_VER >= 1400
   if (0 != fopen_s(&f, filename, mode))
      f=0;
#else
   f = fopen(filename, mode);
#endif
   return f;
}
#endif

CDSM_DEF
cdsm_error_t cdsm_export_to_header(const char *filename, const char *prefix,
    cdsm_metadata_t const *metadata,
    cdsm__default_vertex_t const *vertices, cdsm_index_t const *indices)
{
    FILE *f = cdsm__fopen(filename, "w");
    
    const char *primitive_type = "UNKNOWN";
    if (metadata->primitive_type == CDSM_PRIMITIVE_TYPE_TRIANGLE_LIST)
        primitive_type = "TRIANGLE_LIST";
    else if (metadata->primitive_type == CDSM_PRIMITIVE_TYPE_LINE_LIST)
        primitive_type = "LINE_LIST";
    fprintf(f, "/* Primitive type is %s */\n", primitive_type);

    fprintf(f, "static const int %svertex_count = %d;\n", prefix, metadata->vertex_count);
    fprintf(f, "static const int %sindex_count = %d;\n", prefix, metadata->index_count);

    const char *vertex_type ="\
struct {\n\
    float position[3];\n\
    float normal[3];\n\
    float texcoord[2];\n\
}";
    fprintf(f, "static const %s %svertices[%d] = {\n", vertex_type, prefix, metadata->vertex_count);
    for(cdsm_s32 iVert=0; iVert<metadata->vertex_count; ++iVert)
    {
        cdsm__default_vertex_t temp_vertex = {};
        cdsm_convert_vertex_buffer( (uint8_t*)vertices + iVert*metadata->vertex_layout.stride,
            &metadata->vertex_layout, &temp_vertex, &cdsm__default_vertex_layout, 1);

        fprintf(f, "\t{\t{%.9ff, %.9ff, %.9ff},\n\t\t{%.9ff, %.9ff, %.9ff},\n\t\t{%.9ff, %.9ff} },\n",
            temp_vertex.position[0], temp_vertex.position[1], temp_vertex.position[2],
            temp_vertex.normal[0], temp_vertex.normal[1], temp_vertex.normal[2],
            temp_vertex.texcoord[0], temp_vertex.texcoord[1]);
    }
    fprintf(f, "};\n");

    size_t index_size = sizeof(cdsm_index_t);
    if (index_size == sizeof(uint16_t))
    {
        fprintf(f, "static const %s %sindices[%d] = {\n", "uint16_t", prefix, metadata->index_count);
        cdsm_s32 i=0;
        for(; i<metadata->index_count-11; i+=12)
        {
            fprintf(f, "\t%5u,%5u,%5u,%5u,%5u,%5u,%5u,%5u,%5u,%5u,%5u,%5u,\n",
                indices[i+ 0], indices[i+ 1], indices[i+ 2], indices[i+ 3],
                indices[i+ 4], indices[i+ 5], indices[i+ 6], indices[i+ 7],
                indices[i+ 8], indices[i+ 9], indices[i+10], indices[i+11]);
        }
        if (i < metadata->index_count)
        {
            fprintf(f, "\t");
            for(; i<metadata->index_count; i+=1)
            {
                fprintf(f, "%5u,", indices[i]);
            }
            fprintf(f, "\n");
        }
    }
    else if (index_size == sizeof(uint32_t))
    {
        fprintf(f, "static const %s %sindices[%d] = {\n", "uint32_t", prefix, metadata->index_count);
        cdsm_s32 i=0;
        for(; i<metadata->index_count-5; i+=6)
        {
            fprintf(f, "\t%10u,%10u,%10u,%10u,%10u,%10u,\n",
                indices[i+ 0], indices[i+ 1], indices[i+ 2],
                indices[i+ 3], indices[i+ 4], indices[i+ 5]);
        }
        if (i < metadata->index_count)
        {
            fprintf(f, "\t");
            for(; i<metadata->index_count; i+=1)
            {
                fprintf(f, "%10u,", indices[i]);
            }
            fprintf(f, "\n");
        }
    }
    fprintf(f, "};\n");

    fclose(f);
    return 0;
}


#endif /*------------- end implementation section ---------------*/

#if defined(CDS_MESH_TEST)

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//#include "test_cds_mesh_cube_mesh.h"

int main(int argc, char *argv[])
{
    CDSM__UNUSED(argc);
    CDSM__UNUSED(argv);

    const cdsm_vertex_layout_t vertex_layout = {
        22, 3, {
            {0, 0, CDSM_ATTRIBUTE_FORMAT_R32G32B32_FLOAT},
            {1, 12, CDSM_ATTRIBUTE_FORMAT_R16G16B16_SNORM},
            {2,18, CDSM_ATTRIBUTE_FORMAT_R16G16_FLOAT},
        }
    };

    enum
    {
        MESH_TYPE_CUBE     = 0,
        MESH_TYPE_SPHERE   = 1,
        MESH_TYPE_AXES     = 3,
        MESH_TYPE_CYLINDER = 2,
    } meshType = MESH_TYPE_SPHERE;
    cdsm_cube_recipe_t cube_recipe;
    cube_recipe.vertex_layout = cdsm__default_vertex_layout;
    cube_recipe.min_extent.x = -0.5f;
    cube_recipe.min_extent.y = -0.5f;
    cube_recipe.min_extent.z = -0.5f;
    cube_recipe.max_extent.x = +0.5f;
    cube_recipe.max_extent.y = +0.5f;
    cube_recipe.max_extent.z = +0.5f;
    cube_recipe.front_face = CDSM_FRONT_FACE_CCW;
    cdsm_sphere_recipe_t sphere_recipe;
    sphere_recipe.vertex_layout = vertex_layout;
    sphere_recipe.latitudinal_segments = 30;
    sphere_recipe.longitudinal_segments = 30;
    sphere_recipe.radius = 0.5f;
    cdsm_cylinder_recipe_t cylinder_recipe;
    cylinder_recipe.vertex_layout = vertex_layout;
    cylinder_recipe.length = 1.0f;
    cylinder_recipe.axial_segments = 3;
    cylinder_recipe.radial_segments = 60;
    cylinder_recipe.radius0 = -1.0f;
    cylinder_recipe.radius1 = 1.0f;
    cdsm_axes_recipe_t axes_recipe;
    axes_recipe.vertex_layout = vertex_layout;
    axes_recipe.length = 1.0f;

    cdsm_metadata_t metadata;
    size_t vertices_size = 0, indices_size = 0;
    cdsm_error_t result = 0;
    if      (meshType == MESH_TYPE_CUBE)
      result = cdsm_create_cube(&metadata, NULL, &vertices_size, NULL, &indices_size, &cube_recipe);
    else if (meshType == MESH_TYPE_SPHERE)
      result = cdsm_create_sphere(&metadata, NULL, &vertices_size, NULL, &indices_size, &sphere_recipe);
    else if (meshType == MESH_TYPE_AXES)
      result = cdsm_create_axes(&metadata, NULL, &vertices_size, NULL, &indices_size, &axes_recipe);
    else if (meshType == MESH_TYPE_CYLINDER)
      result = cdsm_create_cylinder(&metadata, NULL, &vertices_size, NULL, &indices_size, &cylinder_recipe);
    assert(result == 0);

    void *vertices = malloc(vertices_size);
    cdsm_index_t *indices = (cdsm_index_t*)malloc(indices_size);
    if      (meshType == MESH_TYPE_CUBE)
      result = cdsm_create_cube(&metadata, vertices, &vertices_size, indices, &indices_size, &cube_recipe);
    else if (meshType == MESH_TYPE_SPHERE)
      result = cdsm_create_sphere(&metadata, vertices, &vertices_size, indices, &indices_size, &sphere_recipe);
    else if (meshType == MESH_TYPE_AXES)
      result = cdsm_create_axes(&metadata, vertices, &vertices_size, indices, &indices_size, &axes_recipe);
    else if (meshType == MESH_TYPE_CYLINDER)
      result = cdsm_create_cylinder(&metadata, vertices, &vertices_size, indices, &indices_size, &cylinder_recipe);
    assert(result == 0);

    result = cdsm_export_to_header("test_cds_mesh_output.h", "mesh_",
            &metadata, (cdsm__default_vertex_t*)vertices, indices);
    assert(result == 0);

    free(vertices);
    free(indices);

    /* Test vertex attribute conversion */
    {
        printf("Testing f32 <-> f16 conversion...\n");
        uint32_t errors = 0;
        for(uint32_t i = 0; i < 65536; ++i)
        {
            cdsm__f16 f16_in, f16_out;
            cdsm__f32 f32;
            f16_in.asInt = (uint16_t)i;
            f32.asFloat = cdsm__convert1_f16_to_f32(f16_in.asInt);
            f16_out.asInt = cdsm__convert1_f32_to_f16(f32.asFloat);
            int isNan = 0;
            if      (f32.parts.exponent == 0xFF)
            {
                if (f32.parts.mantissa == 0)
                {
                    //printf("%04x -> %cinfinity -> %04x\n", h_in.asInt, f.parts.sign ? '-' : '+', h_out.asInt);
                }
                else
                {
                    isNan = 1;
                    //printf("%04x -> %c%cNaN -> %04x\n", h_in.asInt, f.parts.sign ? '-' : '+',
                    //    (f.parts.mantissa & (1<<22)) ? 'q' : 's', h_out.asInt);
                }
            }
            else
            {
                //printf("%04x -> %.20f -> %04x\n", h_in.asInt, (double)f, h_out.asInt);
            }
            if (isNan)
            {
                if (f16_in.parts.sign != f16_out.parts.sign ||
                    f16_in.parts.exponent != 0x1F || f16_out.parts.exponent != 0x1F ||
                    (f16_in.parts.mantissa & (1<<9)) != (f16_out.parts.mantissa & (1<<9)))
                {
                    errors += 1;
                }
            }
            else
            {
                if (f16_in.asInt != f16_out.asInt)
                {
                    errors += 1;
                }
            }
        }
        printf("Test complete (%u errors)\n", errors);
    }
    
    return 0;
}
#endif /*------------------- send self-test section ------------*/
