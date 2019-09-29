#include "shadertoyinfo.h"

#include <json.h>
#include <spokk_platform.h>

#include <cstdio>
#include <vector>

namespace {
const char* JsonParseErrorStr(const json_parse_error_e error_code) {
  switch (error_code) {
  case json_parse_error_none:
    return "Success";
  case json_parse_error_expected_comma_or_closing_bracket:
    return "Expected comma or closing bracket";
  case json_parse_error_expected_colon:
    return "Expected colon separating name and value";
  case json_parse_error_expected_opening_quote:
    return "Expected string to begin with \"";
  case json_parse_error_invalid_string_escape_sequence:
    return "Invalid escape sequence in string";
  case json_parse_error_invalid_number_format:
    return "Invalid number format";
  case json_parse_error_invalid_value:
    return "Invalid value";
  case json_parse_error_premature_end_of_buffer:
    return "Unexpected end of input buffer in mid-object/array";
  case json_parse_error_invalid_string:
    return "Invalid/malformed string";
  case json_parse_error_allocator_failed:
    return "Memory allocation failure";
  case json_parse_error_unexpected_trailing_characters:
    return "Unexpected trailing characters after JSON data";
  case json_parse_error_unknown:
    return "Uncategorized error";
  default:
    return "Legitimately unrecognized error code (something messed up REAL bad)";
  }
}
}  // namespace

ShadertoyRenderPass::ShadertoyRenderPass() {}
ShadertoyRenderPass::~ShadertoyRenderPass() {
  for (auto input : inputs) {
    delete input.second;
  }
  inputs.clear();
}

ShadertoyInfo::ShadertoyInfo() : renderpasses_{} {}
ShadertoyInfo::~ShadertoyInfo() {
  for (auto& pass : renderpasses_) {
    delete pass;
    pass = nullptr;
  }
}

std::string ShadertoyInfo::JsonValueLocationStr(const json_value_s* val) const {
  const json_value_ex_s* val_ex = (const json_value_ex_s*)val;
  return info_filename_ + "(" + std::to_string(val_ex->line_no) + ":" + std::to_string(val_ex->row_no) + ")";
}

int ShadertoyInfo::ParseShader(const json_value_s* val) {
  if (val->type != json_type_object) {
    fprintf(stderr, "%s: error: shader payload must be an object\n", JsonValueLocationStr(val).c_str());
    return -1;
  }

  json_object_s* shader_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  int parse_error = 0;
  for (json_object_element_s* child_elem = shader_obj->start; i_child < shader_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "ver") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -1,
          "%s: error: \"ver\" payload must be a string", JsonValueLocationStr(val).c_str());
      json_string_s* version_string = (json_string_s*)child_elem->value->payload;
      ZOMBO_ASSERT_RETURN(strcmp(version_string->string, "0.1") == 0, -2,
          "%s: Unexpected version '%s' (only version 0.1 is supported)", JsonValueLocationStr(val).c_str(),
          version_string->string);
    } else if (strcmp(child_elem->name->string, "info") == 0) {
      parse_error = ParseInfo(child_elem->value);
      if (parse_error) {
        break;
      }
    } else if (strcmp(child_elem->name->string, "renderpass") == 0) {
      parse_error = ParseRenderPasses(child_elem->value);
      if (parse_error) {
        break;
      }
    }
  }
  return parse_error;
}

int ShadertoyInfo::ParseInfo(const json_value_s* val) {
  if (val->type != json_type_object) {
    fprintf(stderr, "%s: error: info payload must be an object\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  json_object_s* info_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  int parse_error = 0;
  for (json_object_element_s* child_elem = info_obj->start; i_child < info_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "id") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -1, "%s: error: \"id\" payload must be a string",
          JsonValueLocationStr(val).c_str());
      id_ = std::string(((json_string_s*)child_elem->value->payload)->string);
    } else if (strcmp(child_elem->name->string, "name") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -1,
          "%s: error: \"name\" payload must be a string", JsonValueLocationStr(val).c_str());
      name_ = std::string(((json_string_s*)child_elem->value->payload)->string);
    } else if (strcmp(child_elem->name->string, "username") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -1,
          "%s: error: \"username\" payload must be a string", JsonValueLocationStr(val).c_str());
      username_ = std::string(((json_string_s*)child_elem->value->payload)->string);
    } else if (strcmp(child_elem->name->string, "description") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -1,
          "%s: error: \"description\" payload must be a string", JsonValueLocationStr(val).c_str());
      description_ = std::string(((json_string_s*)child_elem->value->payload)->string);
    }
  }
  return parse_error;
}

