#include <spokk_mesh.h>  // for MeshHeader
#include <spokk_platform.h>
#include <spokk_shader_interface.h>
#include <spokk_vertex.h>

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <json.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <shaderc/shaderc.hpp>

#ifdef ZOMBO_PLATFORM_WINDOWS
#include <Shlwapi.h>  // for PathFileExists
#include <direct.h>  // for _chdir()
#endif

#include <stdio.h>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {
// Takes an absolute path to a directory. Creates the directory and all missing parent directories.
BOOL CreateDirectoryAndParentsA(const char* abs_dir) {
  if (PathIsRelativeA(abs_dir)) {
    return FALSE;  // input must be absolute
  }
  if (PathIsDirectoryA(abs_dir)) {
    return TRUE;
  }
  std::string parent = abs_dir;
  PathRemoveFileSpecA(&parent[0]);
  int create_success = CreateDirectoryAndParentsA(parent.c_str());
  ZOMBO_ASSERT_RETURN(create_success, create_success, "CreateDirectory() failed");
  return CreateDirectoryA(abs_dir, nullptr);
}

void ConvertToWindowsSlashes(char* path) {
  char* c = path;
  while (*c != '\0') {
    if (*c == '/') {
      *c = '\\';
    }
    ++c;
  }
}

// If path is relative, combine with root and canonicalize into out_abs_path.
// If path is absolute, ignore root and copy to out_abs_path.
int CreateAbsolutePath(std::string* out_abs_path, const char* root, const char* path) {
  if (PathIsRelativeA(path)) {
    std::array<char, MAX_PATH> abs_path;
    const char* out_path = PathCombineA(abs_path.data(), root, path);
    ZOMBO_ASSERT_RETURN(out_path != nullptr, -1, "ERROR: could not combine root (%s) with path (%s)", root, path);

    ConvertToWindowsSlashes(abs_path.data());

    std::array<char, MAX_PATH> final_abs_path;
    BOOL canonicalize_success = PathCanonicalizeA(final_abs_path.data(), abs_path.data());
    ZOMBO_ASSERT_RETURN(canonicalize_success, -1, "ERROR: could not canonicalize output dir (%s)", abs_path.data());
    *out_abs_path = final_abs_path.data();
  } else {
    *out_abs_path = path;
  }
  return 0;
}

// Converts a UTF8-encoded JSON string to a UTF16 string.
int ConvertUtf8ToWide(const json_string_s* utf8, std::wstring* out_wide) {
  static_assert(sizeof(wchar_t) == sizeof(WCHAR), "This code assume sizeof(wchar_t) == sizeof(WCHAR)");
  int nchars = MultiByteToWideChar(CP_UTF8, 0, utf8->string, (int)utf8->string_size, nullptr, 0);
  ZOMBO_ASSERT_RETURN(nchars != 0, -1, "malformed UTF-8, I guess?");
  out_wide->resize(nchars + 1);
  int nchars_final = MultiByteToWideChar(CP_UTF8, 0, utf8->string, (int)utf8->string_size, &(*out_wide)[0], nchars + 1);
  ZOMBO_ASSERT_RETURN(nchars_final != 0, -2, "failed to decode UTF-8");
  (*out_wide)[nchars_final] = 0;
  return nchars_final;
}

}  // namespace

constexpr uint32_t SPOKK_MAX_VERTEX_COLORS = 4;
constexpr uint32_t SPOKK_MAX_VERTEX_TEXCOORDS = 4;

struct SourceAttribute {
  spokk::VertexLayout layout;
  const void* values;
};

static void handleReadFileError(const std::string& errorString) { fprintf(stderr, "ERROR: %s\n", errorString.c_str()); }

