#pragma once

#include <utility>

namespace platform {
  using WindowHandle = void*;

  std::pair<uint32_t, uint32_t> get_window_size(WindowHandle window);
  void exit_program(int code);
  void message_box(const char* title, const char* message);

};