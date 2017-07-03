#include "platform.h"
#include "vk_vertex.h"
#include "vk_mesh.h"  // for MeshHeader
#include "vk_shader_interface.h"

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <json.h>

#ifdef _MSC_VER
#include <Shlwapi.h>  // for PathFileExists
#endif

#include <array>
#include <stdio.h>
#include <string>
#include <vector>

constexpr uint32_t SPOKK_MAX_VERTEX_COLORS    = 4;
constexpr uint32_t SPOKK_MAX_VERTEX_TEXCOORDS = 4;

struct SourceAttribute {
  spokk::VertexLayout layout;
  const void* values;
};

static void handleReadFileError(const std::string &errorString)
{
  fprintf(stderr, "ERROR: %s\n", errorString.c_str());
}

int ConvertSceneToMesh( const std::string& input_scene_filename, const std::string& output_mesh_filename) {
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
  const aiScene* scene = importer.ReadFile(input_scene_filename.c_str(), 0
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

  static_assert(sizeof(aiVector2D) == 2*sizeof(float), "aiVector2D sizes do not match!");
  static_assert(sizeof(aiVector3D) == 3*sizeof(float), "aiVector3D sizes do not match!");
  static_assert(sizeof(aiColor4D)  == 4*sizeof(float), "aiColor4D sizes do not match!");

  ZOMBO_ASSERT_RETURN(scene->mNumMeshes == 1, -1, "Currently, only one mesh per scene is supported.");

  std::vector<SourceAttribute> src_attributes = {{}};
  uint32_t iMesh = 0;
  const aiMesh *mesh = scene->mMeshes[iMesh];

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

  // Compute bounding volume
  aiVector3D aabb_min = {+FLT_MAX, +FLT_MAX, +FLT_MAX};
  aiVector3D aabb_max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
  const uint32_t vertex_count = mesh->mNumVertices;
  for(uint32_t i = 0; i < vertex_count; ++i) {
    aiVector3D v = mesh->mVertices[i];
    aabb_min.x = std::min(aabb_min.x, v.x);
    aabb_min.y = std::min(aabb_min.y, v.y);
    aabb_min.z = std::min(aabb_min.z, v.z);
    aabb_max.x = std::max(aabb_max.x, v.x);
    aabb_max.y = std::max(aabb_max.y, v.y);
    aabb_max.z = std::max(aabb_max.z, v.z);
  }

  // Build vertex buffer
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
  ZOMBO_ASSERT_RETURN(mesh->HasFaces(), -1, "mesh has no faces! This is (currently) required.");
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
    spokk::MeshFileHeader mesh_header = {};
    mesh_header.magic_number = spokk::MESH_FILE_MAGIC_NUMBER;
    mesh_header.vertex_buffer_count = 1;
    mesh_header.attribute_count = (uint32_t)dst_layout.attributes.size();
    mesh_header.bytes_per_index = bytes_per_index;
    mesh_header.vertex_count = vertex_count;
    mesh_header.index_count = index_count;
    mesh_header.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    mesh_header.aabb_min[0] = aabb_min.x;
    mesh_header.aabb_min[1] = aabb_min.y;
    mesh_header.aabb_min[2] = aabb_min.z;
    mesh_header.aabb_max[0] = aabb_max.x;
    mesh_header.aabb_max[1] = aabb_max.y;
    mesh_header.aabb_max[2] = aabb_max.z;
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

    FILE *out_file = fopen(output_mesh_filename.c_str(), "wb");
    if (out_file == nullptr) {
      fprintf(stderr, "Could not open %s for writing\n", output_mesh_filename.c_str());
      return -1;
    }
    fwrite(&mesh_header, sizeof(mesh_header), 1, out_file);
    fwrite(vb_descs.data(), sizeof(vb_descs[0]), vb_descs.size(), out_file);
    fwrite(attr_descs.data(), sizeof(attr_descs[0]), attr_descs.size(), out_file);
    fwrite(vertices.data(), dst_layout.stride, vertex_count, out_file);
    fwrite(indices.data(), bytes_per_index, index_count, out_file);
    fclose(out_file);
  }

  // We're done. Everything will be cleaned up by the importer destructor
  Assimp::DefaultLogger::kill();
  return 0;
}

//////////////////////////
// manifest parsing
//////////////////////////

