module;

#include <Windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

export module platform;

import <vector>;
import <utility>;

export namespace platform {
  using WindowHandle = HWND;

  std::pair<uint32_t, uint32_t> get_window_size(WindowHandle window) {
    RECT rect;
    GetClientRect(window, &rect);

    return {
      rect.right-rect.left,
      rect.bottom-rect.top
    };
  }

  void exit_program(int code) {
    ExitProcess(code);
  }

  void message_box(const char* title, const char* message) {
    MessageBoxA(nullptr, message, title, 0);
  }


  VkSurfaceKHR create_vulkan_surface(VkInstance instance, platform::WindowHandle window) {
    HWND hwnd = (HWND)window;
  
    VkWin32SurfaceCreateInfoKHR surface_info = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = GetModuleHandleA(nullptr),
      .hwnd = hwnd,
    };
  
    VkSurfaceKHR surface;
    if(vkCreateWin32SurfaceKHR(instance, &surface_info, nullptr, &surface) != VK_SUCCESS) {
      return nullptr;
    }
  
    return surface;
  }
  
  std::vector<const char*> get_platform_instance_extensions() {
    return {
      "VK_KHR_win32_surface",
    };
  }

};