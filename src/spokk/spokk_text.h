#pragma once

#include "spokk_buffer.h"
#include "spokk_context.h"
#include "spokk_image.h"
#include "spokk_pipeline.h"
#include "spokk_renderpass.h"

#include <string>
#include <vector>

// TODO(cort): Forward declarations to avoid putting stb_truetype.h in the header
#include <stb_truetype.h>

namespace spokk {

class Font {
public:
  Font();
  ~Font();

  int Create(const char* ttf_path);
  void Destroy();

  // API for rendering text into a bitmap on the CPU. For text that will be in a fixed location on screen,
  // this is probably the way to go.
  struct StringRenderInfo {
    const char* str;
    float font_size;  // in pixels, from the highest ascent to the lowest descent.
    uint32_t x_start, y_start;  // Top/left pixel to begin rendering.
    uint32_t x_min, x_max;  // text that extends past x_max will be wrapped to a new line, starting at x_min.
    // TODO(cort): destination pixel size? color? Effects?
  };
  // Determine minimum bitmap dimensions required to rasterize a given string.
  void ComputeStringBitmapDimensions(const StringRenderInfo& info, uint32_t* out_w, uint32_t* out_h) const;

  // Rasterizes a string into the provided 8bpp bitmap.
  // This function does *not* clear bitmap before rasterizing. This is a feature; it means you can
  // raster text on top of arbitrary bitmap contents!
  // ComputeStringBitmapDimensions() should be called first to make sure the text will fit.
  int RenderStringToBitmap(
      const StringRenderInfo& info, uint32_t bitmap_w, uint32_t bitmap_h, uint8_t* bitmap_pixels) const;

private:
  // Font data
  std::vector<uint8_t> ttf_;
  // Font metadata
  stbtt_fontinfo font_info_;
  int ascent_, descent_, line_gap_;  // font V-metrics. These must be scaled to be useful.

  // Shared code/data for 2D string rendering
  struct GlyphInfo {
    int glyph_index;  // glyph index, passed to stbtt functions
    float xpos, ypos;  // upper-left corner of this glyph in the string bitmap
    float x_shift, y_shift;  // subpixel shift from the upper-left corner
    int x0, y0, x1, y1;  // top-left and bottom-right corners of this glyph in the string bitmap
  };
  void ComputeGlyphInfoAndBitmapDimensions(
      const StringRenderInfo& info, size_t str_length, GlyphInfo* out_glyphs, uint32_t* out_w, uint32_t* out_h) const;

  friend class FontAtlas;
};

struct FontAtlasCreateInfo {
  const Font* font;
  float font_size;
  uint32_t image_oversample_x, image_oversample_y;
  uint32_t image_width, image_height;
  // TODO: multiple ranges of codepoints?
  uint32_t codepoint_first;
  uint32_t codepoint_count;
};

class FontAtlas {
public:
  int Create(const FontAtlasCreateInfo& ci, uint8_t* out_bitmap);
  void Destroy(void);

  struct Quad {
    float x0, y0, s0, t0;  // top-left
    float x1, y1, s1, t1;  // bottom-right
  };
  int GetStringQuads(const char* str, size_t str_len, Quad* out_quads) const;

private:
  uint32_t image_width_, image_height_;
  uint32_t codepoint_first_;
  uint32_t codepoint_count_;
  std::vector<stbtt_packedchar> char_data_;
};

}  // namespace spokk