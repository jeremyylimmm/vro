module;

#include <vulkan/vulkan.h>

export module platform;

import <vector>;
import <utility>;

export namespace platform {
  using WindowHandle = ;

  std::pair<uint32_t, uint32_t> get_window_size(WindowHandle window) {
  }

  void exit_program(int code) {
  }

  void message_box(const char* title, const char* message) {
  }


  VkSurfaceKHR create_vulkan_surface(VkInstance instance, platform::WindowHandle window) {
  }
  
  std::vector<const char*> get_vulkan_instance_extensions() {
    return {
      "VK_KHR_portability_enumeration",
    };
  }

  VkInstanceCreateFlags get_vulkan_instance_flags() {
    return VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  }

};
