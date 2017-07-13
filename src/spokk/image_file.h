#if !defined(IMAGE_FILE_H)
#define IMAGE_FILE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
typedef enum ImageFileFlagBits {
  IMAGE_FILE_FLAG_CUBE_BIT = 1,
  // TODO(https://github.com/cdwfs/spokk/issues/6): PACKED_FORMAT bit, to indicate >1 pixel per element?
} ImageFileFlagBits;
typedef uint32_t ImageFileFlags;

typedef enum ImageFileType {
  IMAGE_FILE_TYPE_UNKNOWN = 0,
  IMAGE_FILE_TYPE_PNG     = 1,
  IMAGE_FILE_TYPE_JPEG    = 2,
  IMAGE_FILE_TYPE_TGA     = 3,
  IMAGE_FILE_TYPE_BMP     = 4,
  IMAGE_FILE_TYPE_DDS     = 5,
  IMAGE_FILE_TYPE_ASTC    = 6,
  IMAGE_FILE_TYPE_KTX     = 7,
} ImageFileType;

typedef enum ImageFileDataFormat {
  IMAGE_FILE_DATA_FORMAT_UNKNOWN             = 0,
  IMAGE_FILE_DATA_FORMAT_R8G8B8_UNORM        = 1,
  IMAGE_FILE_DATA_FORMAT_R8G8B8A8_UNORM      = 2,
  IMAGE_FILE_DATA_FORMAT_B8G8R8_UNORM        = 3,
  IMAGE_FILE_DATA_FORMAT_B8G8R8A8_UNORM      = 4,
  IMAGE_FILE_DATA_FORMAT_R4G4B4A4_UNORM      = 5,
  IMAGE_FILE_DATA_FORMAT_B4G4R4A4_UNORM      = 6,
  IMAGE_FILE_DATA_FORMAT_R5G6B5_UNORM        = 7,
  IMAGE_FILE_DATA_FORMAT_B5G6R5_UNORM        = 8,
  IMAGE_FILE_DATA_FORMAT_R5G5B5A1_UNORM      = 9,
  IMAGE_FILE_DATA_FORMAT_B5G5R5A1_UNORM      = 10,
  IMAGE_FILE_DATA_FORMAT_A1R5G5B5_UNORM      = 11,
  IMAGE_FILE_DATA_FORMAT_R32G32B32A32_FLOAT  = 12,
  IMAGE_FILE_DATA_FORMAT_R32G32B32_FLOAT     = 13,
  IMAGE_FILE_DATA_FORMAT_R32G32_FLOAT        = 14,
  IMAGE_FILE_DATA_FORMAT_R32_FLOAT           = 15,
  IMAGE_FILE_DATA_FORMAT_R16G16B16A16_FLOAT  = 16,
  IMAGE_FILE_DATA_FORMAT_R16G16B16A16_UNORM  = 17,
  IMAGE_FILE_DATA_FORMAT_R16G16_FLOAT        = 18,
  IMAGE_FILE_DATA_FORMAT_R16G16_UNORM        = 19,
  IMAGE_FILE_DATA_FORMAT_R16_FLOAT           = 20,
  IMAGE_FILE_DATA_FORMAT_R16_UNORM           = 21,
  IMAGE_FILE_DATA_FORMAT_R8_UNORM            = 22,
  IMAGE_FILE_DATA_FORMAT_BC1_UNORM           = 23,
  IMAGE_FILE_DATA_FORMAT_BC1_SRGB            = 24,
  IMAGE_FILE_DATA_FORMAT_BC2_UNORM           = 25,
  IMAGE_FILE_DATA_FORMAT_BC2_SRGB            = 26,
  IMAGE_FILE_DATA_FORMAT_BC3_UNORM           = 27,
  IMAGE_FILE_DATA_FORMAT_BC3_SRGB            = 28,
  IMAGE_FILE_DATA_FORMAT_BC4_UNORM           = 29,
  IMAGE_FILE_DATA_FORMAT_BC4_SNORM           = 30,
  IMAGE_FILE_DATA_FORMAT_BC5_UNORM           = 31,
  IMAGE_FILE_DATA_FORMAT_BC5_SNORM           = 32,
  IMAGE_FILE_DATA_FORMAT_BC6H_UF16           = 33,
  IMAGE_FILE_DATA_FORMAT_BC6H_SF16           = 34,
  IMAGE_FILE_DATA_FORMAT_BC7_UNORM           = 35,
  IMAGE_FILE_DATA_FORMAT_BC7_SRGB            = 36,
  IMAGE_FILE_DATA_FORMAT_ASTC_4x4_UNORM      = 37,
  IMAGE_FILE_DATA_FORMAT_ASTC_4x4_SRGB       = 38,
  IMAGE_FILE_DATA_FORMAT_ASTC_5x4_UNORM      = 39,
  IMAGE_FILE_DATA_FORMAT_ASTC_5x4_SRGB       = 40,
  IMAGE_FILE_DATA_FORMAT_ASTC_5x5_UNORM      = 41,
  IMAGE_FILE_DATA_FORMAT_ASTC_5x5_SRGB       = 42,
  IMAGE_FILE_DATA_FORMAT_ASTC_6x5_UNORM      = 43,
  IMAGE_FILE_DATA_FORMAT_ASTC_6x5_SRGB       = 44,
  IMAGE_FILE_DATA_FORMAT_ASTC_6x6_UNORM      = 45,
  IMAGE_FILE_DATA_FORMAT_ASTC_6x6_SRGB       = 46,
  IMAGE_FILE_DATA_FORMAT_ASTC_8x5_UNORM      = 47,
  IMAGE_FILE_DATA_FORMAT_ASTC_8x5_SRGB       = 48,
  IMAGE_FILE_DATA_FORMAT_ASTC_8x6_UNORM      = 49,
  IMAGE_FILE_DATA_FORMAT_ASTC_8x6_SRGB       = 50,
  IMAGE_FILE_DATA_FORMAT_ASTC_8x8_UNORM      = 51,
  IMAGE_FILE_DATA_FORMAT_ASTC_8x8_SRGB       = 52,
  IMAGE_FILE_DATA_FORMAT_ASTC_10x5_UNORM     = 53,
  IMAGE_FILE_DATA_FORMAT_ASTC_10x5_SRGB      = 54,
  IMAGE_FILE_DATA_FORMAT_ASTC_10x6_UNORM     = 55,
  IMAGE_FILE_DATA_FORMAT_ASTC_10x6_SRGB      = 56,
  IMAGE_FILE_DATA_FORMAT_ASTC_10x8_UNORM     = 57,
  IMAGE_FILE_DATA_FORMAT_ASTC_10x8_SRGB      = 58,
  IMAGE_FILE_DATA_FORMAT_ASTC_10x10_UNORM    = 59,
  IMAGE_FILE_DATA_FORMAT_ASTC_10x10_SRGB     = 60,
  IMAGE_FILE_DATA_FORMAT_ASTC_12x10_UNORM    = 61,
  IMAGE_FILE_DATA_FORMAT_ASTC_12x10_SRGB     = 62,
  IMAGE_FILE_DATA_FORMAT_ASTC_12x12_UNORM    = 63,
  IMAGE_FILE_DATA_FORMAT_ASTC_12x12_SRGB     = 64,
  IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8_UNORM   = 65,
  IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8_SRGB    = 66,
  IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A1_UNORM = 67,
  IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A1_SRGB  = 68,
  IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A8_UNORM = 69,
  IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A8_SRGB  = 70,
  IMAGE_FILE_DATA_FORMAT_EAC_R11_UNORM       = 71,
  IMAGE_FILE_DATA_FORMAT_EAC_R11_SNORM       = 72,
  IMAGE_FILE_DATA_FORMAT_EAC_R11G11_UNORM    = 73,
  IMAGE_FILE_DATA_FORMAT_EAC_R11G11_SNORM    = 74,

  IMAGE_FILE_DATA_FORMAT_COUNT
} ImageFileDataFormat;
uint32_t ImageFileGetBytesPerTexelBlock(ImageFileDataFormat format);
// clang-format on

typedef struct ImageFileSubresource {
  uint32_t mip_level;
  uint32_t array_layer;
} ImageFileSubresource;

typedef struct ImageFile {
  uint32_t width;  // in pixels
  uint32_t height;  // in pixels
  uint32_t depth;  // in pixels
  uint32_t mip_levels;
  uint32_t array_layers;  // If flags.CUBE is set, this counts the number of cube faces, not whole cubes.
  uint32_t row_pitch_bytes;
  uint32_t depth_pitch_bytes;
  ImageFileType file_type;
  ImageFileFlags flags;
  ImageFileDataFormat data_format;
  void *file_contents;  // NOTE: may not include the entire file; headers may be stripped, etc.
} ImageFile;

// Returns 0 on success, non-zero on error
int ImageFileCreate(ImageFile *out_image, const char *image_path);
void ImageFileDestroy(const ImageFile *image);

size_t ImageFileGetSubresourceSize(const ImageFile *image, const ImageFileSubresource subresource);
void *ImageFileGetSubresourceData(const ImageFile *image, const ImageFileSubresource subresource);

#ifdef __cplusplus
}
#endif

#endif  // IMAGE_FILE_H