class AssetManifest {
public:
  explicit AssetManifest(const std::string& json5_filename);
  ~AssetManifest();
private:
  AssetManifest(const AssetManifest& rhs) = delete;
  AssetManifest& operator=(const AssetManifest& rhs) = delete;

  // Converts a json_parse_error_e to a human-readable string
  std::string JsonParseErrorStr(const json_parse_error_e error_code) const;
  // Returns a human-readable location of the specified json_value_s.
  // This assumes the manifest file was parsed with the
  // 'json_parse_flags_allow_location_information' flag enabled.
  std::string JsonValueLocationStr(const json_value_s* val) const;

  enum AssetClass {
    ASSET_CLASS_UNKNOWN = 0,
    ASSET_CLASS_IMAGE   = 1,
    ASSET_CLASS_MESH    = 2,
  };
  AssetClass AssetManifest::GetAssetClassFromInputPath(const json_value_s* input_path_val) const;

  // Individual value parsers. Each returns 0 on success, non-zero on error.
  int ParseRoot(const json_value_s* val);
  int ParseAssets(const json_value_s* val);
  int ParseAsset(const json_value_s* val);

  int IsOutputOutOfDate(const json_string_s* input_path, const json_string_s* output_path, bool *out_result) const;
  bool CopyAssetFile(const json_string_s* input_path, const json_string_s* output_path) const;

  int ProcessImage(const json_string_s* input_path, const json_string_s* output_path);
  int ProcessMesh(const json_string_s* input_path, const json_string_s* output_path);

  std::string manifest_filename_;
  std::string output_root_;
};

AssetManifest::AssetManifest(const std::string& json5_filename)
  : manifest_filename_(json5_filename),
    output_root_(".") {
  FILE *manifest_file = zomboFopen(manifest_filename_.c_str(), "rb");
  if (!manifest_file) {
    fprintf(stderr, "ERROR: Could not open %s\n", manifest_filename_.c_str());
    return;
  }
  fseek(manifest_file, 0, SEEK_END);
  size_t manifest_nbytes = ftell(manifest_file);
  std::vector<uint8_t> manifest_bytes(manifest_nbytes);
  fseek(manifest_file, 0, SEEK_SET);
  size_t read_nbytes = fread(manifest_bytes.data(), 1, manifest_nbytes, manifest_file);
  fclose(manifest_file);
  if (read_nbytes != manifest_nbytes) {
    fprintf(stderr, "ERROR: file I/O error while reading from %s\n", manifest_filename_.c_str());
    return;
  }

  json_parse_result_s parse_result = {};
  json_value_s* manifest = json_parse_ex(manifest_bytes.data(), manifest_bytes.size(),
    json_parse_flags_allow_json5 | json_parse_flags_allow_location_information,
    NULL, NULL, &parse_result);
  if (!manifest) {
    fprintf(stderr, "%s(%u): error %u at column %u (%s)\n", manifest_filename_.c_str(),
      (uint32_t)parse_result.error_line_no, (uint32_t)parse_result.error, (uint32_t)parse_result.error_row_no,
      JsonParseErrorStr((json_parse_error_e)parse_result.error).c_str());
    return;
  }
  int parse_error = ParseRoot(manifest);
  ZOMBO_ASSERT(parse_error == 0, "ParseRoot() returned %d at %s", parse_error, JsonValueLocationStr(manifest).c_str());
  free(manifest);
}
AssetManifest::~AssetManifest() {
}

std::string AssetManifest::JsonParseErrorStr(const json_parse_error_e error_code) const {
  switch(error_code) {
  case json_parse_error_none:
    return "Success";
  case json_parse_error_expected_comma_or_closing_bracket:
    return "Expected comma or closing bracket";
  case json_parse_error_expected_colon:
    return "Expected colon separating name and value";
  case json_parse_error_expected_opening_quote:
    return "Expected string to begin with \"";
  case json_parse_error_invalid_string_escape_sequence:
    return "Invalid escape sequence in string";
  case json_parse_error_invalid_number_format:
    return "Invalid number format";
  case json_parse_error_invalid_value:
    return "Invalid value";
  case json_parse_error_premature_end_of_buffer:
    return "Unexpected end of input buffer in mid-object/array";
  case json_parse_error_invalid_string:
    return "Invalid/malformed string";
  case json_parse_error_allocator_failed:
    return "Memory allocation failure";
  case json_parse_error_unexpected_trailing_characters:
    return "Unexpected trailing characters after JSON data";
  case json_parse_error_unknown:
    return "Uncategorized error";
  default:
    return "Legitimately unrecognized error code (something messed up REAL bad)";
  }
}