int ShadertoyInfo::ParseRenderPasses(const json_value_s* val) {
  if (val->type != json_type_array) {
    fprintf(stderr, "%s: error: renderpass payload must be an array\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  const json_array_s* renderpass_array = (const json_array_s*)(val->payload);
  ZOMBO_ASSERT_RETURN(renderpass_array->length == 1, -2, "%s: Multiple render passes are not currently supported.",
      JsonValueLocationStr(val).c_str());
  size_t i_child = 0;
  int parse_error = 0;
  for (json_array_element_s* child_elem = renderpass_array->start; i_child < renderpass_array->length;
       ++i_child, child_elem = child_elem->next) {
    ShadertoyRenderPass* new_renderpass = new ShadertoyRenderPass;
    parse_error = ParseRenderPass(child_elem->value, new_renderpass);
    if (parse_error) {
      delete new_renderpass;
      break;
    }
    if (new_renderpass->pass_type == SHADERTOY_RENDER_PASS_TYPE_UNKNOWN) {
      delete new_renderpass;
      ZOMBO_ERROR_RETURN(-20, "%s: incomplete \"renderpass\" element", JsonValueLocationStr(val).c_str());
    }
    renderpasses_[new_renderpass->pass_type] = new_renderpass;
  }
  return parse_error;
}

int ShadertoyInfo::ParseRenderPass(const json_value_s* val, ShadertoyRenderPass* out_renderpass) {
  if (val->type != json_type_object) {
    fprintf(stderr, "%s: error: renderpass element must be an object\n", JsonValueLocationStr(val).c_str());
    return -1;
  }

  json_object_s* renderpass_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  int parse_error = 0;
  for (json_object_element_s* child_elem = renderpass_obj->start; i_child < renderpass_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "inputs") == 0) {
      parse_error = ParseInputs(child_elem->value, out_renderpass);
      if (parse_error) {
        break;
      }
    } else if (strcmp(child_elem->name->string, "outputs") == 0) {
      parse_error = ParseOutputs(child_elem->value, out_renderpass);
      if (parse_error) {
        break;
      }
    } else if (strcmp(child_elem->name->string, "name") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -3,
          "%s: error: \"name\" payload must be a string", JsonValueLocationStr(val).c_str());
      json_string_s* name_string = (json_string_s*)child_elem->value->payload;
      if (strcmp(name_string->string, "Image") == 0) {
        out_renderpass->pass_type = SHADERTOY_RENDER_PASS_TYPE_IMAGE;
      } else if (strcmp(name_string->string, "Buffer A") == 0) {
        out_renderpass->pass_type = SHADERTOY_RENDER_PASS_TYPE_BUFFER_A;
      } else if (strcmp(name_string->string, "Buffer B") == 0) {
        out_renderpass->pass_type = SHADERTOY_RENDER_PASS_TYPE_BUFFER_B;
      } else if (strcmp(name_string->string, "Buffer C") == 0) {
        out_renderpass->pass_type = SHADERTOY_RENDER_PASS_TYPE_BUFFER_C;
      } else if (strcmp(name_string->string, "Buffer D") == 0) {
        out_renderpass->pass_type = SHADERTOY_RENDER_PASS_TYPE_BUFFER_D;
      } else if (strcmp(name_string->string, "Cubemap A") == 0) {
        out_renderpass->pass_type = SHADERTOY_RENDER_PASS_TYPE_BUFFER_A;
      } else if (strcmp(name_string->string, "Sound") == 0) {
        out_renderpass->pass_type = SHADERTOY_RENDER_PASS_TYPE_BUFFER_A;
      } else if (strcmp(name_string->string, "Common") == 0) {
        out_renderpass->pass_type = SHADERTOY_RENDER_PASS_TYPE_BUFFER_A;
      } else {
        ZOMBO_ERROR_RETURN(-4, "%s: unrecognized renderpass name \"%s\"",
            JsonValueLocationStr(child_elem->value).c_str(), name_string->string);
      }
    } else if (strcmp(child_elem->name->string, "spokk_local_spv") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -5,
          "%s: error: \"spokk_local_spv\" payload must be a string", JsonValueLocationStr(child_elem->value).c_str());
      json_string_s* spv_string = (json_string_s*)child_elem->value->payload;
      out_renderpass->spv_filename = spv_string->string;
    }
  }
  return parse_error;
}

