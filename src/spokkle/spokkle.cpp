#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <json.h>
#include <subprocess.h>
#include <spokk_mesh.h>  // for MeshHeader
#include <spokk_platform.h>
#include <spokk_shader_interface.h>
#include <spokk_vertex.h>

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>

#if defined(ZOMBO_OS_WINDOWS)
#include <Shlwapi.h>  // for Path*() functions
#elif defined(ZOMBO_OS_POSIX)
#include <unistd.h>
#endif

#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

// Effective Modern C++, Item 21: make_unique() is C++14 only, but easy to implement in C++11.
template <typename T, typename... Ts>
std::unique_ptr<T> my_make_unique(Ts&&... params) {
  return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

int GetFileModificationTime(const char* path, time_t* out_mtime) {
  ZomboStatStruct out_stats = {};
  int stat_error = zomboStat(path, &out_stats);
  if (!stat_error) {
    *out_mtime = out_stats.st_mtime;
  }
  return stat_error;
}

bool IsPathDirectory(const char* path) {
#if defined(ZOMBO_OS_WINDOWS)
  return PathIsDirectoryA(path) ? true : false;
#elif defined(ZOMBO_OS_POSIX)
  // This should work on Windows as well, but S_ISDIR would be _S_ISDIR
  ZomboStatStruct out_stats = {};
  int stat_error = zomboStat(path, &out_stats);
  return !stat_error && S_ISDIR(out_stats.st_mode) ? true : false;
#else
#error unsupported platform
#endif
}

bool IsRelativePath(const char* path) {
#if defined(ZOMBO_OS_WINDOWS)
  return PathIsRelativeA(path) ? true : false;
#elif defined(ZOMBO_OS_POSIX)
  return (path != nullptr) && path[0] != '/';
#else
#error unsupported platform
#endif
}

bool FileExists(const char* path) {
#if defined(ZOMBO_OS_WINDOWS) || defined(ZOMBO_OS_POSIX)
  ZomboStatStruct out_stats = {};
  return (zomboStat(path, &out_stats) == 0);
#else
#error unsupported platform
#endif
}

#if defined(ZOMBO_OS_WINDOWS)
// Modifies str in-place to replace all instances of ca with cb.
void CharFromAToB(char* str, char ca, char cb) {
  char* c = str;
  while (*c != '\0') {
    if (*c == ca) {
      *c = cb;
    }
    ++c;
  }
}
#endif

// if out_buffer is NULL, stores the number of chars necessary to hold the output in buffer_nchars.
// If path is absolute, out_buffer is canonicalize(path).
// If path is relative, out_buffer is canonicalize(abs_dir+path)
// Not safe in multithreaded programs (cwd is shared process-level state)
int CombineAbsDirAndPath(const char* abs_dir, const char* path, int* buffer_nchars, char* out_buffer) {
  ZOMBO_ASSERT_RETURN(!IsRelativePath(abs_dir), -1, "abs_dir (%s) must be an absolute path", abs_dir);
#if defined(ZOMBO_OS_WINDOWS)
  if (out_buffer == NULL) {
    *buffer_nchars = MAX_PATH + 1;  // PathCanonicalize always requires MAX_PATH + 1 chars
    return 0;
  } else {
    char tmp_path[MAX_PATH + 1];
    int err = 0;
    if (IsRelativePath(path)) {
      err = (PathCombineA(tmp_path, abs_dir, path) != NULL) ? 0 : -1;
    } else {
      strncpy(tmp_path, path, MAX_PATH);
      tmp_path[MAX_PATH] = '\0';
    }
    if (!err) {
      CharFromAToB(tmp_path, '/', '\\');
      err = PathCanonicalizeA(out_buffer, tmp_path) ? 0 : -1;
    }
    return err;
  }
#elif defined(ZOMBO_OS_POSIX)
  if (out_buffer == NULL) {
    *buffer_nchars = PATH_MAX;
    return 0;
  } else {
    // Just smoosh 'em together
    std::string tmp_path = IsRelativePath(path) ? (std::string(abs_dir) + std::string("/") + std::string(path)) : path;
    // Frustratingly, realpath() doesn't work if some/all of the path doesn't exist.
    // So, canonicalize manually.
    const char* src = tmp_path.c_str();
    out_buffer[0] = '/';
    char* dst = out_buffer;
    while (*src != '\0') {
      ZOMBO_ASSERT(*src == '/', "invariant failure");
      const char* src_next = strchr(src + 1, '/');
      if (src_next == nullptr) {
        // this is the final component in the path, with no trailing slash.
        src_next = strchr(src + 1, '\0');
      }
      ptrdiff_t nchars = src_next - src;
      if (nchars == 1 && *src_next == '\0') {
        // trailing slash at the end of src. Skip it, and we're done.
      } else if (nchars == 1 && *src_next == '/') {
        // consecutive slashes. Skip all but the last one, writing no output
        while (*(src_next + 1) == '/') {
          ++src_next;
        }
      } else if (nchars == 2 && src[1] == '.') {
        // skip "/." chunk
      } else if (nchars == 3 && src[1] == '.' && src[2] == '.') {
        // Rewind dst to previous dir (unless we're already at the
        // root, in which case ignore it. "/.." == "/" at the root.
        while (dst != out_buffer && *(--dst) != '/') {
        }
      } else {
        ZOMBO_ASSERT_RETURN(dst + nchars <= out_buffer + *buffer_nchars - 1, -2, "output path len exceeds buffer_size");
        strncpy(dst, src, nchars);
        dst += nchars;
      }
      src = src_next;
    }
    if (dst == out_buffer) {
      *dst++ = '/';
    }
    *dst = 0;
    return 0;
  }
  // alternately: getcwd(), chdir(abs_dir), realpath(path), chdir(old_cwd)
#else
#error unsupported platform
#endif
}
// Shortcut to just write the results to a string
int CombineAbsDirAndPath(const char* abs_dir, const char* path, std::string* out_path) {
  int path_nchars = 0;
  int path_error = CombineAbsDirAndPath(abs_dir, path, &path_nchars, nullptr);
  if (!path_error) {
    std::vector<char> abs_path(path_nchars);
    path_error = CombineAbsDirAndPath(abs_dir, path, &path_nchars, abs_path.data());
    if (!path_error) {
      *out_path = abs_path.data();
    }
  }
  return path_error;
}

// buffer_nchars should include space for the terminating character.
// if out_buffer is NULL, stores the number of chars necessary to hold the output in buffer_nchars.
// Returns 0 on success, non-zero on error.
// May attempt to canonicalize/normalize the path (removing duplicate separators,
// eliminating ./.., etc.
// Not safe in multithreaded programs (cwd is shared process-level state)
int MakeAbsolutePath(const char* path, int* buffer_nchars, char* out_buffer) {
#if defined(ZOMBO_OS_WINDOWS)
  if (out_buffer == NULL) {
    *buffer_nchars = MAX_PATH + 1;  // PathCanonicalize always requires MAX_PATH + 1 chars
    return 0;
  } else {
    char tmp_path[MAX_PATH + 1];
    int ret = GetFullPathNameA(path, MAX_PATH + 1, tmp_path, NULL);
    if (ret != 0) {
      CharFromAToB(tmp_path, '/', '\\');
      ret = PathCanonicalizeA(out_buffer, tmp_path) ? 0 : -1;
    }
    return ret;
  }
#elif defined(ZOMBO_OS_POSIX)
  if (out_buffer == NULL) {
    *buffer_nchars = PATH_MAX;
    return 0;
  } else {
    std::vector<char> cwd(PATH_MAX);
    char* cwd_str = getcwd(cwd.data(), PATH_MAX);
    if (cwd_str == NULL) {
      return -1;
    }
    return CombineAbsDirAndPath(cwd_str, path, buffer_nchars, out_buffer);
  }
#else
#error unsupported platform
#endif
}
// Shortcut to just write the results to a string
int MakeAbsolutePath(const char* path, std::string* out_path) {
  int path_nchars = 0;
  int path_error = MakeAbsolutePath(path, &path_nchars, nullptr);
  if (!path_error) {
    std::vector<char> abs_path(path_nchars);
    path_error = MakeAbsolutePath(path, &path_nchars, abs_path.data());
    if (!path_error) {
      *out_path = abs_path.data();
    }
  }
  return path_error;
}

int TruncatePathToDir(char* path) {
#if defined(ZOMBO_OS_WINDOWS)
  return PathRemoveFileSpecA(path) ? 0 : -1;
#elif defined(ZOMBO_OS_POSIX)
  int len = strlen(path);
  // remove trailing slashes
  while (len > 1 && path[len - 1] == '/') {
    path[len - 1] = '\0';
    --len;
  }
  char* last_slash = strrchr(path, '/');
  if (last_slash) {
    *(last_slash + 1) = '\0';
  }
  return 0;
#else
#error unsupported platform
#endif
}

// Takes an absolute path to a directory. Creates the directory and all missing parent directories.
int CreateDirectoryAndParents(const char* abs_dir) {
  if (IsRelativePath(abs_dir)) {
    return -1;  // input must be absolute
  }
  if (IsPathDirectory(abs_dir)) {
    return 0;
  }
  std::string parent = abs_dir;
  int truncate_error = TruncatePathToDir(&parent[0]);
  ZOMBO_ASSERT_RETURN(!truncate_error, -2, "TruncatePathToDir(%s) failed", abs_dir);
  int create_error = CreateDirectoryAndParents(parent.c_str());
  if (create_error) {
    fprintf(stderr, "error: Could not create directory %s\n", parent.c_str());
    return -2;
  }
  return zomboMkdir(abs_dir);
}

}  // namespace