std::string AssetManifest::JsonValueLocationStr(const json_value_s* val) const {
  const json_value_ex_s* val_ex = (const json_value_ex_s*)val;
  return manifest_filename_
    + "line " + std::to_string(val_ex->line_no)
    + ", column " + std::to_string(val_ex->row_no);
}

int AssetManifest::ParseRoot(const json_value_s* val) {
  ZOMBO_ASSERT_RETURN(val->type == json_type_object, -1,
    "ERROR: root payload (%s) must be an object", JsonValueLocationStr(val).c_str());
  json_object_s* root_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  for(json_object_element_s* child_elem = root_obj->start;
      i_child < root_obj->length;
      ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "assets") == 0) {
      int parse_error = ParseAssets(child_elem->value);
      ZOMBO_ASSERT_RETURN(parse_error == 0, -2, "ParseAssets() returned %d at %s", parse_error, JsonValueLocationStr(val).c_str());
    } else if (strcmp(child_elem->name->string, "globals") == 0) {
      // TODO(cort): global asset settings go here
    }
  }
  return 0;
}

int AssetManifest::ParseAssets(const json_value_s* val) {
  ZOMBO_ASSERT_RETURN(val->type == json_type_array, -1,
    "ERROR: assets payload (%s) must be an array\n", JsonValueLocationStr(val).c_str());
  const json_array_s* assets_array = (const json_array_s*)(val->payload);
  size_t i_child = 0;
  for(json_array_element_s* child_elem = assets_array->start;
      i_child < assets_array->length;
      ++i_child, child_elem = child_elem->next) {
    int parse_error = ParseAsset(child_elem->value);
    ZOMBO_ASSERT_RETURN(parse_error == 0, -2, "ParseAsset() returned %d at %s", parse_error, JsonValueLocationStr(val).c_str());
  }
  return 0;
}

int AssetManifest::ParseAsset(const json_value_s* val) {
  ZOMBO_ASSERT_RETURN(val->type == json_type_object, -1,
    "ERROR: asset payload (%s) must be an object", JsonValueLocationStr(val).c_str());
  AssetClass asset_class = ASSET_CLASS_UNKNOWN;
  const json_string_s* input_path = nullptr;
  const json_string_s* output_path = nullptr;
  json_object_s* asset_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  for(json_object_element_s* child_elem = asset_obj->start;
      i_child < asset_obj->length;
      ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "class") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -2,
        "ERROR: asset class payload (%s) must be a string", JsonValueLocationStr(child_elem->value).c_str());
      const json_string_s* asset_class_str = (const json_string_s*)child_elem->value->payload;
      if (strcmp(asset_class_str->string, "image") == 0) {
        asset_class = ASSET_CLASS_IMAGE;
      } else if (strcmp(asset_class_str->string, "mesh") == 0) {
        asset_class = ASSET_CLASS_MESH;
      } else {
        ZOMBO_ERROR_RETURN(-2, "ERROR: unknown asset class '%s' at %s", asset_class_str->string,
          JsonValueLocationStr(child_elem->value).c_str());
      }
    } else if (strcmp(child_elem->name->string, "input") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -3,
        "ERROR: asset input payload (%s) must be a string", JsonValueLocationStr(child_elem->value).c_str());
      input_path = (const json_string_s*)child_elem->value->payload;
    } else if (strcmp(child_elem->name->string, "output") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -4,
        "ERROR: asset output payload (%s) must be a string", JsonValueLocationStr(child_elem->value).c_str());
      output_path = (const json_string_s*)child_elem->value->payload;
    }
  }
  ZOMBO_ASSERT_RETURN(input_path && output_path, -5, "ERROR: incomplete asset at %s",
    JsonValueLocationStr(val).c_str());
  // For now, assets are processed right here.
  // Longer-term, we can build up a list in the AssetManifest and process it later.
  if (asset_class == ASSET_CLASS_IMAGE) {
    int process_error = ProcessImage(input_path, output_path);
    ZOMBO_ASSERT_RETURN(process_error == 0, -2, "ProcessImage() returned %d at %s",
      process_error, JsonValueLocationStr(val).c_str());
  } else if (asset_class == ASSET_CLASS_MESH) {
    int process_error = ProcessMesh(input_path, output_path);
    ZOMBO_ASSERT_RETURN(process_error == 0, -2, "ProcessMesh() returned %d at %s",
      process_error, JsonValueLocationStr(val).c_str());
  } else {
    // unknown asset class; skip it!
    printf("WARNING: skipping asset at %s with unknown asset class\n", JsonValueLocationStr(val).c_str());
  }
  return 0;
}

