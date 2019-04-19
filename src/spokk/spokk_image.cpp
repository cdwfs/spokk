#include "spokk_image.h"

#include "image_file.h"
#include "spokk_barrier.h"
#include "spokk_debug.h"
#include "spokk_utilities.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <array>

namespace {
struct ImageFormatAttributes {
  int32_t texel_block_bytes;
  int32_t texel_block_width;
  int32_t texel_block_height;
  ImageFileDataFormat image_format;  // primary key; g_format_attributes[img_fmt].image_format == img_fmt
  VkFormat vk_format;
};
// clang-format off
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
// clang-format on

const ImageFormatAttributes& GetVkFormatInfo(VkFormat format) {
  for (const auto& attr : g_format_attributes) {
    if (attr.vk_format == format) {
      return attr;
    }
  }
  assert(0);  // not found!
  return g_format_attributes[IMAGE_FILE_DATA_FORMAT_UNKNOWN];
}

void ImageFileToVkImageCreateInfo(VkImageCreateInfo* out_ci, const ImageFile& image) {
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
  out_ci->extent.width = image.width;
  out_ci->extent.height = image.height;
  out_ci->extent.depth = image.depth;
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
  uint32_t out = (base >> mip);
  return (out < 1) ? 1 : out;
}

uint32_t AlignTo(uint32_t x, uint32_t n) {
  assert((n & (n - 1)) == 0);  // n must be a power of 2
  return (x + n - 1) & ~(n - 1);
}

}  // namespace

