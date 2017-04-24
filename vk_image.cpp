#include "vk_debug.h"
#include "vk_image.h"
#include "vk_utilities.h"

#include "image_file.h"

#include <array>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

namespace {

    // TODO(cort): this table probably belongs in image_file.c
    struct ImageFormatAttributes {
      int32_t texel_block_bytes;
      int32_t texel_block_width;
      int32_t texel_block_height;
      ImageFileDataFormat image_format;  // primary key; g_format_attributes[img_fmt].image_format == img_fmt
      VkFormat vk_format;
    };
    const std::array<ImageFormatAttributes, IMAGE_FILE_DATA_FORMAT_COUNT> g_format_attributes = {{
      {  0,  0,  0, IMAGE_FILE_DATA_FORMAT_UNKNOWN,            VK_FORMAT_UNDEFINED, },
      {  3,  1,  1, IMAGE_FILE_DATA_FORMAT_R8G8B8_UNORM,       VK_FORMAT_R8G8B8_UNORM, },
      {  4,  1,  1, IMAGE_FILE_DATA_FORMAT_R8G8B8A8_UNORM,     VK_FORMAT_R8G8B8A8_UNORM, },
      {  3,  1,  1, IMAGE_FILE_DATA_FORMAT_B8G8R8_UNORM,       VK_FORMAT_B8G8R8_UNORM, },
      {  4,  1,  1, IMAGE_FILE_DATA_FORMAT_B8G8R8A8_UNORM,     VK_FORMAT_B8G8R8A8_UNORM, },
      {  2,  1,  1, IMAGE_FILE_DATA_FORMAT_R4G4B4A4_UNORM,     VK_FORMAT_R4G4B4A4_UNORM_PACK16, },
      {  2,  1,  1, IMAGE_FILE_DATA_FORMAT_B4G4R4A4_UNORM,     VK_FORMAT_B4G4R4A4_UNORM_PACK16, },
      {  2,  1,  1, IMAGE_FILE_DATA_FORMAT_R5G6B5_UNORM,       VK_FORMAT_R5G6B5_UNORM_PACK16, },
      {  2,  1,  1, IMAGE_FILE_DATA_FORMAT_B5G6R5_UNORM,       VK_FORMAT_B5G6R5_UNORM_PACK16, },
      {  2,  1,  1, IMAGE_FILE_DATA_FORMAT_R5G5B5A1_UNORM,     VK_FORMAT_R5G5B5A1_UNORM_PACK16, },
      {  2,  1,  1, IMAGE_FILE_DATA_FORMAT_B5G5R5A1_UNORM,     VK_FORMAT_B5G5R5A1_UNORM_PACK16, },
      {  2,  1,  1, IMAGE_FILE_DATA_FORMAT_A1R5G5B5_UNORM,     VK_FORMAT_A1R5G5B5_UNORM_PACK16, },
      { 16,  1,  1, IMAGE_FILE_DATA_FORMAT_R32G32B32A32_FLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, },
      { 12,  1,  1, IMAGE_FILE_DATA_FORMAT_R32G32B32_FLOAT,    VK_FORMAT_R32G32B32_SFLOAT, },
      {  8,  1,  1, IMAGE_FILE_DATA_FORMAT_R32G32_FLOAT,       VK_FORMAT_R32G32_SFLOAT, },
      {  4,  1,  1, IMAGE_FILE_DATA_FORMAT_R32_FLOAT,          VK_FORMAT_R32_SFLOAT, },
      {  8,  1,  1, IMAGE_FILE_DATA_FORMAT_R16G16B16A16_FLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, },
      {  8,  1,  1, IMAGE_FILE_DATA_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_R16G16B16A16_UNORM, },
      {  4,  1,  1, IMAGE_FILE_DATA_FORMAT_R16G16_FLOAT,       VK_FORMAT_R16G16_SFLOAT, },
      {  4,  1,  1, IMAGE_FILE_DATA_FORMAT_R16G16_UNORM,       VK_FORMAT_R16G16_UNORM, },
      {  2,  1,  1, IMAGE_FILE_DATA_FORMAT_R16_FLOAT,          VK_FORMAT_R16_SFLOAT, },
      {  2,  1,  1, IMAGE_FILE_DATA_FORMAT_R16_UNORM,          VK_FORMAT_R16_UNORM, },
      {  1,  1,  1, IMAGE_FILE_DATA_FORMAT_R8_UNORM,           VK_FORMAT_R8_UNORM, },
      {  8,  4,  4, IMAGE_FILE_DATA_FORMAT_BC1_UNORM,          VK_FORMAT_BC1_RGBA_UNORM_BLOCK, },
      {  8,  4,  4, IMAGE_FILE_DATA_FORMAT_BC1_SRGB,           VK_FORMAT_BC1_RGBA_SRGB_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_BC2_UNORM,          VK_FORMAT_BC2_UNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_BC2_SRGB,           VK_FORMAT_BC2_SRGB_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_BC3_UNORM,          VK_FORMAT_BC3_UNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_BC3_SRGB,           VK_FORMAT_BC3_SRGB_BLOCK, },
      {  8,  4,  4, IMAGE_FILE_DATA_FORMAT_BC4_UNORM,          VK_FORMAT_BC4_UNORM_BLOCK, },
      {  8,  4,  4, IMAGE_FILE_DATA_FORMAT_BC4_SNORM,          VK_FORMAT_BC4_SNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_BC5_UNORM,          VK_FORMAT_BC5_UNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_BC5_SNORM,          VK_FORMAT_BC5_SNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_BC6H_UF16,          VK_FORMAT_BC6H_UFLOAT_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_BC6H_SF16,          VK_FORMAT_BC6H_SFLOAT_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_BC7_UNORM,          VK_FORMAT_BC7_UNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_BC7_SRGB,           VK_FORMAT_BC7_SRGB_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_ASTC_4x4_UNORM,     VK_FORMAT_ASTC_4x4_UNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_ASTC_4x4_SRGB,      VK_FORMAT_ASTC_4x4_SRGB_BLOCK, },
      { 16,  5,  4, IMAGE_FILE_DATA_FORMAT_ASTC_5x4_UNORM,     VK_FORMAT_ASTC_5x4_UNORM_BLOCK, },
      { 16,  5,  4, IMAGE_FILE_DATA_FORMAT_ASTC_5x4_SRGB,      VK_FORMAT_ASTC_5x4_SRGB_BLOCK, },
      { 16,  5,  5, IMAGE_FILE_DATA_FORMAT_ASTC_5x5_UNORM,     VK_FORMAT_ASTC_5x5_UNORM_BLOCK, },
      { 16,  5,  5, IMAGE_FILE_DATA_FORMAT_ASTC_5x5_SRGB,      VK_FORMAT_ASTC_5x5_SRGB_BLOCK, },
      { 16,  6,  5, IMAGE_FILE_DATA_FORMAT_ASTC_6x5_UNORM,     VK_FORMAT_ASTC_6x5_UNORM_BLOCK, },
      { 16,  6,  5, IMAGE_FILE_DATA_FORMAT_ASTC_6x5_SRGB,      VK_FORMAT_ASTC_6x5_SRGB_BLOCK, },
      { 16,  6,  6, IMAGE_FILE_DATA_FORMAT_ASTC_6x6_UNORM,     VK_FORMAT_ASTC_6x6_UNORM_BLOCK, },
      { 16,  6,  6, IMAGE_FILE_DATA_FORMAT_ASTC_6x6_SRGB,      VK_FORMAT_ASTC_6x6_SRGB_BLOCK, },
      { 16,  8,  5, IMAGE_FILE_DATA_FORMAT_ASTC_8x5_UNORM,     VK_FORMAT_ASTC_8x5_UNORM_BLOCK, },
      { 16,  8,  5, IMAGE_FILE_DATA_FORMAT_ASTC_8x5_SRGB,      VK_FORMAT_ASTC_8x5_SRGB_BLOCK, },
      { 16,  8,  6, IMAGE_FILE_DATA_FORMAT_ASTC_8x6_UNORM,     VK_FORMAT_ASTC_8x6_UNORM_BLOCK, },
      { 16,  8,  6, IMAGE_FILE_DATA_FORMAT_ASTC_8x6_SRGB,      VK_FORMAT_ASTC_8x6_SRGB_BLOCK, },
      { 16,  8,  8, IMAGE_FILE_DATA_FORMAT_ASTC_8x8_UNORM,     VK_FORMAT_ASTC_8x8_UNORM_BLOCK, },
      { 16,  8,  8, IMAGE_FILE_DATA_FORMAT_ASTC_8x8_SRGB,      VK_FORMAT_ASTC_8x8_SRGB_BLOCK, },
      { 16, 10,  5, IMAGE_FILE_DATA_FORMAT_ASTC_10x5_UNORM,    VK_FORMAT_ASTC_10x5_UNORM_BLOCK, },
      { 16, 10,  5, IMAGE_FILE_DATA_FORMAT_ASTC_10x5_SRGB,     VK_FORMAT_ASTC_10x5_SRGB_BLOCK, },
      { 16, 10,  6, IMAGE_FILE_DATA_FORMAT_ASTC_10x6_UNORM,    VK_FORMAT_ASTC_10x6_UNORM_BLOCK, },
      { 16, 10,  6, IMAGE_FILE_DATA_FORMAT_ASTC_10x6_SRGB,     VK_FORMAT_ASTC_10x6_SRGB_BLOCK, },
      { 16, 10,  8, IMAGE_FILE_DATA_FORMAT_ASTC_10x8_UNORM,    VK_FORMAT_ASTC_10x8_UNORM_BLOCK, },
      { 16, 10,  8, IMAGE_FILE_DATA_FORMAT_ASTC_10x8_SRGB,     VK_FORMAT_ASTC_10x8_SRGB_BLOCK, },
      { 16, 10, 10, IMAGE_FILE_DATA_FORMAT_ASTC_10x10_UNORM,   VK_FORMAT_ASTC_10x10_UNORM_BLOCK, },
      { 16, 10, 10, IMAGE_FILE_DATA_FORMAT_ASTC_10x10_SRGB,    VK_FORMAT_ASTC_10x10_SRGB_BLOCK, },
      { 16, 12, 10, IMAGE_FILE_DATA_FORMAT_ASTC_12x10_UNORM,   VK_FORMAT_ASTC_12x10_UNORM_BLOCK, },
      { 16, 12, 10, IMAGE_FILE_DATA_FORMAT_ASTC_12x10_SRGB,    VK_FORMAT_ASTC_12x10_SRGB_BLOCK, },
      { 16, 12, 12, IMAGE_FILE_DATA_FORMAT_ASTC_12x12_UNORM,   VK_FORMAT_ASTC_12x12_UNORM_BLOCK, },
      { 16, 12, 12, IMAGE_FILE_DATA_FORMAT_ASTC_12x12_SRGB,    VK_FORMAT_ASTC_12x12_SRGB_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8_UNORM,  VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8_SRGB,   VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A1_UNORM,VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A1_SRGB, VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A8_UNORM,VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_ETC2_R8G8B8A8_SRGB, VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_EAC_R11_UNORM,      VK_FORMAT_EAC_R11_UNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_EAC_R11_SNORM,      VK_FORMAT_EAC_R11_SNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_EAC_R11G11_UNORM,   VK_FORMAT_EAC_R11G11_UNORM_BLOCK, },
      { 16,  4,  4, IMAGE_FILE_DATA_FORMAT_EAC_R11G11_SNORM,   VK_FORMAT_EAC_R11G11_SNORM_BLOCK, },
    }};