int ShadertoyInfo::ParseInputs(const json_value_s* val, ShadertoyRenderPass* out_renderpass) {
  if (val->type != json_type_array) {
    fprintf(stderr, "%s: error: inputs payload must be an array\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  const json_array_s* inputs_array = (const json_array_s*)(val->payload);
  size_t i_child = 0;
  int parse_error = 0;
  for (json_array_element_s* child_elem = inputs_array->start; i_child < inputs_array->length;
       ++i_child, child_elem = child_elem->next) {
    parse_error = ParseInput(child_elem->value, out_renderpass);
    if (parse_error) {
      break;
    }
  }
  return parse_error;
}

int ShadertoyInfo::ParseInput(const json_value_s* val, ShadertoyRenderPass* out_renderpass) {
  if (val->type != json_type_object) {
    fprintf(stderr, "%s: error: input element must be an object\n", JsonValueLocationStr(val).c_str());
    return -1;
  }

  ShadertoyInput* new_input = new ShadertoyInput;
  json_object_s* input_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  int parse_error = 0;
  for (json_object_element_s* child_elem = input_obj->start; i_child < input_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "id") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_number, -2, "%s: error: \"id\" payload must be a number",
          JsonValueLocationStr(val).c_str());
      json_number_s* id_num = (json_number_s*)child_elem->value->payload;
      new_input->id = strtol(id_num->number, nullptr, 10);
    } else if (strcmp(child_elem->name->string, "channel") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_number, -3,
          "%s: error: \"channel\" payload must be a number", JsonValueLocationStr(val).c_str());
      json_number_s* channel_num = (json_number_s*)child_elem->value->payload;
      int channel = strtol(channel_num->number, nullptr, 10);
      ZOMBO_ASSERT_RETURN(out_renderpass->inputs.find(channel) == out_renderpass->inputs.end(), -4,
          "%s: duplicate entry for channel %d found", JsonValueLocationStr(val).c_str(), channel);
      new_input->channel = strtol(channel_num->number, nullptr, 10);
    } else if (strcmp(child_elem->name->string, "ctype") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_string, -5,
          "%s: error: \"ctype\" payload must be a string", JsonValueLocationStr(val).c_str());
      json_string_s* ctype_string = (json_string_s*)child_elem->value->payload;
      if (strcmp(ctype_string->string, "texture") == 0) {
        new_input->ctype = SHADERTOY_INPUT_TYPE_TEXTURE;
      } else if (strcmp(ctype_string->string, "cube") == 0) {
        new_input->ctype = SHADERTOY_INPUT_TYPE_CUBE;
      } else {
        ZOMBO_ERROR_RETURN(
            -6, "%s: unsupported input ctype \"%s\"", JsonValueLocationStr(val).c_str(), ctype_string->string);
      }
    } else if (strcmp(child_elem->name->string, "spokk_local_src") == 0) {
      // Parsing this element depends on the input's ctype
      if (child_elem->value->type == json_type_string) {
        json_string_s* src_string = (json_string_s*)child_elem->value->payload;
        new_input->src.clear();
        new_input->src.push_back(src_string->string);
      } else if (child_elem->value->type == json_type_array) {
        json_array_s* src_array = (json_array_s*)child_elem->value->payload;
        ZOMBO_ASSERT_RETURN(src_array->length == 6, -7, "%s: cubemap src array must have exactly 6 elements",
            JsonValueLocationStr(child_elem->value).c_str());
        int i_face = 0;
        new_input->src.clear();
        new_input->src.reserve(6);
        for (json_array_element_s* src_elem = src_array->start; i_face < 6; ++i_face, src_elem = src_elem->next) {
          ZOMBO_ASSERT_RETURN(src_elem->value->type == json_type_string, -8,
              "%s: \"src\" array elements must be strings", JsonValueLocationStr(src_elem->value).c_str());
          json_string_s* face_string = (json_string_s*)src_elem->value->payload;
          new_input->src.push_back(face_string->string);
        }
      }
    }
  }
  if (parse_error) {
    delete new_input;
  }
  if (new_input->channel < 0 || new_input->id < 0 || new_input->src.empty() ||
      new_input->ctype == SHADERTOY_INPUT_TYPE_UNKNOWN) {
    delete new_input;
    ZOMBO_ERROR_RETURN(-20, "%s: incomplete \"input\" element", JsonValueLocationStr(val).c_str());
  }
  out_renderpass->inputs[new_input->channel] = new_input;
  return parse_error;
}

