#include "spokk_text.h"

#include "spokk_debug.h"
#include "spokk_device.h"
#include "spokk_image.h"
#include "spokk_pipeline.h"
#include "spokk_platform.h"
#include "spokk_shader_interface.h"

#if defined(ZOMBO_COMPILER_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"  // L1, L2 are set but unused in debug builds
#elif defined(ZOMBO_COMPILER_GNU)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"  // L1, L2 are set but unused in debug builds
#endif
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>
#if defined(ZOMBO_COMPILER_CLANG)
#pragma clang diagnostic pop
#elif defined(ZOMBO_COMPILER_GNU)
#pragma GCC diagnostic pop
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#include <varargs.h>
#include <algorithm>
#include <array>

// If defined, a mipmap chain will be generated for font atlases, and the sampler will be configured
// for trilinear filtering. This is disabled by default, as glyphs bleed together at lower mip levels.
// I could probably get away with a few levels by adding sufficient padding, but in general it's best
// to render as close to the atlas resolution as possible.
//#define ENABLE_FONT_ATLAS_MIPMAPS

namespace spokk {

//
// Font
//
Font::Font() : ascent_(0), descent_(0), line_gap_(0) {}
Font::~Font() {}

int Font::Create(const char *ttf_path) {
  FILE *ttf_file = zomboFopen(ttf_path, "rb");
  if (ttf_file == nullptr) {
    return -1;  // could not open TTF file
  }
  fseek(ttf_file, 0, SEEK_END);
  size_t ttf_nbytes = ftell(ttf_file);
  fseek(ttf_file, 0, SEEK_SET);
  ttf_.resize(ttf_nbytes);
  fread(ttf_.data(), ttf_nbytes, 1, ttf_file);
  fclose(ttf_file);

  int err = 0;
  err = stbtt_InitFont(&font_info_, ttf_.data(), 0);
  if (err == 0) {
    return -2;  // stbtt_InitFont() error
  }
  stbtt_GetFontVMetrics(&font_info_, &ascent_, &descent_, &line_gap_);

  return 0;
}
void Font::Destroy() {}

void Font::ComputeStringBitmapDimensions(const StringRenderInfo &info, uint32_t *out_w, uint32_t *out_h) const {
  size_t str_length = strlen(info.str);
  std::vector<GlyphInfo> glyphs(str_length);
  ComputeGlyphInfoAndBitmapDimensions(info, str_length, glyphs.data(), out_w, out_h);
}

int Font::RenderStringToBitmap(
    const StringRenderInfo &info, uint32_t bitmap_w, uint32_t bitmap_h, uint8_t *bitmap_pixels) const {
  // First pass through the string:
  // - Look up & cache glyph index for each codepoint, to make future stbtt calls faster.
  // - Determine final placement of each glyph in the output bitmap, taking into account
  //   maximum line length, hyphenation, justification, kerning, etc. This requires peeking
  //   ahead to the next character's glyph.
  size_t str_length = strlen(info.str);
  std::vector<GlyphInfo> glyphs(str_length);
  uint32_t min_bitmap_w = 0, min_bitmap_h = 0;
  ComputeGlyphInfoAndBitmapDimensions(info, str_length, glyphs.data(), &min_bitmap_w, &min_bitmap_h);
  if (bitmap_w < min_bitmap_w || bitmap_h < min_bitmap_h) {
    return -2;  // Provided bitmap dimensions aren't large enough
  }

  float scale = stbtt_ScaleForPixelHeight(&font_info_, (float)info.font_size);

  // Second pass through the string:
  // - Compute sub-pixel shift for each glyph
  // - Compute maximum glyph width/height, to determine the size of the glyph bitmap.
  //   This depends on each glyph's subpixel position, so we need to compute that first.
  int max_glyph_width = 0, max_glyph_height = 0;
  const int baseline = int(ascent_ * scale);
  for (size_t ch = 0; ch < str_length; ++ch) {
    glyphs[ch].x_shift = glyphs[ch].xpos - floorf(glyphs[ch].xpos);
    glyphs[ch].y_shift = glyphs[ch].ypos - floorf(glyphs[ch].ypos);
    stbtt_GetGlyphBitmapBoxSubpixel(&font_info_, glyphs[ch].glyph_index, scale, scale, glyphs[ch].x_shift,
        glyphs[ch].y_shift, &glyphs[ch].x0, &glyphs[ch].y0, &glyphs[ch].x1, &glyphs[ch].y1);
    ZOMBO_ASSERT_RETURN(uint32_t(glyphs[ch].x1) < bitmap_w, -2, "x1 (%d) >= bitmap_w (%d)", glyphs[ch].x1, bitmap_w);
    ZOMBO_ASSERT_RETURN(
        uint32_t(baseline + glyphs[ch].y1) < bitmap_h, -3, "y1 (%d) >= bitmap_h (%d)", glyphs[ch].y1, bitmap_h);
    max_glyph_width = std::max(max_glyph_width, glyphs[ch].x1 - glyphs[ch].x0);
    max_glyph_height = std::max(max_glyph_height, glyphs[ch].y1 - glyphs[ch].y0);
  }
  if (max_glyph_width > (int)(info.x_max - info.x_min)) {
    return -1;  // x_max - x_min must be large enough to render any single glyph
  }
  std::vector<uint8_t> glyph_pixels(max_glyph_width * max_glyph_height);
  glyph_pixels.assign(glyph_pixels.size(), 0);

  // Third pass through the string: actually rasterize the glyphs into the string bitmap.
  for (int ch = 0; info.str[ch] != '\0'; ++ch) {
    // Render this codepoint's glyph into the single-glyph buffer.
    stbtt_MakeGlyphBitmapSubpixel(&font_info_, glyph_pixels.data(), glyphs[ch].x1 - glyphs[ch].x0,
        glyphs[ch].y1 - glyphs[ch].y0, max_glyph_width, scale, scale, glyphs[ch].x_shift, glyphs[ch].y_shift,
        glyphs[ch].glyph_index);
    int si = 0, di = 0;
    uint8_t *dp = &bitmap_pixels[(int(glyphs[ch].ypos) + baseline + glyphs[ch].y0) * bitmap_w + int(glyphs[ch].xpos) +
        glyphs[ch].x0];
    const int glyph_w = glyphs[ch].x1 - glyphs[ch].x0;
    const int glyph_h = glyphs[ch].y1 - glyphs[ch].y0;
    for (int sy = 0; sy < glyph_h; ++sy) {
      di = sy * bitmap_w;  // relative to dp
      si = sy * max_glyph_width;
      for (int sx = 0; sx < glyph_w; ++sx) {
        ZOMBO_ASSERT_RETURN(dp + di <= bitmap_pixels + (bitmap_w * bitmap_h), -4,
            "di (%d) is out of bounds for %dx%d bitmap", di, bitmap_w, bitmap_h);
#if 1
        // "alpha blend" the glyph into the string bitmap
        if (glyph_pixels.at(si) > 0) {
          int d_new = int(dp[di]) + int(glyph_pixels.at(si));
          dp[di] = (d_new > 255) ? 255 : uint8_t(d_new);
          glyph_pixels.at(si) = 0;
        }
#else
        // just overwrite, no blending.
        // This is incorrect; just there to demonstrate what doing it right looks like.
        dp[di] = glyph_pixels.at(si);
        glyph_pixels.at(si) = 0;
#endif
        ++si;
        ++di;
      }
    }
  }

#if 0
  int write_success = stbi_write_png("string.png", bitmap_w, bitmap_h, 1, bitmap_pixels, 0);
  ZOMBO_ASSERT_RETURN(write_success, -1, "String image write failure: %d", write_success);
#endif

  return 0;
}

void Font::ComputeGlyphInfoAndBitmapDimensions(
    const StringRenderInfo &info, size_t str_length, GlyphInfo *out_glyphs, uint32_t *out_w, uint32_t *out_h) const {
  ZOMBO_ASSERT(info.x_max > info.x_min, "x_max (%d) must be greater than x_min (%d)", info.x_max, info.x_min);

  float scale = stbtt_ScaleForPixelHeight(&font_info_, (float)info.font_size);
  float ypos_inc = float(ascent_ - descent_ + line_gap_) * scale;

  // First pass through the string:
  // - Look up & cache glyph index for each codepoint, to make future stbtt calls faster.
  // - Determine final placement of each glyph in the output bitmap, taking into account
  //   maximum line length, hyphenation, justification, kerning, etc. This requires peeking
  //   ahead to the next character's glyph.
  float xpos = (float)info.x_start, ypos = (float)info.y_start;
  float max_line_size = 0;
  if (str_length > 0) {
    out_glyphs[0].glyph_index = stbtt_FindGlyphIndex(&font_info_, (int)info.str[0]);
  }
  size_t line_start = 0;
  for (size_t ch = 0; ch < str_length; ++ch) {
    out_glyphs[ch].glyph_index = stbtt_FindGlyphIndex(&font_info_, (int)info.str[ch]);
    out_glyphs[ch].xpos = xpos;
    out_glyphs[ch].ypos = ypos;
    int advance = 0, left_side_bearing = 0;
    // TODO(cort): glyph metrics are position-invariant, and could be moved to an earlier loop
    // to avoid needless recalculation during wrapping/justification.
    stbtt_GetGlyphHMetrics(&font_info_, out_glyphs[ch].glyph_index, &advance, &left_side_bearing);
    xpos += (advance * scale);
    if (info.str[ch + 1] != '\0') {
      out_glyphs[ch + 1].glyph_index = stbtt_FindGlyphIndex(&font_info_, (int)info.str[ch + 1]);
      xpos +=
          scale * stbtt_GetGlyphKernAdvance(&font_info_, out_glyphs[ch].glyph_index, out_glyphs[ch + 1].glyph_index);
    }
    // If xpos > some user-specified wrap point, rewind to the beginning of the current word,
    // advance ypos to the next line, and continue the loop from there.
    // TODO(cort): This could get arbitrarily complex; you might want to go back and justify the entire line now
    // that you know its contents, or hyphenate a long word. And you need to handle edge cases like a single
    // "word" that won't fit on a single line.
    if (xpos >= info.x_max) {
      size_t ch_last = ch;
      while (ch > line_start && info.str[--ch] != ' ') {  // TODO(cort): better whitespace test
      }
      if (ch == line_start) {  // entire line is one word; just wrap it.
        ch = ch_last - 1;
      }
      xpos = (float)info.x_min;
      ypos += ypos_inc;
      line_start = ch + 1;
      continue;  // this increments ch past the space
    }
    // Track the longest line, for bitmap sizing purposes.
    max_line_size = std::max(max_line_size, xpos);
  }
  *out_w = (uint32_t)max_line_size;
  *out_h = (uint32_t)(ypos + ypos_inc);
}

//////////////////////////////////////////

int FontAtlas::Create(const Device &device, const FontAtlasCreateInfo &ci) {
  image_width_ = ci.image_width;
  image_height_ = ci.image_height;
  codepoint_first_ = ci.codepoint_first;
  codepoint_count_ = ci.codepoint_count;

  std::vector<uint8_t> atlas_pixels(ci.image_width * ci.image_height, 0U);

  stbtt_pack_context pack_context = {};
  const int bitmap_row_nbytes = 0;  // 0 = tightly packed
  const int padding = 1;
  void *alloc_context = nullptr;  // when using a custom allocator with stbtt
  int err = stbtt_PackBegin(
      &pack_context, atlas_pixels.data(), image_width_, image_height_, bitmap_row_nbytes, padding, alloc_context);
  if (err != 1) {
    return -3;  // stbtt_PackBegin() error
  }
  ZOMBO_ASSERT_RETURN(err == 1, -4, "stbtt_PackBegin() error: %d", err);
  stbtt_PackSetOversampling(&pack_context, ci.image_oversample_x, ci.image_oversample_y);

  glyph_data_.resize(codepoint_count_);
  const int font_index = 0;
  err = stbtt_PackFontRange(&pack_context, ci.font->ttf_.data(), font_index, ci.font_size, codepoint_first_,
      codepoint_count_, glyph_data_.data());
  ZOMBO_ASSERT_RETURN(err != 0, -5, "stbtt_PackFontRange() error: %d", err);

  stbtt_PackEnd(&pack_context);

#if 0  // dump font pixels to an image file for verification
  int write_success = stbi_write_png("atlas.png", image_width_, image_height_, 1, atlas_pixels.data(), 0);
  ZOMBO_ASSERT_RETURN(write_success, -6, "Font image write failure: %d", write_success);
#endif

  // Create and populate atlas image
  VkImageCreateInfo atlas_image_ci = {};
  atlas_image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  atlas_image_ci.imageType = VK_IMAGE_TYPE_2D;
  atlas_image_ci.format = VK_FORMAT_R8_UNORM;
  atlas_image_ci.extent.width = ci.image_width;
  atlas_image_ci.extent.height = ci.image_height;
  atlas_image_ci.extent.depth = 1;
  atlas_image_ci.mipLevels =
#if defined(ENABLE_FONT_ATLAS_MIPMAPS)
      GetMaxMipLevels(atlas_image_ci.extent);
#else
      1;
#endif
  atlas_image_ci.arrayLayers = 1;
  atlas_image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
  atlas_image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
  atlas_image_ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
#if defined(ENABLE_FONT_ATLAS_MIPMAPS)
  atlas_image_ci.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  // for mip generation
#endif
  atlas_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  atlas_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  SPOKK_VK_CHECK(atlas_image_.Create(device, atlas_image_ci));
  VkImageSubresource dst_subresource = {};
  dst_subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  dst_subresource.mipLevel = 0;
  dst_subresource.arrayLayer = 0;
  const DeviceQueue *graphics_queue = device.FindQueue(VK_QUEUE_GRAPHICS_BIT);
  int atlas_load_err = atlas_image_.LoadSubresourceFromMemory(device, graphics_queue, atlas_pixels.data(),
      atlas_pixels.size(), ci.image_width, ci.image_height, dst_subresource, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_SHADER_READ_BIT);
  ZOMBO_ASSERT(atlas_load_err == 0, "error (%d) while loading font atlas into memory", atlas_load_err);
#if defined(ENABLE_FONT_ATLAS_MIPMAPS)
  VkImageMemoryBarrier mipmap_barrier = {};
  mipmap_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  mipmap_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  mipmap_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  mipmap_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  mipmap_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  mipmap_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  mipmap_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  mipmap_barrier.image = atlas_image_.handle;
  mipmap_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  mipmap_barrier.subresourceRange.baseMipLevel = 0;
  mipmap_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  mipmap_barrier.subresourceRange.baseArrayLayer = 0;
  mipmap_barrier.subresourceRange.layerCount = 1;
  int mipmap_gen_err = atlas_image_.GenerateMipmaps(device, graphics_queue, mipmap_barrier, 0, 0);
  ZOMBO_ASSERT(mipmap_gen_err == 0, "error (%d) while generating atlas mipmaps", mipmap_gen_err);
#endif

  return 0;
}
void FontAtlas::Destroy(const Device &device) { atlas_image_.Destroy(device); }

void FontAtlas::GetStringQuads(const char *str, size_t str_len, Quad *out_quads, uint32_t *out_quad_count) const {
  float pos_x = 0, pos_y = 0;
  stbtt_aligned_quad *quads = reinterpret_cast<stbtt_aligned_quad *>(out_quads);
  const int align_to_integer = 0;
  uint32_t next_quad = 0;
  for (size_t i = 0; i < str_len; ++i) {
    uint32_t codepoint = (uint32_t)str[i];
    if (codepoint == 0) {
      break;  // in case somebody forgets to leave off the null terminator
    }
    stbtt_GetPackedQuad(glyph_data_.data(), image_width_, image_height_, codepoint - codepoint_first_, &pos_x, &pos_y,
        &quads[next_quad], align_to_integer);
    if (str[i] == ' ') {
      // We need to call stbtt_GetPackedQuad regardless in order to advance pos_x/pos_y, but there's
      // no point in storing the resulting quad for rendering.
      continue;
    }
    ++next_quad;
  }
  // TODO(cort: hey, return pos_x and pos_y!
  *out_quad_count = next_quad;
}

namespace {
struct GlyphVertex {
#if 0
  int16_t pos_x0, pos_y0;
  uint16_t tex_x0, tex_y0;
#else
  float pos_x0, pos_y0;
  float tex_x0, tex_y0;
#endif
};

}  // namespace

const MeshFormat &FontAtlas::GetQuadFormat() {
  static MeshFormat fmt = {};
  if (fmt.vertex_attributes.empty()) {
    fmt.vertex_buffer_bindings = {
        {0, sizeof(GlyphVertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };
    fmt.vertex_attributes = {
#if 0
      {0, 0, VK_FORMAT_R16G16_SINT, 0},
      {1, 0, VK_FORMAT_R16G16_UNORM, 4},
#else
      {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
      {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},
#endif
    };
    fmt.Finalize(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  }
  return fmt;
}

/////////////////////////////////////

struct StringUniforms {
  float color[4];
  float viewport_to_clip[4];
};

int TextRenderer::Create(const Device &device, const CreateInfo &ci) {
  pframe_count_ = ci.pframe_count;
  max_binds_per_pframe_ = ci.max_binds_per_pframe;
  max_glyphs_per_pframe_ = ci.max_glyphs_per_pframe;
  current_glyph_count_ = 0;
  current_bind_index_ = 0;
  current_state_.pframe_index = UINT32_MAX;  // Ensure the first call triggers a mismatch

  // sampler
  VkSamplerMipmapMode mipmap_mode =
#if ENABLE_FONT_ATLAS_MIPMAPS
      VK_SAMPLER_MIPMAP_MODE_LINEAR;
#else
      VK_SAMPLER_MIPMAP_MODE_NEAREST;
#endif
  VkSamplerCreateInfo sampler_ci =
      spokk::GetSamplerCreateInfo(VK_FILTER_LINEAR, mipmap_mode, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
  SPOKK_VK_CHECK(vkCreateSampler(device, &sampler_ci, device.HostAllocator(), &sampler_));

  // index buffer
  std::vector<uint16_t> quad_indices(ci.max_glyphs_per_pframe * 6);
  for (uint16_t i = 0; i < ci.max_glyphs_per_pframe; ++i) {
    quad_indices[6 * i + 0] = 4 * i + 0;
    quad_indices[6 * i + 1] = 4 * i + 1;
    quad_indices[6 * i + 2] = 4 * i + 2;
    quad_indices[6 * i + 3] = 4 * i + 2;
    quad_indices[6 * i + 4] = 4 * i + 1;
    quad_indices[6 * i + 5] = 4 * i + 3;
  }
  VkBufferCreateInfo quad_index_buffer_ci = {};
  quad_index_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  quad_index_buffer_ci.size = quad_indices.size() * sizeof(quad_indices[0]);
  quad_index_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  quad_index_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(quad_index_buffer_.Create(device, quad_index_buffer_ci));
  SPOKK_VK_CHECK(quad_index_buffer_.Load(device, quad_indices.data(), quad_index_buffer_ci.size));

  // Vertex buffers
  VkBufferCreateInfo vertex_buffer_ci = {};
  vertex_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vertex_buffer_ci.size = ci.max_glyphs_per_pframe * 4 * sizeof(GlyphVertex);
  vertex_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  vertex_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(
      vertex_buffers_.Create(device, ci.pframe_count, vertex_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

  // Uniform buffers
  // Currently using dynamic uniform buffers to select which instance of uniforms to use for a given
  // draw call, which means the stride of each instance's data must obey the minimum offset alignment.
  // Other potential solutions that avoid avoid this padding:
  // - Pass a draw index as a push constant
  // - Pass a draw index as the instance ID
  // - Pass the uniforms themselves as push constants
  uniform_buffer_stride_ =
      std::max(device.Properties().limits.minUniformBufferOffsetAlignment, (VkDeviceSize)sizeof(StringUniforms));
  VkBufferCreateInfo uniform_buffer_ci = {};
  uniform_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  uniform_buffer_ci.size = ci.max_binds_per_pframe * uniform_buffer_stride_;
  uniform_buffer_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  uniform_buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  SPOKK_VK_CHECK(
      uniform_buffers_.Create(device, ci.pframe_count, uniform_buffer_ci, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

  // Shaders and shader program
  SPOKK_VK_CHECK(vertex_shader_.CreateAndLoadSpirvFile(device, "data/text.vert.spv"));
  vertex_shader_.OverrideDescriptorType(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
  SPOKK_VK_CHECK(program_.AddShader(&vertex_shader_));
  SPOKK_VK_CHECK(fragment_shader_.CreateAndLoadSpirvFile(device, "data/text.frag.spv"));
  fragment_shader_.OverrideDescriptorType(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
  SPOKK_VK_CHECK(program_.AddShader(&fragment_shader_));
  SPOKK_VK_CHECK(program_.Finalize(device));

  // Create graphics pipeline
  GraphicsPipeline pipeline_settings = {};
  pipeline_settings.Init(&FontAtlas::GetQuadFormat(), &program_, ci.render_pass, ci.subpass);
  // Disable writes to all but the specified color attachment
  for (uint32_t i = 0; i < pipeline_settings.color_blend_state_ci.attachmentCount; ++i) {
    if (i != ci.target_color_attachment_index) {
      pipeline_settings.color_blend_attachment_states[i].colorWriteMask = 0;
    }
  }
  // Disable depth test
  pipeline_settings.depth_stencil_state_ci.depthTestEnable = VK_FALSE;
  // Enable blending
  auto &target_attachment_state = pipeline_settings.color_blend_attachment_states[ci.target_color_attachment_index];
  target_attachment_state.blendEnable = VK_TRUE;
  target_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
  target_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  target_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  SPOKK_VK_CHECK(pipeline_settings.Finalize(device));
  pipeline_ = pipeline_settings.handle;

  // Font atlases
  ZOMBO_ASSERT(ci.font_atlases.size() == 1, "Currently, only one font atlas is supportd.");  // TEMP
  font_atlases_.insert(font_atlases_.end(), ci.font_atlases.begin(), ci.font_atlases.end());

  // Descriptor pool
  ZOMBO_ASSERT(program_.dset_layout_cis.size() == 2, "Expected two dsets in text shader program");
  dpool_.Add(program_.dset_layout_cis[0], uniform_buffers_.Depth());
  dpool_.Add(program_.dset_layout_cis[1], (uint32_t)ci.font_atlases.size());
  dpool_.Finalize(device);

  // Descriptor sets
  uniform_dsets_.resize(uniform_buffers_.Depth());
  std::vector<VkDescriptorSetLayout> uniform_dset_layouts(uniform_dsets_.size(), program_.dset_layouts[0]);
  dpool_.AllocateSets(device, (uint32_t)uniform_dsets_.size(), uniform_dset_layouts.data(), uniform_dsets_.data());
  DescriptorSetWriter uniform_dset_writer(program_.dset_layout_cis[0]);
  for (uint32_t i = 0; i < uniform_buffers_.Depth(); ++i) {
    uniform_dset_writer.BindBuffer(uniform_buffers_.Handle(i), 0);
    uniform_dset_writer.WriteAll(device, uniform_dsets_[i]);
  }
  font_atlas_dsets_.resize(font_atlases_.size());
  std::vector<VkDescriptorSetLayout> atlas_dset_layouts(font_atlas_dsets_.size(), program_.dset_layouts[1]);
  dpool_.AllocateSets(device, (uint32_t)font_atlas_dsets_.size(), atlas_dset_layouts.data(), font_atlas_dsets_.data());
  DescriptorSetWriter atlas_dset_writer(program_.dset_layout_cis[1]);
  for (uint32_t i = 0; i < font_atlases_.size(); ++i) {
    atlas_dset_writer.BindImage(font_atlases_[i]->GetImage().view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0);
    atlas_dset_writer.BindSampler(sampler_, 1);
    atlas_dset_writer.WriteAll(device, font_atlas_dsets_[i]);
  }

  return 0;
}

void TextRenderer::Destroy(const Device &device) {
  font_atlases_.clear();
  font_atlas_dsets_.clear();
  uniform_dsets_.clear();

  dpool_.Destroy(device);

  vkDestroyPipeline(device, pipeline_, device.HostAllocator());
  program_.Destroy(device);
  vertex_shader_.Destroy(device);
  fragment_shader_.Destroy(device);

  uniform_buffers_.Destroy(device);

  vertex_buffers_.Destroy(device);

  quad_index_buffer_.Destroy(device);

  if (sampler_ != VK_NULL_HANDLE) {
    vkDestroySampler(device, sampler_, device.HostAllocator());
    sampler_ = VK_NULL_HANDLE;
  }
}

int TextRenderer::BindDrawState(VkCommandBuffer cb, const State &state) {
  if (state.pframe_index != current_state_.pframe_index) {
    // first state for a new pframe; reset counts and swap uniform buffers
    current_glyph_count_ = 0;
    current_bind_index_ = 0;
  }
  if (current_bind_index_ >= max_binds_per_pframe_) {
    return -1;
  }
  // Write the new state to the next available uniforms slot
  uint32_t uniform_offset = current_bind_index_ * (uint32_t)uniform_buffer_stride_;
  StringUniforms *uniforms =
      (StringUniforms *)(uintptr_t(uniform_buffers_.Mapped(state.pframe_index)) + uniform_offset);
  uniforms->color[0] = state.color[0];
  uniforms->color[1] = state.color[1];
  uniforms->color[2] = state.color[2];
  uniforms->color[3] = state.color[3];
  uniforms->viewport_to_clip[0] = 2.0f / state.viewport.width;
  uniforms->viewport_to_clip[1] = 2.0f / state.viewport.height;  // TODO(cort): proper scale/bias
  uniforms->viewport_to_clip[2] = -1.0f;
  uniforms->viewport_to_clip[3] = -1.0f;
  uniform_buffers_.FlushPframeHostCache(state.pframe_index, uniform_offset, uniform_buffer_stride_);
  // Bind the pipeline and the appropriate descriptor sets
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  ZOMBO_ASSERT(state.font_atlas == font_atlases_[0], "font atlas mismatch");
  std::array<VkDescriptorSet, 2> dsets = {{
      uniform_dsets_[state.pframe_index],
      font_atlas_dsets_[0],
  }};
  vkCmdBindDescriptorSets(
      cb, VK_PIPELINE_BIND_POINT_GRAPHICS, program_.pipeline_layout, 0, 2, dsets.data(), 1, &uniform_offset);
  vkCmdBindIndexBuffer(cb, quad_index_buffer_.Handle(), 0, VK_INDEX_TYPE_UINT16);
  // Update current state
  current_bind_index_ += 1;
  current_state_ = state;
  return 0;
}
void TextRenderer::Printf(VkCommandBuffer cb, float *x, float *y, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int nchars = 1 + vsnprintf(nullptr, 0, format, args);
  std::vector<char> buffer(nchars);
  vsnprintf(buffer.data(), nchars, format, args);
  va_end(args);

  // Generate quads for a string.
  std::vector<FontAtlas::Quad> quads(nchars);
  uint32_t string_quad_count = 0;
  current_state_.font_atlas->GetStringQuads(buffer.data(), nchars, quads.data(), &string_quad_count);
  if (current_glyph_count_ + string_quad_count > max_glyphs_per_pframe_) {
    ZOMBO_ERROR("current glyphs (%d) + string glyphs (%d) > max glyphs (%d)", (uint32_t)current_glyph_count_,
        string_quad_count, max_glyphs_per_pframe_);
    return;
  }
  // Convert raw quads into a compressed vertex buffer
  VkDeviceSize vb_offset = current_glyph_count_ * sizeof(GlyphVertex);
  GlyphVertex *verts = (GlyphVertex *)(uintptr_t(vertex_buffers_.Mapped(current_state_.pframe_index)) + vb_offset);
  for (uint32_t i = 0; i < string_quad_count; ++i) {
    const auto &q = quads[i];
#if 0
    verts[4 * i + 0] = {F32toS16(q.x0 + *x), F32toS16(q.y0 + *y), F32toU16N(q.s0), F32toU16N(q.t0)};
    verts[4 * i + 1] = {F32toS16(q.x0 + *x), F32toS16(q.y1 + *y), F32toU16N(q.s0), F32toU16N(q.t1)};
    verts[4 * i + 2] = {F32toS16(q.x1 + *x), F32toS16(q.y0 + *y), F32toU16N(q.s1), F32toU16N(q.t0)};
    verts[4 * i + 3] = {F32toS16(q.x1 + *x), F32toS16(q.y1 + *y), F32toU16N(q.s1), F32toU16N(q.t1)};
#else
    verts[4 * i + 0] = {q.x0 + *x, q.y0 + *y, q.s0, q.t0};
    verts[4 * i + 1] = {q.x0 + *x, q.y1 + *y, q.s0, q.t1};
    verts[4 * i + 2] = {q.x1 + *x, q.y0 + *y, q.s1, q.t0};
    verts[4 * i + 3] = {q.x1 + *x, q.y1 + *y, q.s1, q.t1};
#endif
  }
  current_glyph_count_ += string_quad_count;
  vertex_buffers_.FlushPframeHostCache(
      current_state_.pframe_index, vb_offset, string_quad_count * 4 * sizeof(GlyphVertex));

  VkBuffer vb = vertex_buffers_.Handle(current_state_.pframe_index);
  vkCmdBindVertexBuffers(cb, 0, 1, &vb, &vb_offset);
  vkCmdDrawIndexed(cb, 6 * string_quad_count, 1, 0, 0, 0);
}

}  // namespace spokk