    const ImageFormatAttributes& GetVkFormatInfo(VkFormat format) {
      for(const auto& attr : g_format_attributes) {
        if (attr.vk_format == format) {
          return attr;
        }
      }
      assert(0);  // not found!
      return g_format_attributes[IMAGE_FILE_DATA_FORMAT_UNKNOWN];
    }

    void ImageFileToVkImageCreateInfo(VkImageCreateInfo *out_ci, const ImageFile &image) {
      *out_ci = {};
      out_ci->sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      if (image.flags & IMAGE_FILE_FLAG_CUBE_BIT) {
        out_ci->flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
      }
      if (image.depth == 1 && image.height == 1) {
        out_ci->imageType = VK_IMAGE_TYPE_1D;
      } else if (image.depth == 1) {
        out_ci->imageType = VK_IMAGE_TYPE_2D;
      } else {
        out_ci->imageType = VK_IMAGE_TYPE_3D;
      }
      out_ci->format = g_format_attributes[image.data_format].vk_format;
      out_ci->extent.width  = image.width;
      out_ci->extent.height = image.height;
      out_ci->extent.depth  = image.depth;
      out_ci->mipLevels = image.mip_levels;
      out_ci->arrayLayers = image.array_layers;
      out_ci->samples = VK_SAMPLE_COUNT_1_BIT;
      // Everything below here is a guess.
      out_ci->tiling = VK_IMAGE_TILING_OPTIMAL;
      out_ci->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      out_ci->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      out_ci->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    uint32_t GetMipDimension(uint32_t base, uint32_t mip) {
      uint32_t out = (base>>mip);
      return (out < 1) ? 1 : out;
    }

    uint32_t AlignTo(uint32_t x, uint32_t n) {
      assert( (n & (n-1)) == 0); // n must be a power of 2
      return (x + n-1) & ~(n-1);
    }

}  // namespace

namespace spokk {

//
// Image
//
VkResult Image::Create(const DeviceContext& device_context, const VkImageCreateInfo& ci,
    VkMemoryPropertyFlags memory_properties, DeviceAllocationScope allocation_scope) {
  assert(handle == VK_NULL_HANDLE);  // can't re-create an existing image!
  VkResult result = vkCreateImage(device_context.Device(), &ci, device_context.HostAllocator(), &handle);
  if (result == VK_SUCCESS) {
    memory = device_context.DeviceAllocAndBindToImage(handle, memory_properties, allocation_scope);
    if (memory.block == nullptr) {
      vkDestroyImage(device_context.Device(), handle, device_context.HostAllocator());
      handle = VK_NULL_HANDLE;
      result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    } else {
      VkImageViewCreateInfo view_ci = GetImageViewCreateInfo(handle, ci);
      result = vkCreateImageView(device_context.Device(), &view_ci, device_context.HostAllocator(), &view);
    }
  }
  image_ci = ci;
  return result;
}
int Image::CreateFromFile(const DeviceContext& device_context, ImageBlitter& blitter, const DeviceQueue *queue,
  const std::string& filename, VkBool32 generate_mipmaps, VkImageLayout final_layout, VkAccessFlags final_access_flags) {
  assert(handle == VK_NULL_HANDLE);  // can't re-create an existing image!

  // Load image file. TODO(cort): ideally, we'd load directly into the staging buffer here to save a memcpy.
  ImageFile image_file = {};
  int load_error = ImageFileCreate(&image_file, filename.c_str());
  if (load_error != 0) {
    return load_error;
  }

  // Create the destination image
  image_ci = {};
  ImageFileToVkImageCreateInfo(&image_ci, image_file);
  VkImageAspectFlags aspect_flags = GetImageAspectFlags(image_ci.format);
  uint32_t mips_to_load = image_file.mip_levels;
  if (generate_mipmaps) {  // Adjust image_ci to include space for extra mipmaps beyond the ones in the image file.
    VkFormatProperties format_properties = {};
    vkGetPhysicalDeviceFormatProperties(device_context.PhysicalDevice(), image_ci.format, &format_properties);
    const VkFormatFeatureFlags blit_mask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    const VkFormatFeatureFlags feature_flags = (image_ci.tiling == VK_IMAGE_TILING_LINEAR)
      ? format_properties.linearTilingFeatures : format_properties.optimalTilingFeatures;
    if ( (feature_flags & blit_mask) != blit_mask ) {
      generate_mipmaps = VK_FALSE;  // format does not support blitting; automatic mipmap generation won't work.
    } else {
      uint32_t num_mip_levels = 1;
      uint32_t max_dim = (image_file.width > image_file.height) ? image_file.width : image_file.height;
      max_dim = (max_dim > image_file.depth) ? max_dim : image_file.depth;
      while (max_dim > 1) {
        max_dim >>= 1;
        num_mip_levels += 1;
      }
      image_ci.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  // needed for self-blitting
                                                          // Reserve space for the full mip chain...
      image_ci.mipLevels = num_mip_levels;
      // ...but only load the base level from the image file.
      mips_to_load = 1;
    }
  }
  // TODO(cort): caller passes in memory properties and scope?
  SPOKK_VK_CHECK(vkCreateImage(device_context.Device(), &image_ci, device_context.HostAllocator(), &handle));
  memory = device_context.DeviceAllocAndBindToImage(handle, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    DEVICE_ALLOCATION_SCOPE_DEVICE);

  // Gimme a command buffer
  OneShotCommandPool cpool(device_context.Device(), queue->handle, queue->family, device_context.HostAllocator());
  VkCommandBuffer cb = cpool.AllocateAndBegin();

  // transition image into TRANSFER_DST most for loading
  VkImageMemoryBarrier barrier_init_to_dst = {};
  barrier_init_to_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier_init_to_dst.srcAccessMask = 0;
  barrier_init_to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier_init_to_dst.oldLayout = image_ci.initialLayout;
  barrier_init_to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier_init_to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_init_to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_init_to_dst.image = handle;
  barrier_init_to_dst.subresourceRange.aspectMask = GetImageAspectFlags(image_ci.format);
  barrier_init_to_dst.subresourceRange.baseArrayLayer = 0;
  barrier_init_to_dst.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
  barrier_init_to_dst.subresourceRange.baseMipLevel = 0;
  barrier_init_to_dst.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
    0, nullptr, 0, nullptr, 1, &barrier_init_to_dst);

