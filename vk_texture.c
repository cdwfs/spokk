#include "image_file.h"
#include "vk_texture.h"

#include <stdlib.h>
#include <string.h>

static VkImageAspectFlags VkFormatToAspectFlags(VkFormat format)
{
    switch(format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

static VkFormat ImageFormatToVkFormat(ImageFileDataFormat format)
{
    switch(format)
    {
    case IMAGE_FILE_DATA_FORMAT_UNKNOWN:            return VK_FORMAT_UNDEFINED;
    case IMAGE_FILE_DATA_FORMAT_R8G8B8_UNORM:       return VK_FORMAT_R8G8B8_UNORM;
    case IMAGE_FILE_DATA_FORMAT_R8G8B8A8_UNORM:     return VK_FORMAT_R8G8B8A8_UNORM;
    case IMAGE_FILE_DATA_FORMAT_B8G8R8_UNORM:       return VK_FORMAT_B8G8R8_UNORM;
    case IMAGE_FILE_DATA_FORMAT_B8G8R8A8_UNORM:     return VK_FORMAT_B8G8R8A8_UNORM;
    case IMAGE_FILE_DATA_FORMAT_R4G4B4A4_UNORM:     return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
    case IMAGE_FILE_DATA_FORMAT_B4G4R4A4_UNORM:     return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
    case IMAGE_FILE_DATA_FORMAT_R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case IMAGE_FILE_DATA_FORMAT_R32G32B32_FLOAT:    return VK_FORMAT_R32G32B32_SFLOAT;
    case IMAGE_FILE_DATA_FORMAT_R32G32_FLOAT:       return VK_FORMAT_R32G32_SFLOAT;
    case IMAGE_FILE_DATA_FORMAT_R32_FLOAT:          return VK_FORMAT_R32_SFLOAT;
    case IMAGE_FILE_DATA_FORMAT_R16G16B16A16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case IMAGE_FILE_DATA_FORMAT_R16G16B16A16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;
    case IMAGE_FILE_DATA_FORMAT_R16G16_FLOAT:       return VK_FORMAT_R16G16_SFLOAT;
    case IMAGE_FILE_DATA_FORMAT_R16G16_UNORM:       return VK_FORMAT_R16G16_UNORM;
    case IMAGE_FILE_DATA_FORMAT_R16_FLOAT:          return VK_FORMAT_R16_SFLOAT;
    case IMAGE_FILE_DATA_FORMAT_R16_UNORM:          return VK_FORMAT_R16_UNORM;
    case IMAGE_FILE_DATA_FORMAT_R8_UNORM:           return VK_FORMAT_R8_UNORM;
    case IMAGE_FILE_DATA_FORMAT_BC1_UNORM:          return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC1_SRGB:           return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC2_UNORM:          return VK_FORMAT_BC2_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC2_SRGB:           return VK_FORMAT_BC2_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC3_UNORM:          return VK_FORMAT_BC3_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC3_SRGB:           return VK_FORMAT_BC3_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC4_UNORM:          return VK_FORMAT_BC4_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC4_SNORM:          return VK_FORMAT_BC4_SNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC5_UNORM:          return VK_FORMAT_BC5_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC5_SNORM:          return VK_FORMAT_BC5_SNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC6H_UF16:          return VK_FORMAT_BC6H_UFLOAT_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC6H_SF16:          return VK_FORMAT_BC6H_SFLOAT_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC7_UNORM:          return VK_FORMAT_BC7_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_BC7_SRGB:           return VK_FORMAT_BC7_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_4x4_UNORM:     return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_4x4_SRGB:      return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x4_UNORM:     return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x4_SRGB:      return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x5_UNORM:     return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_5x5_SRGB:      return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x5_UNORM:     return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x5_SRGB:      return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x6_UNORM:     return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_6x6_SRGB:      return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x5_UNORM:     return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x5_SRGB:      return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x6_UNORM:     return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x6_SRGB:      return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x8_UNORM:     return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_8x8_SRGB:      return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x5_UNORM:    return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x5_SRGB:     return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x6_UNORM:    return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x6_SRGB:     return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x8_UNORM:    return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x8_SRGB:     return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x10_UNORM:   return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_10x10_SRGB:    return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x10_UNORM:   return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x10_SRGB:    return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x12_UNORM:   return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
    case IMAGE_FILE_DATA_FORMAT_ASTC_12x12_SRGB:    return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
    // no default case here, to get warnings when new formats are added!
    }
    return VK_FORMAT_UNDEFINED;
}

static void get_texel_block_dimensions(VkFormat format, uint32_t *out_width, uint32_t *out_height)
{
    switch(format)
    {
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
        *out_width = 4;
        *out_height = 4;
        break;
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
        // TODO(cort): confirm!
        *out_width = 4;
        *out_height = 4;
        break;
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
        // TODO(cort): confirm!
        *out_width = 4;
        *out_height = 4;
        break;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
        *out_width = 4;
        *out_height = 4;
        break;
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
        // TODO(cort): confirm!
        *out_width = 4;
        *out_height = 4;
        break;
    default:
        *out_width = 1;
        *out_height = 1;
        break;
    }
}

static void ImageFileToVkImageCreateInfo(VkImageCreateInfo *out_ci, const ImageFile *image)
{
    memset(out_ci, 0, sizeof(*out_ci));
    out_ci->sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    if (image->flags & IMAGE_FILE_FLAG_CUBE_BIT)
        out_ci->flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    if (image->depth == 1 && image->height == 1)
        out_ci->imageType = VK_IMAGE_TYPE_1D;
    else if (image->depth == 1)
        out_ci->imageType = VK_IMAGE_TYPE_2D;
    else
        out_ci->imageType = VK_IMAGE_TYPE_3D;
    out_ci->format = ImageFormatToVkFormat(image->data_format);
    out_ci->extent.width  = image->width;
    out_ci->extent.height = image->height;
    out_ci->extent.depth  = image->depth;
    out_ci->mipLevels = image->mip_levels;
    out_ci->arrayLayers = image->array_layers;
    out_ci->samples = VK_SAMPLE_COUNT_1_BIT;
    // Everything below here is a guess.
    out_ci->tiling = VK_IMAGE_TILING_OPTIMAL;
    out_ci->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    out_ci->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    out_ci->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

static uint32_t GetMipDimension(uint32_t base, uint32_t mip)
{
    uint32_t out = (base>>mip);
    return (out < 1) ? 1 : out;
}

int load_vkimage_from_file(VkImage *out_image, VkImageCreateInfo *out_image_ci,
    VkDeviceMemory *out_mem, VkDeviceSize *out_mem_offset,
    const stbvk_context *context, const char *filename, VkBool32 generate_mipmaps,
    VkImageLayout final_layout, VkAccessFlags final_access_flags)
{
    VkResult result = VK_SUCCESS;
    int err = 0;

    // Load image file
    ImageFile image_file;
    err = ImageFileCreate(&image_file, filename);
    if (err != 0)
    {
        return err;
    }
    ImageFileToVkImageCreateInfo(out_image_ci, &image_file);
    VkImageAspectFlags aspect_flags = VkFormatToAspectFlags(out_image_ci->format);
    uint32_t mips_to_load = image_file.mip_levels;

    if (generate_mipmaps)
    {
        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties(context->physical_device,
            out_image_ci->format, &format_properties);
        const VkFormatFeatureFlags blit_mask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
        const VkFormatFeatureFlags *feature_flags = (out_image_ci->tiling == VK_IMAGE_TILING_LINEAR)
            ? &(format_properties.linearTilingFeatures) : &(format_properties.optimalTilingFeatures);
        if ( (*feature_flags & blit_mask) != blit_mask )
        {
            generate_mipmaps = VK_FALSE;  // format does not support blitting; automatic mipmap generation won't work.
        }
        else
        {
            uint32_t num_mip_levels = 1;
            uint32_t max_dim = (image_file.width > image_file.height) ? image_file.width : image_file.height;
            max_dim = (max_dim > image_file.depth) ? max_dim : image_file.depth;
            while (max_dim > 1)
            {
                max_dim >>= 1;
                num_mip_levels += 1;
            }

            out_image_ci->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  // needed for self-blitting
            // Reserve space for the full mip chain...
            out_image_ci->mipLevels = num_mip_levels;
            // ...but only load the base level from the image file.
            mips_to_load = 1;
        }
    }

    // Create staging buffer (large enough to hold all subresources we're loading) and map it to host memory.
    VkBufferCreateInfo staging_buffer_ci;
    memset(&staging_buffer_ci, 0, sizeof(staging_buffer_ci));
    staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_ci.size = 0;  // set below
    staging_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageFileSubresource subresource;
    for(uint32_t i_mip=0; i_mip<mips_to_load; ++i_mip)
    {
        subresource.mip_level = i_mip;
        for(uint32_t i_layer=0; i_layer<image_file.array_layers; ++i_layer)
        {
            subresource.array_layer = i_layer;
            staging_buffer_ci.size += ImageFileGetSubresourceSize(&image_file, subresource);
        }
    }
    VkBuffer staging_buffer = stbvk_create_buffer(context, &staging_buffer_ci, "texture loader staging buffer");
    VkDeviceMemory staging_buffer_mem = VK_NULL_HANDLE;
    VkDeviceSize staging_buffer_mem_offset = 0;
    result = stbvk_allocate_and_bind_buffer_memory(context, staging_buffer, NULL,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "texture loader staging buffer memory", &staging_buffer_mem, &staging_buffer_mem_offset);
    uint8_t *staging_buffer_data = NULL;
    result = vkMapMemory(context->device, staging_buffer_mem, staging_buffer_mem_offset, staging_buffer_ci.size,
        (VkMemoryMapFlags)0, (void**)&staging_buffer_data);

    // Populate staging buffer, and build a list of regions to copy into the final image.
    uint32_t copy_region_count = mips_to_load;
    VkBufferImageCopy *copy_regions = (VkBufferImageCopy*)malloc(copy_region_count * sizeof(VkBufferImageCopy));
    memset(copy_regions, 0, copy_region_count*sizeof(VkBufferImageCopy));
    size_t staging_offset = 0;
    uint32_t texel_block_width = 0, texel_block_height = 0;
    get_texel_block_dimensions(out_image_ci->format, &texel_block_width, &texel_block_height);
    for(uint32_t i_mip=0; i_mip<mips_to_load; ++i_mip)
    {
        subresource.mip_level = i_mip;
        subresource.array_layer = 0;
        copy_regions[i_mip].bufferOffset = staging_offset;
        // copy region dimensions are specified in pixels (not texel blocks or bytes), so
        // we need some gymnastics to get the pitch in pixels for compressed formats.
        copy_regions[i_mip].bufferRowLength = GetMipDimension(
            image_file.row_pitch_bytes * texel_block_width / ImageFileGetBytesPerTexelBlock(image_file.data_format),
            i_mip);
        copy_regions[i_mip].bufferImageHeight = GetMipDimension(image_file.height, i_mip);
        copy_regions[i_mip].imageSubresource.aspectMask = aspect_flags;
        copy_regions[i_mip].imageSubresource.mipLevel = i_mip;
        copy_regions[i_mip].imageSubresource.baseArrayLayer = 0;  // TODO(cort): take a VkImageSubresourceRange?
        copy_regions[i_mip].imageSubresource.layerCount = image_file.array_layers;
        copy_regions[i_mip].imageOffset.x = 0;
        copy_regions[i_mip].imageOffset.y = 0;
        copy_regions[i_mip].imageOffset.z = 0;
        copy_regions[i_mip].imageExtent.width = GetMipDimension(image_file.width, i_mip);
        copy_regions[i_mip].imageExtent.height = GetMipDimension(image_file.height, i_mip);
        copy_regions[i_mip].imageExtent.depth = GetMipDimension(image_file.depth, i_mip);
        for(uint32_t i_layer=0; i_layer<image_file.array_layers; ++i_layer)
        {
            subresource.array_layer = i_layer;
            size_t subresource_size = ImageFileGetSubresourceSize(&image_file, subresource);
            memcpy(staging_buffer_data + staging_offset, ImageFileGetSubresourceData(&image_file, subresource),
                subresource_size);
            staging_offset += subresource_size;
        }
    }
    vkUnmapMemory(context->device, staging_buffer_mem);

    // Create final image
    *out_image = stbvk_create_image(context, out_image_ci, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_ACCESS_TRANSFER_WRITE_BIT, filename);
    stbvk_allocate_and_bind_image_memory(context, *out_image, NULL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        filename, out_mem, out_mem_offset);

    // Build command buffer 
    VkCommandPoolCreateInfo cpool_ci;
    memset(&cpool_ci, 0, sizeof(cpool_ci));
    cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    cpool_ci.queueFamilyIndex = context->graphics_queue_family_index;
    VkCommandPool cpool = stbvk_create_command_pool(context, &cpool_ci, "staging command buffer");
    VkCommandBufferAllocateInfo cb_allocate_info;
    memset(&cb_allocate_info, 0, sizeof(cb_allocate_info));
    cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_allocate_info.commandPool = cpool;
    cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_allocate_info.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(context->device, &cb_allocate_info, &cb);

    VkFenceCreateInfo fence_ci;
    memset(&fence_ci, 0, sizeof(fence_ci));
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = stbvk_create_fence(context, &fence_ci, "staging fence");

    VkCommandBufferBeginInfo cb_begin_info;
    memset(&cb_begin_info, 0, sizeof(cb_begin_info));
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &cb_begin_info);

    VkBufferMemoryBarrier buffer_barrier;
    memset(&buffer_barrier, 0, sizeof(buffer_barrier));
    buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    buffer_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    buffer_barrier.srcQueueFamilyIndex = cpool_ci.queueFamilyIndex;
    buffer_barrier.dstQueueFamilyIndex = cpool_ci.queueFamilyIndex;
    buffer_barrier.buffer = staging_buffer;
    buffer_barrier.offset = 0;
    buffer_barrier.size = staging_buffer_ci.size;
    VkImageMemoryBarrier image_barrier;
    memset(&image_barrier, 0, sizeof(image_barrier));
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier.srcAccessMask = 0;
    image_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier.srcQueueFamilyIndex = cpool_ci.queueFamilyIndex;
    image_barrier.dstQueueFamilyIndex = cpool_ci.queueFamilyIndex;
    image_barrier.image = *out_image;
    image_barrier.subresourceRange.aspectMask = aspect_flags;
    image_barrier.subresourceRange.baseArrayLayer = 0;
    image_barrier.subresourceRange.layerCount = out_image_ci->arrayLayers;
    image_barrier.subresourceRange.baseMipLevel = 0;
    image_barrier.subresourceRange.levelCount = mips_to_load;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        (VkDependencyFlags)0, 0,NULL, 1,&buffer_barrier, 1,&image_barrier);
    vkCmdCopyBufferToImage(cb, staging_buffer, *out_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        copy_region_count, copy_regions);

    if (!generate_mipmaps)
    {
        image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_barrier.dstAccessMask = final_access_flags;
        image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_barrier.newLayout = final_layout;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            (VkDependencyFlags)0, 0,NULL, 0,NULL, 1,&image_barrier);
    }
    else
    {
        VkImageMemoryBarrier image_barriers[2]; // 0=src, 1=dst
        memset(image_barriers, 0, sizeof(image_barriers));
        image_barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        image_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        image_barriers[0].srcQueueFamilyIndex = cpool_ci.queueFamilyIndex;
        image_barriers[0].dstQueueFamilyIndex = cpool_ci.queueFamilyIndex;
        image_barriers[0].image = *out_image;
        image_barriers[0].subresourceRange.aspectMask = aspect_flags;
        image_barriers[0].subresourceRange.baseArrayLayer = 0;
        image_barriers[0].subresourceRange.layerCount = out_image_ci->arrayLayers;
        image_barriers[0].subresourceRange.baseMipLevel = 0;
        image_barriers[0].subresourceRange.levelCount = 1;
        image_barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_barriers[1].srcAccessMask = 0;
        image_barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_barriers[1].srcQueueFamilyIndex = cpool_ci.queueFamilyIndex;
        image_barriers[1].dstQueueFamilyIndex = cpool_ci.queueFamilyIndex;
        image_barriers[1].image = *out_image;
        image_barriers[1].subresourceRange.aspectMask = aspect_flags;
        image_barriers[1].subresourceRange.baseArrayLayer = 0;
        image_barriers[1].subresourceRange.layerCount = out_image_ci->arrayLayers;
        image_barriers[1].subresourceRange.baseMipLevel = 1;
        image_barriers[1].subresourceRange.levelCount = 1;
        VkImageBlit blit_region;
        memset(&blit_region, 0, sizeof(blit_region));
        blit_region.srcSubresource.aspectMask = aspect_flags;
        blit_region.srcSubresource.baseArrayLayer = 0;
        blit_region.srcSubresource.layerCount = out_image_ci->arrayLayers;
        blit_region.srcSubresource.mipLevel = 0;
        blit_region.srcOffsets[0].x = 0;
        blit_region.srcOffsets[0].y = 0;
        blit_region.srcOffsets[0].z = 0;
        blit_region.srcOffsets[1].x = GetMipDimension(out_image_ci->extent.width, 0);
        blit_region.srcOffsets[1].y = GetMipDimension(out_image_ci->extent.height, 0);
        blit_region.srcOffsets[1].z = GetMipDimension(out_image_ci->extent.depth, 0);
        blit_region.dstSubresource.aspectMask = aspect_flags;
        blit_region.dstSubresource.baseArrayLayer = 0;
        blit_region.dstSubresource.layerCount = out_image_ci->arrayLayers;
        blit_region.dstSubresource.mipLevel = 1;
        blit_region.dstOffsets[0].x = 0;
        blit_region.dstOffsets[0].y = 0;
        blit_region.dstOffsets[0].z = 0;
        blit_region.dstOffsets[1].x = GetMipDimension(out_image_ci->extent.width, 1);
        blit_region.dstOffsets[1].y = GetMipDimension(out_image_ci->extent.height, 1);
        blit_region.dstOffsets[1].z = GetMipDimension(out_image_ci->extent.depth, 1);
        for(uint32_t dst_mip = 1; dst_mip < out_image_ci->mipLevels; ++dst_mip)
        {
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                (VkDependencyFlags)0, 0,NULL, 0,NULL, 2,image_barriers);
            vkCmdBlitImage(cb, *out_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                *out_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit_region, VK_FILTER_LINEAR);

            image_barriers[0].subresourceRange.baseMipLevel += 1;
            image_barriers[1].subresourceRange.baseMipLevel += 1;
            blit_region.srcSubresource.mipLevel += 1;
            blit_region.srcOffsets[1].x = GetMipDimension(out_image_ci->extent.width, dst_mip);
            blit_region.srcOffsets[1].y = GetMipDimension(out_image_ci->extent.height, dst_mip);
            blit_region.srcOffsets[1].z = GetMipDimension(out_image_ci->extent.depth, dst_mip);
            blit_region.dstSubresource.mipLevel += 1;
            blit_region.dstOffsets[1].x = GetMipDimension(out_image_ci->extent.width, dst_mip+1);
            blit_region.dstOffsets[1].y = GetMipDimension(out_image_ci->extent.height, dst_mip+1);
            blit_region.dstOffsets[1].z = GetMipDimension(out_image_ci->extent.depth, dst_mip+1);
        }
        // Coming out of the loop, all but the last mip are in TRANSFER_SRC mode, and the last mip
        // is in TRANSFER_DST. Convert them all to the final layout/access mode.
        image_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        image_barriers[0].dstAccessMask = final_access_flags;
        image_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        image_barriers[0].newLayout = final_layout;
        image_barriers[0].subresourceRange.baseArrayLayer = 0;
        image_barriers[0].subresourceRange.layerCount = out_image_ci->arrayLayers;
        image_barriers[0].subresourceRange.baseMipLevel = 0;
        image_barriers[0].subresourceRange.levelCount = out_image_ci->mipLevels - 1;
        image_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_barriers[1].dstAccessMask = final_access_flags;
        image_barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_barriers[1].newLayout = final_layout;
        image_barriers[1].subresourceRange.baseMipLevel = out_image_ci->mipLevels - 1;
        image_barriers[1].subresourceRange.levelCount = 1;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            (VkDependencyFlags)0, 0,NULL, 0,NULL, 2,image_barriers);
    }

    vkEndCommandBuffer(cb);
    VkSubmitInfo submit_info;
    memset(&submit_info, 0, sizeof(submit_info));
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cb;
    vkQueueSubmit(context->graphics_queue, 1, &submit_info, fence);
    vkWaitForFences(context->device, 1, &fence, VK_TRUE, UINT64_MAX);

    ImageFileDestroy(&image_file);
    stbvk_destroy_command_pool(context, cpool);
    stbvk_destroy_fence(context, fence);
    stbvk_destroy_buffer(context, staging_buffer);
    stbvk_free_device_memory(context, NULL, staging_buffer_mem, staging_buffer_mem_offset);
    return 0;
}

