#include <utility>
#include <iostream>
#include <format>
#include <functional>
#include <optional>
#include <cassert>

#include "renderer.h"
#include "base.h"

VkSurfaceKHR create_vulkan_surface(VkInstance instance, platform::WindowHandle window);
std::vector<const char*> get_vulkan_instance_extensions();
VkInstanceCreateFlags get_vulkan_instance_flags();

static constexpr VkFormat swapchain_format = VK_FORMAT_R8G8B8A8_UNORM;

template<typename... Args>
void fatal_error(const std::format_string<Args...>& fmt, Args&&... args) {
  std::string s = std::format(fmt, std::forward<Args>(args)...);
  platform::message_box(s.c_str(), "Error");
  platform::exit_program(1);
}

template<typename T, typename I, typename R>
std::vector<T> vk_enumerate(I instance, R(*func)(I, uint32_t*, T*)) {
  uint32_t count = 0;
  func(instance, &count, nullptr);

  std::vector<T> v(count);
  func(instance, &count, v.data());

  return v;
}

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    std::cerr << "Vulkan validation: " << data->pMessage << std::endl;
    assert("vulkan validation; check console" && false);
    return VK_FALSE;
}

Renderer::Renderer(platform::WindowHandle window) {
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
#if _DEBUG
    "VK_EXT_debug_utils",
#endif
  };

  auto platform_extensions = get_vulkan_instance_extensions();
  extensions.insert(extensions.end(), platform_extensions.begin(), platform_extensions.end());

  VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .flags = get_vulkan_instance_flags(),
    .pApplicationInfo = &app_info,
    .enabledLayerCount = (uint32_t)validation_layers.size(),
    .ppEnabledLayerNames = validation_layers.data(),
    .enabledExtensionCount = (uint32_t)extensions.size(),
    .ppEnabledExtensionNames = extensions.data(),
  };

  if (vkCreateInstance(&instance_info, nullptr, &m_instance) != VK_SUCCESS) {
    fatal_error("Failed to create Vulkan instance.");
  }

#if _DEBUG
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = vulkan_debug_callback
  };
  
  auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
  assert(vkCreateDebugUtilsMessengerEXT);
  VkDebugUtilsMessengerEXT debug_messenger;
  vkCreateDebugUtilsMessengerEXT(m_instance, &debug_messenger_info, nullptr, &debug_messenger);
#endif

  m_surface = create_vulkan_surface(m_instance, window);
  
  if (!m_surface) {
    fatal_error("Failed to create Vulkan surface.");
  }

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

  for (auto i : Range<size_t>(FRAMES_IN_FLIGHT)) {
    VkFenceCreateInfo fence_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    vkCreateFence(m_device, &fence_info, nullptr, &m_fences[i]);
  }

  for (auto i : Range<size_t>(FRAMES_IN_FLIGHT)) {
    VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_semaphores[i]);
  }

  m_triangle_vs = load_shader("shaders/triangle.vert.spv");
  m_triangle_fs = load_shader("shaders/triangle.frag.spv");

  std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
    make_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, m_triangle_vs),
    make_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, m_triangle_fs),
  };

  std::vector<VkDynamicState> dynamic_states = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamic_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = (uint32_t)dynamic_states.size(),
    .pDynamicStates = dynamic_states.data()
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  };

  VkPipelineViewportStateCreateInfo viewport_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1
  };

  VkPipelineRasterizationStateCreateInfo rast_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .lineWidth = 1.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisampling = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
    .minSampleShading = 1.0f
  };

  VkPipelineColorBlendAttachmentState blend_attachment = {
    .blendEnable = VK_FALSE,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPipelineColorBlendStateCreateInfo blend_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .attachmentCount = 1,
    .pAttachments = &blend_attachment,
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
  };

  if (vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout) != VK_SUCCESS) {
    fatal_error("Failed to create Vulkan pipeline layout.");
  }

  VkAttachmentDescription color_attachment = {
    .format = swapchain_format,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  };

  VkAttachmentReference color_attachment_ref = {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment_ref,
  };

  VkSubpassDependency subpass_dependency = {
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };
  
  VkRenderPassCreateInfo render_pass_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &color_attachment,
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 1,
    .pDependencies = &subpass_dependency
  };

  if (vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass) != VK_SUCCESS) {
    fatal_error("Failed to create Vulkan render pass.");
  }

  VkGraphicsPipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = (uint32_t)shader_stages.size(),
    .pStages = shader_stages.data(),
    .pVertexInputState = &vertex_input_info,
    .pInputAssemblyState = &input_assembly_info,
    .pViewportState = &viewport_state_info,
    .pRasterizationState = &rast_info,
    .pMultisampleState = &multisampling,
    .pColorBlendState = &blend_state_info,
    .pDynamicState = &dynamic_state_info,
    .layout = m_pipeline_layout,
    .renderPass = m_render_pass,
    .subpass = 0
  };

  if (vkCreateGraphicsPipelines(m_device, nullptr, 1, &pipeline_info, nullptr, &m_pipeline) != VK_SUCCESS) {
    fatal_error("Failed to create Vulkan graphics pipeline.");
  }

  auto [window_w, window_h] = platform::get_window_size(window);
  resize(window_w, window_h);

  VkCommandPoolCreateInfo command_pool_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = queue_id,
  };

  if (vkCreateCommandPool(m_device, &command_pool_info, nullptr, &m_command_pool) != VK_SUCCESS) {
    fatal_error("Failed to create Vulkan command pool.");
  }

  for (auto i : Range<size_t>(FRAMES_IN_FLIGHT)) {
    VkCommandBufferAllocateInfo cmd_buf_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = m_command_pool,
      .commandBufferCount = 1
    };

    if (vkAllocateCommandBuffers(m_device, &cmd_buf_info, &m_command_buffers[i]) != VK_SUCCESS) {
      fatal_error("Failed to create Vulkan command buffer.");
    }
  }
}