int ConvertSceneToMesh(const std::string& input_scene_filename, const std::string& output_mesh_filename) {
  // Uncomment to enable importer logging (can be quite verbose!)
  // Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE, aiDefaultLogStream_STDERR);

  // Create an instance of the Importer class
  Assimp::Importer importer;
  // Configure the importer properties

  // Remove degenerate triangles entirely, rather than degrading them to points/lines.
  importer.SetPropertyBool(AI_CONFIG_PP_FD_REMOVE, true);

  // remove all points/lines from the scene
  importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);

  // uncomment to log timings of various import stages
  // importer.SetPropertyBool(AI_CONFIG_GLOB_MEASURE_TIME, true);

  // Specify maximum angle between neighboring faces such that their
  // shared vertices will have their normals smoothed.
  // Default is 175.0; docs say 80.0 will give a good visual appearance
  // And have it read the given file with some example postprocessing
  // Usually - if speed is not the most important aspect for you - you'll
  // probably to request more postprocessing than we do in this example.
  importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f);

  // clang-format off
  const aiScene* scene = importer.ReadFile(input_scene_filename.c_str(), 0
    | aiProcess_GenSmoothNormals       // Generate per-vertex normals, if none exist
    | aiProcess_CalcTangentSpace       // Compute per-vertex tangent and bitangent vectors (if the mesh already has normals and UVs)
    | aiProcess_Triangulate            // Convert faces with >3 vertices to 2 or more triangles
    | aiProcess_JoinIdenticalVertices  // If this flag is not specified, each vertex is used by exactly one face; no index buffer is required.
    | aiProcess_SortByPType            // Sort faces by primitive type -- one sub-mesh per primitive type.
    | aiProcess_ImproveCacheLocality   // Reorder vertex and index buffers to improve post-transform cache locality.
  //| aiProcess_FlipUVs                // HACK -- the scene we're currently loading has its UVs flipped.
  );
  // clang-format on
  // If the import failed, report it
  if (!scene) {
    handleReadFileError(importer.GetErrorString());
    return -1;
  }

  static_assert(sizeof(aiVector2D) == 2 * sizeof(float), "aiVector2D sizes do not match!");
  static_assert(sizeof(aiVector3D) == 3 * sizeof(float), "aiVector3D sizes do not match!");
  static_assert(sizeof(aiColor4D) == 4 * sizeof(float), "aiColor4D sizes do not match!");

  ZOMBO_ASSERT_RETURN(scene->mNumMeshes == 1, -1, "Currently, only one mesh per scene is supported.");

  std::vector<SourceAttribute> src_attributes = {{}};
  uint32_t iMesh = 0;
  const aiMesh* mesh = scene->mMeshes[iMesh];

  // Query available vertex attributes, and determine the mesh format
  ZOMBO_ASSERT(mesh->HasPositions(), "wtf sort of mesh doesn't include vertex positions?!?");
  if (mesh->HasPositions()) {
    static_assert(sizeof(mesh->mVertices[0]) == sizeof(aiVector3D), "positions aren't vec3s!");
    spokk::VertexLayout::AttributeInfo pos_attr = {};
    pos_attr.location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_POSITION;
    pos_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
    pos_attr.offset = 0;
    src_attributes.push_back({{pos_attr}, mesh->mVertices});
  }
  if (mesh->HasNormals()) {
    // TODO(cort): octohedral normals (https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/)
    static_assert(sizeof(mesh->mNormals[0]) == sizeof(aiVector3D), "normals aren't vec3s!");
    spokk::VertexLayout::AttributeInfo norm_attr = {};
    norm_attr.location = SPOKK_VERTEX_ATTRIBUTE_LOCATION_NORMAL;
    norm_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
    norm_attr.offset = 0;
    src_attributes.push_back({{norm_attr}, mesh->mNormals});
  }
  if (mesh->HasTangentsAndBitangents())  // Assimp always gives you both, or neither.
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
  for (int iColorSet = 0; iColorSet < AI_MAX_NUMBER_OF_COLOR_SETS; ++iColorSet) {
    static_assert(sizeof(mesh->mColors[iColorSet][0]) == sizeof(aiColor4D), "colors aren't vec4s!");
    if (mesh->HasVertexColors(iColorSet)) {
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
  for (int iUvSet = 0; iUvSet < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++iUvSet) {
    static_assert(sizeof(mesh->mTextureCoords[iUvSet][0]) == sizeof(aiVector3D), "texcoords aren't vec3s!");
    if (mesh->HasTextureCoords(iUvSet)) {
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
  for (uint32_t i = 0; i < vertex_count; ++i) {
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
  for (const auto& attrib : src_attributes) {
    int convert_error =
        spokk::ConvertVertexBuffer(attrib.values, attrib.layout, vertices.data(), dst_layout, vertex_count);
    ZOMBO_ASSERT(convert_error == 0, "error converting attribute at location %u", attrib.layout.attributes[0].location);
  }

  // Load index buffer
  ZOMBO_ASSERT_RETURN(mesh->HasFaces(), -1, "mesh has no faces! This is (currently) required.");
  uint32_t max_index_count = mesh->mNumFaces * 3;
  uint32_t bytes_per_index = (vertex_count <= 0x10000) ? sizeof(uint16_t) : sizeof(uint32_t);
  std::vector<uint8_t> indices(max_index_count * bytes_per_index, 0);
  uint32_t index_count = 0;
  for (uint32_t iFace = 0; iFace < mesh->mNumFaces; ++iFace) {
    const aiFace& face = mesh->mFaces[iFace];
    if (face.mNumIndices != 3) {
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
    for (size_t iAttr = 0; iAttr < attr_descs.size(); ++iAttr) {
      attr_descs[iAttr].location = dst_layout.attributes[iAttr].location;
      attr_descs[iAttr].binding = 0;
      attr_descs[iAttr].format = dst_layout.attributes[iAttr].format;
      attr_descs[iAttr].offset = dst_layout.attributes[iAttr].offset;
    }

    FILE* out_file = fopen(output_mesh_filename.c_str(), "wb");
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

struct ImageAsset {
  std::string json_location;
  std::string input_path;
  std::string output_path;
};

struct MeshAsset {
  std::string json_location;
  std::string input_path;
  std::string output_path;
};

struct ShaderAsset {
  std::string json_location;
  std::string input_path;
  std::string output_path;
  std::string entry_point;
  std::string shader_stage;
};

struct FontAsset {
  std::string json_location;
  std::string input_path;
  std::string output_path;
};

class ShaderFileIncluder : public shaderc::CompileOptions::IncluderInterface {
public:
  ShaderFileIncluder(const std::string& manifest_dir, std::vector<std::string>& dirs)
    : manifest_dir_(manifest_dir), include_dirs_(dirs) {}
  ~ShaderFileIncluder() {
    for (auto itor : include_results_) {
      delete itor.second;
    }
    for (auto result : failed_include_results_) {
      delete result;
    }
  }

  // Handles shaderc_include_resolver_fn callbacks.
  shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type,
      const char* requesting_source, size_t /*include_depth*/) override {
    // Acquire lock
    std::lock_guard<std::mutex> results_lock(mutex_);
    FILE* included_file = nullptr;
    std::string abs_header_path;
    if (type == shaderc_include_type_relative) {
      // combine manifest dir + requesting source to get absolute shader path
      std::string abs_shader_path;
      int path_error = CreateAbsolutePath(&abs_shader_path, manifest_dir_.c_str(), requesting_source);
      // remove shader filename to get shader dir
      std::string abs_shader_dir = abs_shader_path;
      path_error = PathRemoveFileSpecA(&abs_shader_dir[0]) ? 0 : 1;
      // combine shader dir with requested_source to get absolute header path
      path_error = CreateAbsolutePath(&abs_header_path, abs_shader_dir.c_str(), requested_source);
      (void)path_error;
      // If it's in the hash table, return its result.
      auto itor = include_results_.find(abs_header_path);
      if (itor != include_results_.end()) {
        return &(itor->second->result);
      }
      included_file = zomboFopen(abs_header_path.c_str(), "rb");
    } else {
      // Search for header in include path
      for (const auto& dir : include_dirs_) {
        // - combine dir with requested_source to get absolute_header_path
        int path_error = CreateAbsolutePath(&abs_header_path, dir.c_str(), requested_source);
        (void)path_error;
        // - If it's in the hash table, return its result.
        auto itor = include_results_.find(abs_header_path);
        if (itor != include_results_.end()) {
          return &(itor->second->result);
        }
        included_file = zomboFopen(abs_header_path.c_str(), "rb");
        if (included_file) {
          break;
        }
      }
    }
    if (!included_file) {
      // header not found. Craft an appropriate include_result and cache it somewhere
      // until it's released.
      IncludeResult* failure = new IncludeResult;
      failure->source_name = "";
      failure->content = "Could not find " + std::string(requested_source);
      failure->result.source_name = failure->source_name.c_str();
      failure->result.source_name_length = strlen(failure->result.source_name);
      failure->result.content = failure->content.c_str();
      failure->result.content_length = strlen(failure->content.c_str());
      failure->result.user_data = nullptr;
      failed_include_results_.push_back(failure);
      return &(failure->result);
    }
    // Load file's contents and add it to the hash table.
    fseek(included_file, 0, SEEK_END);
    size_t included_file_nbytes = ftell(included_file);
    fseek(included_file, 0, SEEK_SET);
    std::vector<char> content(included_file_nbytes + 1);
    size_t read_nbytes = fread(content.data(), 1, included_file_nbytes, included_file);
    content[included_file_nbytes] = '\0';
    fclose(included_file);
    if (read_nbytes != included_file_nbytes) {
      // I/O error; craft a failure result.
      IncludeResult* failure = new IncludeResult;
      failure->source_name = "";
      failure->content = "Error reading from " + std::string(requested_source);
      failure->result.source_name = failure->source_name.c_str();
      failure->result.source_name_length = strlen(failure->result.source_name);
      failure->result.content = failure->content.c_str();
      failure->result.content_length = strlen(failure->content.c_str());
      failure->result.user_data = nullptr;
      failed_include_results_.push_back(failure);
      return &(failure->result);
    }
    IncludeResult* result_data = new IncludeResult;
    result_data->source_name = abs_header_path;
    result_data->content = content.data();
    result_data->result.source_name_length = strlen(abs_header_path.c_str());
    result_data->result.source_name = result_data->source_name.c_str();
    result_data->result.content_length = included_file_nbytes;
    result_data->result.content = result_data->content.c_str();
    result_data->result.user_data = nullptr;

    include_results_[abs_header_path] = result_data;
    return &(result_data->result);
  }

  // Handles shaderc_include_result_release_fn callbacks.
  void ReleaseInclude(shaderc_include_result* /*data*/) override {
    // Just delete everything in the destructor
  }

private:
  ShaderFileIncluder(const ShaderFileIncluder& rhs) = delete;
  ShaderFileIncluder& operator=(const ShaderFileIncluder& rhs) = delete;

  std::string manifest_dir_;  // prepended to relative requester_source paths
  std::vector<std::string> include_dirs_;
  struct IncludeResult {
    std::string source_name;
    std::string content;
    shaderc_include_result result;  // char* members refer to source_name and content std::strings
  };
  std::mutex mutex_;  // Protects access to include_results_ and failed_include_results_
  std::map<std::string, IncludeResult*> include_results_;
  std::vector<IncludeResult*> failed_include_results_;
};

class AssetManifest {
public:
  explicit AssetManifest();
  ~AssetManifest();

  int Load(const std::string& json5_filename);
  int OverrideOutputRoot(const std::string& output_root_dir);
  int Build();

private:
  AssetManifest(const AssetManifest& rhs) = delete;
  AssetManifest& operator=(const AssetManifest& rhs) = delete;

  // Converts a json_parse_error_e to a human-readable string
  std::string JsonParseErrorStr(const json_parse_error_e error_code) const;
  // Returns a human-readable location of the specified json_value_s.
  // This assumes the manifest file was parsed with the
  // 'json_parse_flags_allow_location_information' flag enabled.
  std::string JsonValueLocationStr(const json_value_s* val) const;

  // Individual value parsers. Each returns 0 on success, non-zero on error.
  int ParseRoot(const json_value_s* val);

  int ParseDefaults(const json_value_s* val);
  int ParseDefaultOutputRoot(const json_value_s* val);
  int ParseDefaultShaderIncludeDirs(const json_value_s* val);

  int ParseAssets(const json_value_s* val);
  int ParseAsset(const json_value_s* val);
  int ParseImageAsset(const json_value_s* val);
  int ParseMeshAsset(const json_value_s* val);
  int ParseShaderAsset(const json_value_s* val);
  int ParseFontAsset(const json_value_s* val);

  int AssetManifest::IsOutputOutOfDate(
      const std::string& input_path, const std::string& output_path, bool* out_result) const;
  bool CopyAssetFile(const std::string& input_path, const std::string& output_path) const;

  int ProcessImage(const ImageAsset& image);
  int ProcessMesh(const MeshAsset& image);
  int ProcessShader(const ShaderAsset& image);
  int ProcessFont(const FontAsset& image);

  std::string launch_dir_;
  std::string manifest_dir_;
  std::string manifest_filename_;
  std::string output_root_;

  ULARGE_INTEGER manifest_write_time_;

  std::vector<std::string> shader_include_dirs_;

  std::vector<ImageAsset> image_assets_;
  std::vector<MeshAsset> mesh_assets_;
  std::vector<ShaderAsset> shader_assets_;
  std::vector<FontAsset> font_assets_;
};

AssetManifest::AssetManifest() : launch_dir_("."), manifest_dir_("."), manifest_filename_(""), output_root_(".") {}
AssetManifest::~AssetManifest() {}

int AssetManifest::Load(const std::string& json5_filename) {
  manifest_filename_ = json5_filename;

  FILE* manifest_file = zomboFopen(manifest_filename_.c_str(), "rb");
  ZOMBO_ASSERT_RETURN(manifest_file != nullptr, -1, "ERROR: Could not open %s\n", manifest_filename_.c_str());
  fseek(manifest_file, 0, SEEK_END);
  size_t manifest_nbytes = ftell(manifest_file);
  std::vector<uint8_t> manifest_bytes(manifest_nbytes);
  fseek(manifest_file, 0, SEEK_SET);
  size_t read_nbytes = fread(manifest_bytes.data(), 1, manifest_nbytes, manifest_file);
  fclose(manifest_file);
  ZOMBO_ASSERT_RETURN(
      read_nbytes == manifest_nbytes, -2, "ERROR: file I/O error while reading from %s\n", manifest_filename_.c_str());

#ifdef ZOMBO_PLATFORM_WINDOWS
  // Grab the modification time of the manifest file, so we can compare it to the output modification times later.
  WIN32_FILE_ATTRIBUTE_DATA manifest_attrs = {};
  BOOL attr_success = GetFileAttributesExA(manifest_filename_.c_str(), GetFileExInfoStandard, &manifest_attrs);
  ZOMBO_ASSERT_RETURN(attr_success, -3, "Failed to read file attributes for %s", manifest_filename_.c_str());
  manifest_write_time_.HighPart = manifest_attrs.ftLastWriteTime.dwHighDateTime;
  manifest_write_time_.LowPart = manifest_attrs.ftLastWriteTime.dwLowDateTime;
  // Save the directory we launched from
  launch_dir_.resize(MAX_PATH);
  _getcwd(&launch_dir_[0], (int)launch_dir_.capacity());
  // chdir to the same directory as the manifest file
  manifest_dir_.resize(MAX_PATH);
  DWORD path_nchars = GetFullPathNameA(json5_filename.c_str(), MAX_PATH, &manifest_dir_[0], nullptr);
  ZOMBO_ASSERT_RETURN(path_nchars != 0, -3, "Failed to get full path for manifest file %s", json5_filename.c_str());
  BOOL remove_success = PathRemoveFileSpecA(&manifest_dir_[0]);
  ZOMBO_ASSERT_RETURN(remove_success, -4, "Failed to remove filespec for manifest file %s", json5_filename.c_str());
  _chdir(manifest_dir_.c_str());
#else
#error linux tbi
#endif

  json_parse_result_s parse_result = {};
  json_value_s* manifest = json_parse_ex(manifest_bytes.data(), manifest_bytes.size(),
      json_parse_flags_allow_json5 | json_parse_flags_allow_location_information, NULL, NULL, &parse_result);
  ZOMBO_ASSERT_RETURN(manifest != nullptr, -5, "%s(%u): error %u at column %u (%s)\n", manifest_filename_.c_str(),
      (uint32_t)parse_result.error_line_no, (uint32_t)parse_result.error, (uint32_t)parse_result.error_row_no,
      JsonParseErrorStr((json_parse_error_e)parse_result.error).c_str());
  int parse_error = ParseRoot(manifest);
  free(manifest);
  ZOMBO_ASSERT_RETURN(
      parse_error == 0, -6, "ParseRoot() returned %d at %s", parse_error, JsonValueLocationStr(manifest).c_str());
  return 0;
}

int AssetManifest::OverrideOutputRoot(const std::string& output_root_dir) {
#if defined(ZOMBO_PLATFORM_WINDOWS)
  if (PathIsRelativeA(output_root_dir.c_str())) {
    std::array<char, MAX_PATH> absolute_root;
    const char* root = PathCombineA(absolute_root.data(), launch_dir_.c_str(), output_root_dir.c_str());
    ZOMBO_ASSERT_RETURN(root != nullptr, -1, "ERROR: could not combine launch dir(%s) with output dir (%s)",
        launch_dir_.c_str(), output_root_dir.c_str());

    std::array<char, MAX_PATH> final_abs_root;
    BOOL canonicalize_success = PathCanonicalizeA(final_abs_root.data(), absolute_root.data());
    ZOMBO_ASSERT_RETURN(
        canonicalize_success, -1, "ERROR: could not canonicalize output dir (%s)", absolute_root.data());
    output_root_ = final_abs_root.data();
  } else {
    output_root_ = output_root_dir;
  }
  return 0;
#else
#error linux TBI
#endif
}

int AssetManifest::Build() {
  int process_error = 0;
  for (const auto& image : image_assets_) {
    process_error = ProcessImage(image);
    ZOMBO_ASSERT_RETURN(process_error == 0, -2, "ProcessImage() returned %d while processing %s", process_error,
        image.input_path.c_str());
  }
  for (const auto& mesh : mesh_assets_) {
    process_error = ProcessMesh(mesh);
    ZOMBO_ASSERT_RETURN(process_error == 0, -2, "ProcessMesh() returned %d while processing %s", process_error,
        mesh.input_path.c_str());
  }
  for (const auto& shader : shader_assets_) {
    process_error = ProcessShader(shader);
    ZOMBO_ASSERT_RETURN(process_error == 0, -2, "ProcessShader() returned %d while processing %s", process_error,
        shader.input_path.c_str());
  }
  for (const auto& font : font_assets_) {
    process_error = ProcessFont(font);
    ZOMBO_ASSERT_RETURN(process_error == 0, -2, "ProcessFont() returned %d while processing %s", process_error,
      font.input_path.c_str());
  }
  return 0;
}

std::string AssetManifest::JsonParseErrorStr(const json_parse_error_e error_code) const {
  switch (error_code) {
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
  return manifest_filename_ + "line " + std::to_string(val_ex->line_no) + ", column " + std::to_string(val_ex->row_no);
}

int AssetManifest::ParseRoot(const json_value_s* val) {
  ZOMBO_ASSERT_RETURN(val->type == json_type_object, -1, "ERROR: root payload (%s) must be an object",
      JsonValueLocationStr(val).c_str());
  json_object_s* root_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  for (json_object_element_s *child_elem = root_obj->start; i_child < root_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "assets") == 0) {
      int parse_error = ParseAssets(child_elem->value);
      ZOMBO_ASSERT_RETURN(
          parse_error == 0, -2, "ParseAssets() returned %d at %s", parse_error, JsonValueLocationStr(val).c_str());
    } else if (strcmp(child_elem->name->string, "defaults") == 0) {
      int parse_error = ParseDefaults(child_elem->value);
      ZOMBO_ASSERT_RETURN(
          parse_error == 0, -2, "ParseDefaults() returned %d at %s", parse_error, JsonValueLocationStr(val).c_str());
    }
  }
  return 0;
}

int AssetManifest::ParseDefaults(const json_value_s* val) {
  ZOMBO_ASSERT_RETURN(val->type == json_type_object, -1, "ERROR: defaults payload (%s) must be an object\n",
      JsonValueLocationStr(val).c_str());
  const json_object_s* defaults_obj = (const json_object_s*)(val->payload);
  size_t i_child = 0;
  for (json_object_element_s *child_elem = defaults_obj->start; i_child < defaults_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "output_root") == 0) {
      int parse_error = ParseDefaultOutputRoot(child_elem->value);
      ZOMBO_ASSERT_RETURN(parse_error == 0, -2, "Error parsing default root");
    } else if (strcmp(child_elem->name->string, "shader_include_dirs") == 0) {
      int parse_error = ParseDefaultShaderIncludeDirs(child_elem->value);
      ZOMBO_ASSERT_RETURN(parse_error == 0, -3, "Error parsing default shader include dirs");
    }
  }
  return 0;
}

int AssetManifest::ParseDefaultOutputRoot(const json_value_s* val) {
  ZOMBO_ASSERT_RETURN(val->type == json_type_string, -1, "ERROR: output_root payload (%s) must be a string\n",
      JsonValueLocationStr(val).c_str());
  const json_string_s* output_root_str = (const json_string_s*)(val->payload);
  return CreateAbsolutePath(&output_root_, manifest_dir_.c_str(), output_root_str->string);
}
int AssetManifest::ParseDefaultShaderIncludeDirs(const json_value_s* val) {
  ZOMBO_ASSERT_RETURN(val->type == json_type_array, -1, "ERROR: shader_include_dirs payload (%s) must be an array\n",
      JsonValueLocationStr(val).c_str());
  const json_array_s* includes_array = (const json_array_s*)(val->payload);
  size_t i_child = 0;
  for (json_array_element_s *child_elem = includes_array->start; i_child < includes_array->length;
       ++i_child, child_elem = child_elem->next) {
    // parse array of shader includes. Combine each dir with the manifest dir and canonicalize
    ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -2,
        "ERROR: shader_include_dirs element (%s) must be a string\n", JsonValueLocationStr(val).c_str());
    const json_string_s* include_dir_str = (const json_string_s*)(child_elem->value->payload);
    std::string abs_include_dir;
    int abs_error = CreateAbsolutePath(&abs_include_dir, manifest_dir_.c_str(), include_dir_str->string);
    ZOMBO_ASSERT_RETURN(abs_error == 0, -3, "Error converting %s to an absolute path", include_dir_str->string);
    shader_include_dirs_.push_back(abs_include_dir);
  }
  return 0;
}

int AssetManifest::ParseAssets(const json_value_s* val) {
  ZOMBO_ASSERT_RETURN(val->type == json_type_array, -1, "ERROR: assets payload (%s) must be an array\n",
      JsonValueLocationStr(val).c_str());
  const json_array_s* assets_array = (const json_array_s*)(val->payload);
  size_t i_child = 0;
  for (json_array_element_s *child_elem = assets_array->start; i_child < assets_array->length;
       ++i_child, child_elem = child_elem->next) {
    int parse_error = ParseAsset(child_elem->value);
    ZOMBO_ASSERT_RETURN(
        parse_error == 0, -2, "ParseAsset() returned %d at %s", parse_error, JsonValueLocationStr(val).c_str());
  }
  return 0;
}

int AssetManifest::ParseAsset(const json_value_s* val) {
  ZOMBO_ASSERT_RETURN(val->type == json_type_object, -1, "ERROR: asset payload (%s) must be an object",
      JsonValueLocationStr(val).c_str());
  // First we need to loop through child elements and find the asset class, so we know which Parse*Asset()
  // variant to call.
  json_object_s* asset_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  for (json_object_element_s *child_elem = asset_obj->start; i_child < asset_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "class") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -2,
          "ERROR: asset class payload (%s) must be a string", JsonValueLocationStr(child_elem->value).c_str());
      const json_string_s* asset_class_str = (const json_string_s*)child_elem->value->payload;
      if (strcmp(asset_class_str->string, "image") == 0) {
        return ParseImageAsset(val);
      } else if (strcmp(asset_class_str->string, "mesh") == 0) {
        return ParseMeshAsset(val);
      } else if (strcmp(asset_class_str->string, "shader") == 0) {
        return ParseShaderAsset(val);
      } else if (strcmp(asset_class_str->string, "font") == 0) {
        return ParseFontAsset(val);
      } else {
        ZOMBO_ERROR_RETURN(-3, "ERROR: unknown asset class '%s' at %s", asset_class_str->string,
            JsonValueLocationStr(child_elem->value).c_str());
      }
    }
  }
  ZOMBO_ERROR_RETURN(-4, "ERROR: asset at %s has no 'class'.", JsonValueLocationStr(val).c_str());
}

