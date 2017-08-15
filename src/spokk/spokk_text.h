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

  // API for rendering text into a bitmap on the CPU. For text that will be in a fixed location on screen
  // and persist across several frames (e.g. dialogue text in an RPG, or a UI tooltip), this is probably
  // the way to go.
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

// A FontAtlas uses a Font to pre-render a range of glyphs to a single image, storing the texture coordinates
// of each glyph. By rendering quads with appropriate sizes and UVs, dynamic text can be rendered reasonably
// efficiently at runtime.
struct FontAtlasCreateInfo {
  const Font* font;  // Only needed during creation.
  float font_size;  // in pixels, from the highest ascent to the lowest descent.
  uint32_t image_oversample_x, image_oversample_y;  // 2x in each direction looks good with bilinear filtering
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
  void GetStringQuads(const char* str, size_t str_len, Quad* out_quads, uint32_t* out_quad_count) const;

private:
  uint32_t image_width_, image_height_;
  uint32_t codepoint_first_;
  uint32_t codepoint_count_;
  std::vector<stbtt_packedchar> char_data_;
};

// Hypothetical high-level dynamic text interface:
// Data:
// - FontAtlas
// - Image (atlas + mipmaps)
// - Sampler (bilinear)
// - Pipeline (VS/PS, ShaderProgram. Tied to a particular RenderPass, natch.)
// - Pipelined buffer for vertex data.
//   - What's the vertex format? Well, a fully naive 96 bytes/quad isn't horrific.
//     48 bytes per quad is doable, with more CPU work, and the vertex shader needs
//     to be aware either way.
// - Array of per-string draw parameters: vb offset, quad count, tint, matrix (x,y,scale)
//
// Initialization inputs:
// - Font
// - FontAtlasCreateInfo
// - VkRenderPass + subpass index (for pipeline creation)
// - VS, PS
// - max chars per frame (used to determine vertex buffer size)
// - max strings per frame
//
// Every frame, advance to the next vertex buffer and reset.
// GetStringDimensions(const StringRenderInfo& string_info, uint32_t* out_w, uint32_t* out_h);
// AddString(const StringRenderInfo& string_info);
// DrawStrings();
struct TextRendererCreateInfo {
  const FontAtlasCreateInfo* font_atlas_ci;
  const DeviceQueue* transfer_queue;
  uint32_t pframe_count;
  uint32_t max_chars_per_pframe;
  uint32_t max_strings_per_pframe;
};

class TextRenderer {
public:
  TextRenderer();
  ~TextRenderer();
  int Create(const DeviceContext& device_context, const TextRendererCreateInfo& ci);
  void Destroy(const DeviceContext& device_context);

  const MeshFormat& GetMeshFormat(void) const { return mesh_format_; }
  const Image& GetAtlasImage(void) const { return atlas_image_; }

  // Generates quads for each glyph of the string, converts them to the renderer's MeshFormat,
  // appends them to the current pframe's vertex buffer, and emits a draw command to render
  // the appropriate number of triangles.
  // It is the caller's responsibility to bind a VkPipeline capable of rendering these glyphs.
  void TextRenderer::DrawString(VkCommandBuffer cb, const char* str);

private:
  TextRenderer(const TextRenderer& rhs) = delete;
  TextRenderer& operator=(const TextRenderer& rhs) = delete;

  MeshFormat mesh_format_;
  FontAtlas atlas_;
  Image atlas_image_;
  PipelinedBuffer vertex_buffers_;

  uint32_t pframe_index_;
  uint32_t draw_count_;
  uint32_t quad_count_;
};

}  // namespace spokk