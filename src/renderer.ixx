module;

#include <Windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

export module renderer;

import <utility>;
import <iostream>;
import <format>;
import <vector>;
import <functional>;

template<typename... Args>
void fatal_error(const std::format_string<Args...>& fmt, Args&&... args) {
  std::string s = std::format(fmt, std::forward<Args>(args)...);
  MessageBoxA(nullptr, s.c_str(), "Error", 0);
  ExitProcess(1);
}

template<typename T, typename I, typename R>
std::vector<T> vk_enumerate(I instance, R(*func)(I, uint32_t*, T*)) {
  uint32_t count = 0;
  func(instance, &count, nullptr);

  std::vector<T> v(count);
  func(instance, &count, v.data());

  return v;
}

std::pair<uint32_t, uint32_t> hwnd_size(HWND window) {
  RECT rect;
  GetClientRect(window, &rect);

  return {
    rect.right-rect.left,
    rect.bottom-rect.top
  };
}

export class Renderer {
public:
  Renderer(HWND window) {
    (void)window;

    VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Vro",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "Vro Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0
    };

    std::vector<const char*> validation_layers = {
#if _DEBUG
      "VK_LAYER_KHRONOS_validation",
#endif
    };

    std::vector<const char*> extensions = {
      "VK_KHR_surface",
#ifdef __APPLE__
      "VK_KHR_portability_enumeration",
#else ifdef _WIN32
      "VK_KHR_win32_surface",
#endif
    };

    VkInstanceCreateInfo instance_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledLayerCount = (uint32_t)validation_layers.size(),
      .ppEnabledLayerNames = validation_layers.data(),
      .enabledExtensionCount = (uint32_t)extensions.size(),
      .ppEnabledExtensionNames = extensions.data()
    };

#ifdef __APPLE__
    instance_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    if (vkCreateInstance(&instance_info, nullptr, &m_instance) != VK_SUCCESS) {
      fatal_error("Failed to create Vulkan instance.");
    }

#if _WIN32
    VkWin32SurfaceCreateInfoKHR surface_info = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = GetModuleHandleA(nullptr),
      .hwnd = window,
    };

    if(vkCreateWin32SurfaceKHR(m_instance, &surface_info, nullptr, &m_surface) != VK_SUCCESS) {
      fatal_error("Failed to create Vulkan surface.");
    }
#endif

    std::vector<VkPhysicalDevice> devices = vk_enumerate(m_instance, vkEnumeratePhysicalDevices);
    if (!devices.size()) {
      fatal_error("Failed to find Vulkan device.");
    }

    m_physical_device = devices[0];

    std::vector<VkQueueFamilyProperties> queue_props = vk_enumerate(m_physical_device, vkGetPhysicalDeviceQueueFamilyProperties);

    uint32_t queue_id = 0xffffffff;

    for (uint32_t i = 0; i < queue_props.size(); ++i) {
      auto flags = queue_props[i].queueFlags;

      VkBool32 present_support = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, m_surface, &present_support);

      if (
        flags & VK_QUEUE_GRAPHICS_BIT && 
        present_support
      ) {
        queue_id = i;
        break;
      }
    }

    if (queue_id == 0xffffffff) {
      fatal_error("Failed to find a Vulkan queue family");
    }

    float queue_priority = 1.0f;

    VkDeviceQueueCreateInfo queue_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = queue_id,
      .queueCount = 1,
      .pQueuePriorities = &queue_priority
    };

    VkPhysicalDeviceFeatures device_features = {};

    std::vector<const char*> device_extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo device_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queue_info,
      .enabledExtensionCount = (uint32_t)device_extensions.size(),
      .ppEnabledExtensionNames = device_extensions.data(),
      .pEnabledFeatures = &device_features,
    };

    if(vkCreateDevice(m_physical_device, &device_info, nullptr, &m_device) != VK_SUCCESS) {
      fatal_error("Failed to create Vulkan device.");
    }

    vkGetDeviceQueue(m_device, queue_id, 0, &m_queue);

    auto [window_w, window_h] = hwnd_size(window);
    resize(window_w, window_h);

    VkFenceCreateInfo fence_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    vkCreateFence(m_device, &fence_info, nullptr, &m_fence);
  }

  void resize(uint32_t width, uint32_t height) {
    vkDeviceWaitIdle(m_device);

    VkExtent2D image_extent = {
      .width = width,
      .height = height
    };

    VkSwapchainCreateInfoKHR swapchain_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = m_surface,
      .minImageCount = 2,
      .imageFormat = VK_FORMAT_R8G8B8A8_UNORM,
      .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
      .imageExtent = image_extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .clipped = VK_TRUE,
      .oldSwapchain = m_swapchain
    };

    if(vkCreateSwapchainKHR(m_device, &swapchain_info, nullptr, &m_swapchain) != VK_SUCCESS) {
      fatal_error("Failed to create Vulkan swapchain.");
    }

    if (swapchain_info.oldSwapchain) {
      vkDestroySwapchainKHR(m_device, swapchain_info.oldSwapchain, nullptr);
    }

    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
    m_swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_swapchain_images.data());
  }

  void present() {
    uint32_t image_index;
    vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, nullptr, m_fence, &image_index);
    vkWaitForFences(m_device, 1, &m_fence, true, UINT64_MAX);
    vkResetFences(m_device, 1, &m_fence);

    VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .swapchainCount = 1,
      .pSwapchains = &m_swapchain,
      .pImageIndices = &image_index
    };
    vkQueuePresentKHR(m_queue, &present_info);
  }

private:
  VkInstance m_instance;
  VkPhysicalDevice m_physical_device;
  VkDevice m_device;
  VkQueue m_queue;
  VkSurfaceKHR m_surface;
  VkSwapchainKHR m_swapchain = nullptr;
  std::vector<VkImage> m_swapchain_images;
  VkFence m_fence;
};
