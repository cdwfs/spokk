#include "vk_application.h"  // for DeviceContext
#include "vk_debug.h"
#include "vk_texture.h"
using namespace cdsvk;

#include "image_file.h"

#include <array>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

namespace {
  VkImageAspectFlags vk_format_to_image_aspect(VkFormat format) {
    switch(format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
      return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    case VK_FORMAT_UNDEFINED:
      return static_cast<VkImageAspectFlagBits>(0);
    default:
      return VK_IMAGE_ASPECT_COLOR_BIT;
    }
  }

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
    void image_file_to_vk_image_create_info(VkImageCreateInfo *out_ci, const ImageFile &image) {
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

    uint32_t get_mip_dimension(uint32_t base, uint32_t mip) {
      uint32_t out = (base>>mip);
      return (out < 1) ? 1 : out;
    }

    uint32_t align_to_n(uint32_t x, uint32_t n) {
      assert( (n & (n-1)) == 0); // n must be a power of 2
      return (x + n-1) & ~(n-1);
    }

}  // namespace

TextureLoader::TextureLoader(const DeviceContext& device_context) :
    device_context_(device_context) {
  const DeviceQueueContext* transfer_queue_context = device_context.find_queue_context(VK_QUEUE_TRANSFER_BIT);
  assert(transfer_queue_context != nullptr);
  transfer_queue_ = transfer_queue_context->queue;
  transfer_queue_family_ = transfer_queue_context->queue_family;
  one_shot_cpool_ = my_make_unique<OneShotCommandPool>(device_context.device(),
    transfer_queue_, transfer_queue_family_, device_context.host_allocator());
}
TextureLoader::~TextureLoader() {
}


int TextureLoader::load_vkimage_from_file(VkImage *out_image, VkImageCreateInfo *out_image_ci,
    DeviceMemoryAllocation *out_memory, const std::string &filename, VkBool32 generate_mipmaps,
    VkImageLayout final_layout, VkAccessFlags final_access_flags) const {
  int err = 0;

  // Load image file
  ImageFile image_file;
  err = ImageFileCreate(&image_file, filename.c_str());
  if (err != 0) {
    return err;
  }
  image_file_to_vk_image_create_info(out_image_ci, image_file);
  VkImageAspectFlags aspect_flags = vk_format_to_image_aspect(out_image_ci->format);
  uint32_t mips_to_load = image_file.mip_levels;

  if (generate_mipmaps) {
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(device_context_.physical_device(), out_image_ci->format, &format_properties);
    const VkFormatFeatureFlags blit_mask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    const VkFormatFeatureFlags feature_flags = (out_image_ci->tiling == VK_IMAGE_TILING_LINEAR)
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
      out_image_ci->usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  // needed for self-blitting
      // Reserve space for the full mip chain...
      out_image_ci->mipLevels = num_mip_levels;
      // ...but only load the base level from the image file.
      mips_to_load = 1;
    }
  }

  // Create staging buffer (large enough to hold all subresources we're loading) and map it to host memory.
  VkBufferCreateInfo staging_buffer_ci = {};
  staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  staging_buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  staging_buffer_ci.size = 0;  // set below
  staging_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ImageFileSubresource subresource = {};
  for(uint32_t i_mip=0; i_mip<mips_to_load; ++i_mip) {
    subresource.mip_level = i_mip;
    for(uint32_t i_layer=0; i_layer<image_file.array_layers; ++i_layer) {
      subresource.array_layer = i_layer;
      staging_buffer_ci.size += ImageFileGetSubresourceSize(&image_file, subresource);
    }
  }
  VkBuffer staging_buffer = VK_NULL_HANDLE;
  CDSVK_CHECK(vkCreateBuffer(device_context_.device(), &staging_buffer_ci, device_context_.host_allocator(), &staging_buffer));
  DeviceMemoryAllocation staging_buffer_mem = device_context_.device_alloc_and_bind_to_buffer(staging_buffer,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,  // TODO(cort): manually flush/invalidate
    DEVICE_ALLOCATION_SCOPE_DEVICE);
  uint8_t *staging_buffer_data = reinterpret_cast<uint8_t*>(staging_buffer_mem.mapped());
  assert(staging_buffer_data != nullptr);