int AssetManifest::ParseImageAsset(const json_value_s* val) {
  const json_string_s* input_path = nullptr;
  const json_string_s* output_path = nullptr;
  json_object_s* asset_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  for (json_object_element_s *child_elem = asset_obj->start; i_child < asset_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "class") == 0) {
      // Already handled by caller
    } else if (strcmp(child_elem->name->string, "input") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -1,
          "ERROR: asset 'input' payload at %s must be a string", JsonValueLocationStr(val).c_str());
      input_path = (const json_string_s*)(child_elem->value->payload);
    } else if (strcmp(child_elem->name->string, "output") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -2,
          "ERROR: asset 'output' payload at %s must be a string", JsonValueLocationStr(val).c_str());
      output_path = (const json_string_s*)(child_elem->value->payload);
    } else {
      fprintf(stderr, "WARNING: ignoring unexpected tag '%s' in image asset at %s", child_elem->name->string,
          JsonValueLocationStr(val).c_str());
    }
  }
  ZOMBO_ASSERT_RETURN(
      input_path && output_path, -3, "ERROR: incomplete image asset at %s", JsonValueLocationStr(val).c_str());
  ImageAsset image = {};
  image.json_location = JsonValueLocationStr(val);
  image.input_path = input_path->string;
  image.output_path = output_path->string;
  image_assets_.push_back(image);
  return 0;
}