int ConvertUtf8ToWide(const json_string_s* utf8, std::wstring* out_wide) {
  static_assert(sizeof(wchar_t) == sizeof(WCHAR), "This code assume sizeof(wchar_t) == sizeof(WCHAR)");
  int nchars = MultiByteToWideChar(CP_UTF8, 0, utf8->string, (int)utf8->string_size, nullptr, 0);
  ZOMBO_ASSERT_RETURN(nchars != 0, -1, "malformed UTF-8, I guess?");
  out_wide->resize(nchars+1);
  int nchars_final = MultiByteToWideChar(CP_UTF8, 0, utf8->string, (int)utf8->string_size, &(*out_wide)[0], nchars+1);
  ZOMBO_ASSERT_RETURN(nchars_final != 0, -2, "failed to decode UTF-8");
  (*out_wide)[nchars_final] = 0;
  return nchars_final;
}

int AssetManifest::IsOutputOutOfDate(const json_string_s* input_path, const json_string_s* output_path, bool *out_result) const {
#if defined(ZOMBO_PLATFORM_WINDOWS)
  // Convert UTF-8 paths to wide strings
  std::wstring input_wstr, output_wstr;
  int input_nchars = ConvertUtf8ToWide(input_path, &input_wstr);
  ZOMBO_ASSERT_RETURN(input_nchars > 0, -1, "Failed to decode input_path");
  int output_nchars = ConvertUtf8ToWide(output_path, &output_wstr);
  ZOMBO_ASSERT_RETURN(output_nchars > 0, -2, "Failed to decode output_path");

  // Do the files exist? Missing input = error! Missing output = automatic rebuild!
  BOOL input_exists  = PathFileExists(input_wstr.c_str());
  BOOL output_exists = PathFileExists(output_wstr.c_str());
  if (!input_exists) {
    fwprintf(stderr, L"ERROR: asset %s does not exist\n", input_wstr.c_str());
    return -6;
  }

  // If both files exists, we compare last-write time.
  bool output_is_older = false;
  if (input_exists && output_exists) {
    // Query file attributes
    WIN32_FILE_ATTRIBUTE_DATA input_attrs = {}, output_attrs = {};
    BOOL input_attr_success = GetFileAttributesExW(input_wstr.c_str(), GetFileExInfoStandard, &input_attrs);
    ZOMBO_ASSERT_RETURN(input_attr_success, -3, "Failed to read file attributes for input_path");
    BOOL output_attr_success = GetFileAttributesExW(output_wstr.c_str(), GetFileExInfoStandard, &output_attrs);
    ZOMBO_ASSERT_RETURN(output_attr_success, -4, "Failed to read file attributes for output_path");
    // Compare file write times
    ULARGE_INTEGER input_write_time, output_write_time;
    input_write_time.HighPart  =  input_attrs.ftLastWriteTime.dwHighDateTime;
    input_write_time.LowPart   =  input_attrs.ftLastWriteTime.dwLowDateTime;
    output_write_time.HighPart = output_attrs.ftLastWriteTime.dwHighDateTime;
    output_write_time.LowPart  = output_attrs.ftLastWriteTime.dwLowDateTime;
    if (output_write_time.QuadPart < input_write_time.QuadPart) {
      output_is_older = true;
    }
  }

  // TODO(cort): There's a problem here. If the manifest is changed to reference a different
  // (preexisting) input file that's still older than the output, or if all that changes are
  // asset metadata, the output will not be rebuilt.
  // Not sure how often that will be an issue, but a few possible fixes included:
  // - Track the input file that was used to build each output file (by name or hash)
  // - Check the write time of the manifest itself. If it's newer than an output, rebuild it.
  //   (This is the nuclear option, as any manifest change means a full rebuild. But I already
  //   deal with that on the code side.
  *out_result = (!output_exists || output_is_older);
  return 0;
#elif defined(ZOMBO_PLATFORM_POSIX)
# error I haven't written POSIX support yet. Sorry!
#else
# error Unsupported platform! Sorry!
#endif
}