  // Load those mips!
  int32_t texel_block_bytes = g_format_attributes[image_file.data_format].texel_block_bytes;
  int32_t texel_block_width = g_format_attributes[image_file.data_format].texel_block_width;
  int32_t texel_block_height = g_format_attributes[image_file.data_format].texel_block_height;
  for(uint32_t i_mip=0; i_mip<mips_to_load; ++i_mip) {
    for(uint32_t i_layer=0; i_layer<image_file.array_layers; ++i_layer) {
      ImageFileSubresource subresource;
      subresource.array_layer = i_layer;
      subresource.mip_level = i_mip;
      const void *subresource_data = ImageFileGetSubresourceData(&image_file, subresource);

      VkBufferImageCopy copy_region = {};
      copy_region.bufferOffset = 0;
      // copy region dimensions are specified in pixels (not texel blocks or bytes), but must be
      // an even integer multiple of the texel block dimensions for compressed formats.
      // It must also respect the minImageTransferGranularity, but I don't have a good way of testing
      // that right now.  ...Wait, yes I do! It's in the DeviceQueue!
      copy_region.bufferRowLength = GetMipDimension(image_file.row_pitch_bytes * texel_block_width / texel_block_bytes, i_mip);
      copy_region.bufferImageHeight = GetMipDimension(image_file.height, i_mip);
      copy_region.bufferRowLength   = AlignTo(copy_region.bufferRowLength, texel_block_width);
      copy_region.bufferImageHeight = AlignTo(copy_region.bufferImageHeight, texel_block_height);
      copy_region.imageSubresource.aspectMask = aspect_flags;
      copy_region.imageSubresource.mipLevel = i_mip;
      copy_region.imageSubresource.baseArrayLayer = i_layer;
      copy_region.imageSubresource.layerCount = 1;  // can only copy one layer at a time
      copy_region.imageExtent.width  = AlignTo(GetMipDimension(image_file.width, i_mip), texel_block_width);
      copy_region.imageExtent.height = AlignTo(GetMipDimension(image_file.height, i_mip), texel_block_height);
      copy_region.imageExtent.depth  = GetMipDimension(image_file.depth, i_mip);
      blitter.CopyMemoryToImage(cb, handle, subresource_data, image_ci.format, copy_region);
    }
  }

