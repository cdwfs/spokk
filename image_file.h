#if !defined(IMAGE_FILE_H)
#define IMAGE_FILE_H

#include <stdint.h>

typedef enum ImageFileFlagBits
{
	IMAGE_FILE_FLAG_CUBE_BIT = 1,
    // TODO(cort): COMPRESSED bit? (more accurately, MULTI_TEXEL_BLOCK?)
} ImageFileFlagBits;
typedef uint32_t ImageFileFlags;

typedef enum ImageFileType
{
    IMAGE_FILE_TYPE_UNKNOWN = 0,
    IMAGE_FILE_TYPE_PNG     = 1,
    IMAGE_FILE_TYPE_JPEG    = 2,
    IMAGE_FILE_TYPE_TGA     = 3,
    IMAGE_FILE_TYPE_BMP     = 4,
    IMAGE_FILE_TYPE_DDS     = 5,
    IMAGE_FILE_TYPE_ASTC    = 6,
} ImageFileType;

typedef enum ImageFileDataFormat
{
    IMAGE_FILE_DATA_FORMAT_UNKNOWN            = 0,
    IMAGE_FILE_DATA_FORMAT_R8G8B8_UNORM       = 1,
    IMAGE_FILE_DATA_FORMAT_R8G8B8A8_UNORM     = 2,
    IMAGE_FILE_DATA_FORMAT_B8G8R8_UNORM       = 3,
    IMAGE_FILE_DATA_FORMAT_B8G8R8A8_UNORM     = 4,
    IMAGE_FILE_DATA_FORMAT_R4G4B4A4_UNORM     = 5,
    IMAGE_FILE_DATA_FORMAT_B4G4R4A4_UNORM     = 6,
    IMAGE_FILE_DATA_FORMAT_R32G32B32A32_FLOAT = 7,
    IMAGE_FILE_DATA_FORMAT_R32G32B32_FLOAT    = 8,
    IMAGE_FILE_DATA_FORMAT_R32G32_FLOAT       = 9,
    IMAGE_FILE_DATA_FORMAT_R32_FLOAT          = 10,
    IMAGE_FILE_DATA_FORMAT_R16G16B16A16_FLOAT = 11,
    IMAGE_FILE_DATA_FORMAT_R16G16B16A16_UNORM = 12,
    IMAGE_FILE_DATA_FORMAT_R16G16_FLOAT       = 13,
    IMAGE_FILE_DATA_FORMAT_R16G16_UNORM       = 14,
    IMAGE_FILE_DATA_FORMAT_R16_FLOAT          = 15,
    IMAGE_FILE_DATA_FORMAT_R16_UNORM          = 16,
    IMAGE_FILE_DATA_FORMAT_R8_UNORM           = 17,
    IMAGE_FILE_DATA_FORMAT_BC1_UNORM          = 18,
    IMAGE_FILE_DATA_FORMAT_BC1_SRGB           = 19,
    IMAGE_FILE_DATA_FORMAT_BC2_UNORM          = 20,
    IMAGE_FILE_DATA_FORMAT_BC2_SRGB           = 21,
    IMAGE_FILE_DATA_FORMAT_BC3_UNORM          = 22,
    IMAGE_FILE_DATA_FORMAT_BC3_SRGB           = 23,
    IMAGE_FILE_DATA_FORMAT_BC4_UNORM          = 24,
    IMAGE_FILE_DATA_FORMAT_BC4_SNORM          = 25,
    IMAGE_FILE_DATA_FORMAT_BC5_UNORM          = 26,
    IMAGE_FILE_DATA_FORMAT_BC5_SNORM          = 27,
    IMAGE_FILE_DATA_FORMAT_BC6H_UF16          = 28,
    IMAGE_FILE_DATA_FORMAT_BC6H_SF16          = 29,
    IMAGE_FILE_DATA_FORMAT_BC7_UNORM          = 30,
    IMAGE_FILE_DATA_FORMAT_BC7_SRGB           = 31,
    IMAGE_FILE_DATA_FORMAT_ASTC_4x4_UNORM     = 32,
    IMAGE_FILE_DATA_FORMAT_ASTC_4x4_SRGB      = 33,
    IMAGE_FILE_DATA_FORMAT_ASTC_5x4_UNORM     = 34,
    IMAGE_FILE_DATA_FORMAT_ASTC_5x4_SRGB      = 35,
    IMAGE_FILE_DATA_FORMAT_ASTC_5x5_UNORM     = 36,
    IMAGE_FILE_DATA_FORMAT_ASTC_5x5_SRGB      = 37,
    IMAGE_FILE_DATA_FORMAT_ASTC_6x5_UNORM     = 38,
    IMAGE_FILE_DATA_FORMAT_ASTC_6x5_SRGB      = 39,
    IMAGE_FILE_DATA_FORMAT_ASTC_6x6_UNORM     = 40,
    IMAGE_FILE_DATA_FORMAT_ASTC_6x6_SRGB      = 41,
    IMAGE_FILE_DATA_FORMAT_ASTC_8x5_UNORM     = 42,
    IMAGE_FILE_DATA_FORMAT_ASTC_8x5_SRGB      = 43,
    IMAGE_FILE_DATA_FORMAT_ASTC_8x6_UNORM     = 44,
    IMAGE_FILE_DATA_FORMAT_ASTC_8x6_SRGB      = 45,
    IMAGE_FILE_DATA_FORMAT_ASTC_8x8_UNORM     = 46,
    IMAGE_FILE_DATA_FORMAT_ASTC_8x8_SRGB      = 47,
    IMAGE_FILE_DATA_FORMAT_ASTC_10x5_UNORM    = 48,
    IMAGE_FILE_DATA_FORMAT_ASTC_10x5_SRGB     = 49,
    IMAGE_FILE_DATA_FORMAT_ASTC_10x6_UNORM    = 50,
    IMAGE_FILE_DATA_FORMAT_ASTC_10x6_SRGB     = 51,
    IMAGE_FILE_DATA_FORMAT_ASTC_10x8_UNORM    = 52,
    IMAGE_FILE_DATA_FORMAT_ASTC_10x8_SRGB     = 53,
    IMAGE_FILE_DATA_FORMAT_ASTC_10x10_UNORM   = 54,
    IMAGE_FILE_DATA_FORMAT_ASTC_10x10_SRGB    = 55,
    IMAGE_FILE_DATA_FORMAT_ASTC_12x10_UNORM   = 56,
    IMAGE_FILE_DATA_FORMAT_ASTC_12x10_SRGB    = 57,
    IMAGE_FILE_DATA_FORMAT_ASTC_12x12_UNORM   = 58,
    IMAGE_FILE_DATA_FORMAT_ASTC_12x12_SRGB    = 59,
} ImageFileDataFormat;

typedef struct ImageFileSubresource
{
    uint32_t mip_level;
    uint32_t array_layer;
} ImageFileSubresource;

typedef struct ImageFile
{
	uint32_t width;   // in pixels
	uint32_t height;  // in pixels
	uint32_t depth;   // in pixels
	uint32_t mip_levels;
	uint32_t array_layers; // If flags.CUBE is set, this counts the number of cube faces, not whole cubes.
	uint32_t row_pitch_bytes;
	uint32_t depth_pitch_bytes;
	ImageFileType file_type;
	ImageFileFlags flags;
	ImageFileDataFormat data_format;
    void *file_contents;  // NOTE: may not include the entire file; headers may be stripped, etc.
} ImageFile;

int ImageFileCreate(ImageFile *out_image, const char *image_path);
void ImageFileDestroy(const ImageFile *image);
size_t ImageFileGetSubresourceSize(const ImageFile *image, const ImageFileSubresource subresource);
void *ImageFileGetSubresourceData(const ImageFile *image, const ImageFileSubresource subresource);

struct VkImageCreateInfo; // from vulkan.h
void ImageFileToVkImageCreateInfo(VkImageCreateInfo *out_ci, const ImageFile *image);

#endif // IMAGE_FILE_H