BOOL CreateDirectoryAndParents(const std::wstring& dir) {
  if (PathIsDirectory(dir.c_str())) {
    return TRUE;
  }
  std::wstring parent = dir;
  PathRemoveFileSpec(&parent[0]);
  int create_success = CreateDirectoryAndParents(parent);
  ZOMBO_ASSERT_RETURN(create_success, create_success, "CreateDirectory() failed");
  return CreateDirectory(dir.c_str(), nullptr);
}

bool AssetManifest::CopyAssetFile(const json_string_s* input_path, const json_string_s* output_path) const {
#if defined(ZOMBO_PLATFORM_WINDOWS)
  // Convert UTF-8 paths to wide strings
  std::wstring input_wstr, output_wstr;
  int input_nchars = ConvertUtf8ToWide(input_path, &input_wstr);
  ZOMBO_ASSERT_RETURN(input_nchars > 0, false, "Failed to decode input_path");
  int output_nchars = ConvertUtf8ToWide(output_path, &output_wstr);
  ZOMBO_ASSERT_RETURN(output_nchars > 0, false, "Failed to decode output_path");
  // Create any missing parent directories for the output file
  std::wstring output_dir;
  output_dir.resize(MAX_PATH);
  DWORD path_nchars = GetFullPathName(output_wstr.c_str(), MAX_PATH, &output_dir[0], nullptr);
  ZOMBO_ASSERT_RETURN(path_nchars != 0, false, "Failed to get full path for output file");
  BOOL remove_success = PathRemoveFileSpec(&output_dir[0]);
  ZOMBO_ASSERT_RETURN(remove_success, false, "Failed to remove filespec for output file");
  BOOL create_dir_success = CreateDirectoryAndParents(output_dir);
  ZOMBO_ASSERT_RETURN(create_dir_success, false, "Failed to create parent directories");
  // Copy
  BOOL copy_success = CopyFile(input_wstr.data(), output_wstr.data(), FALSE);
  ZOMBO_ASSERT_RETURN(copy_success, false, "CopyFile() failed");
  return true;
#else
# error Unsupported platform! Sorry!
#endif
}

int AssetManifest::ProcessImage(const json_string_s* input_path, const json_string_s* output_path) {
  bool build_output = false;
  int query_error = IsOutputOutOfDate(input_path, output_path, &build_output);
  ZOMBO_ASSERT_RETURN(query_error == 0, -1, "IsOutputOutOfDate failed");
  if (build_output) {
    bool copy_success = CopyAssetFile(input_path, output_path);
    ZOMBO_ASSERT_RETURN(copy_success, -1, "CopyAssetFile() failed");
    printf("%s -> %s\n", input_path->string, output_path->string);
  } else {
    //wprintf(L"Skipped %s (%s is up to date)\n", input_wstr.c_str(), output_wstr.c_str());
  }
  return 0;
}
int AssetManifest::ProcessMesh(const json_string_s* input_path, const json_string_s* output_path) {
  bool build_output = false;
  int query_error = IsOutputOutOfDate(input_path, output_path, &build_output);
  ZOMBO_ASSERT_RETURN(query_error == 0, -1, "IsOutputOutOfDate failed");
  if (build_output) {
    int process_result = ConvertSceneToMesh(input_path->string, output_path->string);
    ZOMBO_ASSERT_RETURN(process_result == 0, -1, "ConvertSceneToMesh() failed (%d)", process_result);
    printf("%s -> %s\n", input_path->string, output_path->string);
  } else {
    //wprintf(L"Skipped %s (%s is up to date)\n", input_wstr.c_str(), output_wstr.c_str());
  }
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    return -1;
  }

  const char *manifest_filename = argv[1];
  AssetManifest manifest(manifest_filename);

  //ImportFromFile(input_filename);
}