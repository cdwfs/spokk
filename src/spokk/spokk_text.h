#pragma once

#include "spokk_buffer.h"
#include "spokk_image.h"
#include "spokk_pipeline.h"
#include "spokk_renderpass.h"
#include "spokk_shader.h"

#include <string>
#include <vector>

// TODO(cort): Forward declarations to avoid putting stb_truetype.h in the header
#include <stb/stb_truetype.h>

namespace spokk {

class Device;

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

// A FontAtlas manages the mapping of codepoints from a Font to rectangular regions of a 2D image
// containing the gylph for that codepoint. By rendering quads with appropriate sizes and UVs,
// dynamic text can be rendered reasonably efficiently at runtime.
// The FontAtlas handles the generation of quad sizes/locations for a given string, and owns the atlas image
// itself, but isn't otherwise involved in the actual rendering of those quads; that's the TextRenderer's job.
struct FontAtlasCreateInfo {
  const Font* font;  // Only needed during creation.
  float font_size;  // in pixels, from the highest ascent to the lowest descent.
  uint32_t image_oversample_x, image_oversample_y;  // 2x in each direction looks good with bilinear filtering
  uint32_t image_width, image_height;
  // TODO(cort): multiple ranges of codepoints?
  uint32_t codepoint_first;
  uint32_t codepoint_count;
};

class FontAtlas {
public:
  // 0 = success
  // non-zero = error (specificaly, -5 generally means the atlas dimensions are too small)
  int Create(const Device& device, const FontAtlasCreateInfo& ci);

  void Destroy(const Device& device);

  struct Quad {
    float x0, y0, s0, t0;  // top-left
    float x1, y1, s1, t1;  // bottom-right
  };
  void GetStringQuads(
      const char* str, size_t str_len, float spacing, float scale, Quad* out_quads, uint32_t* out_quad_count) const;

  const Image& GetImage() const { return atlas_image_; }
  static const MeshFormat& GetQuadFormat();

private:
  uint32_t image_width_ = 0, image_height_ = 0;
  uint32_t codepoint_first_ = 0;
  uint32_t codepoint_count_ = 0;
  std::vector<stbtt_packedchar> glyph_data_;
  Image atlas_image_ = {};
};

// A TextRenderer uses a FontAtlas to generate quads for a given string, and then renders those quads.
class TextRenderer {
public:
  struct CreateInfo {
    std::vector<const FontAtlas*> font_atlases;

    // Application settings required for pipeline creation.
    const RenderPass* render_pass;
    uint32_t subpass;
    uint32_t target_color_attachment_index;  // Used to set the appropriate bits in the color write mask

    // Upper-bound limits on various quantities, to allow all memory allocations to be made up front.
    uint32_t pframe_count;  // Number of concurrent frames in flight.
    uint32_t max_binds_per_pframe;  // Maximum number of times the text draw settings can be changed per frame.
    uint32_t max_glyphs_per_pframe;  // Maximum number of glyphs that can be rendered per frame.
  };

  int Create(const Device& device, const CreateInfo& ci);
  void Destroy(const Device& device);

  struct State {
    uint32_t pframe_index;  // Which frame's buffer to write to.
    float spacing;  // Horizontal spacing to add/subtract between characters, in pixels. 0 is a safe default.
    float scale;  // Scale factor, applied to quad dimensions. 1.0 is a safe default.
    float color[4];  // Text color, as RGBA.
    VkViewport viewport;  // To transform input pixel coordinates to clip space
    const FontAtlas* font_atlas;
  };

  int BindDrawState(VkCommandBuffer cb, const State& state);

  // x/y here are the coordinates of the point at the baseline where rendering should begin
  // (*not* the upper-left corner of the first glyph)
  void Printf(VkCommandBuffer cb, float* x, float* y, const char* format, ...);

private:
  uint32_t pframe_count_ = 0;  // Number of concurrent frames in flight.
  uint32_t max_binds_per_pframe_ = 0;
  uint32_t max_glyphs_per_pframe_ = 0;
  VkDeviceSize uniform_buffer_stride_ = 0;

  uint32_t current_bind_index_ = 0;
  VkDeviceSize current_glyph_count_ = 0;  // Number of glyphs currently stored in the active vertex buffer.
  State current_state_ = {};

  VkSampler sampler_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  Shader vertex_shader_ = {}, fragment_shader_ = {};
  ShaderProgram program_ = {};
  DescriptorPool dpool_ = {};
  Buffer quad_index_buffer_ = {};
  PipelinedBuffer vertex_buffers_ = {};
  PipelinedBuffer uniform_buffers_ = {};
  std::vector<VkDescriptorSet> uniform_dsets_ = {};  // one per pframe

  std::vector<const FontAtlas*> font_atlases_ = {};
  std::vector<VkDescriptorSet> font_atlas_dsets_ = {};  // one per atlas. Used to bind the atlas image to the shader.
};

}  // namespace spokk