int AssetManifest::ParseMeshAsset(const json_value_s* val) {
  const json_string_s* input_path = nullptr;
  const json_string_s* output_path = nullptr;
  json_object_s* asset_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  for (json_object_element_s *child_elem = asset_obj->start; i_child < asset_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "class") == 0) {
      // Already handled by caller
    } else if (strcmp(child_elem->name->string, "input") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -1,
          "ERROR: asset 'input' payload at %s must be a string", JsonValueLocationStr(val).c_str());
      input_path = (const json_string_s*)(child_elem->value->payload);
    } else if (strcmp(child_elem->name->string, "output") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -2,
          "ERROR: asset 'output' payload at %s must be a string", JsonValueLocationStr(val).c_str());
      output_path = (const json_string_s*)(child_elem->value->payload);
    } else {
      fprintf(stderr, "WARNING: ignoring unexpected tag '%s' in mesh asset at %s", child_elem->name->string,
          JsonValueLocationStr(val).c_str());
    }
  }
  ZOMBO_ASSERT_RETURN(
      input_path && output_path, -3, "ERROR: incomplete mesh asset at %s", JsonValueLocationStr(val).c_str());
  MeshAsset mesh = {};
  mesh.json_location = JsonValueLocationStr(val);
  mesh.input_path = input_path->string;
  mesh.output_path = output_path->string;
  mesh_assets_.push_back(mesh);
  return 0;
}