constexpr int SPOKK_MAX_VERTEX_COLORS = 4;
constexpr int SPOKK_MAX_VERTEX_TEXCOORDS = 4;

struct SourceAttribute {
  spokk::VertexLayout layout;
  const void* values;
};

static void HandleReadFileError(const std::string& errorString) { fprintf(stderr, "ERROR: %s\n", errorString.c_str()); }

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
    HandleReadFileError(importer.GetErrorString());
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
    ZOMBO_ASSERT_RETURN(
        convert_error == 0, -2, "error converting attribute at location %u", attrib.layout.attributes[0].location);
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
    std::vector<VkVertexInputBindingDescription> vb_descs(
        mesh_header.vertex_buffer_count, VkVertexInputBindingDescription{});
    {
      vb_descs[0].binding = 0;
      vb_descs[0].stride = dst_layout.stride;
      vb_descs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    std::vector<VkVertexInputAttributeDescription> attr_descs(
        dst_layout.attributes.size(), VkVertexInputAttributeDescription{});
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

class AssetManifest {
public:
  explicit AssetManifest();
  ~AssetManifest();

  int Load(const std::string& json5_filename);
  int OverrideOutputRoot(const std::string& output_root_dir);
  void SetForceRebuild(bool force_rebuild_all);
  int Build();

private:
  AssetManifest(const AssetManifest& rhs) = delete;
  AssetManifest& operator=(const AssetManifest& rhs) = delete;

  // Converts a json_parse_error_e to a human-readable string
  const char* JsonParseErrorStr(const json_parse_error_e error_code) const;
  // Returns a human-readable location of the specified json_value_s.
  // This assumes the manifest file was parsed with the
  // 'json_parse_flags_allow_location_information' flag enabled.
  std::string JsonValueLocationStr(const json_value_s* val) const;

  enum AssetClass {
    ASSET_CLASS_UNKNOWN = 0,
    ASSET_CLASS_IMAGE = 1,
    ASSET_CLASS_MESH = 2,
    ASSET_CLASS_SHADER = 3,
  };
  AssetClass GetAssetClassFromInputPath(const json_value_s* input_path_val) const;

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

  int IsOutputOutOfDate(const std::string& input_path, const std::string& output_path, bool* out_result) const;
  int CopyAssetFile(const std::string& input_path, const std::string& output_path) const;

  int ProcessImage(const ImageAsset& image);
  int ProcessMesh(const MeshAsset& image);
  int ProcessShader(const ShaderAsset& image);

  std::string launch_dir_;
  std::string manifest_dir_;
  std::string manifest_filename_;
  std::string output_root_;
  bool force_rebuild_;

  time_t manifest_mtime_;

  std::vector<std::string> shader_include_dirs_;

  std::vector<ImageAsset> image_assets_;
  std::vector<MeshAsset> mesh_assets_;
  std::vector<ShaderAsset> shader_assets_;
};

AssetManifest::AssetManifest()
  : launch_dir_("."), manifest_dir_("."), manifest_filename_(""), output_root_("."), force_rebuild_(false) {}
AssetManifest::~AssetManifest() {}

int AssetManifest::Load(const std::string& json5_filename) {
  manifest_filename_ = json5_filename;

  FILE* manifest_file = zomboFopen(manifest_filename_.c_str(), "rb");
  if (!manifest_file) {
    fprintf(stderr, "ERROR: Could not open manifest file %s\n", manifest_filename_.c_str());
    return -1;
  }
  fseek(manifest_file, 0, SEEK_END);
  size_t manifest_nbytes = ftell(manifest_file);
  std::vector<uint8_t> manifest_bytes(manifest_nbytes);
  fseek(manifest_file, 0, SEEK_SET);
  size_t read_nbytes = fread(manifest_bytes.data(), 1, manifest_nbytes, manifest_file);
  fclose(manifest_file);
  if (read_nbytes != manifest_nbytes) {
    fprintf(stderr, "ERROR: file I/O error while loading %s\n", manifest_filename_.c_str());
    return -2;
  }

  // Grab the modification time of the manifest file, so we can compare it to the output modification times later.
  int attr_error = GetFileModificationTime(manifest_filename_.c_str(), &manifest_mtime_);
  ZOMBO_ASSERT_RETURN(!attr_error, -1, "Could not access modification time for %s", manifest_filename_.c_str());
  // Save the directory we launched from
  launch_dir_.resize(300);
  zomboGetcwd(&launch_dir_[0], (int)launch_dir_.capacity());
  // chdir to the same directory as the manifest file
  int path_error = MakeAbsolutePath(json5_filename.c_str(), &manifest_dir_);
  ZOMBO_ASSERT(!path_error, "abs path error");
  path_error = TruncatePathToDir(&manifest_dir_[0]);
  ZOMBO_ASSERT_RETURN(!path_error, -4, "Failed to truncate path to manifest file %s", json5_filename.c_str());
  zomboChdir(manifest_dir_.c_str());

  json_parse_result_s parse_result = {};
  json_value_s* manifest = json_parse_ex(manifest_bytes.data(), manifest_bytes.size(),
      json_parse_flags_allow_json5 | json_parse_flags_allow_location_information, NULL, NULL, &parse_result);
  if (!manifest) {
    fprintf(stderr, "%s(%u): error %u at column %u (%s)\n", manifest_filename_.c_str(),
        (uint32_t)parse_result.error_line_no, (uint32_t)parse_result.error, (uint32_t)parse_result.error_row_no,
        JsonParseErrorStr((json_parse_error_e)parse_result.error));
    return -5;
  }
  int parse_error = ParseRoot(manifest);
  free(manifest);
  return parse_error;
}

int AssetManifest::OverrideOutputRoot(const std::string& output_root_dir) {
  return CombineAbsDirAndPath(launch_dir_.c_str(), output_root_dir.c_str(), &output_root_);
}

void AssetManifest::SetForceRebuild(bool force_rebuild_all) { force_rebuild_ = force_rebuild_all; }

int AssetManifest::Build() {
  int process_error = 0;
  for (const auto& image : image_assets_) {
    process_error = ProcessImage(image);
    if (process_error != 0) {
      return process_error;
    }
  }
  for (const auto& mesh : mesh_assets_) {
    process_error = ProcessMesh(mesh);
    if (process_error != 0) {
      return process_error;
    }
  }
  for (const auto& shader : shader_assets_) {
    process_error = ProcessShader(shader);
    if (process_error != 0) {
      return process_error;
    }
  }
  return 0;
}

const char* AssetManifest::JsonParseErrorStr(const json_parse_error_e error_code) const {
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
  return manifest_filename_ + "(" + std::to_string(val_ex->line_no) + ":" + std::to_string(val_ex->row_no) + ")";
}

int AssetManifest::ParseRoot(const json_value_s* val) {
  if (val->type != json_type_object) {
    fprintf(stderr, "%s: error: root payload must be an object\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  json_object_s* root_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  int parse_error = 0;
  for (json_object_element_s* child_elem = root_obj->start; i_child < root_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "assets") == 0) {
      parse_error = ParseAssets(child_elem->value);
      if (parse_error) {
        break;
      }
    } else if (strcmp(child_elem->name->string, "defaults") == 0) {
      parse_error = ParseDefaults(child_elem->value);
      if (parse_error) {
        break;
      }
    }
  }
  return parse_error;
}

int AssetManifest::ParseDefaults(const json_value_s* val) {
  if (val->type != json_type_object) {
    fprintf(stderr, "%s: error: defaults payload must be an object\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  const json_object_s* defaults_obj = (const json_object_s*)(val->payload);
  size_t i_child = 0;
  int parse_error = 0;
  for (json_object_element_s* child_elem = defaults_obj->start; i_child < defaults_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "output_root") == 0) {
      parse_error = ParseDefaultOutputRoot(child_elem->value);
      if (parse_error) {
        break;
      }
    } else if (strcmp(child_elem->name->string, "shader_include_dirs") == 0) {
      parse_error = ParseDefaultShaderIncludeDirs(child_elem->value);
      if (parse_error) {
        break;
      }
    }
  }
  return parse_error;
}

int AssetManifest::ParseDefaultOutputRoot(const json_value_s* val) {
  if (val->type != json_type_string) {
    fprintf(stderr, "%s: error: output_root payload must be a string\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  const json_string_s* output_root_str = (const json_string_s*)(val->payload);
  return CombineAbsDirAndPath(manifest_dir_.c_str(), output_root_str->string, &output_root_);
}

int AssetManifest::ParseDefaultShaderIncludeDirs(const json_value_s* val) {
  if (val->type != json_type_array) {
    fprintf(stderr, "%s: error: shader_include_dirs payload must be an array\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  const json_array_s* includes_array = (const json_array_s*)(val->payload);
  size_t i_child = 0;
  int parse_error = 0;
  for (json_array_element_s* child_elem = includes_array->start; i_child < includes_array->length;
       ++i_child, child_elem = child_elem->next) {
    // parse array of shader includes. Combine each dir with the manifest dir and canonicalize
    if (child_elem->value->type != json_type_string) {
      fprintf(stderr, "%s: error: shader_include_dirs element must be a string\n",
          JsonValueLocationStr(child_elem->value).c_str());
      return -2;
    }
    const json_string_s* include_dir_str = (const json_string_s*)(child_elem->value->payload);
    std::string abs_include_dir;
    int path_error = CombineAbsDirAndPath(manifest_dir_.c_str(), include_dir_str->string, &abs_include_dir);
    ZOMBO_ASSERT(!path_error, "dir+path combine error");
    if (path_error) {
      break;
    }
    shader_include_dirs_.push_back(abs_include_dir);
  }
  return parse_error;
}

int AssetManifest::ParseAssets(const json_value_s* val) {
  if (val->type != json_type_array) {
    fprintf(stderr, "%s: error: assets payload must be an array\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  const json_array_s* assets_array = (const json_array_s*)(val->payload);
  size_t i_child = 0;
  int parse_error = 0;
  for (json_array_element_s* child_elem = assets_array->start; i_child < assets_array->length;
       ++i_child, child_elem = child_elem->next) {
    parse_error = ParseAsset(child_elem->value);
    if (parse_error) {
      break;
    }
  }
  return parse_error;
}

int AssetManifest::ParseAsset(const json_value_s* val) {
  if (val->type != json_type_object) {
    fprintf(stderr, "%s: error: asset payload must be an object\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  // First we need to loop through child elements and find the asset class, so we know which Parse*Asset()
  // variant to call.
  json_object_s* asset_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  for (json_object_element_s* child_elem = asset_obj->start; i_child < asset_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "class") == 0) {
      if (child_elem->value->type != json_type_string) {
        fprintf(stderr, "%s: error: asset class payload must be a string\n", JsonValueLocationStr(val).c_str());
        return -2;
      }
      const json_string_s* asset_class_str = (const json_string_s*)child_elem->value->payload;
      if (strcmp(asset_class_str->string, "image") == 0) {
        return ParseImageAsset(val);
      } else if (strcmp(asset_class_str->string, "mesh") == 0) {
        return ParseMeshAsset(val);
      } else if (strcmp(asset_class_str->string, "shader") == 0) {
        return ParseShaderAsset(val);
      } else {
        fprintf(stderr, "%s: error: unknown asset class \"%s\"\n", JsonValueLocationStr(child_elem->value).c_str(),
            asset_class_str->string);
        return -3;
      }
    }
  }
  fprintf(stderr, "%s: error: asset has no \"class\" member\n", JsonValueLocationStr(val).c_str());
  return -4;
}

int AssetManifest::ParseImageAsset(const json_value_s* val) {
  const json_string_s* input_path = nullptr;
  const json_string_s* output_path = nullptr;
  json_object_s* asset_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  for (json_object_element_s* child_elem = asset_obj->start; i_child < asset_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "class") == 0) {
      // Already handled by caller
    } else if (strcmp(child_elem->name->string, "input") == 0) {
      if (child_elem->value->type != json_type_string) {
        fprintf(stderr, "%s: error: input payload must be a string\n", JsonValueLocationStr(val).c_str());
        return -1;
      }
      input_path = (const json_string_s*)(child_elem->value->payload);
    } else if (strcmp(child_elem->name->string, "output") == 0) {
      if (child_elem->value->type != json_type_string) {
        fprintf(stderr, "%s: error: output payload must be a string\n", JsonValueLocationStr(val).c_str());
        return -2;
      }
      output_path = (const json_string_s*)(child_elem->value->payload);
    } else {
      fprintf(stderr, "%s: warning: ignoring unexpected tag '%s'\n", JsonValueLocationStr(val).c_str(),
          child_elem->name->string);
    }
  }
  if (!input_path || !output_path) {
    fprintf(stderr, "%s: error: incomplete image asset\n", JsonValueLocationStr(val).c_str());
    return -3;
  }

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
  for (json_object_element_s* child_elem = asset_obj->start; i_child < asset_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "class") == 0) {
      // Already handled by caller
    } else if (strcmp(child_elem->name->string, "input") == 0) {
      if (child_elem->value->type != json_type_string) {
        fprintf(stderr, "%s: error: input payload must be a string\n", JsonValueLocationStr(val).c_str());
        return -1;
      }
      input_path = (const json_string_s*)(child_elem->value->payload);
    } else if (strcmp(child_elem->name->string, "output") == 0) {
      if (child_elem->value->type != json_type_string) {
        fprintf(stderr, "%s: error: output payload must be a string\n", JsonValueLocationStr(val).c_str());
        return -2;
      }
      output_path = (const json_string_s*)(child_elem->value->payload);
    } else {
      fprintf(stderr, "%s: warning: ignoring unexpected tag '%s'\n", JsonValueLocationStr(val).c_str(),
          child_elem->name->string);
    }
  }
  if (!input_path || !output_path) {
    fprintf(stderr, "%s: error: incomplete mesh asset\n", JsonValueLocationStr(val).c_str());
    return -3;
  }

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
  for (json_object_element_s* child_elem = asset_obj->start; i_child < asset_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "class") == 0) {
      // Already handled by caller
    } else if (strcmp(child_elem->name->string, "input") == 0) {
      if (child_elem->value->type != json_type_string) {
        fprintf(stderr, "%s: error: input payload must be a string\n", JsonValueLocationStr(val).c_str());
        return -1;
      }
      input_path = (const json_string_s*)(child_elem->value->payload);
    } else if (strcmp(child_elem->name->string, "output") == 0) {
      if (child_elem->value->type != json_type_string) {
        fprintf(stderr, "%s: error: output payload must be a string\n", JsonValueLocationStr(val).c_str());
        return -2;
      }
      output_path = (const json_string_s*)(child_elem->value->payload);
    } else if (strcmp(child_elem->name->string, "entry") == 0) {
      if (child_elem->value->type != json_type_string) {
        fprintf(stderr, "%s: error: entry payload must be a string\n", JsonValueLocationStr(val).c_str());
        return -3;
      }
      entry_point = (const json_string_s*)(child_elem->value->payload);
    } else if (strcmp(child_elem->name->string, "stage") == 0) {
      if (child_elem->value->type != json_type_string) {
        fprintf(stderr, "%s: error: stage payload must be a string\n", JsonValueLocationStr(val).c_str());
        return -4;
      }
      shader_stage = (const json_string_s*)(child_elem->value->payload);
    } else {
      fprintf(stderr, "%s: warning: ignoring unexpected tag '%s'\n", JsonValueLocationStr(val).c_str(),
          child_elem->name->string);
    }
  }
  if (!input_path || !output_path) {
    fprintf(stderr, "%s: error: incomplete shader asset\n", JsonValueLocationStr(val).c_str());
    return -3;
  }
  ShaderAsset shader = {};
  shader.json_location = JsonValueLocationStr(val);
  shader.input_path = input_path->string;
  shader.output_path = output_path->string;
  shader.entry_point = entry_point ? entry_point->string : "";
  shader.shader_stage = shader_stage ? shader_stage->string : "";
  shader_assets_.push_back(shader);
  return 0;
}

int AssetManifest::IsOutputOutOfDate(
    const std::string& input_path, const std::string& output_path, bool* out_result) const {
  // Do the files exist? Missing input = error! Missing output = automatic rebuild!
  bool input_exists = FileExists(input_path.c_str());
  if (!input_exists) {
    fprintf(stderr, "%s: error: input file '%s' does not exist\n", manifest_filename_.c_str(), input_path.c_str());
    return -6;
  }
  bool output_exists = FileExists(output_path.c_str());

  // Force rebuild = automatically out of date
  if (force_rebuild_) {
    *out_result = true;
    return 0;
  }

  // If both files exists, we compare last-write time.
  bool output_is_older = false;
  if (input_exists && output_exists) {
    time_t input_mtime = 0, output_mtime = 0;
    int input_attr_error = GetFileModificationTime(input_path.c_str(), &input_mtime);
    ZOMBO_ASSERT_RETURN(!input_attr_error, -3, "Failed to read file attributes for %s", input_path.c_str());
    int output_attr_error = GetFileModificationTime(output_path.c_str(), &output_mtime);
    ZOMBO_ASSERT_RETURN(!output_attr_error, -3, "Failed to read file attributes for %s", output_path.c_str());
    // Compare file write times
    if (output_mtime < input_mtime) {
      output_is_older = true;
    }
    // Also compare output write time to manifest write time; if the manifest is newer, assume everything
    // is out of date. This does mean you get a full asset rebuild every time the manifest changes, which would
    // probably be unacceptable in a large production environment, but I'm really not there yet.
    if (output_mtime < manifest_mtime_) {
      output_is_older = true;
    }
    // TODO(https://github.com/cdwfs/spokk/issues/27): modifying shader #includes doesn't rebuild
    // the shaders that include them. Keep track of included headers, and check them here as well.
  }
  *out_result = (!output_exists || output_is_older);
  return 0;
}

int AssetManifest::CopyAssetFile(const std::string& input_path, const std::string& output_path) const {
  // Create any missing parent directories for the output file
  std::string abs_output_dir;
  int path_error = MakeAbsolutePath(output_path.c_str(), &abs_output_dir);
  ZOMBO_ASSERT_RETURN(!path_error, -1, "Can't make absolute path for %s", output_path.c_str());
  path_error = TruncatePathToDir(&abs_output_dir[0]);
  ZOMBO_ASSERT_RETURN(!path_error, -2, "Can't make truncate dir for %s", abs_output_dir.c_str());
  int dir_error = CreateDirectoryAndParents(abs_output_dir.c_str());
  ZOMBO_ASSERT_RETURN(!dir_error, -3, "Can't create directory %s", abs_output_dir.c_str());
  // Open input and output files
  FILE* input_file = zomboFopen(input_path.c_str(), "rb");
  if (!input_file) {
    return -4;
  }
  fseek(input_file, 0, SEEK_END);
  size_t input_file_nbytes = ftell(input_file);
  fseek(input_file, 0, SEEK_SET);
  FILE* output_file = zomboFopen(output_path.c_str(), "wb");
  if (!output_file) {
    fclose(input_file);
    return -5;
  }
  // Copy file contents through a decently sized intermediate buffer
  size_t copied_nbytes = 0;
  const size_t max_batch_nbytes = 64 * 1024;
  std::vector<uint8_t> batch_data(max_batch_nbytes);
  int copy_error = 0;
  while (copied_nbytes != input_file_nbytes) {
    size_t read_nbytes = fread(batch_data.data(), 1, max_batch_nbytes, input_file);
    // batch_nbytes may be less than the full batch if it's the last batch of the file
    if (read_nbytes != max_batch_nbytes && copied_nbytes + read_nbytes != input_file_nbytes) {
      fprintf(stderr, "error: I/O error while reading from %s: fread() returned %d, expected %d", input_path.c_str(),
          (int)read_nbytes, (int)max_batch_nbytes);
      copy_error = -10;
      break;
    }

    size_t write_nbytes = fwrite(batch_data.data(), 1, read_nbytes, output_file);
    if (write_nbytes != read_nbytes) {
      fprintf(stderr, "error: I/O error writing to %s: fwrite() returned %d, expected %d", output_path.c_str(),
          (int)write_nbytes, (int)read_nbytes);
      copy_error = -11;
      break;
    }
    copied_nbytes += read_nbytes;
  }
  fclose(input_file);
  fclose(output_file);
  return copy_error;
}

int AssetManifest::ProcessImage(const ImageAsset& image) {
  bool build_output = false;
  std::string abs_output_path;
  int path_error = CombineAbsDirAndPath(output_root_.c_str(), image.output_path.c_str(), &abs_output_path);
  ZOMBO_ASSERT_RETURN(!path_error, -1, "CombineAbsDirAndPath('%s', '%s') failed (%d) for image at %s",
      output_root_.c_str(), image.output_path.c_str(), path_error, image.json_location.c_str());
  int query_error = IsOutputOutOfDate(image.input_path, abs_output_path, &build_output);
  if (query_error) {
    return query_error;
  }
  if (build_output) {
    // create missing subdirectories in abs_output_path if necessary.
    // This should be a helper function.
    std::string output_dir = abs_output_path;
    int truncate_error = TruncatePathToDir(&output_dir[0]);
    ZOMBO_ASSERT_RETURN(!truncate_error, -1, "TruncatePathToDir('%s') failed (%d)", output_dir.c_str(), truncate_error);
    int create_dir_error = CreateDirectoryAndParents(output_dir.c_str());
    ZOMBO_ASSERT_RETURN(
        !create_dir_error, -1, "CreateDirectoryAndParents('%s') failed (%d)", output_dir.c_str(), create_dir_error);

    int copy_error = CopyAssetFile(image.input_path, abs_output_path.c_str());
    if (copy_error) {
      fprintf(stderr, "%s: error: CopyAssetFile() failed for image\n", image.json_location.c_str());
      return -3;
    }
    printf("%s -> %s\n", image.input_path.c_str(), abs_output_path.c_str());
  } else {
    // printf("Skipped %s (%s is up to date)\n", image.input_path.c_str(), abs_output_path.c_str());
  }
  return 0;
}

int AssetManifest::ProcessMesh(const MeshAsset& mesh) {
  bool build_output = false;
  std::string abs_output_path;
  int path_error = CombineAbsDirAndPath(output_root_.c_str(), mesh.output_path.c_str(), &abs_output_path);
  ZOMBO_ASSERT_RETURN(path_error == 0, -1, "CreateAbsoluteOutputPath failed (%d) for mesh at %s", path_error,
      mesh.json_location.c_str());
  int query_error = IsOutputOutOfDate(mesh.input_path, abs_output_path, &build_output);
  if (query_error) {
    return query_error;
  }
  if (build_output) {
    // create missing subdirectories in abs_output_path if necessary.
    // This should be a helper function.
    std::string output_dir = abs_output_path;
    int truncate_error = TruncatePathToDir(&output_dir[0]);
    ZOMBO_ASSERT_RETURN(!truncate_error, -1, "TruncatePathToDir('%s') failed (%d)", output_dir.c_str(), truncate_error);
    int create_dir_error = CreateDirectoryAndParents(output_dir.c_str());
    ZOMBO_ASSERT_RETURN(
        !create_dir_error, -1, "CreateDirectoryAndParents('%s') failed (%d)", output_dir.c_str(), create_dir_error);

    int process_error = ConvertSceneToMesh(mesh.input_path, abs_output_path.c_str());
    if (process_error) {
      return process_error;
    }
    printf("%s -> %s\n", mesh.input_path.c_str(), abs_output_path.c_str());
  } else {
    // printf("Skipped %s (%s is up to date)\n", mesh.input_path.c_str(), abs_output_path.c_str());
  }
  return 0;
}

int AssetManifest::ProcessShader(const ShaderAsset& shader) {
  bool build_output = false;
  std::string abs_output_path;
  int path_error = CombineAbsDirAndPath(output_root_.c_str(), shader.output_path.c_str(), &abs_output_path);
  ZOMBO_ASSERT_RETURN(path_error == 0, -1, "CreateAbsoluteOutputPath failed (%d) for shader at %s", path_error,
      shader.json_location.c_str());
  int query_error = IsOutputOutOfDate(shader.input_path, abs_output_path, &build_output);
  if (query_error) {
    return query_error;
  }
  if (build_output) {
    // create missing subdirectories in abs_output_path if necessary.
    // This should be a helper function.
    std::string output_dir = abs_output_path;
    int truncate_error = TruncatePathToDir(&output_dir[0]);
    ZOMBO_ASSERT_RETURN(!truncate_error, -1, "TruncatePathToDir('%s') failed (%d)", output_dir.c_str(), truncate_error);
    int create_dir_error = CreateDirectoryAndParents(output_dir.c_str());
    ZOMBO_ASSERT_RETURN(
        !create_dir_error, -1, "CreateDirectoryAndParents('%s') failed (%d)", output_dir.c_str(), create_dir_error);

    // TODO(cort): move to init, this lookup can happen once
    const char* vulkan_sdk_dir = zomboGetEnv("VULKAN_SDK");
    ZOMBO_ASSERT_RETURN(vulkan_sdk_dir != nullptr, -1, "VULKAN_SDK environment variable not found?");
    std::string abs_glslc_path;
    int glslc_path_error = CombineAbsDirAndPath(vulkan_sdk_dir, "bin/glslc", &abs_glslc_path);
    ZOMBO_ASSERT(!glslc_path_error, "dir+path combine error");

    std::vector<const char*> glslc_args = {
        // clang-format off
      abs_glslc_path.c_str(),
      //"-O",
      "--target-env=vulkan1.0", // Can't target vulkan 1.1 yet due to https://github.com/chaoticbob/SPIRV-Reflect/issues/67
      "-o",
      abs_output_path.c_str()
        // clang-format on
    };
    if (shader.shader_stage == "vert" || shader.shader_stage == "vertex") {
      glslc_args.push_back("-fshader-stage=vert");
    } else if (shader.shader_stage == "frag" || shader.shader_stage == "fragment") {
      glslc_args.push_back("-fshader-stage=frag");
    } else if (shader.shader_stage == "geom" || shader.shader_stage == "geometry") {
      glslc_args.push_back("-fshader-stage=geom");
    } else if (shader.shader_stage == "tese" || shader.shader_stage == "tesseval") {
      glslc_args.push_back("-fshader-stage=tess");
    } else if (shader.shader_stage == "comp" || shader.shader_stage == "compute") {
      glslc_args.push_back("-fshader-stage=comp");
    } else {
      fprintf(stderr, "%s: error: Unrecognized shader stage '%s'\n", shader.json_location.c_str(),
          shader.shader_stage.c_str());
      return -3;
    }
    for (auto& dir : shader_include_dirs_) {
      glslc_args.push_back("-I");
      glslc_args.push_back(dir.c_str());
    }
    glslc_args.push_back(shader.input_path.c_str());
    glslc_args.push_back(nullptr);
    struct subprocess_s glslc_process;
    int subprocess_options = subprocess_option_combined_stdout_stderr;
    int subprocess_create_error = subprocess_create(glslc_args.data(), subprocess_options, &glslc_process);
    if (subprocess_create_error != 0) {
      fprintf(stderr, "%s: Error creating glslc subprocess '", shader.json_location.c_str());
      for (const char* arg : glslc_args) {
        fprintf(stderr, "%s", arg);
      }
      fprintf(stderr, "'\n");
      return -4;
    }

    int glslc_return_code = 0;
    int subprocess_join_error = subprocess_join(&glslc_process, &glslc_return_code);
    ZOMBO_ASSERT_RETURN(subprocess_join_error == 0, -5, "%s: shader compiler exited unexpectedly (process join error %d)",
        shader.json_location.c_str(), subprocess_join_error);

    FILE* glslc_output = subprocess_stdout(&glslc_process);
    char glslc_output_buffer[128];
#if 1
    while (fgets(glslc_output_buffer, 128, glslc_output)) {
      fprintf(stderr, "%s", glslc_output_buffer);
    }
#else
    while (!feof(glslc_output) && !ferror(glslc_output)) {
      const char* str = fgets(glslc_output_buffer, 127, glslc_output);
      int eof = feof(glslc_output);
      int err = ferror(glslc_output);
      if (str && !eof && !err) fprintf(stderr, "%s", glslc_output_buffer);
    }
#endif
    int subprocess_destroy_error = subprocess_destroy(&glslc_process);
    ZOMBO_ASSERT_RETURN(subprocess_destroy_error == 0, -6,
        "%s: shader compiler exited unexpectedly (process destroy error %d)", shader.json_location.c_str(),
        subprocess_destroy_error);
    ZOMBO_ASSERT_RETURN(glslc_return_code == 0, -7,
        "%s: shader compilation failed with error code %d; see compiler output for details\n",
        shader.json_location.c_str(), glslc_return_code);

    printf("%s -> %s\n", shader.input_path.c_str(), abs_output_path.c_str());
  } else {
    // printf("Skipped %s (%s is up to date)\n", shader.input_path.c_str(), abs_output_path.c_str());
  }
  return 0;
}

void PrintUsage(const char* argv0) {
  printf(R"usage(\
Usage: %s [options] manifest.json5
Options:
  -h, --help:            Prints this message.
  -f, --force-rebuild    Force all assets to rebuild.
  -o <root>              Override output root in manifest with the specified
                         directory.
)usage",
      argv0);
}

int main(int argc, char* argv[]) {
  // TODO(cort): Command line options
  // -v, --verbose: Extra logging?
  // -t, --test-only: Print what would be done but don't actually do it
  const char* new_output_root = nullptr;
  const char* manifest_filename = nullptr;
  bool force_rebuild = false;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      PrintUsage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      new_output_root = argv[++i];
    } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force-rebuild") == 0) {
      force_rebuild = true;
    } else if (i == argc - 1) {
      manifest_filename = argv[i];
    } else {
      PrintUsage(argv[0]);
      return -1;
    }
  }
  if (!manifest_filename) {
    PrintUsage(argv[0]);
    return -1;
  }

  AssetManifest manifest;
  int load_error = manifest.Load(manifest_filename);
  if (load_error) {
    return load_error;
  }

  if (new_output_root) {
    int override_error = manifest.OverrideOutputRoot(new_output_root);
    if (override_error) {
      return override_error;
    }
  }

  manifest.SetForceRebuild(force_rebuild);

  int build_error = manifest.Build();
  if (build_error) {
    return build_error;
  }

  return 0;
}