namespace spokk {

//
// Image
//
VkResult Image::Create(const Device& device, const VkImageCreateInfo& ci, VkMemoryPropertyFlags memory_properties,
    DeviceAllocationScope allocation_scope) {
  ZOMBO_ASSERT_RETURN(handle == VK_NULL_HANDLE, VK_ERROR_INITIALIZATION_FAILED, "Can't re-create an existing Image");
  image_ci = ci;
  VkResult result = vkCreateImage(device, &ci, device.HostAllocator(), &handle);
  if (result != VK_SUCCESS) {
    return result;
  }
  result = device.DeviceAllocAndBindToImage(handle, memory_properties, allocation_scope, &memory);
  if (result != VK_SUCCESS) {
    vkDestroyImage(device, handle, device.HostAllocator());
    handle = VK_NULL_HANDLE;
    return result;
  }
  VkImageViewCreateInfo view_ci = GetImageViewCreateInfo(handle, ci);
  result = vkCreateImageView(device, &view_ci, device.HostAllocator(), &view);
  if (result != VK_SUCCESS) {
    vkDestroyImage(device, handle, device.HostAllocator());
    handle = VK_NULL_HANDLE;
    return result;
  }
  return result;
}
int Image::CreateFromFile(const Device& device, const DeviceQueue* queue, const std::string& filename,
    VkBool32 generate_mipmaps, ThsvsAccessType final_access) {
  ZOMBO_ASSERT_RETURN(handle == VK_NULL_HANDLE, -1, "Can't re-create an existing Image");

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
    vkGetPhysicalDeviceFormatProperties(device.Physical(), image_ci.format, &format_properties);
    const VkFormatFeatureFlags blit_mask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    const VkFormatFeatureFlags feature_flags = (image_ci.tiling == VK_IMAGE_TILING_LINEAR)
        ? format_properties.linearTilingFeatures
        : format_properties.optimalTilingFeatures;
    if ((feature_flags & blit_mask) != blit_mask) {
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
  SPOKK_VK_CHECK(vkCreateImage(device, &image_ci, device.HostAllocator(), &handle));
  SPOKK_VK_CHECK(device.DeviceAllocAndBindToImage(
      handle, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DEVICE_ALLOCATION_SCOPE_DEVICE, &memory));

  // Gimme a command buffer
  OneShotCommandPool cpool(device, *queue, queue->family, device.HostAllocator());
  VkCommandBuffer cb = cpool.AllocateAndBegin();

  // Load those mips!
  int32_t texel_block_bytes = g_format_attributes[image_file.data_format].texel_block_bytes;
  int32_t texel_block_width = g_format_attributes[image_file.data_format].texel_block_width;
  int32_t texel_block_height = g_format_attributes[image_file.data_format].texel_block_height;
  // TODO(cort): move staging buffer into Device?
  size_t total_upload_size = 0;
  for (uint32_t iMip = 0; iMip < mips_to_load; ++iMip) {
    ImageFileSubresource subresource = {};
    subresource.mip_level = iMip;
    total_upload_size += ImageFileGetSubresourceSize(&image_file, subresource) * image_file.array_layers;
  }
  VkBufferCreateInfo staging_buffer_ci = {};
  staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  staging_buffer_ci.size = total_upload_size;
  staging_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  staging_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  Buffer staging_buffer = {};
  SPOKK_VK_CHECK(staging_buffer.Create(
      device, staging_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, spokk::DEVICE_ALLOCATION_SCOPE_FRAME));
  // transition image into TRANSFER_DST for loading
  ThsvsAccessType src_access = THSVS_ACCESS_NONE;
  ThsvsAccessType dst_access = THSVS_ACCESS_TRANSFER_WRITE;
  ThsvsImageBarrier th_barrier_init_to_dst = {};
  th_barrier_init_to_dst.prevAccessCount = 1;
  th_barrier_init_to_dst.pPrevAccesses = &src_access;
  th_barrier_init_to_dst.nextAccessCount = 1;
  th_barrier_init_to_dst.pNextAccesses = &dst_access;
  th_barrier_init_to_dst.prevLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_barrier_init_to_dst.nextLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_barrier_init_to_dst.discardContents = VK_TRUE;
  th_barrier_init_to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_barrier_init_to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_barrier_init_to_dst.image = handle;
  th_barrier_init_to_dst.subresourceRange.aspectMask = GetImageAspectFlags(image_ci.format);
  th_barrier_init_to_dst.subresourceRange.baseArrayLayer = 0;
  th_barrier_init_to_dst.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
  th_barrier_init_to_dst.subresourceRange.baseMipLevel = 0;
  th_barrier_init_to_dst.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  VkImageMemoryBarrier barrier_init_to_dst = {};
  VkPipelineStageFlags barrier_src_stages = 0, barrier_dst_stages = 0;
  thsvsGetVulkanImageMemoryBarrier(
      th_barrier_init_to_dst, &barrier_src_stages, &barrier_dst_stages, &barrier_init_to_dst);
  // barrier between host writes and transfer reads
  VkMemoryBarrier staging_buffer_memory_barrier = {};
  spokk::BuildVkMemoryBarrier(THSVS_ACCESS_HOST_WRITE, THSVS_ACCESS_TRANSFER_READ, &barrier_src_stages,
      &barrier_dst_stages, &staging_buffer_memory_barrier);
  // emit both barriers
  vkCmdPipelineBarrier(cb, barrier_src_stages, barrier_dst_stages, 0, 1, &staging_buffer_memory_barrier, 0, nullptr, 1,
      &barrier_init_to_dst);
  VkDeviceSize src_offset = 0;
  for (uint32_t i_mip = 0; i_mip < mips_to_load; ++i_mip) {
    ImageFileSubresource subresource;
    subresource.array_layer = 0;
    subresource.mip_level = i_mip;
    size_t subresource_size = ImageFileGetSubresourceSize(&image_file, subresource);
    for (uint32_t i_layer = 0; i_layer < image_file.array_layers; ++i_layer) {
      // Copy subresource into staging buffer
      subresource.array_layer = i_layer;
      const void* subresource_data = ImageFileGetSubresourceData(&image_file, subresource);
      memcpy((uint8_t*)(staging_buffer.Mapped()) + src_offset, subresource_data, subresource_size);
      // Emit commands to copy subresource from staging buffer to image
      VkBufferImageCopy copy_region = {};
      copy_region.bufferOffset = src_offset;
      // copy region dimensions are specified in pixels (not texel blocks or bytes), but must be
      // an even integer multiple of the texel block dimensions for compressed formats.
      // It must also respect the minImageTransferGranularity, but in practice that just means we
      // need to transfer whole mips here, which we are.
      copy_region.bufferRowLength =
          GetMipDimension(image_file.row_pitch_bytes * texel_block_width / texel_block_bytes, i_mip);
      copy_region.bufferImageHeight = GetMipDimension(image_file.height, i_mip);
      copy_region.bufferRowLength = AlignTo(copy_region.bufferRowLength, texel_block_width);
      copy_region.bufferImageHeight = AlignTo(copy_region.bufferImageHeight, texel_block_height);
      copy_region.imageSubresource.aspectMask = aspect_flags;
      copy_region.imageSubresource.mipLevel = i_mip;
      copy_region.imageSubresource.baseArrayLayer = i_layer;
      copy_region.imageSubresource.layerCount = 1;  // TODO(cort): copy all layers from a single mip in one go?
      copy_region.imageExtent.width = AlignTo(GetMipDimension(image_file.width, i_mip), texel_block_width);
      copy_region.imageExtent.height = AlignTo(GetMipDimension(image_file.height, i_mip), texel_block_height);
      copy_region.imageExtent.depth = GetMipDimension(image_file.depth, i_mip);
      vkCmdCopyBufferToImage(
          cb, staging_buffer.Handle(), handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
      src_offset += subresource_size;
    }
  }

  // Generate remaining mips, if requested
  ThsvsImageBarrier th_barrier_dst_to_final = th_barrier_init_to_dst;
  src_access = THSVS_ACCESS_TRANSFER_WRITE;
  dst_access = final_access;
  if (generate_mipmaps) {
    for (uint32_t i_layer = 0; i_layer < image_file.array_layers; ++i_layer) {
      GenerateMipmapsImpl(cb, th_barrier_dst_to_final, i_layer, 0, image_ci.mipLevels - 1);
    }
  } else {
    // transition to final layout/access
    VkImageMemoryBarrier barrier_dst_to_final = {};
    barrier_src_stages = 0;
    barrier_dst_stages = 0;
    thsvsGetVulkanImageMemoryBarrier(
        th_barrier_dst_to_final, &barrier_src_stages, &barrier_dst_stages, &barrier_dst_to_final);
    vkCmdPipelineBarrier(cb, barrier_src_stages, barrier_dst_stages, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0,
        nullptr, 1, &barrier_dst_to_final);
  }
  SPOKK_VK_CHECK(staging_buffer.FlushHostCache(device));
  cpool.EndSubmitAndFree(&cb);
  staging_buffer.Destroy(device);
  ImageFileDestroy(&image_file);

  VkImageViewCreateInfo view_ci = GetImageViewCreateInfo(handle, image_ci);
  VkResult result = vkCreateImageView(device, &view_ci, device.HostAllocator(), &view);
  if (result != VK_SUCCESS) {
    Destroy(device);
    return -1;
  }

  if (result == VK_SUCCESS) {
    result = device.SetObjectName(handle, filename);
  }
  if (result == VK_SUCCESS) {
    result = device.SetObjectName(view, filename + " view");
  }

  return 0;
}

void Image::Destroy(const Device& device) {
  device.DeviceFree(memory);
  if (view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, view, device.HostAllocator());
    view = VK_NULL_HANDLE;
  }
  if (handle != VK_NULL_HANDLE) {
    vkDestroyImage(device, handle, device.HostAllocator());
    handle = VK_NULL_HANDLE;
  }
}

int Image::LoadSubresourceFromMemory(const Device& device, const DeviceQueue* queue, const void* src_data,
    size_t src_nbytes, uint32_t src_row_nbytes, uint32_t src_layer_height, const VkImageSubresource& dst_subresource,
    ThsvsAccessType final_access) {
  ZOMBO_ASSERT_RETURN(handle != VK_NULL_HANDLE, -1, "Call Create() first!");

  // Gimme a command buffer
  OneShotCommandPool cpool(device, *queue, queue->family, device.HostAllocator());
  VkCommandBuffer cb = cpool.AllocateAndBegin();

  // TODO(cort): global staging buffer
  VkBufferCreateInfo staging_buffer_ci = {};
  staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  staging_buffer_ci.size = src_nbytes;
  staging_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  staging_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  Buffer staging_buffer = {};
  SPOKK_VK_CHECK(staging_buffer.Create(
      device, staging_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, spokk::DEVICE_ALLOCATION_SCOPE_FRAME));
  memcpy((uint8_t*)staging_buffer.Mapped(), src_data, src_nbytes);
  // transition destination subresource into TRANSFER_DST for loading
  ThsvsAccessType src_access = THSVS_ACCESS_NONE;
  ThsvsAccessType dst_access = THSVS_ACCESS_TRANSFER_WRITE;
  ThsvsImageBarrier th_barrier_init_to_dst = {};
  th_barrier_init_to_dst.prevAccessCount = 1;
  th_barrier_init_to_dst.pPrevAccesses = &src_access;
  th_barrier_init_to_dst.nextAccessCount = 1;
  th_barrier_init_to_dst.pNextAccesses = &dst_access;
  th_barrier_init_to_dst.prevLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_barrier_init_to_dst.nextLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_barrier_init_to_dst.discardContents = VK_TRUE;
  th_barrier_init_to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_barrier_init_to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_barrier_init_to_dst.image = handle;
  th_barrier_init_to_dst.subresourceRange.aspectMask = dst_subresource.aspectMask;
  th_barrier_init_to_dst.subresourceRange.baseArrayLayer = dst_subresource.arrayLayer;
  th_barrier_init_to_dst.subresourceRange.layerCount = 1;
  th_barrier_init_to_dst.subresourceRange.baseMipLevel = dst_subresource.mipLevel;
  th_barrier_init_to_dst.subresourceRange.levelCount = 1;
  VkImageMemoryBarrier barrier_init_to_dst = {};
  VkPipelineStageFlags barrier_src_stages = 0, barrier_dst_stages = 0;
  thsvsGetVulkanImageMemoryBarrier(
      th_barrier_init_to_dst, &barrier_src_stages, &barrier_dst_stages, &barrier_init_to_dst);
  // barrier between host writes and transfer reads
  VkMemoryBarrier staging_buffer_memory_barrier = {};
  spokk::BuildVkMemoryBarrier(THSVS_ACCESS_HOST_WRITE, THSVS_ACCESS_TRANSFER_READ, &barrier_src_stages,
      &barrier_dst_stages, &staging_buffer_memory_barrier);
  // emit both barriers
  vkCmdPipelineBarrier(cb, barrier_src_stages, barrier_dst_stages, 0, 1, &staging_buffer_memory_barrier, 0, nullptr, 1,
      &barrier_init_to_dst);

  // Load!
  const ImageFormatAttributes& format_info = GetVkFormatInfo(image_ci.format);
  const int32_t texel_block_bytes = format_info.texel_block_bytes;
  const int32_t texel_block_width = format_info.texel_block_width;
  const int32_t texel_block_height = format_info.texel_block_height;

  VkBufferImageCopy copy_region = {};
  copy_region.bufferOffset = 0;
  // copy region dimensions are specified in pixels (not texel blocks or bytes), but must be
  // an even integer multiple of the texel block dimensions for compressed formats.
  // It must also respect the DeviceQueue's minImageTransferGranularity, but copying full mip levels is
  // always supported, and that's all I'm doing here.
  ZOMBO_ASSERT_RETURN((src_row_nbytes % texel_block_bytes) == 0, -1,
      "src_row_nbytes (%d) must be a multiple of image's texel_block_bytes (%d)", src_row_nbytes, texel_block_bytes);
  ZOMBO_ASSERT_RETURN((src_layer_height % texel_block_height) == 0, -1,
      "src_layer_height (%d) must be a multiple of image's texel_block_height (%d)", src_layer_height,
      texel_block_height);
  copy_region.bufferRowLength = src_row_nbytes * texel_block_width / texel_block_bytes;
  copy_region.bufferImageHeight = src_layer_height;
  copy_region.imageSubresource.aspectMask = dst_subresource.aspectMask;
  copy_region.imageSubresource.mipLevel = dst_subresource.mipLevel;
  copy_region.imageSubresource.baseArrayLayer = dst_subresource.arrayLayer;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageExtent.width =
      AlignTo(GetMipDimension(image_ci.extent.width, dst_subresource.mipLevel), texel_block_width);
  copy_region.imageExtent.height =
      AlignTo(GetMipDimension(image_ci.extent.height, dst_subresource.mipLevel), texel_block_height);
  copy_region.imageExtent.depth = GetMipDimension(image_ci.extent.depth, dst_subresource.mipLevel);
  vkCmdCopyBufferToImage(cb, staging_buffer.Handle(), handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

  // transition to final layout/access
  ThsvsImageBarrier th_barrier_dst_to_final = th_barrier_init_to_dst;
  src_access = THSVS_ACCESS_TRANSFER_WRITE;
  dst_access = final_access;
  VkImageMemoryBarrier barrier_dst_to_final = {};
  barrier_src_stages = 0;
  barrier_dst_stages = 0;
  thsvsGetVulkanImageMemoryBarrier(
      th_barrier_dst_to_final, &barrier_src_stages, &barrier_dst_stages, &barrier_dst_to_final);
  vkCmdPipelineBarrier(cb, barrier_src_stages, barrier_dst_stages, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr,
      1, &barrier_dst_to_final);

  SPOKK_VK_CHECK(staging_buffer.FlushHostCache(device));
  cpool.EndSubmitAndFree(&cb);
  staging_buffer.Destroy(device);
  return 0;
}

int Image::GenerateMipmaps(const Device& device, const DeviceQueue* queue, const ThsvsImageBarrier& barrier,
    uint32_t layer, uint32_t src_mip_level, uint32_t mips_to_gen) {
  ZOMBO_ASSERT(handle != VK_NULL_HANDLE, "must create image first!");

  // Gimme a command buffer
  OneShotCommandPool cpool(device, *queue, queue->family, device.HostAllocator());
  VkCommandBuffer cb = cpool.AllocateAndBegin();

  int err = GenerateMipmapsImpl(cb, barrier, layer, src_mip_level, mips_to_gen);
  if (err) {
    return err;
  }

  cpool.EndSubmitAndFree(&cb);
  return 0;
}

int Image::GenerateMipmapsImpl(VkCommandBuffer cb, const ThsvsImageBarrier& dst_barrier, uint32_t layer,
    uint32_t src_mip_level, uint32_t mips_to_gen) {
  if (mips_to_gen == 0) {
    return 0;  // nothing to do
  }
  if (src_mip_level >= image_ci.mipLevels) {
    return -5;  // invalid src mip level
  } else if (src_mip_level == image_ci.mipLevels - 1) {
    return 0;  // nothing to do; src mip is already the last in the chain.
  }
  if (mips_to_gen == VK_REMAINING_MIP_LEVELS) {
    mips_to_gen = (image_ci.mipLevels - src_mip_level) - 1;
  }

  VkImageAspectFlags aspect_flags = GetImageAspectFlags(image_ci.format);

  // transition mip 0 to TRANSFER_READ, mip 1 to TRANSFER_WRITE
  const ThsvsAccessType access_none = THSVS_ACCESS_NONE;
  const ThsvsAccessType access_transfer_read = THSVS_ACCESS_TRANSFER_READ;
  const ThsvsAccessType access_transfer_write = THSVS_ACCESS_TRANSFER_WRITE;
  std::array<ThsvsImageBarrier, 2> th_image_barriers = {};
  th_image_barriers[0].prevAccessCount = dst_barrier.prevAccessCount;
  th_image_barriers[0].pPrevAccesses = dst_barrier.pPrevAccesses;
  th_image_barriers[0].nextAccessCount = 1;
  th_image_barriers[0].pNextAccesses = &access_transfer_read;
  th_image_barriers[0].prevLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_image_barriers[0].nextLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_image_barriers[0].discardContents = VK_FALSE;
  th_image_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_image_barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_image_barriers[0].image = handle;
  th_image_barriers[0].subresourceRange.aspectMask = aspect_flags;
  th_image_barriers[0].subresourceRange.baseArrayLayer = layer;
  th_image_barriers[0].subresourceRange.layerCount = 1;
  th_image_barriers[0].subresourceRange.baseMipLevel = src_mip_level;
  th_image_barriers[0].subresourceRange.levelCount = 1;
  th_image_barriers[1].prevAccessCount = 1;
  th_image_barriers[1].pPrevAccesses = &access_none;
  th_image_barriers[1].nextAccessCount = 1;
  th_image_barriers[1].pNextAccesses = &access_transfer_write;
  th_image_barriers[1].prevLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_image_barriers[1].nextLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_image_barriers[1].discardContents = VK_TRUE;
  th_image_barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_image_barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_image_barriers[1].image = handle;
  th_image_barriers[1].subresourceRange.aspectMask = aspect_flags;
  th_image_barriers[1].subresourceRange.baseArrayLayer = layer;
  th_image_barriers[1].subresourceRange.layerCount = 1;
  th_image_barriers[1].subresourceRange.baseMipLevel = src_mip_level + 1;
  th_image_barriers[1].subresourceRange.levelCount = 1;
  std::array<VkImageMemoryBarrier, 2> image_barriers = {};
  VkPipelineStageFlags image_barrier_src_stages = 0, image_barrier_dst_stages = 0;
  thsvsGetVulkanImageMemoryBarrier(
      th_image_barriers[0], &image_barrier_src_stages, &image_barrier_dst_stages, &image_barriers[0]);
  thsvsGetVulkanImageMemoryBarrier(
      th_image_barriers[1], &image_barrier_src_stages, &image_barrier_dst_stages, &image_barriers[1]);
  vkCmdPipelineBarrier(cb, image_barrier_src_stages, image_barrier_dst_stages, (VkDependencyFlags)0, 0, nullptr, 0,
      nullptr, (uint32_t)image_barriers.size(), image_barriers.data());
  // recycle image_barriers[0] to transition each dst_mip from TRANSFER_DST to TRANSFER_SRC after its blit
  image_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  image_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  image_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  image_barriers[0].subresourceRange.baseMipLevel = src_mip_level + 1;

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
  blit_region.dstSubresource.mipLevel = src_mip_level + 1;
  blit_region.dstOffsets[0].x = 0;
  blit_region.dstOffsets[0].y = 0;
  blit_region.dstOffsets[0].z = 0;
  blit_region.dstOffsets[1].x = GetMipDimension(image_ci.extent.width, src_mip_level + 1);
  blit_region.dstOffsets[1].y = GetMipDimension(image_ci.extent.height, src_mip_level + 1);
  blit_region.dstOffsets[1].z = GetMipDimension(image_ci.extent.depth, src_mip_level + 1);
  for (uint32_t dst_mip = src_mip_level + 1; dst_mip <= src_mip_level + mips_to_gen; ++dst_mip) {
    vkCmdBlitImage(cb, handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
        &blit_region, VK_FILTER_LINEAR);
    if (dst_mip != src_mip_level + mips_to_gen) {  // all but the last mip must be switched from WRITE/DST to READ/SRC
      vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
          VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, NULL, 0, NULL, 1,
          &image_barriers[0]);
    }
    image_barriers[0].subresourceRange.baseMipLevel += 1;

    blit_region.srcSubresource.mipLevel += 1;
    blit_region.srcOffsets[1].x = GetMipDimension(image_ci.extent.width, dst_mip);
    blit_region.srcOffsets[1].y = GetMipDimension(image_ci.extent.height, dst_mip);
    blit_region.srcOffsets[1].z = GetMipDimension(image_ci.extent.depth, dst_mip);
    blit_region.dstSubresource.mipLevel += 1;
    blit_region.dstOffsets[1].x = GetMipDimension(image_ci.extent.width, dst_mip + 1);
    blit_region.dstOffsets[1].y = GetMipDimension(image_ci.extent.height, dst_mip + 1);
    blit_region.dstOffsets[1].z = GetMipDimension(image_ci.extent.depth, dst_mip + 1);
  }
  // Coming out of the loop, all but the last mip are in TRANSFER_SRC mode, and the last mip
  // is in TRANSFER_DST. Convert them all to the final layout/access mode.
  th_image_barriers[0].prevAccessCount = 1;
  th_image_barriers[0].pPrevAccesses = &access_transfer_read;
  th_image_barriers[0].nextAccessCount = dst_barrier.nextAccessCount;
  th_image_barriers[0].pNextAccesses = dst_barrier.pNextAccesses;
  th_image_barriers[0].prevLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_image_barriers[0].nextLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_image_barriers[0].discardContents = VK_FALSE;
  th_image_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_image_barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_image_barriers[0].image = handle;
  th_image_barriers[0].subresourceRange.aspectMask = aspect_flags;
  th_image_barriers[0].subresourceRange.baseArrayLayer = layer;
  th_image_barriers[0].subresourceRange.layerCount = 1;
  th_image_barriers[0].subresourceRange.baseMipLevel = src_mip_level;
  th_image_barriers[0].subresourceRange.levelCount = mips_to_gen;
  th_image_barriers[1].prevAccessCount = 1;
  th_image_barriers[1].pPrevAccesses = &access_transfer_write;
  th_image_barriers[1].nextAccessCount = dst_barrier.nextAccessCount;
  th_image_barriers[1].pNextAccesses = dst_barrier.pNextAccesses;
  th_image_barriers[1].prevLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_image_barriers[1].nextLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
  th_image_barriers[1].discardContents = VK_FALSE;
  th_image_barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_image_barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  th_image_barriers[1].image = handle;
  th_image_barriers[1].subresourceRange.aspectMask = aspect_flags;
  th_image_barriers[1].subresourceRange.baseArrayLayer = layer;
  th_image_barriers[1].subresourceRange.layerCount = 1;
  th_image_barriers[1].subresourceRange.baseMipLevel = src_mip_level + mips_to_gen;
  th_image_barriers[1].subresourceRange.levelCount = 1;

  image_barrier_src_stages = 0;
  image_barrier_dst_stages = 0;
  thsvsGetVulkanImageMemoryBarrier(
      th_image_barriers[0], &image_barrier_src_stages, &image_barrier_dst_stages, &image_barriers[0]);
  thsvsGetVulkanImageMemoryBarrier(
      th_image_barriers[1], &image_barrier_src_stages, &image_barrier_dst_stages, &image_barriers[1]);
  vkCmdPipelineBarrier(cb, image_barrier_src_stages, image_barrier_dst_stages, (VkDependencyFlags)0, 0, nullptr, 0,
      nullptr, (uint32_t)image_barriers.size(), image_barriers.data());

  return 0;
}

}  // namespace spokk