int AssetManifest::ParseShaderAsset(const json_value_s* val) {
  const json_string_s* input_path = nullptr;
  const json_string_s* output_path = nullptr;
  const json_string_s* entry_point = nullptr;
  const json_string_s* shader_stage = nullptr;
  json_object_s* asset_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  for (json_object_element_s *child_elem = asset_obj->start; i_child < asset_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "class") == 0) {
      // Already handled by caller
    } else if (strcmp(child_elem->name->string, "input") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -1,
          "ERROR: asset 'input' payload at %s must be a string", JsonValueLocationStr(val).c_str());
      input_path = (const json_string_s*)(child_elem->value->payload);
    } else if (strcmp(child_elem->name->string, "output") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -2,
          "ERROR: asset 'output' payload at %s must be a string", JsonValueLocationStr(val).c_str());
      output_path = (const json_string_s*)(child_elem->value->payload);
    } else if (strcmp(child_elem->name->string, "entry") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -2,
          "ERROR: asset 'entry' payload at %s must be a string", JsonValueLocationStr(val).c_str());
      entry_point = (const json_string_s*)(child_elem->value->payload);
    } else if (strcmp(child_elem->name->string, "stage") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -2,
          "ERROR: asset 'stage' payload at %s must be a string", JsonValueLocationStr(val).c_str());
      shader_stage = (const json_string_s*)(child_elem->value->payload);
    } else {
      fprintf(stderr, "WARNING: ignoring unexpected tag '%s' in shader asset at %s", child_elem->name->string,
          JsonValueLocationStr(val).c_str());
    }
  }
  ZOMBO_ASSERT_RETURN(
      input_path && output_path, -3, "ERROR: incomplete shader asset at %s", JsonValueLocationStr(val).c_str());
  ShaderAsset shader = {};
  shader.json_location = JsonValueLocationStr(val);
  shader.input_path = input_path->string;
  shader.output_path = output_path->string;
  shader.entry_point = entry_point ? entry_point->string : "";
  shader.shader_stage = shader_stage ? shader_stage->string : "";
  shader_assets_.push_back(shader);
  return 0;
}

