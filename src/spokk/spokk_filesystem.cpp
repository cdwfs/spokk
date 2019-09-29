#include "spokk_filesystem.h"

#include <spokk_platform.h>

#if defined(ZOMBO_PLATFORM_WINDOWS)
#include <Shlwapi.h>  // for Path*() functions
#elif defined(ZOMBO_PLATFORM_POSIX)
#include <unistd.h>
#endif

#include <vector>

namespace {

#if defined(ZOMBO_PLATFORM_WINDOWS)
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

bool IsRelativePath(const char* path) {
#if defined(ZOMBO_PLATFORM_WINDOWS)
  return PathIsRelativeA(path) ? true : false;
#elif defined(ZOMBO_PLATFORM_POSIX)
  return (path != nullptr) && path[0] != '/';
#else
#error unsupported platform
#endif
}

}  // namespace

namespace spokk {

int GetFileModificationTime(const char* path, time_t* out_mtime) {
  ZomboStatStruct out_stats = {};
  int stat_error = zomboStat(path, &out_stats);
  if (!stat_error) {
    *out_mtime = out_stats.st_mtime;
  }
  return stat_error;
}

bool IsPathDirectory(const char* path) {
#if defined(ZOMBO_PLATFORM_WINDOWS)
  return PathIsDirectoryA(path) ? true : false;
#elif defined(ZOMBO_PLATFORM_POSIX)
  // This should work on Windows as well, but S_ISDIR would be _S_ISDIR
  ZomboStatStruct out_stats = {};
  int stat_error = zomboStat(path, &out_stats);
  return !stat_error && S_ISDIR(out_stats.st_mode) ? true : false;
#else
#error unsupported platform
#endif
}

bool FileExists(const char* path) {
#if defined(ZOMBO_PLATFORM_WINDOWS) || defined(ZOMBO_PLATFORM_POSIX)
  ZomboStatStruct out_stats = {};
  return (zomboStat(path, &out_stats) == 0);
#else
#error unsupported platform
#endif
}

int CombineAbsDirAndPath(const char* abs_dir, const char* path, int* buffer_nchars, char* out_buffer) {
  ZOMBO_ASSERT_RETURN(!IsRelativePath(abs_dir), -1, "abs_dir (%s) must be an absolute path", abs_dir);
#if defined(ZOMBO_PLATFORM_WINDOWS)
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
#elif defined(ZOMBO_PLATFORM_POSIX)
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

int MakeAbsolutePath(const char* path, int* buffer_nchars, char* out_buffer) {
#if defined(ZOMBO_PLATFORM_WINDOWS)
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
#elif defined(ZOMBO_PLATFORM_POSIX)
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
#if defined(ZOMBO_PLATFORM_WINDOWS)
  return PathRemoveFileSpecA(path) ? 0 : -1;
#elif defined(ZOMBO_PLATFORM_POSIX)
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

}  // namespace spokk
