#include "spokk_text.h"

#include "spokk_debug.h"
#include "spokk_image.h"
#include "spokk_platform.h"

#include <array>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

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
  for (int ch = 0; ch < str_length; ++ch) {
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
  int line_start = 0;
  for (int ch = 0; ch < str_length; ++ch) {
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
      int ch_last = ch;
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

int FontAtlas::Create(const FontAtlasCreateInfo &ci, uint8_t *out_bitmap) {
  image_width_ = ci.image_width;
  image_height_ = ci.image_height;
  codepoint_first_ = ci.codepoint_first;
  codepoint_count_ = ci.codepoint_count;

  stbtt_pack_context pack_context = {};
  const int bitmap_row_nbytes = 0;  // 0 = tightly packed
  const int padding = 1;
  void *alloc_context = nullptr;  // when using a custom allocator with stbtt
  int err = stbtt_PackBegin(
      &pack_context, out_bitmap, image_width_, image_height_, bitmap_row_nbytes, padding, alloc_context);
  if (err != 1) {
    return -3;  // stbtt_PackBegin() error
  }
  ZOMBO_ASSERT_RETURN(err == 1, -4, "stbtt_PackBegin() error: %d", err);
  stbtt_PackSetOversampling(&pack_context, ci.image_oversample_x, ci.image_oversample_y);

  char_data_.resize(codepoint_count_);
  const int font_index = 0;
  err = stbtt_PackFontRange(&pack_context, ci.font->ttf_.data(), font_index, ci.font_size,
      codepoint_first_, codepoint_count_, char_data_.data());
  ZOMBO_ASSERT_RETURN(err != 0, -5, "stbtt_PackFontRange() error: %d", err);

  stbtt_PackEnd(&pack_context);

#if 0  // dump font pixels to an image file for verification
  int write_success = stbi_write_png("font.png", image_width_, image_height_, 1, out_bitmap, 0);
  ZOMBO_ASSERT_RETURN(write_success, -6, "Font image write failure: %d", write_success);
#endif
  return 0;
}
void FontAtlas::Destroy(void) {}

int FontAtlas::GetStringQuads(const char *str, size_t str_len, Quad *out_quads) const {
  float pos_x = 0, pos_y = 0;
  stbtt_aligned_quad *quads = reinterpret_cast<stbtt_aligned_quad *>(out_quads);
  const int align_to_integer = 0;
  for (size_t i = 0; i < str_len; ++i) {
    uint32_t codepoint = (uint32_t)str[i];
    stbtt_GetPackedQuad(char_data_.data(), image_width_, image_height_,
        codepoint - codepoint_first_, &pos_x, &pos_y, &quads[i], align_to_integer);
    if (str[i] == ' ') {
      // We need to call stbtt_GetPackedQuad regardless in order to advance pos_x/pos_y, but there's
      // no point in storing the resulting quad for rendering.
      continue;
    }
  }
  return (int)str_len;
}

}  // namespace spokk