void Renderer::resize(uint32_t width, uint32_t height) {
  m_swapchain_width = width;
  m_swapchain_height = height;

  vkDeviceWaitIdle(m_device);

  if (m_swapchain_image_views.size()) {
    for (auto view : m_swapchain_image_views) {
      vkDestroyImageView(m_device, view, nullptr);
    }

    for (auto fb : m_swapchain_framebuffers) {
      vkDestroyFramebuffer(m_device, fb, nullptr);
    }
  }

  VkExtent2D image_extent = {
    .width = width,
    .height = height
  };

  VkSwapchainCreateInfoKHR swapchain_info = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = m_surface,
    .minImageCount = 2,
    .imageFormat = swapchain_format,
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
  m_swapchain_image_views.resize(image_count);
  m_swapchain_framebuffers.resize(image_count);

  vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_swapchain_images.data());

  for (auto i : Range<uint32_t>(image_count)) {
    VkImageSubresourceRange subresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = 1,
      .layerCount = 1
    };

    VkImageViewCreateInfo view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = m_swapchain_images[i],
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = swapchain_info.imageFormat,
      .subresourceRange = subresource
    };

    if (vkCreateImageView(m_device, &view_info, nullptr, &m_swapchain_image_views[i]) != VK_SUCCESS) {
      fatal_error("Failed to create Vulkan swapchain image view.");
    }

    VkFramebufferCreateInfo framebuffer_info = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, 
      .renderPass = m_render_pass,
      .attachmentCount = 1,
      .pAttachments = &m_swapchain_image_views[i],
      .width = width,
      .height = height,
      .layers = 1
    };

    if (vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_swapchain_framebuffers[i]) != VK_SUCCESS) {
      fatal_error("Failed to create Vulkan framebuffer.");
    }
  }
}

void Renderer::present() {
  VkCommandBuffer cmd_buf = m_command_buffers[m_frame_index];
  vkWaitForFences(m_device, 1, &m_fences[m_frame_index], true, UINT64_MAX);
  vkResetFences(m_device, 1, &m_fences[m_frame_index]);

  VkCommandBufferBeginInfo cmd_begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  vkResetCommandBuffer(cmd_buf, 0);
  if(vkBeginCommandBuffer(cmd_buf, &cmd_begin_info) != VK_SUCCESS) {
    fatal_error("Failed to begin Vulkan command buffer.");
  }

  uint32_t image_index;
  vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_semaphores[m_frame_index], nullptr, &image_index);

  VkExtent2D render_extent = {
    .width = m_swapchain_width,
    .height = m_swapchain_height
  };

  VkRect2D render_area = {
    .extent = render_extent
  };

  VkClearValue clear_color = {
    {0.1f, 0.1f, 0.1f, 1.0f}
  };

  VkRenderPassBeginInfo render_pass_begin_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = m_render_pass,
    .framebuffer = m_swapchain_framebuffers[image_index],
    .renderArea = render_area,
    .clearValueCount = 1,
    .pClearValues = &clear_color
  };

  vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline); 

  VkViewport viewport = {
    .width = (float)m_swapchain_width,
    .height = (float)m_swapchain_height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  VkRect2D scissor = {
    .extent = render_extent
  };

  vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
  vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

  vkCmdDraw(cmd_buf, 3, 1, 0, 0);

  vkCmdEndRenderPass(cmd_buf);

  if(vkEndCommandBuffer(cmd_buf) != VK_SUCCESS) {
    fatal_error("Failed to end Vulkan command buffer.");
  }

  VkPipelineStageFlags wait_stages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  
  VkSubmitInfo submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &m_semaphores[m_frame_index],
    .pWaitDstStageMask = wait_stages,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd_buf,
  };

  vkQueueSubmit(m_queue, 1, &submit_info, m_fences[m_frame_index]);

  VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .swapchainCount = 1,
    .pSwapchains = &m_swapchain,
    .pImageIndices = &image_index,
  };

  vkQueuePresentKHR(m_queue, &present_info);

  m_frame_index = (m_frame_index + 1) % FRAMES_IN_FLIGHT;
}

VkShaderModule Renderer::load_shader(const char* path) {
  std::optional<std::vector<uint8_t>> shader_code = load_binary(path);

  if (!shader_code) {
    fatal_error("Missing shader at '{}'", path);
  }

  VkShaderModuleCreateInfo module_info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = shader_code->size(),
    .pCode = (uint32_t*)shader_code->data(),
  };

  VkShaderModule module;
  if (vkCreateShaderModule(m_device, &module_info, nullptr, &module) != VK_SUCCESS) {
    fatal_error("Error in shader creation '{}'", path);
  }

  return module;
}

VkPipelineShaderStageCreateInfo Renderer::make_shader_stage(VkShaderStageFlagBits stage, VkShaderModule module) {
  return VkPipelineShaderStageCreateInfo {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = stage,
    .module = module,
    .pName = "main"
  };
}