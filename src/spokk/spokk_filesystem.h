#pragma once

#include <ctime>
#include <string>

namespace spokk {

int GetFileModificationTime(const char* path, time_t* out_mtime);

// buffer_nchars should include space for the terminating character.
// if out_buffer is NULL, stores the number of chars necessary to hold the output in buffer_nchars.
// Returns 0 on success, non-zero on error.
// May attempt to canonicalize/normalize the path (removing duplicate separators,
// eliminating ./.., etc.
// Not safe in multithreaded programs (cwd is shared process-level state)
int MakeAbsolutePath(const char* path, int* buffer_nchars, char* out_buffer);
// Shortcut to just write the results to a string
int MakeAbsolutePath(const char* path, std::string* out_path);

int TruncatePathToDir(char* path);

// if out_buffer is NULL, stores the number of chars necessary to hold the output in buffer_nchars.
// If path is absolute, out_buffer is canonicalize(path).
// If path is relative, out_buffer is canonicalize(abs_dir+path)
// Not safe in multithreaded programs (cwd is shared process-level state)
int CombineAbsDirAndPath(const char* abs_dir, const char* path, int* buffer_nchars, char* out_buffer);
// Shortcut to just write the results to a string
int CombineAbsDirAndPath(const char* abs_dir, const char* path, std::string* out_path);

bool FileExists(const char* path);

// Takes an absolute path to a directory. Creates the directory and all missing parent directories.
int CreateDirectoryAndParents(const char* abs_dir);

}  // namespace spokk
