#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "platform/platform.h"

static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

class Renderer {
public:
  Renderer(platform::WindowHandle window);
  void resize(uint32_t width, uint32_t height);
  void present();

private:
  VkShaderModule load_shader(const char* path);
  VkPipelineShaderStageCreateInfo make_shader_stage(VkShaderStageFlagBits stage, VkShaderModule module);

private:
  VkInstance m_instance;
  VkPhysicalDevice m_physical_device;
  VkDevice m_device;
  VkQueue m_queue;
  VkSurfaceKHR m_surface;
  VkSwapchainKHR m_swapchain = nullptr;
  uint32_t m_swapchain_width;
  uint32_t m_swapchain_height;
  std::vector<VkImage> m_swapchain_images;
  std::vector<VkImageView> m_swapchain_image_views;
  std::vector<VkFramebuffer> m_swapchain_framebuffers;
  VkFence m_fences[FRAMES_IN_FLIGHT] = {};
  VkShaderModule m_triangle_vs;
  VkShaderModule m_triangle_fs;
  VkPipelineLayout m_pipeline_layout;
  VkRenderPass m_render_pass;
  VkPipeline m_pipeline;
  VkCommandPool m_command_pool;
  VkSemaphore m_semaphores[FRAMES_IN_FLIGHT] = {};
  VkCommandBuffer m_command_buffers[FRAMES_IN_FLIGHT] = {};
  uint32_t m_frame_index = 0;
};
