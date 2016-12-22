#include "image_file.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC // declare all public symbols as static
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_TGA
#define STBI_ONLY_BMP
#ifdef _MSC_VER
#   pragma warning(push)
#   pragma warning(disable:4244) // conversion from int to uint16_t
#endif
#include "stb_image.h"
#ifdef _MSC_VER
#   pragma warning(pop)
// Warning C4505 (unreferenced function removed) can not be suppressed, by design.
// When STB_IMAGE_STATIC is defined, you get a ton of them. Sorry!
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define IMAGEFILE__MIN(a, b) ((a)<(b) ? (a) : (b))
#define IMAGEFILE__MAX(a, b) ((a)>(b) ? (a) : (b))

static int IsSubresourceValid(const ImageFile *image, const ImageFileSubresource subresource)
{
    return (subresource.mip_level >= 0 && subresource.mip_level < image->mip_levels &&
        subresource.array_layer >= 0 && subresource.array_layer < image->array_layers);
}

uint32_t ImageFileGetBytesPerTexelBlock(ImageFileDataFormat format)
{
    switch(format)
    {
    case IMAGE_FILE_DATA_FORMAT_UNKNOWN:            return 0;
    case IMAGE_FILE_DATA_FORMAT_R8G8B8_UNORM:       return 3;
    case IMAGE_FILE_DATA_FORMAT_R8G8B8A8_UNORM:     return 4;
    case IMAGE_FILE_DATA_FORMAT_B8G8R8_UNORM:       return 3;
    case IMAGE_FILE_DATA_FORMAT_B8G8R8A8_UNORM:     return 4;
    case IMAGE_FILE_DATA_FORMAT_R4G4B4A4_UNORM:     return 2;
    case IMAGE_FILE_DATA_FORMAT_B4G4R4A4_UNORM:     return 2;
    case IMAGE_FILE_DATA_FORMAT_R32G32B32A32_FLOAT: return 16;
    case IMAGE_FILE_DATA_FORMAT_R32G32B32_FLOAT:    return 12;
    case IMAGE_FILE_DATA_FORMAT_R32G32_FLOAT:       return 8;
    case IMAGE_FILE_DATA_FORMAT_R32_FLOAT:          return 4;
    case IMAGE_FILE_DATA_FORMAT_R16G16B16A16_FLOAT: return 8;
    case IMAGE_FILE_DATA_FORMAT_R16G16B16A16_UNORM: return 8;
    case IMAGE_FILE_DATA_FORMAT_R16G16_FLOAT:       return 4;
    case IMAGE_FILE_DATA_FORMAT_R16G16_UNORM:       return 4;
    case IMAGE_FILE_DATA_FORMAT_R16_FLOAT:          return 2;
    case IMAGE_FILE_DATA_FORMAT_R16_UNORM:          return 2;
    case IMAGE_FILE_DATA_FORMAT_R8_UNORM:           return 1;
    case IMAGE_FILE_DATA_FORMAT_BC1_UNORM:          return 8;
    case IMAGE_FILE_DATA_FORMAT_BC1_SRGB:           return 8;
    case IMAGE_FILE_DATA_FORMAT_BC2_UNORM:          return 16;
    case IMAGE_FILE_DATA_FORMAT_BC2_SRGB:           return 16;
    case IMAGE_FILE_DATA_FORMAT_BC3_UNORM:          return 16;
    case IMAGE_FILE_DATA_FORMAT_BC3_SRGB:           return 16;
    case IMAGE_FILE_DATA_FORMAT_BC4_UNORM:          return 8;
    case IMAGE_FILE_DATA_FORMAT_BC4_SNORM:          return 8;
    case IMAGE_FILE_DATA_FORMAT_BC5_UNORM:          return 16;
    case IMAGE_FILE_DATA_FORMAT_BC5_SNORM:          return 16;
    case IMAGE_FILE_DATA_FORMAT_BC6H_UF16:          return 16;
    case IMAGE_FILE_DATA_FORMAT_BC6H_SF16:          return 16;
    case IMAGE_FILE_DATA_FORMAT_BC7_UNORM:          return 16;
    case IMAGE_FILE_DATA_FORMAT_BC7_SRGB:           return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_4x4_UNORM:     return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_4x4_SRGB:      return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x4_UNORM:     return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x4_SRGB:      return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x5_UNORM:     return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x5_SRGB:      return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x5_UNORM:     return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x5_SRGB:      return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x6_UNORM:     return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x6_SRGB:      return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x5_UNORM:     return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x5_SRGB:      return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x6_UNORM:     return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x6_SRGB:      return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x8_UNORM:     return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x8_SRGB:      return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x5_UNORM:    return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x5_SRGB:     return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x6_UNORM:    return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x6_SRGB:     return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x8_UNORM:    return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x8_SRGB:     return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x10_UNORM:   return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x10_SRGB:    return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x10_UNORM:   return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x10_SRGB:    return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x12_UNORM:   return 16;
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x12_SRGB:    return 16;
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8_UNORM:  return 16;
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8_SRGB:   return 16;
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A1_UNORM:return 16;
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A1_SRGB: return 16;
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A8_UNORM:return 16;
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A8_SRGB: return 16;
    case IMAGE_FILE_DATA_FORMAT_EAC_R11_UNORM:      return 16;
    case IMAGE_FILE_DATA_FORMAT_EAC_R11_SNORM:      return 16;
    case IMAGE_FILE_DATA_FORMAT_EAC_R11G11_UNORM:   return 16;
    case IMAGE_FILE_DATA_FORMAT_EAC_R11G11_SNORM:   return 16;

    // no default case here, to get warnings when new formats are added!
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int LoadImageFromStb(ImageFile *out_image, const char *image_path, ImageFileType file_type)
{
    int img_x = 0, img_y = 0, img_comp = 0;
    stbi_uc *pixels = stbi_load(image_path, &img_x, &img_y, &img_comp, 4);  // force RGBA
    if (pixels == NULL)
    {
        return -3;  // Image load error -- corrupt file, unsupported features, etc.
    }
    out_image->width = img_x;
    out_image->height = img_y;
    out_image->depth = 1;
    out_image->mip_levels = 1;
    out_image->array_layers = 1;
    out_image->row_pitch_bytes = out_image->width * 4*sizeof(stbi_uc);
    out_image->depth_pitch_bytes = out_image->row_pitch_bytes * out_image->height;
    out_image->file_type = file_type;
    out_image->flags = (ImageFileFlags)0;
    out_image->data_format = IMAGE_FILE_DATA_FORMAT_R8G8B8A8_UNORM;
    out_image->file_contents = (void*)pixels;
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum DdsHeaderFlag
{
    HEADER_FLAGS_CAPS        = 0x00000001,
    HEADER_FLAGS_HEIGHT      = 0x00000002,
    HEADER_FLAGS_WIDTH       = 0x00000004,
    HEADER_FLAGS_PITCH       = 0x00000008,
    HEADER_FLAGS_PIXELFORMAT = 0x00001000,
    HEADER_FLAGS_LINEARSIZE  = 0x00080000,
    HEADER_FLAGS_DEPTH       = 0x00800000,
    HEADER_FLAGS_TEXTURE     = 0x00001007,  // CAPS | HEIGHT | WIDTH | PIXELFORMAT
    HEADER_FLAGS_MIPMAP      = 0x00020000,
} DdsHeaderFlag;

typedef enum DdsSurfaceFlags
{
    SURFACE_FLAGS_TEXTURE = 0x00001000, // HEADER_FLAGS_TEXTURE
    SURFACE_FLAGS_MIPMAP  = 0x00400008, // COMPLEX | MIPMAP
    SURFACE_FLAGS_COMPLEX = 0x00000008, // COMPLEX
} DdsSurfaceFlags;

typedef enum DdsCubemapFlags
{
    CUBEMAP_FLAG_ISCUBEMAP = 0x00000200, // CUBEMAP
    CUBEMAP_FLAG_POSITIVEX = 0x00000600, // CUBEMAP | POSITIVEX
    CUBEMAP_FLAG_NEGATIVEX = 0x00000a00, // CUBEMAP | NEGATIVEX
    CUBEMAP_FLAG_POSITIVEY = 0x00001200, // CUBEMAP | POSITIVEY
    CUBEMAP_FLAG_NEGATIVEY = 0x00002200, // CUBEMAP | NEGATIVEY
    CUBEMAP_FLAG_POSITIVEZ = 0x00004200, // CUBEMAP | POSITIVEZ
    CUBEMAP_FLAG_NEGATIVEZ = 0x00008200, // CUBEMAP | NEGATIVEZ
    CUBEMAP_FLAG_VOLUME    = 0x00200000, // VOLUME
} DdsCubemapFlags;

typedef enum DdsDimensions
{
    DIMENSIONS_UNKNOWN   = 0,
    DIMENSIONS_BUFFER    = 1,
    DIMENSIONS_TEXTURE1D = 2,
    DIMENSIONS_TEXTURE2D = 3,
    DIMENSIONS_TEXTURE3D = 4,
} DdsDimensions;

typedef enum DxFormat
{
    DX_FORMAT_UNKNOWN                     = 0,
    DX_FORMAT_R32G32B32A32_TYPELESS       = 1,
    DX_FORMAT_R32G32B32A32_FLOAT          = 2,
    DX_FORMAT_R32G32B32A32_UINT           = 3,
    DX_FORMAT_R32G32B32A32_SINT           = 4,
    DX_FORMAT_R32G32B32_TYPELESS          = 5,
    DX_FORMAT_R32G32B32_FLOAT             = 6,
    DX_FORMAT_R32G32B32_UINT              = 7,
    DX_FORMAT_R32G32B32_SINT              = 8,
    DX_FORMAT_R16G16B16A16_TYPELESS       = 9,
    DX_FORMAT_R16G16B16A16_FLOAT          = 10,
    DX_FORMAT_R16G16B16A16_UNORM          = 11,
    DX_FORMAT_R16G16B16A16_UINT           = 12,
    DX_FORMAT_R16G16B16A16_SNORM          = 13,
    DX_FORMAT_R16G16B16A16_SINT           = 14,
    DX_FORMAT_R32G32_TYPELESS             = 15,
    DX_FORMAT_R32G32_FLOAT                = 16,
    DX_FORMAT_R32G32_UINT                 = 17,
    DX_FORMAT_R32G32_SINT                 = 18,
    DX_FORMAT_R32G8X24_TYPELESS           = 19,
    DX_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
    DX_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
    DX_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
    DX_FORMAT_R10G10B10A2_TYPELESS        = 23,
    DX_FORMAT_R10G10B10A2_UNORM           = 24,
    DX_FORMAT_R10G10B10A2_UINT            = 25,
    DX_FORMAT_R11G11B10_FLOAT             = 26,
    DX_FORMAT_R8G8B8A8_TYPELESS           = 27,
    DX_FORMAT_R8G8B8A8_UNORM              = 28,
    DX_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
    DX_FORMAT_R8G8B8A8_UINT               = 30,
    DX_FORMAT_R8G8B8A8_SNORM              = 31,
    DX_FORMAT_R8G8B8A8_SINT               = 32,
    DX_FORMAT_R16G16_TYPELESS             = 33,
    DX_FORMAT_R16G16_FLOAT                = 34,
    DX_FORMAT_R16G16_UNORM                = 35,
    DX_FORMAT_R16G16_UINT                 = 36,
    DX_FORMAT_R16G16_SNORM                = 37,
    DX_FORMAT_R16G16_SINT                 = 38,
    DX_FORMAT_R32_TYPELESS                = 39,
    DX_FORMAT_D32_FLOAT                   = 40,
    DX_FORMAT_R32_FLOAT                   = 41,
    DX_FORMAT_R32_UINT                    = 42,
    DX_FORMAT_R32_SINT                    = 43,
    DX_FORMAT_R24G8_TYPELESS              = 44,
    DX_FORMAT_D24_UNORM_S8_UINT           = 45,
    DX_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
    DX_FORMAT_X24_TYPELESS_G8_UINT        = 47,
    DX_FORMAT_R8G8_TYPELESS               = 48,
    DX_FORMAT_R8G8_UNORM                  = 49,
    DX_FORMAT_R8G8_UINT                   = 50,
    DX_FORMAT_R8G8_SNORM                  = 51,
    DX_FORMAT_R8G8_SINT                   = 52,
    DX_FORMAT_R16_TYPELESS                = 53,
    DX_FORMAT_R16_FLOAT                   = 54,
    DX_FORMAT_D16_UNORM                   = 55,
    DX_FORMAT_R16_UNORM                   = 56,
    DX_FORMAT_R16_UINT                    = 57,
    DX_FORMAT_R16_SNORM                   = 58,
    DX_FORMAT_R16_SINT                    = 59,
    DX_FORMAT_R8_TYPELESS                 = 60,
    DX_FORMAT_R8_UNORM                    = 61,
    DX_FORMAT_R8_UINT                     = 62,
    DX_FORMAT_R8_SNORM                    = 63,
    DX_FORMAT_R8_SINT                     = 64,
    DX_FORMAT_A8_UNORM                    = 65,
    DX_FORMAT_R1_UNORM                    = 66,
    DX_FORMAT_R9G9B9E5_SHAREDEXP          = 67,
    DX_FORMAT_R8G8_B8G8_UNORM             = 68,
    DX_FORMAT_G8R8_G8B8_UNORM             = 69,
    DX_FORMAT_BC1_TYPELESS                = 70,
    DX_FORMAT_BC1_UNORM                   = 71,
    DX_FORMAT_BC1_UNORM_SRGB              = 72,
    DX_FORMAT_BC2_TYPELESS                = 73,
    DX_FORMAT_BC2_UNORM                   = 74,
    DX_FORMAT_BC2_UNORM_SRGB              = 75,
    DX_FORMAT_BC3_TYPELESS                = 76,
    DX_FORMAT_BC3_UNORM                   = 77,
    DX_FORMAT_BC3_UNORM_SRGB              = 78,
    DX_FORMAT_BC4_TYPELESS                = 79,
    DX_FORMAT_BC4_UNORM                   = 80,
    DX_FORMAT_BC4_SNORM                   = 81,
    DX_FORMAT_BC5_TYPELESS                = 82,
    DX_FORMAT_BC5_UNORM                   = 83,
    DX_FORMAT_BC5_SNORM                   = 84,
    DX_FORMAT_B5G6R5_UNORM                = 85,
    DX_FORMAT_B5G5R5A1_UNORM              = 86,
    DX_FORMAT_B8G8R8A8_UNORM              = 87,
    DX_FORMAT_B8G8R8X8_UNORM              = 88,
    DX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
    DX_FORMAT_B8G8R8A8_TYPELESS           = 90,
    DX_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
    DX_FORMAT_B8G8R8X8_TYPELESS           = 92,
    DX_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,
    DX_FORMAT_BC6H_TYPELESS               = 94,
    DX_FORMAT_BC6H_UF16                   = 95,
    DX_FORMAT_BC6H_SF16                   = 96,
    DX_FORMAT_BC7_TYPELESS                = 97,
    DX_FORMAT_BC7_UNORM                   = 98,
    DX_FORMAT_BC7_UNORM_SRGB              = 99,
} DxFormat;

typedef struct DdsPixelFormat
{
    uint32_t structSize;
    uint32_t flags;
    uint32_t code4;
    uint32_t numBitsRGB;
    uint32_t maskR;
    uint32_t maskG;
    uint32_t maskB;
    uint32_t maskA;
} DdsPixelFormat;

typedef enum DdsPixelFormatFlags
{
    PF_FLAGS_CODE4     = 0x00000004,  // DDPF_FOURCC
    PF_FLAGS_RGB       = 0x00000040,  // DDPF_RGB
    PF_FLAGS_RGBA      = 0x00000041,  // DDPF_RGB | DDPF_ALPHAPIXELS
    PF_FLAGS_LUMINANCE = 0x00020000,  // DDPF_LUMINANCE
    PF_FLAGS_ALPHA     = 0x00000002,  // DDPF_ALPHA
} DdsPixelFormatFlags;

typedef struct DdsHeader
{
    uint32_t structSize;
    DdsHeaderFlag flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth; // only if HEADER_FLAGS_VOLUME is set in flags
    uint32_t mipCount;
    uint32_t unused1[11];
    DdsPixelFormat pixelFormat;
    uint32_t caps;
    uint32_t caps2;
    uint32_t unused2[3];
} DdsHeader;

typedef struct DdsHeader10
{
    DxFormat dxgiFormat;
    DdsDimensions resourceDimension;
    uint32_t flag;
    uint32_t arraySize;
    uint32_t unused;
} DdsHeader10;

static uint32_t DdsMakeCode4(char c0, char c1, char c2, char c3)
{
    return
        ((uint32_t)(uint8_t)(c0) <<  0) |
        ((uint32_t)(uint8_t)(c1) <<  8) |
        ((uint32_t)(uint8_t)(c2) << 16) |
        ((uint32_t)(uint8_t)(c3) << 24);
}

static int DdsContainsCompressedTexture(ImageFileDataFormat format)
{
    switch(format)
    {
    case IMAGE_FILE_DATA_FORMAT_BC1_UNORM:
    case IMAGE_FILE_DATA_FORMAT_BC1_SRGB:
    case IMAGE_FILE_DATA_FORMAT_BC2_UNORM:
    case IMAGE_FILE_DATA_FORMAT_BC2_SRGB:
    case IMAGE_FILE_DATA_FORMAT_BC3_UNORM:
    case IMAGE_FILE_DATA_FORMAT_BC3_SRGB:
    case IMAGE_FILE_DATA_FORMAT_BC4_UNORM:
    case IMAGE_FILE_DATA_FORMAT_BC4_SNORM:
    case IMAGE_FILE_DATA_FORMAT_BC5_UNORM:
    case IMAGE_FILE_DATA_FORMAT_BC5_SNORM:
    case IMAGE_FILE_DATA_FORMAT_BC6H_UF16:
    case IMAGE_FILE_DATA_FORMAT_BC6H_SF16:
    case IMAGE_FILE_DATA_FORMAT_BC7_UNORM:
    case IMAGE_FILE_DATA_FORMAT_BC7_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_4x4_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_4x4_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x4_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x4_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x5_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x5_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x5_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x5_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x6_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x6_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x5_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x5_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x6_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x6_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x8_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x8_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x5_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x5_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x6_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x6_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x8_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x8_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x10_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x10_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x10_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x10_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x12_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x12_SRGB:
        return 1;
    case IMAGE_FILE_DATA_FORMAT_UNKNOWN:
    case IMAGE_FILE_DATA_FORMAT_R8G8B8_UNORM:
    case IMAGE_FILE_DATA_FORMAT_R8G8B8A8_UNORM:
    case IMAGE_FILE_DATA_FORMAT_B8G8R8_UNORM:
    case IMAGE_FILE_DATA_FORMAT_B8G8R8A8_UNORM:
    case IMAGE_FILE_DATA_FORMAT_R4G4B4A4_UNORM:
    case IMAGE_FILE_DATA_FORMAT_B4G4R4A4_UNORM:
    case IMAGE_FILE_DATA_FORMAT_R32G32B32A32_FLOAT:
    case IMAGE_FILE_DATA_FORMAT_R32G32B32_FLOAT:
    case IMAGE_FILE_DATA_FORMAT_R32G32_FLOAT:
    case IMAGE_FILE_DATA_FORMAT_R32_FLOAT:
    case IMAGE_FILE_DATA_FORMAT_R16G16B16A16_FLOAT:
    case IMAGE_FILE_DATA_FORMAT_R16G16B16A16_UNORM:
    case IMAGE_FILE_DATA_FORMAT_R16G16_FLOAT:
    case IMAGE_FILE_DATA_FORMAT_R16G16_UNORM:
    case IMAGE_FILE_DATA_FORMAT_R16_FLOAT:
    case IMAGE_FILE_DATA_FORMAT_R16_UNORM:
    case IMAGE_FILE_DATA_FORMAT_R8_UNORM:
        return 0;
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A1_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A1_SRGB:
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A8_UNORM:
    case IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A8_SRGB:
    case IMAGE_FILE_DATA_FORMAT_EAC_R11_UNORM:
    case IMAGE_FILE_DATA_FORMAT_EAC_R11_SNORM:
    case IMAGE_FILE_DATA_FORMAT_EAC_R11G11_UNORM:
    case IMAGE_FILE_DATA_FORMAT_EAC_R11G11_SNORM:
        return 1; // ...but DDS files can't contain these formats, I think
    // no default case here, to get warnings when new formats are added!
    }
    return 0;
}

static int DdsIsPfMask(const DdsPixelFormat pf, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    return (pf.maskR == r && pf.maskG == g && pf.maskB == b && pf.maskA == a);
}
static ImageFileDataFormat DdsParsePixelFormat(const DdsPixelFormat pf)
{
    if( pf.flags & PF_FLAGS_RGBA )
    {
        switch (pf.numBitsRGB)
        {
        case 32:
            if( DdsIsPfMask(pf, 0x00ff0000,0x0000ff00,0x000000ff,0xff000000) ) // BGRA
                return IMAGE_FILE_DATA_FORMAT_B8G8R8A8_UNORM;
            else if( DdsIsPfMask(pf, 0x00ff0000,0x0000ff00,0x000000ff,0x00000000) ) // BGRX
                return IMAGE_FILE_DATA_FORMAT_B8G8R8A8_UNORM;
            else if( DdsIsPfMask(pf, 0x000000ff,0x0000ff00,0x00ff0000,0xff000000) )
                return IMAGE_FILE_DATA_FORMAT_R8G8B8A8_UNORM;
            else if( DdsIsPfMask(pf, 0x000000ff,0x0000ff00,0x00ff0000,0x00000000) )
                return IMAGE_FILE_DATA_FORMAT_R8G8B8A8_UNORM;
            else if( DdsIsPfMask(pf, 0x0000ffff,0xffff0000,0x00000000,0x00000000) )
                return IMAGE_FILE_DATA_FORMAT_R16G16_UNORM;
            break;
        case 24:
            if( DdsIsPfMask(pf, 0x00ff0000,0x0000ff00,0x000000ff,0x00000000) )
                return IMAGE_FILE_DATA_FORMAT_R8G8B8_UNORM;
            else if( DdsIsPfMask(pf, 0x000000ff,0x0000ff00,0x00ff0000,0x00000000) )
                return IMAGE_FILE_DATA_FORMAT_B8G8R8_UNORM;
            break;
        case 16:
            if( DdsIsPfMask(pf, 0x00000f00,0x000000f0,0x0000000f,0x0000f000) )
                return IMAGE_FILE_DATA_FORMAT_R4G4B4A4_UNORM;
            else if( DdsIsPfMask(pf, 0x00000f00,0x000000f0,0x0000000f,0x00000000) )
                return IMAGE_FILE_DATA_FORMAT_B4G4R4A4_UNORM;
            break;

        case 8:
            if( DdsIsPfMask(pf, 0x000000ff,0x00000000,0x00000000,0x00000000) )
                return IMAGE_FILE_DATA_FORMAT_R8_UNORM;
            break;
        }
    }
    else if( pf.flags & PF_FLAGS_LUMINANCE )
    {
        switch(pf.numBitsRGB)
        {
        case 8:
            if( DdsIsPfMask(pf, 0x000000ff,0x00000000,0x00000000,0x00000000) ) // L8
                return IMAGE_FILE_DATA_FORMAT_R8_UNORM;
            break;
        case 16:
            if( DdsIsPfMask(pf, 0x0000ffff,0x00000000,0x00000000,0x00000000) ) // L16
                return IMAGE_FILE_DATA_FORMAT_R16_UNORM;
            break;
        }
    }
    else if( pf.flags & PF_FLAGS_ALPHA )
    {
        // Not curently supported
    }
    else if( pf.flags & PF_FLAGS_CODE4 )
    {
        if(      DdsMakeCode4( 'D', 'X', 'T', '1' ) == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_BC1_UNORM;
        else if( DdsMakeCode4( 'D', 'X', 'T', '2' ) == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_BC2_UNORM;
        else if( DdsMakeCode4( 'D', 'X', 'T', '3' ) == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_BC2_UNORM;
        else if( DdsMakeCode4( 'D', 'X', 'T', '4' ) == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_BC3_UNORM;
        else if( DdsMakeCode4( 'D', 'X', 'T', '5' ) == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_BC3_UNORM;
        else if( DdsMakeCode4( 'B', 'C', '4', 'U' ) == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_BC4_UNORM;
        else if( DdsMakeCode4( 'B', 'C', '4', 'S' ) == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_BC4_SNORM;
        else if( DdsMakeCode4( 'B', 'C', '5', 'U' ) == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_BC5_UNORM;
        else if( DdsMakeCode4( 'B', 'C', '5', 'S' ) == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_BC5_SNORM;
        // Certain values are hard-coded into the FourCC field for specific formats
        else if ( 111 == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_R16_FLOAT;
        else if ( 112 == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_R16G16_FLOAT;
        else if ( 113 == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_R16G16B16A16_FLOAT;
        else if ( 114 == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_R32_FLOAT;
        else if ( 115 == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_R32G32_FLOAT;
        else if ( 116 == pf.code4 )
            return IMAGE_FILE_DATA_FORMAT_R32G32B32A32_FLOAT;
    }
    // If we get here, the format wasn't recognized.
    return IMAGE_FILE_DATA_FORMAT_UNKNOWN;
}

static ImageFileDataFormat DdsParseDxFormat(DxFormat dx_format)
{
    switch(dx_format)
    {
    case DX_FORMAT_R32G32B32A32_FLOAT:
        return IMAGE_FILE_DATA_FORMAT_R32G32B32A32_FLOAT;
    case DX_FORMAT_R32G32B32_FLOAT:
        return IMAGE_FILE_DATA_FORMAT_R32G32B32_FLOAT;
    case DX_FORMAT_R16G16B16A16_FLOAT:
        return IMAGE_FILE_DATA_FORMAT_R16G16B16A16_FLOAT;
    case DX_FORMAT_R16G16B16A16_UNORM:
        return IMAGE_FILE_DATA_FORMAT_R16G16B16A16_UNORM;
    case DX_FORMAT_R32G32_FLOAT:
        return IMAGE_FILE_DATA_FORMAT_R32G32_FLOAT;
    case DX_FORMAT_R8G8B8A8_UNORM:
        return IMAGE_FILE_DATA_FORMAT_R8G8B8A8_UNORM;
    case DX_FORMAT_R16G16_FLOAT:
        return IMAGE_FILE_DATA_FORMAT_R16G16_FLOAT;
    case DX_FORMAT_R16G16_UNORM:
        return IMAGE_FILE_DATA_FORMAT_R16G16_UNORM;
    case DX_FORMAT_R32_FLOAT:
        return IMAGE_FILE_DATA_FORMAT_R32_FLOAT;
    case DX_FORMAT_R16_FLOAT:
        return IMAGE_FILE_DATA_FORMAT_R16_FLOAT;
    case DX_FORMAT_R16_UNORM:
        return IMAGE_FILE_DATA_FORMAT_R16_UNORM;
    case DX_FORMAT_R8_UNORM:
        return IMAGE_FILE_DATA_FORMAT_R8_UNORM;
    case DX_FORMAT_BC1_TYPELESS:
    case DX_FORMAT_BC1_UNORM:
        return IMAGE_FILE_DATA_FORMAT_BC1_UNORM;
    case DX_FORMAT_BC1_UNORM_SRGB:
        return IMAGE_FILE_DATA_FORMAT_BC1_SRGB;
    case DX_FORMAT_BC2_TYPELESS:
    case DX_FORMAT_BC2_UNORM:
        return IMAGE_FILE_DATA_FORMAT_BC2_UNORM;
    case DX_FORMAT_BC2_UNORM_SRGB:
        return IMAGE_FILE_DATA_FORMAT_BC2_SRGB;
    case DX_FORMAT_BC3_TYPELESS:
    case DX_FORMAT_BC3_UNORM:
        return IMAGE_FILE_DATA_FORMAT_BC3_UNORM;
    case DX_FORMAT_BC3_UNORM_SRGB:
        return IMAGE_FILE_DATA_FORMAT_BC3_SRGB;
    case DX_FORMAT_BC4_TYPELESS:
    case DX_FORMAT_BC4_UNORM:
        return IMAGE_FILE_DATA_FORMAT_BC4_UNORM;
    case DX_FORMAT_BC4_SNORM:
        return IMAGE_FILE_DATA_FORMAT_BC4_SNORM;
    case DX_FORMAT_BC5_TYPELESS:
    case DX_FORMAT_BC5_UNORM:
        return IMAGE_FILE_DATA_FORMAT_BC5_UNORM;
    case DX_FORMAT_BC5_SNORM:
        return IMAGE_FILE_DATA_FORMAT_BC5_SNORM;
    case DX_FORMAT_BC6H_UF16:
        return IMAGE_FILE_DATA_FORMAT_BC6H_UF16;
    case DX_FORMAT_BC6H_SF16:
        return IMAGE_FILE_DATA_FORMAT_BC6H_SF16;
    case DX_FORMAT_BC7_UNORM:
        return IMAGE_FILE_DATA_FORMAT_BC7_UNORM;
    case DX_FORMAT_BC7_UNORM_SRGB:
        return IMAGE_FILE_DATA_FORMAT_BC7_SRGB;
    case DX_FORMAT_B8G8R8A8_UNORM:
    case DX_FORMAT_B8G8R8X8_UNORM:
        return IMAGE_FILE_DATA_FORMAT_B8G8R8A8_UNORM;
    case DX_FORMAT_UNKNOWN:
    case DX_FORMAT_R32G32B32A32_TYPELESS:
    case DX_FORMAT_R32G32B32_TYPELESS:
    case DX_FORMAT_R16G16B16A16_TYPELESS:
    case DX_FORMAT_R32G32_TYPELESS:
    case DX_FORMAT_R32G8X24_TYPELESS:
    case DX_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DX_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DX_FORMAT_R10G10B10A2_TYPELESS:
    case DX_FORMAT_R8G8B8A8_TYPELESS:
    case DX_FORMAT_R16G16_TYPELESS:
    case DX_FORMAT_R32_TYPELESS:
    case DX_FORMAT_R24G8_TYPELESS:
    case DX_FORMAT_R24_UNORM_X8_TYPELESS:
    case DX_FORMAT_X24_TYPELESS_G8_UINT:
    case DX_FORMAT_R8G8_TYPELESS:
    case DX_FORMAT_R16_TYPELESS:
    case DX_FORMAT_R8_TYPELESS:
    case DX_FORMAT_D32_FLOAT_S8X24_UINT:
    case DX_FORMAT_D24_UNORM_S8_UINT:
    case DX_FORMAT_R9G9B9E5_SHAREDEXP:
    case DX_FORMAT_R8G8_B8G8_UNORM:
    case DX_FORMAT_G8R8_G8B8_UNORM:
    case DX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DX_FORMAT_B8G8R8A8_TYPELESS:
    case DX_FORMAT_B8G8R8X8_TYPELESS:
    case DX_FORMAT_R1_UNORM:
    case DX_FORMAT_A8_UNORM:
    case DX_FORMAT_R32G32B32A32_UINT:
    case DX_FORMAT_R32G32B32A32_SINT:
    case DX_FORMAT_R32G32B32_UINT:
    case DX_FORMAT_R32G32B32_SINT:
    case DX_FORMAT_R16G16B16A16_UINT:
    case DX_FORMAT_R16G16B16A16_SNORM:
    case DX_FORMAT_R16G16B16A16_SINT:
    case DX_FORMAT_R32G32_UINT:
    case DX_FORMAT_R32G32_SINT:
    case DX_FORMAT_R10G10B10A2_UNORM:
    case DX_FORMAT_R10G10B10A2_UINT:
    case DX_FORMAT_R11G11B10_FLOAT:
    case DX_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DX_FORMAT_R8G8B8A8_UINT:
    case DX_FORMAT_R8G8B8A8_SNORM:
    case DX_FORMAT_R8G8B8A8_SINT:
    case DX_FORMAT_R16G16_UINT:
    case DX_FORMAT_R16G16_SNORM:
    case DX_FORMAT_R16G16_SINT:
    case DX_FORMAT_D32_FLOAT:
    case DX_FORMAT_R32_UINT:
    case DX_FORMAT_R32_SINT:
    case DX_FORMAT_R8G8_UNORM:
    case DX_FORMAT_R8G8_UINT:
    case DX_FORMAT_R8G8_SNORM:
    case DX_FORMAT_R8G8_SINT:
    case DX_FORMAT_D16_UNORM:
    case DX_FORMAT_R16_UINT:
    case DX_FORMAT_R16_SNORM:
    case DX_FORMAT_R16_SINT:
    case DX_FORMAT_R8_UINT:
    case DX_FORMAT_R8_SNORM:
    case DX_FORMAT_R8_SINT:
    case DX_FORMAT_B5G6R5_UNORM:
    case DX_FORMAT_B5G5R5A1_UNORM:
    case DX_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DX_FORMAT_B8G8R8X8_UNORM_SRGB:
        return IMAGE_FILE_DATA_FORMAT_UNKNOWN;
    }
    return IMAGE_FILE_DATA_FORMAT_UNKNOWN;
}

static int LoadImageFromDds(ImageFile *out_image, const char *image_path)
{
    FILE *dds_file = fopen(image_path, "rb");
    if (!dds_file)
        return -3;  // Couldn't open file for reading
    fseek(dds_file, 0, SEEK_END);
    size_t dds_file_size = ftell(dds_file);
    fseek(dds_file, 0, SEEK_SET);
    uint8_t *dds_bytes = (uint8_t*)malloc(dds_file_size);
    size_t read_size = fread(dds_bytes, dds_file_size, 1, dds_file);
    fclose(dds_file);
    if (read_size != 1)
    {
        free(dds_bytes);
        return -4;  // Couldn't read file contents
    }

        // Check magic number and header validity
    const uint32_t *magic = (const uint32_t*)dds_bytes;
    const uint32_t kDdsPrefixMagic = 0x20534444;
    if (*magic != kDdsPrefixMagic)
    {
        free(dds_bytes);
        return -4; // Incorrect magic number
    }
    const DdsHeader *header = (const DdsHeader*)(dds_bytes + sizeof(uint32_t));
    if (header->structSize != sizeof(DdsHeader) || header->pixelFormat.structSize != sizeof(DdsPixelFormat))
    {
        free(dds_bytes);
        return -5; // Incorrect header size
    }
    if ((header->flags & (HEADER_FLAGS_WIDTH | HEADER_FLAGS_HEIGHT)) != (HEADER_FLAGS_WIDTH | HEADER_FLAGS_HEIGHT))
    {
        // technically DDSD_CAPS and DDSD_PIXELFORMAT are required as well, but their absence is so widespread that they can't be relied upon.
        free(dds_bytes);
        return -6; // Required flag is missing from header
    }

    // Note according to msdn:  when you read a .dds file, you should not rely on the DDSCAPS_TEXTURE
    //	and DDSCAPS_COMPLEX flags being set because some writers of such a file might not set these flags.
    //if ((header->caps & SURFACE_FLAGS_TEXTURE) == 0)
    //{
    //	free(ddsFileData);
    //	return -7; // Required flag is missing from header
    //}
    uint32_t pixel_offset = sizeof(uint32_t) + sizeof(DdsHeader);

    // Check for DX10 header
    const DdsHeader10 *header10 = NULL;
    if ( (header->pixelFormat.flags & PF_FLAGS_CODE4) && (DdsMakeCode4( 'D', 'X', '1', '0' ) == header->pixelFormat.code4) )
    {
        // Must be long enough for both headers and magic value
        if( dds_file_size < (sizeof(DdsHeader)+sizeof(uint32_t)+sizeof(DdsHeader10)) )
        {
            free(dds_bytes);
            return -8; // File too small to contain a valid DX10 DDS
        }
        header10 = (const DdsHeader10*)(dds_bytes + sizeof(uint32_t) + sizeof(DdsHeader));
        pixel_offset += sizeof(DdsHeader10);
    }

    // Check if the contents are a cubemap.  If so, all six faces must be present.
    int is_cube_map = 0;
    if ((header->caps & SURFACE_FLAGS_COMPLEX) && (header->caps2 & CUBEMAP_FLAG_ISCUBEMAP))
    {
        const uint32_t kCubemapFlagAllFaces =
            CUBEMAP_FLAG_ISCUBEMAP |
            CUBEMAP_FLAG_POSITIVEX | CUBEMAP_FLAG_NEGATIVEX |
            CUBEMAP_FLAG_POSITIVEY | CUBEMAP_FLAG_NEGATIVEY |
            CUBEMAP_FLAG_POSITIVEZ | CUBEMAP_FLAG_NEGATIVEZ;
        if ((header->caps2 & kCubemapFlagAllFaces) != kCubemapFlagAllFaces)
        {
            free(dds_bytes);
            return -9; // The cubemap is missing one or more faces.
        }
        is_cube_map = 1;
    }

    // Check if the contents are a volume texture.
    int is_volume_texture = 0;
    if ((header->flags & HEADER_FLAGS_DEPTH) && (header->caps2 & CUBEMAP_FLAG_VOLUME)) // (header->dwCaps & SURFACE_FLAGS_COMPLEX) -- doesn't always seem to be set?
    {
        if (header->depth == 0)
        {
            free(dds_bytes);
            return -10; // The file is marked as a volume texture, but depth is <1
        }
        is_volume_texture = 1;
    }

    uint32_t mipMapCount = 1;
    if ((header->flags & HEADER_FLAGS_MIPMAP) == HEADER_FLAGS_MIPMAP)
    {
        mipMapCount = header->mipCount;
    }

    ImageFileDataFormat data_format = IMAGE_FILE_DATA_FORMAT_UNKNOWN;
    if (header10 != NULL)
    {
        data_format = DdsParseDxFormat(header10->dxgiFormat);
    }
    else
    {
        data_format = DdsParsePixelFormat(header->pixelFormat);
    }
    if (data_format == IMAGE_FILE_DATA_FORMAT_UNKNOWN)
    {
        free(dds_bytes);
        return -11; // It is either unknown or unsupported format
    }

    uint32_t bytes_per_texel_block = (uint32_t)ImageFileGetBytesPerTexelBlock(data_format);
    int is_compressed = DdsContainsCompressedTexture(data_format);

    out_image->width = header->width;
    out_image->height = header->height;
    out_image->depth = is_volume_texture ? header->depth : 1;
    out_image->mip_levels = mipMapCount;
    out_image->array_layers = header10 ? header10->arraySize : 1;
    if (is_cube_map)
    {
        out_image->array_layers *= 6;  // ImageFile counts individual faces as layers.
    }
    // Official DDS specs on MSDN suggest that the pitchOrLinearSize field can not be trusted.
    // and recommend the following computation for pitch.
    out_image->row_pitch_bytes = is_compressed ?
        (bytes_per_texel_block * IMAGEFILE__MAX(1U, (header->width+3U)/4U)) :
        (header->width * bytes_per_texel_block);
    out_image->depth_pitch_bytes = out_image->row_pitch_bytes * header->height;
    out_image->file_type = IMAGE_FILE_TYPE_DDS;
    out_image->flags = 0;
    if (is_cube_map)
    {
        out_image->flags |= IMAGE_FILE_FLAG_CUBE_BIT;
    }
    out_image->data_format = data_format;
    out_image->file_contents = dds_bytes;  // NOTE: includes header data
    
    *(uint32_t*)dds_bytes = pixel_offset; // overwrite magic number with offset to start of pixel data
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct AstcHeader
{
    uint8_t magic[4];
    uint8_t blockdim_x;
    uint8_t blockdim_y;
    uint8_t blockdim_z;
    uint8_t xsize[3];
    uint8_t ysize[3];
    uint8_t zsize[3];
} AstcHeader;

static int LoadImageFromAstc(ImageFile *out_image, const char *image_path)
{
    FILE *astc_file = fopen(image_path, "rb");
    if (!astc_file)
        return -2;  // Couldn't open file for reading
    fseek(astc_file, 0, SEEK_END);
    size_t astc_file_size = ftell(astc_file);
    fseek(astc_file, 0, SEEK_SET);
    uint8_t *astc_bytes = (uint8_t*)malloc(astc_file_size);
    if (!astc_bytes)
    {
      fclose(astc_file);
      return -3;
    }
    size_t read_size = fread(astc_bytes, astc_file_size, 1, astc_file);
    fclose(astc_file);
    if (read_size != 1)
        return -4;  // Couldn't read file contents

    const AstcHeader *header = (const AstcHeader*)astc_bytes;
    const uint32_t kMagic = 0x5CA1AB13;
    if (memcmp(&header->magic, &kMagic, sizeof(header->magic)) != 0)
    {
        free(astc_bytes);
        return -5;  // invalid magic number
    }
    if (header->blockdim_z != 1)
    {
        free(astc_bytes);
        return -6;  // This loader is not aware of any ASTC blocks with Z!=1
    }

    // Merge x,y,z-sizes from 3 chars into one integer value.
    uint32_t xsize = header->xsize[0] + (header->xsize[1] << 8) + (header->xsize[2] << 16);
    uint32_t ysize = header->ysize[0] + (header->ysize[1] << 8) + (header->ysize[2] << 16);
    uint32_t zsize = header->zsize[0] + (header->zsize[1] << 8) + (header->zsize[2] << 16);
    // Compute number of blocks in each direction.
    uint32_t xblocks = (xsize + header->blockdim_x - 1) / header->blockdim_x;
    uint32_t yblocks = (ysize + header->blockdim_y - 1) / header->blockdim_y;
    //uint32_t zblocks = (zsize + header->blockdim_z - 1) / header->blockdim_z;
    // Each block is encoded on 16 bytes, so calculate total compressed image data size.
    //uint32_t total_size = xblocks * yblocks * zblocks * 16;

    out_image->width = xsize;
    out_image->height = ysize;
    out_image->depth = zsize;
    out_image->mip_levels = 1;
    out_image->array_layers = 1;
    out_image->row_pitch_bytes = xblocks * 16;
    out_image->depth_pitch_bytes = xblocks * yblocks * 16;
    out_image->file_type = IMAGE_FILE_TYPE_ASTC;
    out_image->flags = 0;
    int is_srgb = 0; // TODO(cort): where would this come from?
#define ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT(dimx, dimy) \
    if (header->blockdim_x == (dimx) && header->blockdim_y == (dimy)) \
    out_image->data_format = is_srgb ? \
        IMAGE_FILE_DATA_FORMAT_ASTC_ ## dimx ## x ## dimy ## _SRGB : \
        IMAGE_FILE_DATA_FORMAT_ASTC_ ## dimx ## x ## dimy ## _UNORM

    if (header->blockdim_x == 0 && header->blockdim_y == 0)
        out_image->data_format = IMAGE_FILE_DATA_FORMAT_UNKNOWN;
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT( 4, 4);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT( 5, 4);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT( 5, 5);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT( 6, 5);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT( 6, 6);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT( 8, 5);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT( 8, 6);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT( 8, 8);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT(10, 5);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT(10, 6);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT(10, 8);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT(10,10);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT(12,10);
    ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT(12,12);
    else
        out_image->data_format = IMAGE_FILE_DATA_FORMAT_UNKNOWN;
#undef ELSE_IF_BLOCKDIM_THEN_SET_DATA_FORMAT
    out_image->file_contents = astc_bytes;  // NOTE: includes header data
    
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct KtxHeader {
    uint8_t identifier[12];
    uint32_t endianness;
    uint32_t glType;
    uint32_t glTypeSize;
    uint32_t glFormat;
    uint32_t glInternalFormat;
    uint32_t glBaseInternalFormat;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
    uint32_t pixelDepth;
    uint32_t numberOfArrayElements;
    uint32_t numberOfFaces;
    uint32_t numberOfMipmapLevels;
    uint32_t bytesOfKeyValueData;
} KtxHeader;

typedef enum GlInternalFormat {
    GL__COMPRESSED_RGB_S3TC_DXT1_EXT           = 0x83F0, // BC1 (no alpha)
    GL__COMPRESSED_RGBA_S3TC_DXT1_EXT          = 0x83F1, // BC1 (alpha)
    GL__COMPRESSED_RGBA_S3TC_DXT5_EXT          = 0x83F3, // BC3
    GL__COMPRESSED_RGBA_BPTC_UNORM_ARB         = 0x8E8C, // BC7
} GlInternalFormat;

static uint16_t byte_swap_u16(uint16_t in)
{
    return ((in & 0xFF) << 8) | ((in & 0xFF00) >> 8);
}
static uint32_t byte_swap_u32(uint32_t in)
{
    return ((in & 0xFF) << 24) | ((in & 0xFF00) << 8) | ((in & 0xFF0000) >> 8) | ((in & 0xFF000000) >> 24);
}
static uint64_t byte_swap_u64(uint64_t in)
{
    return ((in & 0xFFULL) << 56) | ((in & 0xFF00ULL) << 40) | ((in & 0xFF0000ULL) << 24) | ((in & 0xFF000000ULL) << 8)
        | ((in &     0xFF00000000ULL) >> 8)  | ((in &     0xFF0000000000ULL) >> 24)
        | ((in & 0xFF000000000000ULL) >> 40) | ((in & 0xFF00000000000000ULL) >> 56);
}
static void byte_swap_in_place(uint8_t *p, uint32_t nbytes)
{
    if (nbytes == 2) {
        uint16_t *p16 = (uint16_t*)p;
        *p16 = byte_swap_u16(*p16);
    } else if (nbytes == 4) {
        uint32_t *p32 = (uint32_t*)p;
        *p32 = byte_swap_u32(*p32);
    } else if (nbytes == 8) {
        uint64_t *p64 = (uint64_t*)p;
        *p64 = byte_swap_u64(*p64);
    } else {
        fprintf(stderr, "Unsupported nbytes (%u) in byte_swap_in_place()", nbytes);
        assert(0);
    }
}

static int LoadImageFromKtx(ImageFile *out_image, const char *image_path)
{
    FILE *ktx_file = fopen(image_path, "rb");
    if (!ktx_file)
        return -3;  // Couldn't open file for reading
    fseek(ktx_file, 0, SEEK_END);
    size_t ktx_file_size = ftell(ktx_file);
    fseek(ktx_file, 0, SEEK_SET);
    uint8_t *ktx_bytes = (uint8_t*)malloc(ktx_file_size);

    KtxHeader *header = (KtxHeader*)ktx_bytes;
    size_t read_size = fread(header, sizeof(*header), 1, ktx_file);
    if (read_size != 1)
    {
        fclose(ktx_file);
        free(ktx_bytes);
        return -1;
    }
    const uint8_t ktx_magic_id[12] =
    {
        0xAB, 0x4B, 0x54, 0x58,
        0x20, 0x31, 0x31, 0xBB,
        0x0D, 0x0A, 0x1A, 0x0A
    };
    if (memcmp(ktx_magic_id, header->identifier, 12) != 0)
    {
        fclose(ktx_file);
        free(ktx_bytes);
        return -2;
    }
    int byte_swap_contents = 0;
    if (header->endianness == 0x04030201)
    {
        // No byte swap necessary
    }
    else if (header->endianness == 0x01020304)
    {
        // Remaining fields must be byte-swapped. We swap the header in-place
        byte_swap_contents = 1;
        header->glType = byte_swap_u32(header->glType);
        header->glTypeSize = byte_swap_u32(header->glTypeSize);
        header->glFormat = byte_swap_u32(header->glFormat);
        header->glInternalFormat = byte_swap_u32(header->glInternalFormat);
        header->glBaseInternalFormat = byte_swap_u32(header->glBaseInternalFormat);
        header->pixelWidth = byte_swap_u32(header->pixelWidth);
        header->pixelHeight = byte_swap_u32(header->pixelHeight);
        header->pixelDepth = byte_swap_u32(header->pixelDepth);
        header->numberOfArrayElements = byte_swap_u32(header->numberOfArrayElements);
        header->numberOfFaces = byte_swap_u32(header->numberOfFaces);
        header->numberOfMipmapLevels = byte_swap_u32(header->numberOfMipmapLevels);
        header->bytesOfKeyValueData = byte_swap_u32(header->bytesOfKeyValueData);
    }
    else
    {
        fclose(ktx_file);
        free(ktx_bytes);
        return -3;
    }
    assert(byte_swap_contents == 0);

    // Read key/value pairs.
    if (header->bytesOfKeyValueData != 0)
    {
        uint8_t *key_value_bytes = ktx_bytes + sizeof(*header);
        const uint8_t *key_value_bytes_end = key_value_bytes + header->bytesOfKeyValueData;
        read_size = fread(key_value_bytes, header->bytesOfKeyValueData, 1, ktx_file);
        if (read_size != 1)
        {
            fclose(ktx_file);
            free(ktx_bytes);
            return -4;
        }
        const uint8_t *next_key_value = key_value_bytes;
        while(next_key_value != key_value_bytes_end)
        {
            const uint32_t *pair_size = (const uint32_t*)next_key_value;
            uint32_t final_pair_size = byte_swap_contents ? byte_swap_u32(*pair_size) : *pair_size;
            const uint8_t *key_and_value = next_key_value + sizeof(*pair_size);
            uint32_t padding_size = 3 - ((final_pair_size + 3) % 4);
            next_key_value += final_pair_size + padding_size;

            const uint8_t *key_utf8 = key_and_value;
            const uint8_t *value = (const uint8_t*)memchr(key_and_value, 0, final_pair_size);
            // TODO(cort): what to look for here?
            (void)key_utf8;
            (void)value;
        }
    }

    // Read surface data
    uint8_t *surface_data = ktx_bytes + sizeof(*header) + header->bytesOfKeyValueData;
    size_t surface_data_size = ktx_file_size - (sizeof(*header) + header->bytesOfKeyValueData);
    read_size = fread(surface_data, surface_data_size, 1, ktx_file);
    fclose(ktx_file);
    if (read_size != 1)
    {
        free(ktx_bytes);
        return -5;
    }
    if (header->endianness == 0x01020304)
    {
        uint8_t *surface_start = surface_data;
        for(uint32_t iMip=0; iMip < header->numberOfMipmapLevels; ++iMip)
        {
            uint32_t *image_size = (uint32_t*)surface_start;
            *image_size = byte_swap_u32(*image_size);
            uint32_t padded_image_size = (*image_size + 3) & ~3; // TODO: this is different for non-array cubemaps. Test it.
            uint8_t *image_start = (uint8_t*)(image_size+1);
            uint8_t *image_end = image_start + padded_image_size;
            if (header->glTypeSize != 1)
            {
                uint8_t *next_pixel = image_start;
                while (next_pixel + header->glTypeSize <= image_end)
                {
                    byte_swap_in_place(next_pixel, header->glTypeSize);
                    next_pixel += header->glTypeSize;
                }
            }
            surface_start = image_end;
        }
    }
    out_image->width = header->pixelWidth;
    out_image->height = IMAGEFILE__MAX(1, header->pixelHeight); // will be 0 for 1D textures
    out_image->depth = IMAGEFILE__MAX(1, header->pixelDepth); // will be 0 for 1D/2D textures
    out_image->mip_levels = IMAGEFILE__MAX(1, header->numberOfMipmapLevels); // 0 is a request to generate the full mip chain.
    // For basic (non-array/cubemap) textures, arrayElements=0 and faces=1.
    // For non-array cubemaps: arrayElements=0 and faces=6.
    // For non-cubemap arrays: arrayElements=#elements and faces=1.
    // For cubemap arrays: arrayElements=#cubes and faces=6.
    out_image->array_layers = IMAGEFILE__MAX(1, header->numberOfArrayElements) * header->numberOfFaces;
    out_image->file_type = IMAGE_FILE_TYPE_KTX;
    uint32_t block_dim_x = 1;
    int is_compressed = 0;
    if      (header->glInternalFormat == GL__COMPRESSED_RGB_S3TC_DXT1_EXT) {
        out_image->data_format = IMAGE_FILE_DATA_FORMAT_BC1_UNORM;
        is_compressed = 1;
        block_dim_x = 4;
    } else if (header->glInternalFormat == GL__COMPRESSED_RGBA_S3TC_DXT1_EXT) {
        out_image->data_format = IMAGE_FILE_DATA_FORMAT_BC1_UNORM;
        is_compressed = 1;
        block_dim_x = 4;
    } else if (header->glInternalFormat == GL__COMPRESSED_RGBA_S3TC_DXT5_EXT) {
        out_image->data_format = IMAGE_FILE_DATA_FORMAT_BC3_UNORM;
        is_compressed = 1;
        block_dim_x = 4;
    } else if (header->glInternalFormat == GL__COMPRESSED_RGBA_BPTC_UNORM_ARB) {
        out_image->data_format = IMAGE_FILE_DATA_FORMAT_BC7_UNORM;
        is_compressed = 1;
        block_dim_x = 4;
    } else {
        free(ktx_bytes);
        return -6;
    }
    uint32_t bytes_per_texel_block = ImageFileGetBytesPerTexelBlock(out_image->data_format);
    out_image->row_pitch_bytes = is_compressed ? 
            (bytes_per_texel_block * IMAGEFILE__MAX(1U, (header->pixelWidth + block_dim_x - 1) / block_dim_x)) :
            (bytes_per_texel_block * header->pixelWidth);
    out_image->depth_pitch_bytes = out_image->row_pitch_bytes * out_image->height;
    out_image->file_contents = ktx_bytes;
    out_image->flags = 0;
    if (header->numberOfFaces == 6)
        out_image->flags |= IMAGE_FILE_FLAG_CUBE_BIT;
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int ImageFileCreate(ImageFile *out_image, const char *image_path)
{
    memset(out_image, 0, sizeof(*out_image));

    const char *suffix = strrchr(image_path, (int)'.');
    if (suffix == NULL)
        return -1;  // No filename suffix
    char suffix_lower[16];
    for(int i=0; i<16; ++i)
    {
        char c = suffix[i];
        suffix_lower[i] = (c >= 'A' && c <= 'Z') ? (c + ('a'-'A')) : c;
        if (c == 0)
            break;
    }
    suffix_lower[15] = 0;

    ImageFileType file_type = IMAGE_FILE_TYPE_UNKNOWN;
    if (strcmp(suffix_lower, ".png") == 0)
        file_type = IMAGE_FILE_TYPE_PNG;
    else if (strcmp(suffix_lower, ".tga") == 0)
        file_type = IMAGE_FILE_TYPE_TGA;
    else if (strcmp(suffix_lower, ".jpg") == 0 || strcmp(suffix_lower, ".jpeg") == 0)
        file_type = IMAGE_FILE_TYPE_JPEG;
    else if (strcmp(suffix_lower, ".bmp") == 0)
        file_type = IMAGE_FILE_TYPE_BMP;
    else if (strcmp(suffix_lower, ".dds") == 0)
        file_type = IMAGE_FILE_TYPE_DDS;
    else if (strcmp(suffix_lower, ".astc") == 0)
        file_type = IMAGE_FILE_TYPE_ASTC;
    else if (strcmp(suffix_lower, ".ktx") == 0)
        file_type = IMAGE_FILE_TYPE_KTX;
    else
        return -2;  // Unrecognized filename suffix

    int load_error = 0;
    switch(file_type)
    {
    case IMAGE_FILE_TYPE_PNG:
    case IMAGE_FILE_TYPE_TGA:
    case IMAGE_FILE_TYPE_JPEG:
    case IMAGE_FILE_TYPE_BMP:
        load_error = LoadImageFromStb(out_image, image_path, file_type);
        break;
    case IMAGE_FILE_TYPE_DDS:
        load_error = LoadImageFromDds(out_image, image_path);
        break;
    case IMAGE_FILE_TYPE_ASTC:
        load_error = LoadImageFromAstc(out_image, image_path);
        break;
    case IMAGE_FILE_TYPE_KTX:
        load_error = LoadImageFromKtx(out_image, image_path);
        break;
    case IMAGE_FILE_TYPE_UNKNOWN:
        break;  // unrecognized file types already handled above
    }

    if (load_error != 0)
    {
        out_image->file_type = IMAGE_FILE_TYPE_UNKNOWN;
        out_image->data_format = IMAGE_FILE_DATA_FORMAT_UNKNOWN;
    }
    return load_error;
}

void ImageFileDestroy(const ImageFile *image)
{
    switch(image->file_type)
    {
    case IMAGE_FILE_TYPE_PNG:
    case IMAGE_FILE_TYPE_TGA:
    case IMAGE_FILE_TYPE_JPEG:
    case IMAGE_FILE_TYPE_BMP:
        stbi_image_free(image->file_contents);
        break;
    case IMAGE_FILE_TYPE_DDS:
        free(image->file_contents);
        break;
    case IMAGE_FILE_TYPE_ASTC:
        free(image->file_contents);
        break;
    case IMAGE_FILE_TYPE_KTX:
        free(image->file_contents);
        break;
    case IMAGE_FILE_TYPE_UNKNOWN:
        break;
    }
}

size_t ImageFileGetSubresourceSize(const ImageFile *image, const ImageFileSubresource subresource)
{
    if (!IsSubresourceValid(image, subresource))
        return 0;
    switch(image->file_type)
    {
    case IMAGE_FILE_TYPE_PNG:
    case IMAGE_FILE_TYPE_TGA:
    case IMAGE_FILE_TYPE_JPEG:
    case IMAGE_FILE_TYPE_BMP:
        return image->depth_pitch_bytes * image->depth;
    case IMAGE_FILE_TYPE_DDS:
    {
        int is_compressed = DdsContainsCompressedTexture(image->data_format);
        uint32_t bytes_per_texel_block = (uint32_t)ImageFileGetBytesPerTexelBlock(image->data_format);
        uint32_t mip_width  = IMAGEFILE__MAX(image->width  >> subresource.mip_level, 1U);
        uint32_t mip_height = IMAGEFILE__MAX(image->height >> subresource.mip_level, 1U);
        uint32_t mip_depth  = IMAGEFILE__MAX(image->depth  >> subresource.mip_level, 1U);
        uint32_t mip_pitch  = is_compressed ? ((mip_width+3)/4)*bytes_per_texel_block : mip_width*bytes_per_texel_block;
        uint32_t num_rows = is_compressed ? ((mip_height+3)/4) : mip_height;
        return mip_pitch * num_rows * mip_depth;
    }
    case IMAGE_FILE_TYPE_ASTC:
        return image->depth_pitch_bytes * image->depth;
    case IMAGE_FILE_TYPE_KTX:
    {
        const KtxHeader *header = (const KtxHeader*)image->file_contents;
        uintptr_t next_mip = (uintptr_t)(header) + sizeof(*header) + header->bytesOfKeyValueData;
        uint32_t image_size = 0;
        for(uint32_t i = 0; i <= subresource.mip_level; ++i)
        {
            image_size = *(const uint32_t*)(next_mip);
            image_size = (image_size + 3) & ~3;
            if ((image->flags & IMAGE_FILE_FLAG_CUBE_BIT) && image->array_layers == 1)
            {
                // For non-array cubemaps, the imageSize field stores the unpadded size of one cube face.
                // Otherwise, it's the unpadded size of all data in this mip level.
                // TODO(cort): this needs to be tested; I'm pretty sure it's wrong.
                image_size *= 6;
                image_size = (image_size + 3) & ~3;
            }
            next_mip += sizeof(image_size) + image_size;
        }
        return image_size / image->array_layers;
    }
    case IMAGE_FILE_TYPE_UNKNOWN:
        break;
    }
    return 0;
}

void *ImageFileGetSubresourceData(const ImageFile *image, const ImageFileSubresource subresource)
{
    if (!IsSubresourceValid(image, subresource))
        return 0;
    switch(image->file_type)
    {
    case IMAGE_FILE_TYPE_PNG:
    case IMAGE_FILE_TYPE_TGA:
    case IMAGE_FILE_TYPE_JPEG:
    case IMAGE_FILE_TYPE_BMP:
        // These file types only have one subresource; easy peasy.
        return image->file_contents;
    case IMAGE_FILE_TYPE_DDS:
    {
        // DDS files store all mips of layer 0 (large to small), then all mips of layer 1, etc.
        assert(image->mip_levels > 0 && image->mip_levels <= 64);  // 64 mip levels should be enough for anyone!
        size_t mip_offsets[64];  // offset within an array layer of each mip
        size_t layer_size = 0;  // total mipchain size for each layer
        for(uint32_t iMip=0; iMip<image->mip_levels; ++iMip)
        {
            ImageFileSubresource temp_sub;
            temp_sub.array_layer = 0;
            temp_sub.mip_level = iMip;
            mip_offsets[iMip] = layer_size;
            layer_size += ImageFileGetSubresourceSize(image, temp_sub);
        }
        uint32_t texels_base_offset = *(const uint32_t*)(image->file_contents);
        intptr_t out_ptr = (intptr_t)image->file_contents + texels_base_offset +
            subresource.array_layer * layer_size +
            mip_offsets[subresource.mip_level];
        return (void*)out_ptr;
    }
    case IMAGE_FILE_TYPE_ASTC:
    {
        intptr_t out_ptr = (intptr_t)image->file_contents + sizeof(AstcHeader);
        return (void*)out_ptr;
    }
    case IMAGE_FILE_TYPE_KTX:
    {
        const KtxHeader *header = (const KtxHeader*)image->file_contents;
        uintptr_t next_mip = (uintptr_t)(header) + sizeof(*header) + header->bytesOfKeyValueData;
        uint32_t image_size = 0;
        for(uint32_t i = 0; i < subresource.mip_level; ++i)
        {
            image_size = *(const uint32_t*)(next_mip);
            image_size = (image_size + 3) & ~3;
            if ((image->flags & IMAGE_FILE_FLAG_CUBE_BIT) && image->array_layers == 1)
            {
                // For non-array cubemaps, the imageSize field stores the unpadded size of one cube face.
                // Otherwise, it's the unpadded size of all data in this mip level.
                // TODO(cort): this needs to be tested; I'm pretty sure it's wrong.
                image_size *= 6;
                image_size = (image_size + 3) & ~3;
            }
            next_mip += sizeof(image_size) + image_size;
        }
        image_size = *(const uint32_t*)(next_mip);
        return (void*)( next_mip + sizeof(image_size) + (image_size / image->array_layers) * subresource.array_layer );
    }
    case IMAGE_FILE_TYPE_UNKNOWN:
        break;
    }
    return NULL;
}
