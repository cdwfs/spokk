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

  // prerequisite: if imgui is in use, NewFrame() has already been called.
  if (!ImGui::GetIO().WantCaptureKeyboard) {
#define SPOKK__IS_KEY_DOWN(k) ((ImGui::GetIO().KeysDown[(k)]) ? 1 : 0)
    // TODO(https://github.com/cdwfs/spokk/issues/8): custom key bindings
    current_.digital[DIGITAL_LPAD_UP] = SPOKK__IS_KEY_DOWN(GLFW_KEY_W);
    current_.digital[DIGITAL_LPAD_LEFT] = SPOKK__IS_KEY_DOWN(GLFW_KEY_A);
    current_.digital[DIGITAL_LPAD_RIGHT] = SPOKK__IS_KEY_DOWN(GLFW_KEY_D);
    current_.digital[DIGITAL_LPAD_DOWN] = SPOKK__IS_KEY_DOWN(GLFW_KEY_S);
    current_.digital[DIGITAL_RPAD_LEFT] = SPOKK__IS_KEY_DOWN(GLFW_KEY_LEFT_SHIFT);
    current_.digital[DIGITAL_RPAD_DOWN] = SPOKK__IS_KEY_DOWN(GLFW_KEY_SPACE);
    current_.digital[DIGITAL_RPAD_UP] = SPOKK__IS_KEY_DOWN(GLFW_KEY_V);
    current_.digital[DIGITAL_MENU] = SPOKK__IS_KEY_DOWN(GLFW_KEY_GRAVE_ACCENT);
    current_.digital[DIGITAL_ENTER_KEY] = SPOKK__IS_KEY_DOWN(GLFW_KEY_ENTER);
#undef SPOKK__IS_KEY_DOWN
  } else {
    // If imgui has captured the keyboard, pretend all keys have been released.
    current_.digital.fill(0);
  }

  // TODO(cort): THIS DOES NOT BELONG HERE! I just don't have a way to query Alt+Enter from
  // application code yet. As soon as I can, this should go into Application::Run()!
  if (IsPressed(DIGITAL_ENTER_KEY) && ImGui::GetIO().KeyAlt) {
    static bool started_in_fullscreen = (glfwGetWindowMonitor(pw) != nullptr);
    static int prev_window_xpos = 100, prev_window_ypos = 100;
    static int prev_window_w = 0, prev_window_h = 0;
    GLFWmonitor *monitor = glfwGetWindowMonitor(pw);
    if (!monitor) {
      // windowed -> fullscreen.
      // Save current window pos/size to restore later.
      glfwGetWindowPos(pw, &prev_window_xpos, &prev_window_ypos);
      glfwGetWindowSize(pw, &prev_window_w, &prev_window_h);
      GLFWmonitor *primary_monitor = glfwGetPrimaryMonitor();
      const GLFWvidmode *vid_mode = glfwGetVideoMode(primary_monitor);
      glfwWindowHint(GLFW_RED_BITS, vid_mode->redBits);
      glfwWindowHint(GLFW_GREEN_BITS, vid_mode->greenBits);
      glfwWindowHint(GLFW_BLUE_BITS, vid_mode->blueBits);
      glfwWindowHint(GLFW_REFRESH_RATE, vid_mode->refreshRate);
      int fullscreen_w = started_in_fullscreen ? vid_mode->width : prev_window_w;
      int fullscreen_h = started_in_fullscreen ? vid_mode->height : prev_window_h;
      glfwSetWindowMonitor(pw, glfwGetPrimaryMonitor(), 0, 0, fullscreen_w, fullscreen_h, vid_mode->refreshRate);
    } else {
      // fullscreen -> windowed.
      // Use previous pos/size if possible, or default to 1/2 monitor dimensions
      const GLFWvidmode *vid_mode = glfwGetVideoMode(monitor);
      int window_w = prev_window_w ? prev_window_w : (vid_mode->width / 2);
      int window_h = prev_window_h ? prev_window_h : (vid_mode->height / 2);
      glfwSetWindowMonitor(pw, nullptr, prev_window_xpos, prev_window_ypos, window_w, window_h, GLFW_DONT_CARE);
    }
  }

  // Another option here would be if (!ImGui::GetIO().WantCaptureMouse),
  // but with the current mouse-look implementation, if you can see the cursor, the UI
  // is active and the application shouldn't respond to mouse input.
  int cursor_mode = glfwGetInputMode(pw, GLFW_CURSOR);
  if (cursor_mode != GLFW_CURSOR_NORMAL) {
    double mx = 0, my = 0;
    glfwGetCursorPos(pw, &mx, &my);
    current_.analog[ANALOG_MOUSE_X] = (float)mx;
    current_.analog[ANALOG_MOUSE_Y] = (float)my;
  }
}

void InputState::ClearHistory() {
  // This is meant to be called when starting to update inputstate after a discontinuity (e.g. when leaving UI mode).
  std::shared_ptr<GLFWwindow> w = window_.lock();
  if (w == nullptr) {
    return;  // nothing to clear anyway
  }
  GLFWwindow *pw = w.get();

  double mx = 0, my = 0;
  glfwGetCursorPos(pw, &mx, &my);
  current_.analog[ANALOG_MOUSE_X] = (float)mx;
  current_.analog[ANALOG_MOUSE_Y] = (float)my;

  prev_ = current_;
}