  // Populate staging buffer, and build a list of regions to copy into the final image.
  std::vector<VkBufferImageCopy> copy_regions(mips_to_load);
  size_t staging_offset = 0;
  int32_t texel_block_bytes = g_format_attributes[image_file.data_format].texel_block_bytes;
  int32_t texel_block_width = g_format_attributes[image_file.data_format].texel_block_width;
  int32_t texel_block_height = g_format_attributes[image_file.data_format].texel_block_height;
  for(uint32_t i_mip=0; i_mip<mips_to_load; ++i_mip) {
    subresource.mip_level = i_mip;
    subresource.array_layer = 0;
    copy_regions[i_mip].bufferOffset = staging_offset;
    // copy region dimensions are specified in pixels (not texel blocks or bytes), but must be
    // an even integer multiple of the texel block dimensions for compressed formats.
    // It must also respect the minImageTransferGranularity, but I don't have a good way of testing
    // that right now.
    copy_regions[i_mip].bufferRowLength = get_mip_dimension(
      image_file.row_pitch_bytes * texel_block_width / texel_block_bytes, i_mip);
    copy_regions[i_mip].bufferImageHeight = get_mip_dimension(image_file.height, i_mip) * texel_block_height;
    copy_regions[i_mip].bufferRowLength   = align_to_n(copy_regions[i_mip].bufferRowLength, texel_block_width);
    copy_regions[i_mip].bufferImageHeight = align_to_n(copy_regions[i_mip].bufferImageHeight, texel_block_height);
    copy_regions[i_mip].imageSubresource.aspectMask = aspect_flags;
    copy_regions[i_mip].imageSubresource.mipLevel = i_mip;
    copy_regions[i_mip].imageSubresource.baseArrayLayer = 0;  // TODO(cort): take a VkImageSubresourceRange?
    copy_regions[i_mip].imageSubresource.layerCount = image_file.array_layers;
    copy_regions[i_mip].imageExtent.width  = get_mip_dimension(image_file.width, i_mip);
    copy_regions[i_mip].imageExtent.height = get_mip_dimension(image_file.height, i_mip);
    copy_regions[i_mip].imageExtent.depth  = get_mip_dimension(image_file.depth, i_mip);
    for(uint32_t i_layer=0; i_layer<image_file.array_layers; ++i_layer) {
      subresource.array_layer = i_layer;
      size_t subresource_size = ImageFileGetSubresourceSize(&image_file, subresource);
      memcpy(staging_buffer_data + staging_offset, ImageFileGetSubresourceData(&image_file, subresource),
        subresource_size);
      staging_offset += subresource_size;
    }
  }

  // Create final image.
  // TODO(cort): take memory properties and scope?
  CDSVK_CHECK(vkCreateImage(device_context_.device(), out_image_ci, device_context_.host_allocator(), out_image));
  *out_memory = device_context_.device_alloc_and_bind_to_image(*out_image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    DEVICE_ALLOCATION_SCOPE_DEVICE);

  // Build command buffer 
  VkCommandBuffer cb = one_shot_cpool_->allocate_and_begin();
  VkBufferMemoryBarrier buffer_barrier = {};
  buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  buffer_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
  buffer_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  buffer_barrier.srcQueueFamilyIndex = transfer_queue_family_;
  buffer_barrier.dstQueueFamilyIndex = transfer_queue_family_;
  buffer_barrier.buffer = staging_buffer;
  buffer_barrier.offset = 0;
  buffer_barrier.size = staging_buffer_ci.size;
  VkImageMemoryBarrier image_barrier = {};
  image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_barrier.srcAccessMask = 0;
  image_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barrier.srcQueueFamilyIndex = transfer_queue_family_;
  image_barrier.dstQueueFamilyIndex = transfer_queue_family_;
  image_barrier.image = *out_image;
  image_barrier.subresourceRange.aspectMask = aspect_flags;
  image_barrier.subresourceRange.baseArrayLayer = 0;
  image_barrier.subresourceRange.layerCount = out_image_ci->arrayLayers;
  image_barrier.subresourceRange.baseMipLevel = 0;
  image_barrier.subresourceRange.levelCount = mips_to_load;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    (VkDependencyFlags)0, 0,NULL, 1,&buffer_barrier, 1,&image_barrier);
  vkCmdCopyBufferToImage(cb, staging_buffer, *out_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    (uint32_t)copy_regions.size(), copy_regions.data());

  if (!generate_mipmaps) {
    image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barrier.dstAccessMask = final_access_flags;
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier.newLayout = final_layout;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      (VkDependencyFlags)0, 0,NULL, 0,NULL, 1,&image_barrier);
  } else {
    int record_error = record_mipmap_generation(cb, *out_image, *out_image_ci,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, final_layout, final_access_flags);
    assert(!record_error);
    (void)record_error;
  }
  one_shot_cpool_->end_submit_and_free(&cb);
  device_context_.device_free(staging_buffer_mem);
  vkDestroyBuffer(device_context_.device(), staging_buffer, device_context_.host_allocator());
  ImageFileDestroy(&image_file);
  return 0;
}

