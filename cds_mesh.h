/* cds_mesh.h -- procedural 3D mesh generation in C89
 *               No warranty implied; use at your own risk.
 *
 * Do this:
 *   #define CDS_MESH_IMPLEMENTATION
 * before including this file in *one* C/C++ file to provide the function
 * implementations.
 *
 * For a unit test on gcc/Clang:
 *   cc -Wall -std=c89 -D_POSIX_C_SOURCE=199309L -g -x c -DCDS_MESH_TEST -o test_cds_mesh.exe cds_mesh.h
 *
 * For a unit test on Visual C++:
 *   "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat"
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

#ifdef __cplusplus
extern "C"
{
#endif

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

    typedef struct
    {
        float position[3];
        float normal[3];
#if 0
        float tangent[3];
        float bitangent[3];
#endif
        float texcoord[2];
    } cdsm_vertex_t;

    typedef enum {
        CDSM_PRIMITIVE_TYPE_TRIANGLE_LIST = 0,
        CDSM_PRIMITIVE_TYPE_LINE_LIST     = 1,
    } cdsm_primitive_type_t;

    typedef struct
    {
        cdsm_primitive_type_t primitive_type;
        cdsm_s32 vertex_count;
        cdsm_s32 index_count;
    } cdsm_metadata_t;

    typedef enum {
        CDSM_FRONT_FACE_CCW = 0,
        CDSM_FRONT_FACE_CW  = 1,
    } cdsm_front_face_t;

    typedef struct
    {
        struct { float x,y,z; } min_extent;
        struct { float x,y,z; } max_extent;
        cdsm_front_face_t front_face;
    } cdsm_cube_recipe_t;

    CDSM_DEF
    cdsm_error_t cdsm_create_cube(cdsm_metadata_t *out_metadata,
        cdsm_vertex_t *out_vertices, size_t *out_vertices_size,
        cdsm_index_t *out_indices, size_t *out_indices_size,
        const cdsm_cube_recipe_t *recipe);

    typedef struct
    {
        cdsm_s32 latitudinal_segments;
        cdsm_s32 longitudinal_segments;
        float radius;
    } cdsm_sphere_recipe_t;

    CDSM_DEF
    cdsm_error_t cdsm_create_sphere(cdsm_metadata_t *out_metadata,
        cdsm_vertex_t *out_vertices, size_t *out_vertices_size,
        cdsm_index_t *out_indices, size_t *out_indices_size,
        const cdsm_sphere_recipe_t *recipe);

    typedef struct
    {
        float length;
    } cdsm_axes_recipe_t;

    CDSM_DEF
    cdsm_error_t cdsm_create_axes(cdsm_metadata_t *out_metadata,
        cdsm_vertex_t *out_vertices, size_t *out_vertices_size,
        cdsm_index_t *out_indices, size_t *out_indices_size,
        const cdsm_axes_recipe_t *recipe);

    typedef struct
    {
        float length;
        float radius0, radius1;
        cdsm_s32 axial_segments;
        cdsm_s32 radial_segments;
    } cdsm_cylinder_recipe_t;

    CDSM_DEF
    cdsm_error_t cdsm_create_cylinder(cdsm_metadata_t *out_metadata,
        cdsm_vertex_t *out_vertices, size_t *out_vertices_size,
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
#include <stdint.h>

#define CDSM__MIN(a,b) ( (a)<(b) ? (a) : (b) )
#define CDSM__UNUSED(x) ((void)x)
static const float CDSM__PI = 3.14159265358979323846f;
CDSM_DEF
cdsm_error_t cdsm_create_cube(cdsm_metadata_t *out_metadata,
    cdsm_vertex_t *out_vertices, size_t *out_vertices_size,
    cdsm_index_t *out_indices, size_t *out_indices_size,
    const cdsm_cube_recipe_t *recipe)
{
    if (!( (out_vertices && out_indices) || (!out_vertices && !out_indices) ))
    {
        return -1; // both must be NULL or both must be non-NULL.
    }
    out_metadata->index_count  = 3 * 2 * 6;
    out_metadata->vertex_count = 4 * 6;
    out_metadata->primitive_type = CDSM_PRIMITIVE_TYPE_TRIANGLE_LIST;

    const size_t min_vertices_size = out_metadata->vertex_count * sizeof(cdsm_vertex_t);
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

    // min = 0,1,2,  max = 3,4,5,
    float face_pos[6];
    face_pos[0] = recipe->min_extent.x;
    face_pos[1] = recipe->min_extent.y;
    face_pos[2] = recipe->min_extent.z;
    face_pos[3] = recipe->max_extent.x;
    face_pos[4] = recipe->max_extent.y;
    face_pos[5] = recipe->max_extent.z;

    const int face_pos_indices[] = {
        3,1,5,  3,1,2,  3,4,5,  3,4,2, // +X
        0,1,2,  0,1,5,  0,4,2,  0,4,5, // -X
        0,4,5,  3,4,5,  0,4,2,  3,4,2, // +Y
        0,1,2,  3,1,2,  0,1,5,  3,1,5, // -Y
        0,1,5,  3,1,5,  0,4,5,  3,4,5, // +Z
        3,1,2,  0,1,2,  3,4,2,  0,4,2, // -Z
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
    for(int iFace=0; iFace<6; ++iFace)
    {
        out_vertices[4*iFace+0].position[0] = face_pos[ face_pos_indices[12*iFace+ 0] ];
        out_vertices[4*iFace+0].position[1] = face_pos[ face_pos_indices[12*iFace+ 1] ];
        out_vertices[4*iFace+0].position[2] = face_pos[ face_pos_indices[12*iFace+ 2] ];
        out_vertices[4*iFace+0].normal[0] = face_normals[3*iFace+0];
        out_vertices[4*iFace+0].normal[1] = face_normals[3*iFace+1];
        out_vertices[4*iFace+0].normal[2] = face_normals[3*iFace+2];
        out_vertices[4*iFace+0].texcoord[0] = face_uvs[0];
        out_vertices[4*iFace+0].texcoord[1] = face_uvs[1];

        out_vertices[4*iFace+1].position[0] = face_pos[ face_pos_indices[12*iFace+ 3] ];
        out_vertices[4*iFace+1].position[1] = face_pos[ face_pos_indices[12*iFace+ 4] ];
        out_vertices[4*iFace+1].position[2] = face_pos[ face_pos_indices[12*iFace+ 5] ];
        out_vertices[4*iFace+1].normal[0] = face_normals[3*iFace+0];
        out_vertices[4*iFace+1].normal[1] = face_normals[3*iFace+1];
        out_vertices[4*iFace+1].normal[2] = face_normals[3*iFace+2];
        out_vertices[4*iFace+1].texcoord[0] = face_uvs[2];
        out_vertices[4*iFace+1].texcoord[1] = face_uvs[3];

        out_vertices[4*iFace+2].position[0] = face_pos[ face_pos_indices[12*iFace+ 6] ];
        out_vertices[4*iFace+2].position[1] = face_pos[ face_pos_indices[12*iFace+ 7] ];
        out_vertices[4*iFace+2].position[2] = face_pos[ face_pos_indices[12*iFace+ 8] ];
        out_vertices[4*iFace+2].normal[0] = face_normals[3*iFace+0];
        out_vertices[4*iFace+2].normal[1] = face_normals[3*iFace+1];
        out_vertices[4*iFace+2].normal[2] = face_normals[3*iFace+2];
        out_vertices[4*iFace+2].texcoord[0] = face_uvs[4];
        out_vertices[4*iFace+2].texcoord[1] = face_uvs[5];

        out_vertices[4*iFace+3].position[0] = face_pos[ face_pos_indices[12*iFace+ 9] ];
        out_vertices[4*iFace+3].position[1] = face_pos[ face_pos_indices[12*iFace+10] ];
        out_vertices[4*iFace+3].position[2] = face_pos[ face_pos_indices[12*iFace+11] ];
        out_vertices[4*iFace+3].normal[0] = face_normals[3*iFace+0];
        out_vertices[4*iFace+3].normal[1] = face_normals[3*iFace+1];
        out_vertices[4*iFace+3].normal[2] = face_normals[3*iFace+2];
        out_vertices[4*iFace+3].texcoord[0] = face_uvs[6];
        out_vertices[4*iFace+3].texcoord[1] = face_uvs[7];

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
    cdsm_vertex_t *out_vertices, size_t *out_vertices_size,
    cdsm_index_t *out_indices, size_t *out_indices_size,
    const cdsm_sphere_recipe_t *recipe)
{
    if (!( (out_vertices && out_indices) || (!out_vertices && !out_indices) ))
    {
        return -1; // both must be NULL or both must be non-NULL.
    }
    if (recipe->latitudinal_segments < 2 || recipe->longitudinal_segments < 3)
    {
        return -3;
    }
    // Each longitudinal segment has one triangle for each of the firs two latitudinal segments,
    // and two triangles for each additional latitudinal segment beyond that.
    out_metadata->index_count  = recipe->longitudinal_segments * (1 + 1 + 2*(recipe->latitudinal_segments-2)) * 3;
    // Every latitudinal segment adds one vertex per longitudinal segment.
    out_metadata->vertex_count = (recipe->latitudinal_segments+1) * recipe->longitudinal_segments;
    out_metadata->primitive_type = CDSM_PRIMITIVE_TYPE_TRIANGLE_LIST;

    const size_t min_vertices_size = out_metadata->vertex_count * sizeof(cdsm_vertex_t);
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

    cdsm_vertex_t *vert = out_vertices;
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
            vert->position[0] = cos_t * ring_radius;
            vert->position[1] = sin_t * ring_radius;
            vert->position[2] = z;
            vert->normal[0] = cos_t * normal_xy_scale;
            vert->normal[1] = sin_t * normal_xy_scale;
            vert->normal[2] = normal_z;
            vert->texcoord[0] = radial_lerp + texcoord_u_offset;
            vert->texcoord[1] = phi_lerp;
            vert += 1;
        }
    }
    assert(vert == out_vertices + out_metadata->vertex_count);

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
    cdsm_vertex_t *out_vertices, size_t *out_vertices_size,
    cdsm_index_t *out_indices, size_t *out_indices_size,
    const cdsm_axes_recipe_t *recipe)
{
    if (!( (out_vertices && out_indices) || (!out_vertices && !out_indices) ))
    {
        return -1; // both must be NULL or both must be non-NULL.
    }
    out_metadata->index_count  = 2 * 3;
    out_metadata->vertex_count = 2 * 3;
    out_metadata->primitive_type = CDSM_PRIMITIVE_TYPE_LINE_LIST;

    const size_t min_vertices_size = out_metadata->vertex_count * sizeof(cdsm_vertex_t);
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

    out_vertices[0].position[0] = 0;
    out_vertices[0].position[1] = 0;
    out_vertices[0].position[2] = 0;
    out_vertices[0].normal[0] = 1;
    out_vertices[0].normal[1] = 0;
    out_vertices[0].normal[2] = 0;
    out_vertices[1].position[0] = recipe->length;
    out_vertices[1].position[1] = 0;
    out_vertices[1].position[2] = 0;
    out_vertices[1].normal[0] = 1;
    out_vertices[1].normal[1] = 0;
    out_vertices[1].normal[2] = 0;

    out_vertices[2].position[0] = 0;
    out_vertices[2].position[1] = 0;
    out_vertices[2].position[2] = 0;
    out_vertices[2].normal[0] = 0;
    out_vertices[2].normal[1] = 1;
    out_vertices[2].normal[2] = 0;
    out_vertices[3].position[0] = 0;
    out_vertices[3].position[1] = recipe->length;
    out_vertices[3].position[2] = 0;
    out_vertices[3].normal[0] = 0;
    out_vertices[3].normal[1] = 1;
    out_vertices[3].normal[2] = 0;

    out_vertices[4].position[0] = 0;
    out_vertices[4].position[1] = 0;
    out_vertices[4].position[2] = 0;
    out_vertices[4].normal[0] = 0;
    out_vertices[4].normal[1] = 0;
    out_vertices[4].normal[2] = 1;
    out_vertices[5].position[0] = 0;
    out_vertices[5].position[1] = 0;
    out_vertices[5].position[2] = recipe->length;
    out_vertices[5].normal[0] = 0;
    out_vertices[5].normal[1] = 0;
    out_vertices[5].normal[2] = 1;

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
    cdsm_vertex_t *out_vertices, size_t *out_vertices_size,
    cdsm_index_t *out_indices, size_t *out_indices_size,
    const cdsm_cylinder_recipe_t *recipe)
{
    if (!( (out_vertices && out_indices) || (!out_vertices && !out_indices) ))
    {
        return -1; // both must be NULL or both must be non-NULL.
    }
    if (recipe->radial_segments < 3 || recipe->axial_segments < 1)
    {
        return -3;
    }
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

    const size_t min_vertices_size = out_metadata->vertex_count * sizeof(cdsm_vertex_t);
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

    cdsm_vertex_t *vert = out_vertices;
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
            vert->position[0] = cos_t * ring_radius;
            vert->position[1] = sin_t * ring_radius;
            vert->position[2] = z;
            vert->normal[0] = cos_t * normal_xy_scale;
            vert->normal[1] = sin_t * normal_xy_scale;
            vert->normal[2] = normal_z;
            vert->texcoord[0] = radial_lerp;
            vert->texcoord[1] = axial_lerp;
            vert += 1;
        }
    }

    i_ring = 0;
    cdsm_index_t cap_start0 = (cdsm_index_t)(vert - out_vertices);
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
            vert->position[0] = cos_t * ring_radius;
            vert->position[1] = sin_t * ring_radius;
            vert->position[2] = z;
            vert->normal[0] = 0;
            vert->normal[1] = 0;
            vert->normal[2] = -1;
            vert->texcoord[0] = radial_lerp;
            vert->texcoord[1] = axial_lerp;
            vert += 1;
        }
    }
    cdsm_index_t cap_center0 = (cdsm_index_t)(vert - out_vertices);
    vert->position[0] = 0;
    vert->position[1] = 0;
    vert->position[2] = 0;
    vert->normal[0] = 0;
    vert->normal[1] = 0;
    vert->normal[2] = -1;
    vert->texcoord[0] = 0;
    vert->texcoord[1] = 0;
    vert += 1;

    i_ring = recipe->axial_segments;
    cdsm_index_t cap_start1 = (cdsm_index_t)(vert - out_vertices);
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
            vert->position[0] = cos_t * ring_radius;
            vert->position[1] = sin_t * ring_radius;
            vert->position[2] = z;
            vert->normal[0] = 0;
            vert->normal[1] = 0;
            vert->normal[2] = 1;
            vert->texcoord[0] = radial_lerp;
            vert->texcoord[1] = axial_lerp;
            vert += 1;
        }
    }
    cdsm_index_t cap_center1 = (cdsm_index_t)(vert - out_vertices);
    vert->position[0] = 0;
    vert->position[1] = 0;
    vert->position[2] = recipe->length;
    vert->normal[0] = 0;
    vert->normal[1] = 0;
    vert->normal[2] = 1;
    vert->texcoord[0] = 0;
    vert->texcoord[1] = 0;
    vert += 1;
    assert(vert == out_vertices + out_metadata->vertex_count);

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

#endif /*------------- end implementation section ---------------*/

#if defined(CDS_MESH_TEST)

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[])
{
    CDSM__UNUSED(argc);
    CDSM__UNUSED(argv);
    return 0;
}
#endif /*------------------- send self-test section ------------*/