int AssetManifest::ParseFontAsset(const json_value_s* val) {
  const json_string_s* input_path = nullptr;
  const json_string_s* output_path = nullptr;
  json_object_s* asset_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  for (json_object_element_s *child_elem = asset_obj->start; i_child < asset_obj->length;
    ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "class") == 0) {
      // Already handled by caller
    } else if (strcmp(child_elem->name->string, "input") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -1,
        "ERROR: asset 'input' payload at %s must be a string", JsonValueLocationStr(val).c_str());
      input_path = (const json_string_s*)(child_elem->value->payload);
    } else if (strcmp(child_elem->name->string, "output") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -2,
        "ERROR: asset 'output' payload at %s must be a string", JsonValueLocationStr(val).c_str());
      output_path = (const json_string_s*)(child_elem->value->payload);
    } else {
      fprintf(stderr, "WARNING: ignoring unexpected tag '%s' in font asset at %s", child_elem->name->string,
        JsonValueLocationStr(val).c_str());
    }
  }
  ZOMBO_ASSERT_RETURN(
    input_path && output_path, -3, "ERROR: incomplete font asset at %s", JsonValueLocationStr(val).c_str());
  FontAsset font = {};
  font.json_location = JsonValueLocationStr(val);
  font.input_path = input_path->string;
  font.output_path = output_path->string;
  font_assets_.push_back(font);
  return 0;
}

