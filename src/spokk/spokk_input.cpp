#include "spokk_input.h"
#include "spokk_platform.h"
using namespace spokk;

#include <GLFW/glfw3.h>
#include <imgui.h>

//
// InputState
//
InputState::InputState() : current_{}, prev_{}, window_{} {}
InputState::InputState(const std::shared_ptr<GLFWwindow> &window) : current_{}, prev_{}, window_{} {
  SetWindow(window);
}

void InputState::SetWindow(const std::shared_ptr<GLFWwindow> &window) {
  window_ = window;
  // Force an update to get meaningful deltas on the first frame
  Update();
}

void InputState::Update(void) {
  std::shared_ptr<GLFWwindow> w = window_.lock();
  ZOMBO_ASSERT(w != nullptr, "window pointer is NULL");
  GLFWwindow *pw = w.get();

  prev_ = current_;

  ImGuiIO &io = ImGui::GetIO();
  bool is_imgui_enabled = (io.Fonts[0].TexID != 0);  // proxy for "has imgui been initialized?"
  bool is_imgui_visible = (io.RenderDrawListsFn != nullptr);

  // prerequisite: if imgui is in use, NewFrame() has already been called.
  if (!io.WantCaptureKeyboard) {
#define SPOKK__IS_KEY_DOWN(k) ((is_imgui_enabled) ? io.KeysDown[(k)] : glfwGetKey(pw, (k))) ? 1 : 0
    // TODO(https://github.com/cdwfs/spokk/issues/8): custom key bindings
    current_.digital[DIGITAL_LPAD_UP] = SPOKK__IS_KEY_DOWN(GLFW_KEY_W);
    current_.digital[DIGITAL_LPAD_LEFT] = SPOKK__IS_KEY_DOWN(GLFW_KEY_A);
    current_.digital[DIGITAL_LPAD_RIGHT] = SPOKK__IS_KEY_DOWN(GLFW_KEY_D);
    current_.digital[DIGITAL_LPAD_DOWN] = SPOKK__IS_KEY_DOWN(GLFW_KEY_S);
    current_.digital[DIGITAL_RPAD_LEFT] = SPOKK__IS_KEY_DOWN(GLFW_KEY_LEFT_SHIFT);
    current_.digital[DIGITAL_RPAD_DOWN] = SPOKK__IS_KEY_DOWN(GLFW_KEY_SPACE);
    current_.digital[DIGITAL_MENU] = SPOKK__IS_KEY_DOWN(GLFW_KEY_GRAVE_ACCENT);
#undef SPOKK__IS_KEY_DOWN
  } else {
    // If imgui has captured the keyboard, pretend all keys have been released.
    current_.digital.fill(0);
  }

  if (!is_imgui_visible) {
    double mx = 0, my = 0;
    glfwGetCursorPos(pw, &mx, &my);
    current_.analog[ANALOG_MOUSE_X] = (float)mx;
    current_.analog[ANALOG_MOUSE_Y] = (float)my;
  }
}

void InputState::ClearHistory() {
  // This is meant to be called when starting to update inputstate after a discontinuity (e.g. when leaving UI mode).
  std::shared_ptr<GLFWwindow> w = window_.lock();
  ZOMBO_ASSERT(w != nullptr, "window pointer is NULL");
  GLFWwindow *pw = w.get();

  double mx = 0, my = 0;
  glfwGetCursorPos(pw, &mx, &my);
  current_.analog[ANALOG_MOUSE_X] = (float)mx;
  current_.analog[ANALOG_MOUSE_Y] = (float)my;

  prev_ = current_;
}