  // Generate remaining mips, if requested
  VkImageMemoryBarrier barrier_dst_to_final = barrier_init_to_dst;
  barrier_dst_to_final.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier_dst_to_final.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier_dst_to_final.dstAccessMask = final_access_flags;
  barrier_dst_to_final.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier_dst_to_final.newLayout = final_layout;
  if (generate_mipmaps) {
    for(uint32_t i_layer=0; i_layer < image_file.array_layers; ++i_layer) {
      GenerateMipmapsImpl(cb, barrier_dst_to_final, i_layer, 0, image_ci.mipLevels - 1);
    }
  } else {
    // transition to final layout/access
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_BY_REGION_BIT,
      0, nullptr, 0, nullptr, 1, &barrier_dst_to_final);
  }

  cpool.EndSubmitAndFree(&cb);
  ImageFileDestroy(&image_file);

  VkImageViewCreateInfo view_ci = GetImageViewCreateInfo(handle, image_ci);
  VkResult result = vkCreateImageView(device_context.Device(), &view_ci, device_context.HostAllocator(), &view);
  if (result != VK_SUCCESS) {
    Destroy(device_context);
    return -1;
  }

  return 0;
}

void Image::Destroy(const DeviceContext& device_context) {
  device_context.DeviceFree(memory);
  memory.block = nullptr;
  if (view != VK_NULL_HANDLE) {
    vkDestroyImageView(device_context.Device(), view, device_context.HostAllocator());
    view = VK_NULL_HANDLE;
  }
  if (handle != VK_NULL_HANDLE) {
    vkDestroyImage(device_context.Device(), handle, device_context.HostAllocator());
    handle = VK_NULL_HANDLE;
  }
}