int AssetManifest::IsOutputOutOfDate(
    const std::string& input_path, const std::string& output_path, bool* out_result) const {
#if defined(ZOMBO_PLATFORM_WINDOWS)
  // Do the files exist? Missing input = error! Missing output = automatic rebuild!
  BOOL input_exists = PathFileExistsA(input_path.c_str());
  ZOMBO_ASSERT_RETURN(input_exists, -6, "ERROR: asset %s does not exist\n", input_path.c_str());
  BOOL output_exists = PathFileExistsA(output_path.c_str());

  // If both files exists, we compare last-write time.
  bool output_is_older = false;
  if (input_exists && output_exists) {
    // Query file attributes
    WIN32_FILE_ATTRIBUTE_DATA input_attrs = {}, output_attrs = {};
    BOOL input_attr_success = GetFileAttributesExA(input_path.c_str(), GetFileExInfoStandard, &input_attrs);
    ZOMBO_ASSERT_RETURN(input_attr_success, -3, "Failed to read file attributes for input_path");
    BOOL output_attr_success = GetFileAttributesExA(output_path.c_str(), GetFileExInfoStandard, &output_attrs);
    ZOMBO_ASSERT_RETURN(output_attr_success, -4, "Failed to read file attributes for output_path");
    // Compare file write times
    ULARGE_INTEGER input_write_time, output_write_time;
    input_write_time.HighPart = input_attrs.ftLastWriteTime.dwHighDateTime;
    input_write_time.LowPart = input_attrs.ftLastWriteTime.dwLowDateTime;
    output_write_time.HighPart = output_attrs.ftLastWriteTime.dwHighDateTime;
    output_write_time.LowPart = output_attrs.ftLastWriteTime.dwLowDateTime;
    if (output_write_time.QuadPart < input_write_time.QuadPart) {
      output_is_older = true;
    }
    // Also compare output write time to manifest write time; if the manifest is newer, assume everything
    // is out of date. This does mean you get a full asset rebuild every time the manifest changes, which would
    // probably be unacceptable in a large production environment, but I'm really not there yet.
    if (output_write_time.QuadPart < manifest_write_time_.QuadPart) {
      output_is_older = true;
    }
  }
  *out_result = (!output_exists || output_is_older);
  return 0;
#else
#error Unsupported platform! Sorry!
#endif
}

