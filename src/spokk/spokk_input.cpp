#include "spokk_input.h"
#include "spokk_platform.h"
using namespace spokk;

#include <GLFW/glfw3.h>

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

  // TODO(https://github.com/cdwfs/spokk/issues/8): custom key bindings
  current_.digital[DIGITAL_LPAD_UP] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_W)) ? 1 : 0;
  current_.digital[DIGITAL_LPAD_LEFT] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_A)) ? 1 : 0;
  current_.digital[DIGITAL_LPAD_RIGHT] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_D)) ? 1 : 0;
  current_.digital[DIGITAL_LPAD_DOWN] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_S)) ? 1 : 0;
  current_.digital[DIGITAL_RPAD_LEFT] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_LEFT_SHIFT)) ? 1 : 0;
  current_.digital[DIGITAL_RPAD_DOWN] = (GLFW_PRESS == glfwGetKey(pw, GLFW_KEY_SPACE)) ? 1 : 0;

  double mx = 0, my = 0;
  glfwGetCursorPos(pw, &mx, &my);
  current_.analog[ANALOG_MOUSE_X] = (float)mx;
  current_.analog[ANALOG_MOUSE_Y] = (float)my;
}
