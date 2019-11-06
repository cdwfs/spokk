#if !defined(SHADERTOYINFO_H)
#define SHADERTOYINFO_H

namespace spokk {
class Device;
struct GraphicsPipeline;
struct RenderPass;
struct Shader;
struct ShaderProgram;
}  // namespace spokk

#include <array>
#include <map>
#include <string>
#include <vector>

struct json_value_s;  // from json.h

typedef enum ShadertoyInputType {
  SHADERTOY_INPUT_TYPE_UNKNOWN = 0,
  SHADERTOY_INPUT_TYPE_TEXTURE = 1,
  SHADERTOY_INPUT_TYPE_CUBE = 2,
} ShadertoyInputType;

typedef struct ShadertoyInput {
  std::vector<std::string> src;
  ShadertoyInputType ctype;
  int channel;
  int id;

  ShadertoyInput() {
    ctype = SHADERTOY_INPUT_TYPE_UNKNOWN;
    channel = -1;
    id = -1;
  }

} ShadertoyInput;

typedef enum ShadertoyRenderPassType {
  SHADERTOY_RENDER_PASS_TYPE_UNKNOWN = 0,
  SHADERTOY_RENDER_PASS_TYPE_IMAGE = 1,
  SHADERTOY_RENDER_PASS_TYPE_BUFFER_A = 2,
  SHADERTOY_RENDER_PASS_TYPE_BUFFER_B = 3,
  SHADERTOY_RENDER_PASS_TYPE_BUFFER_C = 4,
  SHADERTOY_RENDER_PASS_TYPE_BUFFER_D = 5,
  SHADERTOY_RENDER_PASS_TYPE_CUBEMAP_A = 6,
  SHADERTOY_RENDER_PASS_TYPE_SOUND = 7,
  SHADERTOY_RENDER_PASS_TYPE_COMMON = 8,

  SHADERTOY_RENDER_PASS_TYPE_COUNT
} ShadertoyRenderPassType;

typedef struct ShadertoyRenderPass {
  ShadertoyRenderPass();
  ~ShadertoyRenderPass();

  ShadertoyRenderPassType pass_type;
  std::string spv_filename;

  std::map<int, ShadertoyInput*> inputs;

  spokk::Shader* frag_shader;
  spokk::ShaderProgram* shader_program;
  spokk::GraphicsPipeline* pipeline;
} ShadertoyRenderPass;

class ShadertoyInfo {
public:
  ShadertoyInfo();
  ~ShadertoyInfo();

  int Load(const std::string& json5_filename, const spokk::Device& device, const spokk::Shader* vertex_shader,
      const spokk::RenderPass* render_pass, int32_t subpass);

private:
  std::string JsonValueLocationStr(const json_value_s* val) const;
  int ParseRoot(const json_value_s* val);
  int ParseShader(const json_value_s* val);
  int ParseInfo(const json_value_s* val);
  int ParseRenderPasses(const json_value_s* val);
  int ParseRenderPass(const json_value_s* val, ShadertoyRenderPass* out_renderpass);
  int ParseInputs(const json_value_s* val, ShadertoyRenderPass* out_renderpass);
  int ParseInput(const json_value_s* val, ShadertoyRenderPass* out_renderpass);
  int ParseOutputs(const json_value_s* val, ShadertoyRenderPass* out_renderpass);
  int ParseOutput(const json_value_s* val, ShadertoyRenderPass* out_renderpass);

  std::string info_filename_;
  std::string id_;
  std::string name_;
  std::string username_;
  std::string description_;

  std::array<ShadertoyRenderPass*, SHADERTOY_RENDER_PASS_TYPE_COUNT> renderpasses_;
};

#endif  // !defined(SHADERTOYINFO_H)