bool AssetManifest::CopyAssetFile(const std::string& input_path, const std::string& output_path) const {
#if defined(ZOMBO_PLATFORM_WINDOWS)
  // Create any missing parent directories for the output file
  std::array<char, MAX_PATH> output_dir;
  DWORD path_nchars = GetFullPathNameA(output_path.c_str(), (int)output_dir.size(), output_dir.data(), nullptr);
  ZOMBO_ASSERT_RETURN(path_nchars != 0, false, "Failed to get full path for output file");
  BOOL remove_success = PathRemoveFileSpecA(output_dir.data());
  ZOMBO_ASSERT_RETURN(remove_success, false, "Failed to remove filespec for output file");
  BOOL create_dir_success = CreateDirectoryAndParentsA(output_dir.data());
  ZOMBO_ASSERT_RETURN(create_dir_success, false, "Failed to create parent directories");
  // Copy
  BOOL copy_success = CopyFileA(input_path.c_str(), output_path.c_str(), FALSE);
  ZOMBO_ASSERT_RETURN(copy_success, false, "CopyFile() failed");
  // Update output file modification time (otherwise preserved by CopyFile)
  // TODO(cort): potential race condition here? Technically CopyFile and SetFileTime should be a single transaction.
  HANDLE out_handle =
      CreateFileA(output_path.c_str(), FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  SYSTEMTIME system_time = {};
  GetSystemTime(&system_time);
  FILETIME file_time = {};
  SystemTimeToFileTime(&system_time, &file_time);
  BOOL time_success = SetFileTime(out_handle, (LPFILETIME)NULL, (LPFILETIME)NULL, &file_time);
  ZOMBO_ASSERT_RETURN(time_success, false, "SetFileTime() failed");
  BOOL close_success = CloseHandle(out_handle);
  ZOMBO_ASSERT_RETURN(close_success, false, "CloseHandle() failed");
  return true;
#else
#error Unsupported platform! Sorry!
#endif
}

int AssetManifest::ProcessImage(const ImageAsset& image) {
  bool build_output = false;
  std::string abs_output_path;
  int path_error = CreateAbsolutePath(&abs_output_path, output_root_.c_str(), image.output_path.c_str());
  ZOMBO_ASSERT_RETURN(path_error == 0, -1, "CreateAbsoluteOutputPath failed (%d) for image at %s", path_error,
      image.json_location.c_str());
  int query_error = IsOutputOutOfDate(image.input_path, abs_output_path, &build_output);
  ZOMBO_ASSERT_RETURN(
      query_error == 0, -2, "IsOutputOutOfDate failed (%d) for image at %s", query_error, image.json_location.c_str());
  if (build_output) {
    bool copy_success = CopyAssetFile(image.input_path, abs_output_path.c_str());
    ZOMBO_ASSERT_RETURN(copy_success, -3, "CopyAssetFile failed for image at %s", image.json_location.c_str());
    printf("%s -> %s\n", image.input_path.c_str(), abs_output_path.c_str());
  } else {
    // printf("Skipped %s (%s is up to date)\n", image.input_path.c_str(), abs_output_path.c_str());
  }
  return 0;
}

int AssetManifest::ProcessMesh(const MeshAsset& mesh) {
  bool build_output = false;
  std::string abs_output_path;
  int path_error = CreateAbsolutePath(&abs_output_path, output_root_.c_str(), mesh.output_path.c_str());
  ZOMBO_ASSERT_RETURN(path_error == 0, -1, "CreateAbsoluteOutputPath failed (%d) for mesh at %s", path_error,
      mesh.json_location.c_str());
  int query_error = IsOutputOutOfDate(mesh.input_path, abs_output_path, &build_output);
  ZOMBO_ASSERT_RETURN(
      query_error == 0, -1, "IsOutputOutOfDate failed (%d) for mesh at %s", query_error, mesh.json_location.c_str());
  if (build_output) {
    int process_result = ConvertSceneToMesh(mesh.input_path, abs_output_path.c_str());
    ZOMBO_ASSERT_RETURN(process_result == 0, -1, "ConvertSceneToMesh failed (%d) for mesh at %s", process_result,
        mesh.json_location.c_str());
    printf("%s -> %s\n", mesh.input_path.c_str(), abs_output_path.c_str());
  } else {
    // printf("Skipped %s (%s is up to date)\n", mesh.input_path.c_str(), abs_output_path.c_str());
  }
  return 0;
}

int AssetManifest::ProcessShader(const ShaderAsset& shader) {
  bool build_output = false;
  std::string abs_output_path;
  int path_error = CreateAbsolutePath(&abs_output_path, output_root_.c_str(), shader.output_path.c_str());
  ZOMBO_ASSERT_RETURN(path_error == 0, -1, "CreateAbsoluteOutputPath failed (%d) for shader at %s", path_error,
      shader.json_location.c_str());
  int query_error = IsOutputOutOfDate(shader.input_path, abs_output_path, &build_output);
  ZOMBO_ASSERT_RETURN(query_error == 0, -2, "IsOutputOutOfDate failed (%d) for shader at %s", query_error,
      shader.json_location.c_str());
  if (build_output) {
    shaderc_shader_kind shader_kind = shaderc_glsl_infer_from_source;
    if (shader.shader_stage == "vert" || shader.shader_stage == "vertex") {
      shader_kind = shaderc_vertex_shader;
    } else if (shader.shader_stage == "frag" || shader.shader_stage == "fragment") {
      shader_kind = shaderc_fragment_shader;
    } else if (shader.shader_stage == "geom" || shader.shader_stage == "geometry") {
      shader_kind = shaderc_geometry_shader;
    } else if (shader.shader_stage == "tese" || shader.shader_stage == "tesseval") {
      shader_kind = shaderc_tess_evaluation_shader;
    } else if (shader.shader_stage == "comp" || shader.shader_stage == "compute") {
      shader_kind = shaderc_compute_shader;
    } else {
      ZOMBO_ERROR_RETURN(
          -3, "Unrecognized shader stage '%s' at %s", shader.shader_stage.c_str(), shader.json_location.c_str());
    }
    FILE* source_file = zomboFopen(shader.input_path.c_str(), "rb");
    ZOMBO_ASSERT_RETURN(source_file, -4, "Could not open %s for reading", shader.input_path.c_str());
    fseek(source_file, 0, SEEK_END);
    size_t source_nbytes = ftell(source_file);
    fseek(source_file, 0, SEEK_SET);
    std::vector<char> source_contents(source_nbytes + 1);
    size_t read_nbytes = fread(source_contents.data(), 1, source_nbytes, source_file);
    source_contents[source_nbytes] = '\0';
    fclose(source_file);
    ZOMBO_ASSERT_RETURN(read_nbytes == source_nbytes, -4, "Read error while loading %s", shader.input_path.c_str());

    shaderc::CompileOptions options;
    std::unique_ptr<ShaderFileIncluder> includer =
        std::make_unique<ShaderFileIncluder>(manifest_dir_, shader_include_dirs_);
    options.SetIncluder(std::move(includer));
    shaderc::Compiler compiler;
    shaderc::SpvCompilationResult compile_result = compiler.CompileGlslToSpv(
        source_contents.data(), shader_kind, shader.input_path.c_str(), shader.entry_point.c_str(), options);
    if (compile_result.GetCompilationStatus() != shaderc_compilation_status_success) {
      fprintf(stderr, "%s\n", compile_result.GetErrorMessage().c_str());
      return compile_result.GetCompilationStatus();
    }

    size_t spv_dword_count = static_cast<size_t>(compile_result.cend() - compile_result.cbegin());
    FILE* spv_file = zomboFopen(abs_output_path.c_str(), "wb");
    ZOMBO_ASSERT_RETURN(spv_file, -5, "Could not open %s for writing", abs_output_path.c_str());
    size_t write_ndwords = fwrite(compile_result.cbegin(), sizeof(uint32_t), spv_dword_count, spv_file);
    fclose(spv_file);
    ZOMBO_ASSERT_RETURN(
        spv_dword_count == write_ndwords, -6, "Write error while writing to %s", abs_output_path.c_str());
    printf("%s -> %s\n", shader.input_path.c_str(), abs_output_path.c_str());
  } else {
    // printf("Skipped %s (%s is up to date)\n", shader.input_path.c_str(), abs_output_path.c_str());
  }
  return 0;
}

int AssetManifest::ProcessFont(const FontAsset& font) {
  bool build_output = false;
  std::string abs_output_path;
  int path_error = CreateAbsolutePath(&abs_output_path, output_root_.c_str(), font.output_path.c_str());
  ZOMBO_ASSERT_RETURN(path_error == 0, -1, "CreateAbsoluteOutputPath failed (%d) for image at %s", path_error,
    font.json_location.c_str());
  int query_error = IsOutputOutOfDate(font.input_path, abs_output_path, &build_output);
  ZOMBO_ASSERT_RETURN(
    query_error == 0, -2, "IsOutputOutOfDate failed (%d) for font at %s", query_error, font.json_location.c_str());
  if (build_output) {
    bool copy_success = CopyAssetFile(font.input_path, abs_output_path.c_str());
    ZOMBO_ASSERT_RETURN(copy_success, -3, "CopyAssetFile failed for font at %s", font.json_location.c_str());
    printf("%s -> %s\n", font.input_path.c_str(), abs_output_path.c_str());
  } else {
    // printf("Skipped %s (%s is up to date)\n", image.input_path.c_str(), abs_output_path.c_str());
  }
  return 0;
}

void PrintUsage(const char* argv0) {
  printf(R"usage(\
Usage: %s [options] manifest.json5
Options:
  -h, --help:       Prints this message
  -o <root>         Override output root in manifest with the specified directory.
)usage",
      argv0);
}

int main(int argc, char* argv[]) {
  // TODO(cort): Command line options
  // -f, --force-rebuild: Assume all outputs are dirty
  // -v, --verbose: Extra logging?
  // -t, --test-only: Print what would be done but don't actually do it
  const char* new_output_root = nullptr;
  const char* manifest_filename = nullptr;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      PrintUsage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      new_output_root = argv[++i];
    } else if (i == argc - 1) {
      manifest_filename = argv[i];
    } else {
      PrintUsage(argv[0]);
      return -1;
    }
  }

  AssetManifest manifest;
  int load_error = manifest.Load(manifest_filename);
  ZOMBO_ASSERT_RETURN(load_error == 0, -1, "load error (%d) while loading manifest %s", load_error, manifest_filename);

  if (new_output_root) {
    int override_error = manifest.OverrideOutputRoot(new_output_root);
    ZOMBO_ASSERT_RETURN(override_error == 0, -1, "override error (%d)", load_error);
  }

  int build_error = manifest.Build();
  ZOMBO_ASSERT_RETURN(
      build_error == 0, -2, "build error (%d) while loading manifest %s", build_error, manifest_filename);
}