int generate_vkimage_mipmaps(VkImage image, const VkImageCreateInfo *image_ci,
    const stbvk_context *context, VkImageLayout input_layout, VkAccessFlags input_access_flags,
    VkImageLayout final_layout, VkAccessFlags final_access_flags)
{
    if (image_ci->mipLevels == 1)
    {
        return 0;  // nothing to do
    }
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(context->physical_device,
        image_ci->format, &format_properties);
    const VkFormatFeatureFlags blit_mask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    const VkFormatFeatureFlags *feature_flags = (image_ci->tiling == VK_IMAGE_TILING_LINEAR)
        ? &(format_properties.linearTilingFeatures) : &(format_properties.optimalTilingFeatures);
    if ( (*feature_flags & blit_mask) != blit_mask )
    {
        return -1;  // format does not support blitting; automatic mipmap generation won't work.
    }

    // Build command buffer 
    VkCommandPoolCreateInfo cpool_ci;
    memset(&cpool_ci, 0, sizeof(cpool_ci));
    cpool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    cpool_ci.queueFamilyIndex = context->graphics_queue_family_index;
    VkCommandPool cpool = stbvk_create_command_pool(context, &cpool_ci, "staging command buffer");
    VkCommandBufferAllocateInfo cb_allocate_info;
    memset(&cb_allocate_info, 0, sizeof(cb_allocate_info));
    cb_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cb_allocate_info.commandPool = cpool;
    cb_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cb_allocate_info.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(context->device, &cb_allocate_info, &cb);

    VkFenceCreateInfo fence_ci;
    memset(&fence_ci, 0, sizeof(fence_ci));
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = stbvk_create_fence(context, &fence_ci, "staging fence");

    VkCommandBufferBeginInfo cb_begin_info;
    memset(&cb_begin_info, 0, sizeof(cb_begin_info));
    cb_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cb_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &cb_begin_info);

    VkImageAspectFlags aspect_flags = VkFormatToAspectFlags(image_ci->format);

    VkImageMemoryBarrier image_barriers[2];
    memset(image_barriers, 0, sizeof(image_barriers));
    image_barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barriers[0].srcAccessMask = input_access_flags;
    image_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_barriers[0].oldLayout = input_layout;
    image_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_barriers[0].srcQueueFamilyIndex = cpool_ci.queueFamilyIndex;
    image_barriers[0].dstQueueFamilyIndex = cpool_ci.queueFamilyIndex;
    image_barriers[0].image = image;
    image_barriers[0].subresourceRange.aspectMask = aspect_flags;
    image_barriers[0].subresourceRange.baseArrayLayer = 0;
    image_barriers[0].subresourceRange.layerCount = image_ci->arrayLayers;
    image_barriers[0].subresourceRange.baseMipLevel = 0;
    image_barriers[0].subresourceRange.levelCount = 1;
    image_barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barriers[1].srcAccessMask = 0;
    image_barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barriers[1].srcQueueFamilyIndex = cpool_ci.queueFamilyIndex;
    image_barriers[1].dstQueueFamilyIndex = cpool_ci.queueFamilyIndex;
    image_barriers[1].image = image;
    image_barriers[1].subresourceRange.aspectMask = aspect_flags;
    image_barriers[1].subresourceRange.baseArrayLayer = 0;
    image_barriers[1].subresourceRange.layerCount = image_ci->arrayLayers;
    image_barriers[1].subresourceRange.baseMipLevel = 1;
    image_barriers[1].subresourceRange.levelCount = 1;
    VkImageBlit blit_region;
    memset(&blit_region, 0, sizeof(blit_region));
    blit_region.srcSubresource.aspectMask = aspect_flags;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.layerCount = image_ci->arrayLayers;
    blit_region.srcSubresource.mipLevel = 0;
    blit_region.srcOffsets[0].x = 0;
    blit_region.srcOffsets[0].y = 0;
    blit_region.srcOffsets[0].z = 0;
    blit_region.srcOffsets[1].x = GetMipDimension(image_ci->extent.width, 0);
    blit_region.srcOffsets[1].y = GetMipDimension(image_ci->extent.height, 0);
    blit_region.srcOffsets[1].z = GetMipDimension(image_ci->extent.depth, 0);
    blit_region.dstSubresource.aspectMask = aspect_flags;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.layerCount = image_ci->arrayLayers;
    blit_region.dstSubresource.mipLevel = 1;
    blit_region.dstOffsets[0].x = 0;
    blit_region.dstOffsets[0].y = 0;
    blit_region.dstOffsets[0].z = 0;
    blit_region.dstOffsets[1].x = GetMipDimension(image_ci->extent.width, 1);
    blit_region.dstOffsets[1].y = GetMipDimension(image_ci->extent.height, 1);
    blit_region.dstOffsets[1].z = GetMipDimension(image_ci->extent.depth, 1);
    for(uint32_t dst_mip = 1; dst_mip < image_ci->mipLevels; ++dst_mip)
    {
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            (VkDependencyFlags)0, 0,NULL, 0,NULL, 2,image_barriers);
        vkCmdBlitImage(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit_region, VK_FILTER_LINEAR);
        image_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        image_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        image_barriers[0].subresourceRange.baseMipLevel += 1;

        image_barriers[1].subresourceRange.baseMipLevel += 1;

        blit_region.srcSubresource.mipLevel += 1;
        blit_region.srcOffsets[1].x = GetMipDimension(image_ci->extent.width, dst_mip);
        blit_region.srcOffsets[1].y = GetMipDimension(image_ci->extent.height, dst_mip);
        blit_region.srcOffsets[1].z = GetMipDimension(image_ci->extent.depth, dst_mip);
        blit_region.dstSubresource.mipLevel += 1;
        blit_region.dstOffsets[1].x = GetMipDimension(image_ci->extent.width, dst_mip+1);
        blit_region.dstOffsets[1].y = GetMipDimension(image_ci->extent.height, dst_mip+1);
        blit_region.dstOffsets[1].z = GetMipDimension(image_ci->extent.depth, dst_mip+1);
    }
    // Coming out of the loop, all but the last mip are in TRANSFER_SRC mode, and the last mip
    // is in TRANSFER_DST. Convert them all to the final layout/access mode.
    image_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_barriers[0].dstAccessMask = final_access_flags;
    image_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_barriers[0].newLayout = final_layout;
    image_barriers[0].subresourceRange.baseArrayLayer = 0;
    image_barriers[0].subresourceRange.layerCount = image_ci->arrayLayers;
    image_barriers[0].subresourceRange.baseMipLevel = 0;
    image_barriers[0].subresourceRange.levelCount = image_ci->mipLevels - 1;
    image_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barriers[1].dstAccessMask = final_access_flags;
    image_barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barriers[1].newLayout = final_layout;
    image_barriers[1].subresourceRange.baseMipLevel = image_ci->mipLevels - 1;
    image_barriers[1].subresourceRange.levelCount = 1;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        (VkDependencyFlags)0, 0,NULL, 0,NULL, 2,image_barriers);

    vkEndCommandBuffer(cb);
    VkSubmitInfo submit_info;
    memset(&submit_info, 0, sizeof(submit_info));
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cb;
    vkQueueSubmit(context->graphics_queue, 1, &submit_info, fence);
    vkWaitForFences(context->device, 1, &fence, VK_TRUE, UINT64_MAX);

    stbvk_destroy_command_pool(context, cpool);
    stbvk_destroy_fence(context, fence);
    return 0;
}