int ShadertoyInfo::ParseOutputs(const json_value_s* val, ShadertoyRenderPass* out_renderpass) {
  if (val->type != json_type_array) {
    fprintf(stderr, "%s: error: outputs payload must be an array\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  const json_array_s* outputs_array = (const json_array_s*)(val->payload);
  ZOMBO_ASSERT_RETURN(outputs_array->length == 1, -2, "%s: Multiple outputs are not currently supported.",
      JsonValueLocationStr(val).c_str());
  size_t i_child = 0;
  int parse_error = 0;
  for (json_array_element_s* child_elem = outputs_array->start; i_child < outputs_array->length;
       ++i_child, child_elem = child_elem->next) {
    parse_error = ParseOutput(child_elem->value, out_renderpass);
    if (parse_error) {
      break;
    }
  }
  return parse_error;
}

int ShadertoyInfo::ParseOutput(const json_value_s* val, ShadertoyRenderPass* /*out_renderpass*/) {
  if (val->type != json_type_object) {
    fprintf(stderr, "%s: error: output element must be an object\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  json_object_s* output_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  int parse_error = 0;
  for (json_object_element_s* child_elem = output_obj->start; i_child < output_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "id") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_number, -2, "%s: error: \"id\" payload must be a number",
          JsonValueLocationStr(val).c_str());
      json_number_s* id_num = (json_number_s*)child_elem->value->payload;
      int32_t id = strtol(id_num->number, nullptr, 10);
      (void)id;
    } else if (strcmp(child_elem->name->string, "channel") == 0) {
      ZOMBO_ASSERT_RETURN(child_elem->value->type == json_type_number, -3,
          "%s: error: \"channel\" payload must be a number", JsonValueLocationStr(val).c_str());
      json_number_s* channel_num = (json_number_s*)child_elem->value->payload;
      int32_t channel = strtol(channel_num->number, nullptr, 10);
      ZOMBO_ASSERT_RETURN(
          channel == 0, -4, "%s: error: \"channel\" (%d) must be zero.", JsonValueLocationStr(val).c_str(), channel);
    }
  }
  return parse_error;
}

int ShadertoyInfo::ParseRoot(const json_value_s* val) {
  if (val->type != json_type_object) {
    fprintf(stderr, "%s: error: root payload must be an object\n", JsonValueLocationStr(val).c_str());
    return -1;
  }
  json_object_s* root_obj = (json_object_s*)(val->payload);
  size_t i_child = 0;
  int parse_error = 0;
  for (json_object_element_s* child_elem = root_obj->start; i_child < root_obj->length;
       ++i_child, child_elem = child_elem->next) {
    if (strcmp(child_elem->name->string, "Shader") == 0) {
      parse_error = ParseShader(child_elem->value);
      if (parse_error) {
        break;
      }
    }
  }
  return parse_error;
}

int ShadertoyInfo::Load(const std::string& json5_filename) {
  // Load JSON file contents
  FILE* shader_info_file = zomboFopen(json5_filename.c_str(), "rb");
  if (!shader_info_file) {
    fprintf(stderr, "ERROR: Could not open shader info file %s\n", json5_filename.c_str());
    return -1;
  }
  fseek(shader_info_file, 0, SEEK_END);
  size_t shader_info_nbytes = ftell(shader_info_file);
  std::vector<uint8_t> shader_info_bytes(shader_info_nbytes);
  fseek(shader_info_file, 0, SEEK_SET);
  size_t read_nbytes = fread(shader_info_bytes.data(), 1, shader_info_nbytes, shader_info_file);
  fclose(shader_info_file);
  if (read_nbytes != shader_info_nbytes) {
    fprintf(stderr, "ERROR: file I/O error while loading %s\n", json5_filename.c_str());
    return -2;
  }

  // Parse JSON
  json_parse_result_s parse_result = {};
  json_value_s* shader_info_json = json_parse_ex(shader_info_bytes.data(), shader_info_bytes.size(),
      json_parse_flags_allow_json5 | json_parse_flags_allow_location_information, NULL, NULL, &parse_result);
  if (!shader_info_json) {
    fprintf(stderr, "%s(%u): error %u at column %u (%s)\n", json5_filename.c_str(),
        (uint32_t)parse_result.error_line_no, (uint32_t)parse_result.error, (uint32_t)parse_result.error_row_no,
        JsonParseErrorStr((json_parse_error_e)parse_result.error));
    return -5;
  }
  info_filename_ = json5_filename;
  int parse_error = ParseRoot(shader_info_json);
  free(shader_info_json);
  return parse_error;
}
