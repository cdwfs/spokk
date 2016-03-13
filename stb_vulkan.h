/* stb_vulkan - v0.01 - public domain Vulkan helper
                                     no warranty implied; use at your own risk

   Do this:
      #define STB_VULKAN_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.

   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define STB_VULKAN_IMPLEMENTATION
   #include "stb_vulkan.h"

   You can #define STBVK_ASSERT(x) before the #include to avoid using assert.h.
   And #define STBVK_MALLOC, STBVK_REALLOC, and STBVK_FREE to avoid using malloc,realloc,free



LICENSE

This software is in the public domain. Where that dedication is not
recognized, you are granted a perpetual, irrevocable license to copy,
distribute, and modify this file as you see fit.

*/

#ifndef STBVK_INCLUDE_STB_VULKAN_H
#define STBVK_INCLUDE_STB_VULKAN_H

#include <vulkan/vulkan.h>

#ifndef STBVK_NO_STDIO
#   include <stdio.h>
#endif // STBVK_NO_STDIO

#define STBVK_VERSION 1

typedef unsigned char stbvk_uc;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef STB_VULKAN_STATIC
#   define STBVKDEF static
#else
#   define STBVKDEF extern
#endif

//////////////////////////////////////////////////////////////////////////////
//
// PUBLIC API
//

    typedef struct
    {
        VkAllocationCallbacks allocation_callbacks;
        VkInstance instance;
        VkDevice device;
        VkPhysicalDevice physical_device;
    } stbvk_context;

    STBVKDEF stbvk_init_context(stbvk_context *c);

    typedef struct
    {
       int      (*read)  (void *user,char *data,int size);   // fill 'data' with 'size' bytes.  return number of bytes actually read
       void     (*skip)  (void *user,int n);                 // skip the next 'n' bytes, or 'unget' the last -n bytes if negative
       int      (*eof)   (void *user);                       // returns nonzero if we are at end of file/data
    } stbvk_io_callbacks;

    STBVKDEF VkShaderModule stbvk_load_shader_from_memory(stbvk_context *c, stbvk_uc const *buffer, int len);
    STBVKDEF VkShaderModule stbvk_load_shader_from_callbacks(stbvk_context *c, stbvk_io_callbacks const *clbk, void *user);
#ifndef STBVK_NO_STDIO
    STBVKDEF VkShaderModule stbvk_load_shader(stbvk_context *c, char const *filename);
    STBVKDEF VkShaderModule stbvk_load_shader_from_file(stbvk_context *c, FILE *f, int len);
#endif


#ifdef __cplusplus
}
#endif

//
//
////   end header file   /////////////////////////////////////////////////////
#endif // STBVK_INCLUDE_STB_VULKAN_H

#ifdef STB_VULKAN_IMPLEMENTATION

#ifndef STBVK_NO_STDIO
#   include <stdio.h>
#endif

#ifndef STBVK_ASSERT
#   include <assert.h>
#   define STBVK_ASSERT(x) assert(x)
#endif

#ifndef _MSC_VER
#   ifdef __cplusplus
#       define stbvk_inline inline
#   else
#       define stbvk_inline
#   endif
#else
#   define stbvk_inline __forceinline
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1700)
typedef UINT16 stbvk__uint16;
typedef  INT16 stbvk__int16;
typedef UINT32 stbvk__uint32;
typedef  INT32 stbvk__int32;
#else
#include <stdint.h>
typedef uint16_t stbvk__uint16;
typedef int16_t  stbvk__int16;
typedef uint32_t stbvk__uint32;
typedef int32_t  stbvk__int32;
#endif
// should produce compiler error if size is wrong
typedef unsigned char validate_uint32[sizeof(stbvk__uint32)==4 ? 1 : -1];

#ifdef _MSC_VER
#   define STBVK_NOTUSED(v)  (void)(v)
#else
#   define STBVK_NOTUSED(v)  (void)sizeof(v)
#endif

#if defined(STBVK_MALLOC) && defined(STBVK_FREE) && (defined(STBVK_REALLOC) || defined(STBVK_REALLOC_SIZED))
// ok
#elif !defined(STBVK_MALLOC) && !defined(STBVK_FREE) && !defined(STBVK_REALLOC) && !defined(STBVK_REALLOC_SIZED)
// ok
#else
#   error "Must define all or none of STBVK_MALLOC, STBVK_FREE, and STBVK_REALLOC (or STBVK_REALLOC_SIZED)."
#endif

#ifndef STBVK_MALLOC
#   define STBVK_MALLOC(sz)           malloc(sz)
#   define STBVK_REALLOC(p,newsz)     realloc(p,newsz)
#   define STBVK_FREE(p)              free(p)
#endif

#ifndef STBVK_REALLOC_SIZED
#   define STBVK_REALLOC_SIZED(p,oldsz,newsz) STBVK_REALLOC(p,newsz)
#endif

// x86/x64 detection
#if defined(__x86_64__) || defined(_M_X64)
#   define STBVK__X64_TARGET
#elif defined(__i386) || defined(_M_IX86)
#   define STBVK__X86_TARGET
#endif

STBVKDEF stbvk_init_context(stbvk_context *c)
{

}


#ifndef STBVK_NO_STDIO
static FILE *stbvk__fopen(char const *filename, char const *mode)
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

STBVKDEF VkShaderModule stbvk_load_shader_from_file(stbvk_context *c, FILE *f, int len)
{
    void *shader_bin = STBVK_MALLOC(len);
    size_t bytes_read = fread(shader_bin, 1, len, f);
    if (bytes_read != len)
    {
        free(shader_bin);
        return VK_NULL_HANDLE;
    }
    VkShaderModule shader_module = stbvk_load_shader_from_memory(c, shader_bin, len);
    STBVK_FREE(shader_bin);
    return VK_NULL_HANDLE;
}

STBVKDEF VkShaderModule stbvk_load_shader(stbvk_context *c, char const *filename)
{
    FILE *spv_file = stbvk__fopen(filename, "rb");
    if (!spv_file)
    {
        return VK_NULL_HANDLE;
    }
    fseek(spv_file, 0, SEEK_END);
    long spv_file_size = ftell(spv_file);
    fseek(spv_file, 0, SEEK_SET);
    VkShaderModule shader_module = stbvk_load_shader_from_file(c, spv_file, spv_file_size);
    fclose(spv_file);
    return shader_module;
}

#endif

STBVKDEF VkShaderModule stbvk_load_shader_from_memory(stbvk_context *c, stbvk_uc const *buffer, int len)
{
    VkShaderModuleCreateInfo smci = {0};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.pNext = NULL;
    smci.flags = 0;
    smci.codeSize = len;
    smci.pCode = (uint32_t*)buffer;
    VkShaderModule shader_module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(c->device, &smci, &c->allocation_callbacks, &shader_module);
    return shader_module;
}
STBVKDEF VkShaderModule stbvk_load_shader_from_callbacks(stbvk_context *c, stbvk_io_callbacks const *clbk, void *user)
{
    return VK_NULL_HANDLE;
}


#endif // STB_VULKAN_IMPLEMENTATION
