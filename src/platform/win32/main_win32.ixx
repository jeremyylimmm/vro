#include <windows.h>

import <iostream>;
import <optional>;
import <vector>;

import renderer;
import platform;

struct Events { // Reset every time events are polled
  bool closed;
  std::optional<std::pair<uint32_t, uint32_t>> resize;
};

// Window event callback
static LRESULT CALLBACK window_proc(HWND window, UINT msg, WPARAM w_param, LPARAM l_param) {
  LRESULT result = 0;

  Events& events = *(Events*)GetWindowLongPtrA(window, GWLP_USERDATA);

  switch (msg) {
    default:
      result = DefWindowProcA(window, msg, w_param, l_param);
      break;
    case WM_CLOSE:
      events.closed = true;
      break;
    case WM_SIZE:
      events.resize = { LOWORD(l_param), HIWORD(l_param) };
      break;
  }

  return result;
}

int main() {
  // Register the window class
  WNDCLASSA wc = {
    .lpfnWndProc = window_proc,
    .hInstance = GetModuleHandleA(nullptr),
    .lpszClassName = "bruh",
  };

  RegisterClassA(&wc);

  // Create window
  Events e = {};
  HWND window = CreateWindowA(wc.lpszClassName, "Vro", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, wc.hInstance, nullptr);
  
  // Set events pointer
  SetWindowLongPtrA(window, GWLP_USERDATA, (LONG_PTR)&e);
  ShowWindow(window, SW_MAXIMIZE);

  // Create the renderer
  Renderer r(window);

  // Loop while open
  while (true) {
    e = {}; // Reset events and poll
    MSG msg;
    while (PeekMessageA(&msg, window, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }

    if (e.closed) { // Check if window is closed
      break;
    }

    if (e.resize) { // Handle resize callback -> resize renderer resources
      auto [w, h] = *e.resize;

      if (w == 0 || h == 0) {
        continue;
      }

      r.resize(w, h);
    }

    // Call renderer
    r.present();
  }

  return 0;
}