int TextureLoader::generate_vkimage_mipmaps(VkImage image, const VkImageCreateInfo &image_ci,
    VkImageLayout input_layout, VkAccessFlags input_access_flags,
    VkImageLayout final_layout, VkAccessFlags final_access_flags) const {
  if (image_ci.mipLevels == 1) {
    return 0;  // nothing to do
  }
  VkFormatProperties format_properties = {};
  vkGetPhysicalDeviceFormatProperties(device_context_.physical_device(), image_ci.format, &format_properties);
  const VkFormatFeatureFlags blit_mask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
  const VkFormatFeatureFlags feature_flags = (image_ci.tiling == VK_IMAGE_TILING_LINEAR)
    ? format_properties.linearTilingFeatures : format_properties.optimalTilingFeatures;
  if ( (feature_flags & blit_mask) != blit_mask ) {
    return -1;  // format does not support blitting; automatic mipmap generation won't work.
  }

  // Build command buffer 
  VkCommandBuffer cb = one_shot_cpool_->allocate_and_begin();
  int record_error = record_mipmap_generation(cb, image, image_ci,
    input_layout, input_access_flags, final_layout, final_access_flags);
  one_shot_cpool_->end_submit_and_free(&cb);
  return record_error;
}

int TextureLoader::record_mipmap_generation(VkCommandBuffer cb, VkImage image, const VkImageCreateInfo &image_ci,
    VkImageLayout input_layout, VkAccessFlags input_access_flags,
    VkImageLayout final_layout, VkAccessFlags final_access_flags) const {
  assert(image_ci.mipLevels > 1); // higher-level code should be checking for this already

  VkFormatProperties format_properties = {};
  vkGetPhysicalDeviceFormatProperties(device_context_.physical_device(), image_ci.format, &format_properties);
  const VkFormatFeatureFlags blit_mask = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
  const VkFormatFeatureFlags feature_flags = (image_ci.tiling == VK_IMAGE_TILING_LINEAR)
    ? format_properties.linearTilingFeatures : format_properties.optimalTilingFeatures;
  if ( (feature_flags & blit_mask) != blit_mask ) {
    return -1;  // format does not support blitting; automatic mipmap generation won't work.
  }

  VkImageAspectFlags aspect_flags = vk_format_to_image_aspect(image_ci.format);

  std::array<VkImageMemoryBarrier,2> image_barriers = {};
  image_barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_barriers[0].srcAccessMask = input_access_flags;
  image_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  image_barriers[0].oldLayout = input_layout;
  image_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  image_barriers[0].srcQueueFamilyIndex = transfer_queue_family_;
  image_barriers[0].dstQueueFamilyIndex = transfer_queue_family_;
  image_barriers[0].image = image;
  image_barriers[0].subresourceRange.aspectMask = aspect_flags;
  image_barriers[0].subresourceRange.baseArrayLayer = 0;
  image_barriers[0].subresourceRange.layerCount = image_ci.arrayLayers;
  image_barriers[0].subresourceRange.baseMipLevel = 0;
  image_barriers[0].subresourceRange.levelCount = 1;
  image_barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_barriers[1].srcAccessMask = 0;
  image_barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  image_barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barriers[1].srcQueueFamilyIndex = transfer_queue_family_;
  image_barriers[1].dstQueueFamilyIndex = transfer_queue_family_;
  image_barriers[1].image = image;
  image_barriers[1].subresourceRange.aspectMask = aspect_flags;
  image_barriers[1].subresourceRange.baseArrayLayer = 0;
  image_barriers[1].subresourceRange.layerCount = image_ci.arrayLayers;
  image_barriers[1].subresourceRange.baseMipLevel = 1;
  image_barriers[1].subresourceRange.levelCount = 1;
  VkImageBlit blit_region = {};
  blit_region.srcSubresource.aspectMask = aspect_flags;
  blit_region.srcSubresource.baseArrayLayer = 0;
  blit_region.srcSubresource.layerCount = image_ci.arrayLayers;
  blit_region.srcSubresource.mipLevel = 0;
  blit_region.srcOffsets[0].x = 0;
  blit_region.srcOffsets[0].y = 0;
  blit_region.srcOffsets[0].z = 0;
  blit_region.srcOffsets[1].x = get_mip_dimension(image_ci.extent.width, 0);
  blit_region.srcOffsets[1].y = get_mip_dimension(image_ci.extent.height, 0);
  blit_region.srcOffsets[1].z = get_mip_dimension(image_ci.extent.depth, 0);
  blit_region.dstSubresource.aspectMask = aspect_flags;
  blit_region.dstSubresource.baseArrayLayer = 0;
  blit_region.dstSubresource.layerCount = image_ci.arrayLayers;
  blit_region.dstSubresource.mipLevel = 1;
  blit_region.dstOffsets[0].x = 0;
  blit_region.dstOffsets[0].y = 0;
  blit_region.dstOffsets[0].z = 0;
  blit_region.dstOffsets[1].x = get_mip_dimension(image_ci.extent.width, 1);
  blit_region.dstOffsets[1].y = get_mip_dimension(image_ci.extent.height, 1);
  blit_region.dstOffsets[1].z = get_mip_dimension(image_ci.extent.depth, 1);
  for(uint32_t dst_mip = 1; dst_mip < image_ci.mipLevels; ++dst_mip) {
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
      (VkDependencyFlags)0, 0,NULL, 0,NULL, (uint32_t)image_barriers.size(),image_barriers.data());
    vkCmdBlitImage(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1, &blit_region, VK_FILTER_LINEAR);
    image_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    image_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    image_barriers[0].subresourceRange.baseMipLevel += 1;

    image_barriers[1].subresourceRange.baseMipLevel += 1;

    blit_region.srcSubresource.mipLevel += 1;
    blit_region.srcOffsets[1].x = get_mip_dimension(image_ci.extent.width, dst_mip);
    blit_region.srcOffsets[1].y = get_mip_dimension(image_ci.extent.height, dst_mip);
    blit_region.srcOffsets[1].z = get_mip_dimension(image_ci.extent.depth, dst_mip);
    blit_region.dstSubresource.mipLevel += 1;
    blit_region.dstOffsets[1].x = get_mip_dimension(image_ci.extent.width, dst_mip+1);
    blit_region.dstOffsets[1].y = get_mip_dimension(image_ci.extent.height, dst_mip+1);
    blit_region.dstOffsets[1].z = get_mip_dimension(image_ci.extent.depth, dst_mip+1);
  }
  // Coming out of the loop, all but the last mip are in TRANSFER_SRC mode, and the last mip
  // is in TRANSFER_DST. Convert them all to the final layout/access mode.
  image_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  image_barriers[0].dstAccessMask = final_access_flags;
  image_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  image_barriers[0].newLayout = final_layout;
  image_barriers[0].subresourceRange.baseArrayLayer = 0;
  image_barriers[0].subresourceRange.layerCount = image_ci.arrayLayers;
  image_barriers[0].subresourceRange.baseMipLevel = 0;
  image_barriers[0].subresourceRange.levelCount = image_ci.mipLevels - 1;
  image_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  image_barriers[1].dstAccessMask = final_access_flags;
  image_barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barriers[1].newLayout = final_layout;
  image_barriers[1].subresourceRange.baseMipLevel = image_ci.mipLevels - 1;
  image_barriers[1].subresourceRange.levelCount = 1;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    (VkDependencyFlags)0, 0,NULL, 0,NULL, (uint32_t)image_barriers.size(),image_barriers.data());

  return 0;
}
