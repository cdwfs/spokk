#pragma once

#include <stdint.h>
#include <array>
#include <memory>

struct GLFWwindow;

namespace spokk {

class InputState {
public:
  InputState();
  explicit InputState(const std::shared_ptr<GLFWwindow>& window);
  ~InputState() = default;

  void SetWindow(const std::shared_ptr<GLFWwindow>& window);

  enum Digital {
    DIGITAL_LPAD_UP = 0,
    DIGITAL_LPAD_LEFT = 1,
    DIGITAL_LPAD_RIGHT = 2,
    DIGITAL_LPAD_DOWN = 3,
    DIGITAL_RPAD_UP = 4,
    DIGITAL_RPAD_LEFT = 5,
    DIGITAL_RPAD_RIGHT = 6,
    DIGITAL_RPAD_DOWN = 7,

    DIGITAL_MENU = 8,

    DIGITAL_ENTER_KEY = 9,

    DIGITAL_COUNT
  };
  enum Analog {
    ANALOG_L_X = 0,
    ANALOG_L_Y = 1,
    ANALOG_R_X = 2,
    ANALOG_R_Y = 3,
    ANALOG_MOUSE_X = 4,
    ANALOG_MOUSE_Y = 5,

    ANALOG_COUNT
  };
  void Update();
  int32_t GetDigital(Digital id) const { return current_.digital[id]; }
  int32_t GetDigitalDelta(Digital id) const { return current_.digital[id] - prev_.digital[id]; }
  float GetAnalog(Analog id) const { return current_.analog[id]; }
  float GetAnalogDelta(Analog id) const { return current_.analog[id] - prev_.analog[id]; }

  bool IsPressed(Digital id) const { return GetDigitalDelta(id) > 0; }
  bool IsReleased(Digital id) const { return GetDigitalDelta(id) < 0; }

  void ClearHistory();

private:
  struct {
    std::array<int32_t, DIGITAL_COUNT> digital;
    std::array<float, ANALOG_COUNT> analog;
  } current_, prev_;
  std::weak_ptr<GLFWwindow> window_;
};

}  // namespace spokk