int Image::LoadSubresourceFromMemory(const DeviceContext& device_context, ImageBlitter& blitter, const DeviceQueue *queue,
    const void* src_data, uint32_t src_row_nbytes, uint32_t src_layer_height,
    const VkImageSubresource& dst_subresource, VkImageLayout final_layout, VkAccessFlags final_access_flags) {
  assert(handle != VK_NULL_HANDLE);  // must create image first

  // Gimme a command buffer
  OneShotCommandPool cpool(device_context.Device(), queue->handle, queue->family, device_context.HostAllocator());
  VkCommandBuffer cb = cpool.AllocateAndBegin();

  // transition destination subresource into TRANSFER_DST most for loading
  VkImageMemoryBarrier barrier_init_to_dst = {};
  barrier_init_to_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier_init_to_dst.srcAccessMask = 0;
  barrier_init_to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier_init_to_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier_init_to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier_init_to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_init_to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_init_to_dst.image = handle;
  barrier_init_to_dst.subresourceRange.aspectMask = dst_subresource.aspectMask;
  barrier_init_to_dst.subresourceRange.baseArrayLayer = dst_subresource.arrayLayer;
  barrier_init_to_dst.subresourceRange.layerCount = 1;
  barrier_init_to_dst.subresourceRange.baseMipLevel = dst_subresource.mipLevel;
  barrier_init_to_dst.subresourceRange.levelCount = 1;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
    0, nullptr, 0, nullptr, 1, &barrier_init_to_dst);

  // Load!
  const ImageFormatAttributes& format_info = GetVkFormatInfo(image_ci.format);
  const int32_t texel_block_bytes  = format_info.texel_block_bytes;
  const int32_t texel_block_width  = format_info.texel_block_width;
  const int32_t texel_block_height = format_info.texel_block_height;
  const uint32_t i_mip = dst_subresource.mipLevel;
  const uint32_t i_layer = dst_subresource.arrayLayer;
  const void *subresource_data = src_data;

  VkBufferImageCopy copy_region = {};
  copy_region.bufferOffset = 0;
  // copy region dimensions are specified in pixels (not texel blocks or bytes), but must be
  // an even integer multiple of the texel block dimensions for compressed formats.
  // It must also respect the minImageTransferGranularity, but I don't have a good way of testing
  // that right now.  ...Wait, yes I do! It's in the DeviceQueue!
  copy_region.bufferRowLength = GetMipDimension(src_row_nbytes * texel_block_width / texel_block_bytes, i_mip);
  copy_region.bufferImageHeight = GetMipDimension(src_layer_height, i_mip);
  copy_region.bufferRowLength   = AlignTo(copy_region.bufferRowLength, texel_block_width);
  copy_region.bufferImageHeight = AlignTo(copy_region.bufferImageHeight, texel_block_height);
  copy_region.imageSubresource.aspectMask = dst_subresource.aspectMask;
  copy_region.imageSubresource.mipLevel = i_mip;
  copy_region.imageSubresource.baseArrayLayer = i_layer;
  copy_region.imageSubresource.layerCount = 1;  // can only copy one layer at a time
  copy_region.imageExtent.width  = AlignTo(GetMipDimension(image_ci.extent.width, i_mip), texel_block_width);
  copy_region.imageExtent.height = AlignTo(GetMipDimension(image_ci.extent.height, i_mip), texel_block_height);
  copy_region.imageExtent.depth  = GetMipDimension(image_ci.extent.depth, i_mip);
  blitter.CopyMemoryToImage(cb, handle, subresource_data, image_ci.format, copy_region);

  // transition to final layout/access
  VkImageMemoryBarrier barrier_dst_to_final = barrier_init_to_dst;
  barrier_dst_to_final.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier_dst_to_final.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier_dst_to_final.dstAccessMask = final_access_flags;
  barrier_dst_to_final.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier_dst_to_final.newLayout = final_layout;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_BY_REGION_BIT,
    0, nullptr, 0, nullptr, 1, &barrier_dst_to_final);

  cpool.EndSubmitAndFree(&cb);

  return 0;
}

