#include "platform.h"
#include "vk_vertex.h"
#include "vk_mesh.h"

#include "assimp/DefaultLogger.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <array>
#include <stdio.h>
#include <string>
#include <vector>

struct Scene {
  uint32_t mesh_count;
  spokk::MeshFormat mesh_format;
};

constexpr uint32_t SPOKK_MAX_VERTEX_COLORS    = 4;
constexpr uint32_t SPOKK_MAX_VERTEX_TEXCOORDS = 4;

enum VertexAttributeLocation {
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION     =  0,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL       =  1,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_TANGENT      =  2,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_BITANGENT    =  3,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_BONE_INDEX   =  4,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_BONE_WEIGHT  =  5,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_COLOR0       =  6,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_COLOR1       =  7,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_COLOR2       =  8,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_COLOR3       =  9,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD0    = 10,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD1    = 11,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD2    = 12,
  SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD3    = 13,

  SPOKK_VERTEX_ATTRIBUTE_LOCATION_COUNT
};

struct SourceAttribute {
  spokk::VertexLayout layout;
  const void* values;
};

static void handleReadFileError(const std::string &errorString)
{
  fprintf(stderr, "ERROR: %s\n", errorString.c_str());
}

static int processScene(const aiScene *inScene, Scene *outScene)
{
  static_assert(sizeof(aiVector2D) == 2*sizeof(float), "aiVector2D sizes do not match!");
  static_assert(sizeof(aiVector3D) == 3*sizeof(float), "aiVector3D sizes do not match!");
  static_assert(sizeof(aiColor4D)  == 4*sizeof(float), "aiColor4D sizes do not match!");

  outScene->mesh_count = inScene->mNumMeshes;

  std::vector<SourceAttribute> src_attributes = {{}};
  uint32_t iMesh = 0;
  const aiMesh *mesh = inScene->mMeshes[iMesh];

  // Query available vertex attributes, and determine the mesh format
  ZOMBO_ASSERT(mesh->HasPositions(), "wtf sort of mesh doesn't include vertex positions?!?");
  if (mesh->HasPositions())
  {
    static_assert(sizeof(mesh->mVertices[0]) == sizeof(aiVector3D), "positions aren't vec3s!");
    spokk::VertexLayout::AttributeInfo pos_attr = {};
    pos_attr.location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION;
    pos_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
    pos_attr.offset = 0;
    src_attributes.push_back({{pos_attr}, mesh->mVertices});
  }
  if (mesh->HasNormals())
  {
    // TODO(cort): octohedral normals (https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/)
    static_assert(sizeof(mesh->mNormals[0]) == sizeof(aiVector3D), "normals aren't vec3s!");
    spokk::VertexLayout::AttributeInfo norm_attr = {};
    norm_attr.location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL;
    norm_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
    norm_attr.offset = 0;
    src_attributes.push_back({{norm_attr}, mesh->mNormals});
  }
  if (mesh->HasTangentsAndBitangents()) // Assimp always gives you both, or neither.
  {
    static_assert(sizeof(mesh->mTangents[0]) == sizeof(aiVector3D), "tangents aren't vec3s!");
    spokk::VertexLayout::AttributeInfo tan_attr = {};
    tan_attr.location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_TANGENT;
    tan_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
    tan_attr.offset = 0;
    src_attributes.push_back({{tan_attr}, mesh->mTangents});

    static_assert(sizeof(mesh->mBitangents[0]) == sizeof(aiVector3D), "bitangents aren't vec3s!");
    spokk::VertexLayout::AttributeInfo bitan_attr = {};
    bitan_attr.location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_BITANGENT;
    bitan_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
    bitan_attr.offset = 0;
    src_attributes.push_back({{bitan_attr}, mesh->mBitangents});
  }
  for(int iColorSet = 0; iColorSet < AI_MAX_NUMBER_OF_COLOR_SETS; ++iColorSet)
  {
    static_assert(sizeof(mesh->mColors[iColorSet][0]) == sizeof(aiColor4D), "colors aren't vec4s!");
    if (mesh->HasVertexColors(iColorSet))
    {
      if (iColorSet > SPOKK_MAX_VERTEX_COLORS) {
        fprintf(stderr, "WARNING: ignoring vertex color set %u in mesh %u\n", iColorSet, iMesh);
        continue;
      }
      spokk::VertexLayout::AttributeInfo color_attr = {};
      color_attr.location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_COLOR0 + iColorSet;
      color_attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
      color_attr.offset = 0;
      src_attributes.push_back({{color_attr}, mesh->mColors[iColorSet]});
    }
  }
  for(int iUvSet = 0; iUvSet < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++iUvSet)
  {
    static_assert(sizeof(mesh->mTextureCoords[iUvSet][0]) == sizeof(aiVector3D), "texcoords aren't vec3s!");
    if (mesh->HasTextureCoords(iUvSet))
    {
      if (iUvSet > SPOKK_MAX_VERTEX_TEXCOORDS) {
        fprintf(stderr, "WARNING: ignoring vertex texcoord set %u in mesh %u\n", iUvSet, iMesh);
        continue;
      }
      uint32_t components = mesh->mNumUVComponents[iUvSet];
      ZOMBO_ASSERT(components >= 1 && components <= 3, "invalid texcoord component count (%u)", components);
      spokk::VertexLayout::AttributeInfo tc_attr = {};
      tc_attr.location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD0 + iUvSet;
      tc_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
      tc_attr.offset = 0;
      src_attributes.push_back({{tc_attr}, mesh->mTextureCoords[iUvSet]});
    }
  }

  // Build vertex buffer
  const uint32_t vertex_count = mesh->mNumVertices;
  const spokk::VertexLayout dst_layout = {
    {SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION, VK_FORMAT_R32G32B32_SFLOAT, 0},
    {SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL, VK_FORMAT_R32G32B32_SFLOAT, 12},
    {SPOKK_VERTEX_ATTRIBUTE_LOCATION_TEXCOORD0, VK_FORMAT_R32G32_SFLOAT, 24},
  };
  std::vector<uint8_t> vertices(dst_layout.stride * vertex_count, 0);
  for(const auto& attrib : src_attributes) {
    int convert_error = spokk::ConvertVertexBuffer(attrib.values, attrib.layout,
      vertices.data(), dst_layout, vertex_count);
    ZOMBO_ASSERT(convert_error == 0, "error converting attribute at location %u",
      attrib.layout.attributes[0].location);
  }

  // Load index buffer
  if (!mesh->HasFaces())
  {
    fprintf(stderr, "ERROR: currently, only meshes with faces are supported.");
    return -1;
  }
  // Extract face data into a flat array
  uint32_t max_index_count = mesh->mNumFaces * 3;
  uint32_t bytes_per_index = (vertex_count <= 0x10000) ? sizeof(uint16_t) : sizeof(uint32_t);
  std::vector<uint8_t> indices(max_index_count * bytes_per_index, 0);
  uint32_t index_count = 0;
  for(uint32_t iFace=0; iFace<mesh->mNumFaces; ++iFace)
  {
    const aiFace &face = mesh->mFaces[iFace];
    if (face.mNumIndices != 3)
    {
      // skip non-triangles. We triangulated at import time, so these should be lines & points.
      ZOMBO_ASSERT(face.mNumIndices < 3, "face %u has %u indices -- didn't we triangulate & discard degenerates?",
        iFace, face.mNumIndices);
      continue;
    }
    if (bytes_per_index == 4) {
      uint32_t* next_tri = reinterpret_cast<uint32_t*>(indices.data()) + index_count;
      next_tri[0] = face.mIndices[0];
      next_tri[1] = face.mIndices[1];
      next_tri[2] = face.mIndices[2];
    } else if (bytes_per_index == 2) {
      uint16_t* next_tri = reinterpret_cast<uint16_t*>(indices.data()) + index_count;
      next_tri[0] = (uint16_t)face.mIndices[0];
      next_tri[1] = (uint16_t)face.mIndices[1];
      next_tri[2] = (uint16_t)face.mIndices[2];
    }
    index_count += 3;
  }

  // Write mesh to disk
  {
    struct {
      uint32_t magic_number;
      uint32_t vertex_buffer_count;
      uint32_t attribute_count;
      uint32_t bytes_per_index;
      uint32_t vertex_count;
      uint32_t index_count;
      uint32_t topology; // currently ignored, assume triangles
    } mesh_header = {};
    mesh_header.magic_number = 0x12345678;
    mesh_header.vertex_buffer_count = 1;
    mesh_header.attribute_count = (uint32_t)dst_layout.attributes.size();
    mesh_header.bytes_per_index = bytes_per_index;
    mesh_header.vertex_count = vertex_count;
    mesh_header.index_count = index_count;
    mesh_header.topology = 1;
    std::vector<VkVertexInputBindingDescription> vb_descs(mesh_header.vertex_buffer_count, {});
    {
      vb_descs[0].binding = 0;
      vb_descs[0].stride = dst_layout.stride;
      vb_descs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    std::vector<VkVertexInputAttributeDescription> attr_descs(dst_layout.attributes.size(), {});
    for(size_t iAttr = 0; iAttr < attr_descs.size(); ++iAttr) {
      attr_descs[iAttr].location = dst_layout.attributes[iAttr].location;
      attr_descs[iAttr].binding = 0;
      attr_descs[iAttr].format = dst_layout.attributes[iAttr].format;
      attr_descs[iAttr].offset = dst_layout.attributes[iAttr].offset;
    }

    const std::string out_filename = "data/teapot.mesh";
    FILE *out_file = fopen(out_filename.c_str(), "wb");
    if (out_file == nullptr) {
      fprintf(stderr, "Could not open %s for writing\n", out_filename.c_str());
      return -1;
    }
    fwrite(&mesh_header, sizeof(mesh_header), 1, out_file);
    fwrite(vb_descs.data(), sizeof(vb_descs[0]), vb_descs.size(), out_file);
    fwrite(attr_descs.data(), sizeof(attr_descs[0]), attr_descs.size(), out_file);
    fwrite(vertices.data(), dst_layout.stride, vertex_count, out_file);
    fwrite(indices.data(), bytes_per_index, index_count, out_file);
    fclose(out_file);
  }
  return 0;
}

int ImportFromFile( const std::string& filename )
{
  // Uncomment to enable importer logging (can be quite verbose!)
  //Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE, aiDefaultLogStream_STDERR);

  // Create an instance of the Importer class
  Assimp::Importer importer;
  // Configure the importer properties
  importer.SetPropertyBool(AI_CONFIG_PP_FD_REMOVE, true); // Remove degenerate triangles entirely, rather than degrading them to points/lines.
  importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT); // remove all points/lines from the scene
                                                                                                      //importer.SetPropertyBool(AI_CONFIG_GLOB_MEASURE_TIME, true); // uncomment to log timings of various import stages
  importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f); // Specify maximum angle between neighboring faces such that their
                                                                          // shared vertices will have their normals smoothed.
                                                                          // Default is 175.0; docs say 80.0 will give a good visual appearance
                                                                          // And have it read the given file with some example postprocessing
                                                                          // Usually - if speed is not the most important aspect for you - you'll
                                                                          // probably to request more postprocessing than we do in this example.
  const aiScene* scene = importer.ReadFile( filename, 0
    | aiProcess_GenSmoothNormals       // Generate per-vertex normals, if none exist
    | aiProcess_CalcTangentSpace       // Compute per-vertex tangent and bitangent vectors (if the mesh already has normals and UVs)
    | aiProcess_Triangulate            // Convert faces with >3 vertices to 2 or more triangles
    | aiProcess_JoinIdenticalVertices  // If this flag is not specified, each vertex is used by exactly one face; no index buffer is required.
    | aiProcess_SortByPType            // Sort faces by primitive type -- one sub-mesh per primitive type.
    | aiProcess_ImproveCacheLocality   // Reorder vertex and index buffers to improve post-transform cache locality.
  //  | aiProcess_FlipUVs                // HACK -- the scene we're currently loading has its UVs flipped.
  );
  // If the import failed, report it
  if( !scene)
  {
    handleReadFileError( importer.GetErrorString());
    return -1;
  }
  // Now we can access the file's contents.
  Scene output_scene = {};
  int error = processScene(scene, &output_scene);
  // We're done. Everything will be cleaned up by the importer destructor
  Assimp::DefaultLogger::kill();
  return error;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    return -1;
  }
  const char *input_filename = argv[1];
  ImportFromFile(input_filename);
}