int Image::GenerateMipmaps(const DeviceContext& device_context, const DeviceQueue *queue,
    const VkImageMemoryBarrier& barrier, uint32_t layer, uint32_t src_mip_level, uint32_t mips_to_gen) {
  assert(handle != VK_NULL_HANDLE);  // must create image first!

  // Gimme a command buffer
  OneShotCommandPool cpool(device_context.Device(), queue->handle, queue->family, device_context.HostAllocator());
  VkCommandBuffer cb = cpool.AllocateAndBegin();

  int err = GenerateMipmapsImpl(cb, barrier, layer, src_mip_level, mips_to_gen);
  if (err) {
    return err;
  }

  cpool.EndSubmitAndFree(&cb);
  return 0;
}

int Image::GenerateMipmapsImpl(VkCommandBuffer cb, const VkImageMemoryBarrier& dst_barrier,
    uint32_t layer, uint32_t src_mip_level, uint32_t mips_to_gen) {
  assert(mips_to_gen > 0);  // TODO(cort): graceful no-op?

  if (src_mip_level >= image_ci.mipLevels) {
    return -5;  // invalid src mip level
  } else if (src_mip_level == image_ci.mipLevels - 1) {
    return 0;  // nothing to do; src mip is already the last in the chain.
  }
  if (mips_to_gen == VK_REMAINING_MIP_LEVELS) {
    mips_to_gen = (image_ci.mipLevels - src_mip_level) - 1;
  }

#if 0  // validation -- higher-level code should be doing this already
  VkFormatProperties format_properties = {};
  vkGetPhysicalDeviceFormatProperties(device_context_.PhysicalDevice(), image_ci.format, &format_properties);
  const VkFormatFeatureFlags blit_mask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
  const VkFormatFeatureFlags feature_flags = (image_ci.tiling == VK_IMAGE_TILING_LINEAR)
    ? format_properties.linearTilingFeatures : format_properties.optimalTilingFeatures;
  if ( (feature_flags & blit_mask) != blit_mask ) {
    return -1;  // format does not support blitting; automatic mipmap generation won't work.
  }
#endif

  VkImageAspectFlags aspect_flags = GetImageAspectFlags(image_ci.format);

  // transition mip 0 to TRANSFER_READ, mip 1 to TRANSFER_WRITE
  std::array<VkImageMemoryBarrier,2> image_barriers = {};
  image_barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_barriers[0].srcAccessMask = dst_barrier.srcAccessMask;
  image_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  image_barriers[0].oldLayout = dst_barrier.oldLayout;
  image_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  image_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barriers[0].image = handle;
  image_barriers[0].subresourceRange.aspectMask = aspect_flags;
  image_barriers[0].subresourceRange.baseArrayLayer = layer;
  image_barriers[0].subresourceRange.layerCount = 1;
  image_barriers[0].subresourceRange.baseMipLevel = src_mip_level;
  image_barriers[0].subresourceRange.levelCount = 1;
  image_barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_barriers[1].srcAccessMask = 0;
  image_barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  image_barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barriers[1].image = handle;
  image_barriers[1].subresourceRange.aspectMask = aspect_flags;
  image_barriers[1].subresourceRange.baseArrayLayer = layer;
  image_barriers[1].subresourceRange.layerCount = 1;
  image_barriers[1].subresourceRange.baseMipLevel = src_mip_level + 1;
  image_barriers[1].subresourceRange.levelCount = mips_to_gen;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
    (VkDependencyFlags)0, 0,NULL, 0,NULL, (uint32_t)image_barriers.size(),image_barriers.data());
  // recycle image_barriers[0] to transition each dst_mip from TRANSFER_DST to TRANSFER_SRC after its blit
  image_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  image_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  image_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  image_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barriers[0].subresourceRange.baseMipLevel = src_mip_level+1;

  VkImageBlit blit_region = {};
  blit_region.srcSubresource.aspectMask = aspect_flags;
  blit_region.srcSubresource.baseArrayLayer = layer;
  blit_region.srcSubresource.layerCount = 1;
  blit_region.srcSubresource.mipLevel = src_mip_level;
  blit_region.srcOffsets[0].x = 0;
  blit_region.srcOffsets[0].y = 0;
  blit_region.srcOffsets[0].z = 0;
  blit_region.srcOffsets[1].x = GetMipDimension(image_ci.extent.width, src_mip_level);
  blit_region.srcOffsets[1].y = GetMipDimension(image_ci.extent.height, src_mip_level);
  blit_region.srcOffsets[1].z = GetMipDimension(image_ci.extent.depth, src_mip_level);
  blit_region.dstSubresource.aspectMask = aspect_flags;
  blit_region.dstSubresource.baseArrayLayer = layer;
  blit_region.dstSubresource.layerCount = 1;
  blit_region.dstSubresource.mipLevel = src_mip_level+1;
  blit_region.dstOffsets[0].x = 0;
  blit_region.dstOffsets[0].y = 0;
  blit_region.dstOffsets[0].z = 0;
  blit_region.dstOffsets[1].x = GetMipDimension(image_ci.extent.width, src_mip_level+1);
  blit_region.dstOffsets[1].y = GetMipDimension(image_ci.extent.height, src_mip_level+1);
  blit_region.dstOffsets[1].z = GetMipDimension(image_ci.extent.depth, src_mip_level+1);
  for(uint32_t dst_mip = src_mip_level+1; dst_mip <= src_mip_level+mips_to_gen; ++dst_mip) {
    vkCmdBlitImage(cb, handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1, &blit_region, VK_FILTER_LINEAR);
    if (dst_mip != src_mip_level+mips_to_gen) { // all but the last mip must be switched from WRITE/DST to READ/SRC
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        (VkDependencyFlags)0, 0,NULL, 0,NULL, 1,&image_barriers[0]);
    }
    image_barriers[0].subresourceRange.baseMipLevel += 1;

    blit_region.srcSubresource.mipLevel += 1;
    blit_region.srcOffsets[1].x = GetMipDimension(image_ci.extent.width, dst_mip);
    blit_region.srcOffsets[1].y = GetMipDimension(image_ci.extent.height, dst_mip);
    blit_region.srcOffsets[1].z = GetMipDimension(image_ci.extent.depth, dst_mip);
    blit_region.dstSubresource.mipLevel += 1;
    blit_region.dstOffsets[1].x = GetMipDimension(image_ci.extent.width, dst_mip+1);
    blit_region.dstOffsets[1].y = GetMipDimension(image_ci.extent.height, dst_mip+1);
    blit_region.dstOffsets[1].z = GetMipDimension(image_ci.extent.depth, dst_mip+1);
  }
  // Coming out of the loop, all but the last mip are in TRANSFER_SRC mode, and the last mip
  // is in TRANSFER_DST. Convert them all to the final layout/access mode.
  image_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  image_barriers[0].dstAccessMask = dst_barrier.dstAccessMask;
  image_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  image_barriers[0].newLayout = dst_barrier.newLayout;
  image_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barriers[0].subresourceRange.baseArrayLayer = layer;
  image_barriers[0].subresourceRange.layerCount = 1;
  image_barriers[0].subresourceRange.baseMipLevel = src_mip_level;
  image_barriers[0].subresourceRange.levelCount = mips_to_gen;
  image_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  image_barriers[1].dstAccessMask = dst_barrier.dstAccessMask;
  image_barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barriers[1].newLayout = dst_barrier.newLayout;
  image_barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barriers[1].subresourceRange.baseMipLevel = src_mip_level + mips_to_gen;
  image_barriers[1].subresourceRange.levelCount = 1;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    (VkDependencyFlags)0, 0,NULL, 0,NULL, (uint32_t)image_barriers.size(),image_barriers.data());

  return 0;
}

//
// ImageBlitter
//
ImageBlitter::ImageBlitter() 
  : current_pframe_(0),
    current_offset_(0) {
}
ImageBlitter::~ImageBlitter() {
}

VkResult ImageBlitter::Create(const DeviceContext& device_context, uint32_t pframe_count, VkDeviceSize staging_bytes_per_pframe) {
  VkBufferCreateInfo staging_buffer_ci = {};
  staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  staging_buffer_ci.size = staging_bytes_per_pframe;
  staging_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  staging_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VkResult result = staging_buffer_.Create(device_context, pframe_count, staging_buffer_ci,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);  // NOTE: not coherent! writes from the host must be invalidated!
  if (result != VK_SUCCESS) {
    return result;
  }

  staging_ranges_.resize(pframe_count);
  for(uint32_t i = 0; i < pframe_count; ++i) {
    staging_ranges_[i].start = staging_buffer_.Mapped(i);
    staging_ranges_[i].end = (void*)( uintptr_t(staging_ranges_[i].start) + staging_buffer_.BytesPerPframe() );
  }

  return VK_SUCCESS;
}

void ImageBlitter::Destroy(const DeviceContext& device_context) {
  staging_buffer_.Destroy(device_context);
}

int ImageBlitter::CopyMemoryToImage(VkCommandBuffer cb, VkImage dst_image, const void *src_data,
    VkFormat format, const VkBufferImageCopy& copy) {
  // If src_data is in the current staging buffer, skip the copy
  bool copy_src_to_staging = true;
  for(uint32_t pf = 0; pf < staging_buffer_.Depth(); ++pf) {
    if (src_data >= staging_ranges_[pf].start && src_data < staging_ranges_[pf].end) {
      if (pf == current_pframe_) {
        copy_src_to_staging = false;
        break;
      } else {
        return -1;  // src_data is in the wrong pframe's staging buffer!
      }
    }
  }

  // Determine size of the source data.
  const ImageFormatAttributes& format_attr = GetVkFormatInfo(format);
  assert((copy.bufferRowLength   % format_attr.texel_block_width) == 0);
  assert((copy.bufferImageHeight % format_attr.texel_block_height) == 0);
  assert((copy.imageOffset.x % format_attr.texel_block_width) == 0);
  assert((copy.imageOffset.y % format_attr.texel_block_height) == 0);
  assert((copy.bufferOffset % format_attr.texel_block_bytes) == 0);
  assert((copy.imageExtent.width  % format_attr.texel_block_width) == 0);
  assert((copy.imageExtent.height % format_attr.texel_block_height) == 0);
  assert(copy.imageSubresource.layerCount == 1);
  // bufferRowLength=0 or imageHeight=0 means those dimensions are tightly packed according to the image extent.
  const uint32_t row_length_pixels = (copy.bufferRowLength != 0) ? copy.bufferRowLength : copy.imageExtent.width;
  const uint32_t num_pixels = row_length_pixels * (copy.imageExtent.height-1) + copy.imageExtent.width;
  const VkDeviceSize src_nbytes = (num_pixels / (format_attr.texel_block_width * format_attr.texel_block_height))
    * GetVkFormatInfo(format).texel_block_bytes;

  VkBufferImageCopy copy_final = copy;
  if (!copy_src_to_staging) {  // source data is already in the staging buffer.
    copy_final.bufferOffset = uintptr_t(src_data) - uintptr_t(staging_ranges_[current_pframe_].start);
  } else {  // Copy source data to staging buffer.
    // Allocate space from the staging buffer for the src data.
    // TODO(cort): align base address?  test with current_offset_ = 1.
    if (current_offset_ + src_nbytes >= staging_buffer_.BytesPerPframe()) {
      return -2;  // Not enough room in the staging buffer; make it larger!
    }
    void *staging_src = (void*)( uintptr_t(staging_ranges_[current_pframe_].start) + current_offset_ );
    memcpy(staging_src, src_data, src_nbytes);

    copy_final.bufferOffset = current_offset_;
    current_offset_ += src_nbytes;
  }
  staging_buffer_.InvalidatePframeHostCache(current_pframe_, current_offset_, src_nbytes);

  // Assume dst_image is already in TRANSFER_DST layout, TRANSFER_READ access, and owned by the appropriate queue family.
  // The staging buffer must be transferred from HOST_WRITE to TRANSFER_SRC
  VkBufferMemoryBarrier buffer_barrier = {};
  buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  buffer_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
  buffer_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  buffer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  buffer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  buffer_barrier.buffer = staging_buffer_.Handle(current_pframe_);
  buffer_barrier.offset = copy_final.bufferOffset;
  buffer_barrier.size = src_nbytes;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
    0,nullptr, 1,&buffer_barrier, 0,nullptr);
  vkCmdCopyBufferToImage(cb, staging_buffer_.Handle(current_pframe_), dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1, &copy_final);
  // transition staging buffer back to host access
  buffer_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  buffer_barrier.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
    0,nullptr, 1,&buffer_barrier, 0,nullptr);

  return 0;
}

void ImageBlitter::NextPframe(void) {
  current_pframe_ = (current_pframe_ == staging_buffer_.Depth() - 1) ? 0 : (current_pframe_ + 1);
  current_offset_ = 0;
}

}  // namespace spokk
