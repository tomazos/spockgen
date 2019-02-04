
#include "Engine.h"
#include "Instance.h"
#include "Surface.h"
#include "Window.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

std::vector<char> slurp_file(const std::experimental::filesystem::path& path) {
  if (!exists(path)) {
    std::cerr << "No such file: " << path;
    std::exit(EXIT_FAILURE);
  }
  std::ifstream file;
  file.exceptions(std::ios::badbit | std::ios::failbit | std::ios::eofbit);

  file.open(path.string(), std::ios::ate | std::ios::binary);
  size_t n = file.tellg();
  std::vector<char> buf(n);
  file.seekg(0);
  file.read(buf.data(), n);
  return buf;
}

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;

  static vk::VertexInputBindingDescription get_binding_description() {
    vk::VertexInputBindingDescription binding_description;
    binding_description.setBinding(0);
    binding_description.setStride(sizeof(Vertex));
    binding_description.setInputRate(vk::VertexInputRate::eVertex);
    return binding_description;
  }

  static std::array<vk::VertexInputAttributeDescription, 2>
  get_attribute_descriptions() {
    std::array<vk::VertexInputAttributeDescription, 2> attribute_descriptions;

    attribute_descriptions[0].setBinding(0);
    attribute_descriptions[0].setLocation(0);
    attribute_descriptions[0].setFormat(vk::Format::eR32G32Sfloat);
    attribute_descriptions[0].setOffset(offsetof(Vertex, pos));
    attribute_descriptions[1].setBinding(0);
    attribute_descriptions[1].setLocation(1);
    attribute_descriptions[1].setFormat(vk::Format::eR32G32B32Sfloat);
    attribute_descriptions[1].setOffset(offsetof(Vertex, color));

    return attribute_descriptions;
  }
};

const std::vector<Vertex> vertices = {{{0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

int main(int argc, char** argv) {
  try {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    spk::Engine engine;

    spk::Window window("sdltest");

    std::vector<std::string> extensions = window.extensions();
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    spk::Instance instance("sdltest", spk::Version(1, 0, 0), "custom",
                           spk::Version(1, 0, 0), spk::Version(1, 1, 0),
                           extensions, {"VK_LAYER_LUNARG_standard_validation"});

    spk::Surface surface(instance, window);

    if (!instance.physical_device.getSurfaceSupportKHR(0, surface.surface))
      throw std::runtime_error("surface not supported by physical device");

    vk::SurfaceCapabilitiesKHR surface_capabilities =
        instance.physical_device.getSurfaceCapabilitiesKHR(surface.surface);

    vk::Extent2D extent = surface_capabilities.currentExtent;
    std::cerr << extent.height << std::endl;
    std::cerr << extent.width << std::endl;

    std::vector<vk::SurfaceFormatKHR> surface_formats =
        instance.physical_device.getSurfaceFormatsKHR(surface.surface);

    vk::SurfaceFormatKHR surface_format = surface_formats.at(0);
    vk::Format format = surface_format.format;

    std::vector<vk::PresentModeKHR> present_modes =
        instance.physical_device.getSurfacePresentModesKHR(surface.surface);

    vk::SwapchainCreateInfoKHR swapchain_create_info;
    swapchain_create_info.setSurface(surface.surface);
    swapchain_create_info.setMinImageCount(2);
    swapchain_create_info.setImageExtent(extent);
    swapchain_create_info.setImageArrayLayers(1);
    swapchain_create_info.setImageUsage(
        vk::ImageUsageFlagBits::eColorAttachment);
    swapchain_create_info.setImageFormat(format);
    swapchain_create_info.setImageColorSpace(surface_format.colorSpace);
    swapchain_create_info.setImageSharingMode(vk::SharingMode::eExclusive);
    swapchain_create_info.setPreTransform(
        surface_capabilities.currentTransform);
    swapchain_create_info.setCompositeAlpha(
        vk::CompositeAlphaFlagBitsKHR::eOpaque);
    swapchain_create_info.setPresentMode(vk::PresentModeKHR::eFifo);
    swapchain_create_info.setClipped(true);
    vk::SwapchainKHR swapchain =
        instance.device.createSwapchainKHR(swapchain_create_info);

    std::vector<vk::Image> swapchain_images =
        instance.device.getSwapchainImagesKHR(swapchain);

    std::vector<vk::ImageView> swapchain_image_views;

    for (vk::Image& image : swapchain_images) {
      vk::ImageViewCreateInfo image_view_create_info;
      image_view_create_info.setImage(image);
      image_view_create_info.setViewType(vk::ImageViewType::e2D);
      image_view_create_info.setFormat(swapchain_create_info.imageFormat);
      image_view_create_info.components.setR(vk::ComponentSwizzle::eIdentity);
      image_view_create_info.components.setG(vk::ComponentSwizzle::eIdentity);
      image_view_create_info.components.setB(vk::ComponentSwizzle::eIdentity);
      image_view_create_info.components.setA(vk::ComponentSwizzle::eIdentity);
      image_view_create_info.subresourceRange.setAspectMask(
          vk::ImageAspectFlagBits::eColor);
      image_view_create_info.subresourceRange.setBaseMipLevel(0);
      image_view_create_info.subresourceRange.setLevelCount(1);
      image_view_create_info.subresourceRange.setBaseArrayLayer(0);
      image_view_create_info.subresourceRange.setLayerCount(1);
      swapchain_image_views.push_back(
          instance.device.createImageView(image_view_create_info));
    }

    std::vector<char> vert_shader_code = slurp_file("sdltest/vert.spv");
    vk::ShaderModuleCreateInfo vert_shader_module_create_info;
    vert_shader_module_create_info.setCodeSize(vert_shader_code.size());
    vert_shader_module_create_info.setPCode(
        (const uint32_t*)vert_shader_code.data());
    vk::ShaderModule vert_shader =
        instance.device.createShaderModule(vert_shader_module_create_info);

    std::vector<char> frag_shader_code = slurp_file("sdltest/frag.spv");
    vk::ShaderModuleCreateInfo frag_shader_module_create_info;
    frag_shader_module_create_info.setCodeSize(frag_shader_code.size());
    frag_shader_module_create_info.setPCode(
        (const uint32_t*)frag_shader_code.data());
    vk::ShaderModule frag_shader =
        instance.device.createShaderModule(frag_shader_module_create_info);

    vk::PipelineShaderStageCreateInfo vert_pipeline_shader_stage_create_info;
    vert_pipeline_shader_stage_create_info.setStage(
        vk::ShaderStageFlagBits::eVertex);
    vert_pipeline_shader_stage_create_info.setModule(vert_shader);
    vert_pipeline_shader_stage_create_info.setPName("main");

    vk::PipelineShaderStageCreateInfo frag_pipeline_shader_stage_create_info;
    frag_pipeline_shader_stage_create_info.setStage(
        vk::ShaderStageFlagBits::eFragment);
    frag_pipeline_shader_stage_create_info.setModule(frag_shader);
    frag_pipeline_shader_stage_create_info.setPName("main");

    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vert_pipeline_shader_stage_create_info,
        frag_pipeline_shader_stage_create_info};

    vk::PipelineVertexInputStateCreateInfo
        pipeline_vertex_input_state_create_info;

    vk::DescriptorSetLayoutBinding descriptor_set_layout_binding;
    descriptor_set_layout_binding.setBinding(0);
    descriptor_set_layout_binding.setDescriptorType(
        vk::DescriptorType::eUniformBuffer);
    descriptor_set_layout_binding.setDescriptorCount(1);
    descriptor_set_layout_binding.setStageFlags(
        vk::ShaderStageFlagBits::eVertex);
    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
    descriptor_set_layout_create_info.setBindingCount(1);
    descriptor_set_layout_create_info.setPBindings(
        &descriptor_set_layout_binding);
    vk::DescriptorSetLayout descriptor_set_layout =
        instance.device.createDescriptorSetLayout(
            descriptor_set_layout_create_info);

    vk::VertexInputBindingDescription binding_description =
        Vertex::get_binding_description();
    auto attribute_descriptions = Vertex::get_attribute_descriptions();

    pipeline_vertex_input_state_create_info.setVertexBindingDescriptionCount(1);
    pipeline_vertex_input_state_create_info.setPVertexBindingDescriptions(
        &binding_description);
    pipeline_vertex_input_state_create_info.setVertexAttributeDescriptionCount(
        attribute_descriptions.size());
    pipeline_vertex_input_state_create_info.setPVertexAttributeDescriptions(
        attribute_descriptions.data());

    vk::PipelineInputAssemblyStateCreateInfo
        pipeline_input_assembly_state_create_info;
    pipeline_input_assembly_state_create_info.setTopology(
        vk::PrimitiveTopology::eTriangleList);
    pipeline_input_assembly_state_create_info.setPrimitiveRestartEnable(false);

    vk::Viewport viewport;
    viewport.setX(0);
    viewport.setY(0);
    viewport.setWidth(extent.width);
    viewport.setHeight(extent.height);
    viewport.setMinDepth(0);
    viewport.setMaxDepth(1);

    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = extent;

    vk::PipelineViewportStateCreateInfo pipeline_viewport_state_create_info;
    pipeline_viewport_state_create_info.setViewportCount(1);
    pipeline_viewport_state_create_info.setPViewports(&viewport);
    pipeline_viewport_state_create_info.setScissorCount(1);
    pipeline_viewport_state_create_info.setPScissors(&scissor);

    vk::PipelineRasterizationStateCreateInfo
        pipeline_rasterization_state_create_info;
    pipeline_rasterization_state_create_info.setDepthClampEnable(false);
    pipeline_rasterization_state_create_info.setRasterizerDiscardEnable(false);
    pipeline_rasterization_state_create_info.setPolygonMode(
        vk::PolygonMode::eFill);
    pipeline_rasterization_state_create_info.setLineWidth(1);
    pipeline_rasterization_state_create_info.setCullMode(
        vk::CullModeFlagBits::eBack);
    pipeline_rasterization_state_create_info.setFrontFace(
        vk::FrontFace::eClockwise);
    pipeline_rasterization_state_create_info.setDepthBiasEnable(false);

    vk::PipelineMultisampleStateCreateInfo pipeline_multisample_create_info;
    pipeline_multisample_create_info.setSampleShadingEnable(false);
    pipeline_multisample_create_info.setRasterizationSamples(
        vk::SampleCountFlagBits::e1);

    vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state;
    pipeline_color_blend_attachment_state.setColorWriteMask(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    pipeline_color_blend_attachment_state.setBlendEnable(false);

    vk::PipelineColorBlendStateCreateInfo
        pipeline_color_blend_state_create_info;
    pipeline_color_blend_state_create_info.setLogicOpEnable(false);
    pipeline_color_blend_state_create_info.setAttachmentCount(1);
    pipeline_color_blend_state_create_info.setPAttachments(
        &pipeline_color_blend_attachment_state);
    pipeline_color_blend_state_create_info.setBlendConstants(
        std::array<float, 4>({0.0f, 0.0f, 0.0f, 0.0f}));

    vk::PipelineLayoutCreateInfo pipeline_layout_create_info;
    pipeline_layout_create_info.setSetLayoutCount(1);
    pipeline_layout_create_info.setPSetLayouts(&descriptor_set_layout);

    vk::PipelineLayout pipeline_layout =
        instance.device.createPipelineLayout(pipeline_layout_create_info);

    vk::AttachmentDescription color_attachment;
    color_attachment.setFormat(swapchain_create_info.imageFormat);
    color_attachment.setSamples(vk::SampleCountFlagBits::e1);
    color_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    color_attachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    color_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    color_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    color_attachment.setInitialLayout(vk::ImageLayout::eUndefined);
    color_attachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference color_attachment_ref;
    color_attachment_ref.setAttachment(0);
    color_attachment_ref.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass_description;
    subpass_description.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    subpass_description.setColorAttachmentCount(1);
    subpass_description.setPColorAttachments(&color_attachment_ref);

    vk::SubpassDependency subpass_dependency;
    subpass_dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
    subpass_dependency.setDstSubpass(0);
    subpass_dependency.setSrcStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);
    subpass_dependency.setSrcAccessMask(vk::AccessFlags());
    subpass_dependency.setDstStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);
    subpass_dependency.setDstAccessMask(
        vk::AccessFlagBits::eColorAttachmentRead |
        vk::AccessFlagBits::eColorAttachmentWrite);

    vk::RenderPassCreateInfo render_pass_create_info;
    render_pass_create_info.setAttachmentCount(1);
    render_pass_create_info.setPAttachments(&color_attachment);
    render_pass_create_info.setSubpassCount(1);
    render_pass_create_info.setPSubpasses(&subpass_description);
    render_pass_create_info.setDependencyCount(1);
    render_pass_create_info.setPDependencies(&subpass_dependency);
    vk::RenderPass render_pass =
        instance.device.createRenderPass(render_pass_create_info);

    vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info;
    graphics_pipeline_create_info.setStageCount(2);
    graphics_pipeline_create_info.setPStages(shader_stages);
    graphics_pipeline_create_info.setPVertexInputState(
        &pipeline_vertex_input_state_create_info);
    graphics_pipeline_create_info.setPInputAssemblyState(
        &pipeline_input_assembly_state_create_info);
    graphics_pipeline_create_info.setPViewportState(
        &pipeline_viewport_state_create_info);
    graphics_pipeline_create_info.setPRasterizationState(
        &pipeline_rasterization_state_create_info);
    graphics_pipeline_create_info.setPMultisampleState(
        &pipeline_multisample_create_info);
    graphics_pipeline_create_info.setPColorBlendState(
        &pipeline_color_blend_state_create_info);
    graphics_pipeline_create_info.setLayout(pipeline_layout);
    graphics_pipeline_create_info.setRenderPass(render_pass);
    graphics_pipeline_create_info.setSubpass(0);

    vk::PipelineCacheCreateInfo pipeline_cache_create_info;
    vk::PipelineCache pipeline_cache =
        instance.device.createPipelineCache(pipeline_cache_create_info);
    vk::Pipeline pipeline = instance.device.createGraphicsPipeline(
        pipeline_cache, graphics_pipeline_create_info);

    std::vector<vk::Framebuffer> swapchain_framebuffers;

    for (auto& swapchain_image_view : swapchain_image_views) {
      vk::FramebufferCreateInfo framebuffer_create_info;
      framebuffer_create_info.setRenderPass(render_pass);
      framebuffer_create_info.setAttachmentCount(1);
      framebuffer_create_info.setPAttachments(&swapchain_image_view);
      framebuffer_create_info.width = extent.width;
      framebuffer_create_info.height = extent.height;
      framebuffer_create_info.setLayers(1);
      swapchain_framebuffers.push_back(
          instance.device.createFramebuffer(framebuffer_create_info));
    }

    vk::BufferCreateInfo buffer_info;
    buffer_info.setSize(vertices.size() * sizeof(Vertex));
    buffer_info.setUsage(vk::BufferUsageFlagBits::eVertexBuffer);
    buffer_info.setSharingMode(vk::SharingMode::eExclusive);
    vk::Buffer vertex_buffer = instance.device.createBuffer(buffer_info);
    vk::MemoryRequirements memory_requirements =
        instance.device.getBufferMemoryRequirements(vertex_buffer);
    vk::PhysicalDeviceMemoryProperties physical_device_memory_properties =
        instance.physical_device.getMemoryProperties();

    auto find_memory_type_index =
        [](const vk::PhysicalDeviceMemoryProperties& memory_properties,
           uint32_t memory_type_bits_requirement,
           vk::MemoryPropertyFlags required_properties) -> int32_t {
      const uint32_t memoryCount = memory_properties.memoryTypeCount;
      for (uint32_t memoryIndex = 0; memoryIndex < memoryCount; ++memoryIndex) {
        const uint32_t memoryTypeBits = (1 << memoryIndex);
        const bool isRequiredMemoryType =
            memory_type_bits_requirement & memoryTypeBits;
        const vk::MemoryPropertyFlags properties =
            memory_properties.memoryTypes[memoryIndex].propertyFlags;
        const bool hasRequiredProperties =
            (properties & required_properties) == required_properties;
        if (isRequiredMemoryType && hasRequiredProperties)
          return static_cast<int32_t>(memoryIndex);
      }
      // failed to find memory type
      throw std::runtime_error("Unable to find suitable memory");
    };

    int32_t memory_type_index = find_memory_type_index(
        physical_device_memory_properties, memory_requirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

    vk::MemoryAllocateInfo memory_allocate_info;
    memory_allocate_info.setAllocationSize(memory_requirements.size);
    memory_allocate_info.setMemoryTypeIndex(memory_type_index);

    vk::DeviceMemory vertex_buffer_memory =
        instance.device.allocateMemory(memory_allocate_info);
    instance.device.bindBufferMemory(vertex_buffer, vertex_buffer_memory,
                                     0 /*offset*/);

    void* data = instance.device.mapMemory(vertex_buffer_memory, 0 /*offset*/,
                                           buffer_info.size);
    std::memcpy(data, vertices.data(), buffer_info.size);
    instance.device.unmapMemory(vertex_buffer_memory);

    vk::CommandPoolCreateInfo command_pool_create_info;
    command_pool_create_info.setQueueFamilyIndex(0);
    vk::CommandPool command_pool =
        instance.device.createCommandPool(command_pool_create_info);

    vk::CommandBufferAllocateInfo command_buffer_allocate_info;
    command_buffer_allocate_info.setCommandPool(command_pool);
    command_buffer_allocate_info.setLevel(vk::CommandBufferLevel::ePrimary);
    command_buffer_allocate_info.setCommandBufferCount(
        swapchain_framebuffers.size());

    std::vector<vk::CommandBuffer> command_buffers =
        instance.device.allocateCommandBuffers(command_buffer_allocate_info);

    for (size_t i = 0; i < swapchain_framebuffers.size(); i++) {
      vk::CommandBuffer& command_buffer = command_buffers.at(i);
      vk::Framebuffer& framebuffer = swapchain_framebuffers.at(i);

      vk::CommandBufferBeginInfo command_buffer_begin_info;
      command_buffer_begin_info.setFlags(
          vk::CommandBufferUsageFlagBits::eSimultaneousUse);
      command_buffer.begin(command_buffer_begin_info);

      vk::RenderPassBeginInfo render_pass_begin_info;
      render_pass_begin_info.setRenderPass(render_pass);
      render_pass_begin_info.setFramebuffer(framebuffer);
      render_pass_begin_info.renderArea.setOffset({0, 0});
      render_pass_begin_info.renderArea.extent = extent;

      vk::ClearValue clear_color =
          vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
      render_pass_begin_info.setClearValueCount(1);
      render_pass_begin_info.setPClearValues(&clear_color);

      command_buffer.beginRenderPass(render_pass_begin_info,
                                     vk::SubpassContents::eInline);

      command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

      vk::DeviceSize offset = 0;
      command_buffer.bindVertexBuffers(0 /*first_binding*/, 1 /*count*/,
                                       &vertex_buffer, &offset);

      command_buffer.draw(vertices.size(), 1 /*instanceCount*/,
                          0 /*firstVertex*/, 0 /*firstInstance*/);
      command_buffer.endRenderPass();
      command_buffer.end();
    }

    constexpr size_t num_in_flight = 2;
    std::vector<vk::Semaphore> image_available_semaphores(num_in_flight);
    std::vector<vk::Semaphore> render_finished_semaphores(num_in_flight);
    std::vector<vk::Fence> fences(num_in_flight);

    for (size_t i = 0; i < num_in_flight; i++) {
      image_available_semaphores[i] =
          instance.device.createSemaphore(vk::SemaphoreCreateInfo());
      render_finished_semaphores[i] =
          instance.device.createSemaphore(vk::SemaphoreCreateInfo());
      fences[i] = instance.device.createFence(
          vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
    }

    size_t next_flight = 0;

    //  // short circuit:
    // goto done;

    while (1) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        switch (event.type) {
          case SDL_QUIT:
            goto done;
          case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_q) goto done;
            std::cout << "KEYDOWN SCANCODE '"
                      << SDL_GetScancodeName(event.key.keysym.scancode)
                      << "' KEYCODE '" << SDL_GetKeyName(event.key.keysym.sym)
                      << "'" << std::endl;
            break;
          case SDL_KEYUP:
            std::cout << "KEYUP" << std::endl;
            break;
        }
      }

      vk::Semaphore& image_available_semaphore =
          image_available_semaphores[next_flight];
      vk::Semaphore& render_finished_semaphore =
          render_finished_semaphores[next_flight];
      vk::Fence& fence = fences[next_flight];

      if (instance.device.getFenceStatus(fence) == vk::Result::eNotReady)
        continue;

      instance.device.resetFences(1, &fence);

      next_flight = (next_flight + 1) % num_in_flight;

      vk::ResultValue<unsigned int> image_index =
          instance.device.acquireNextImageKHR(
              swapchain, std::numeric_limits<uint64_t>::max(),
              image_available_semaphore, vk::Fence());

      if (image_index.result != vk::Result::eSuccess)
        throw std::runtime_error("acquireNextImageKHR failed");

      vk::PipelineStageFlags wait_stage =
          vk::PipelineStageFlagBits::eColorAttachmentOutput;
      vk::SubmitInfo submit_info;
      submit_info.setWaitSemaphoreCount(1);
      submit_info.setPWaitSemaphores(&image_available_semaphore);
      submit_info.setPWaitDstStageMask(&wait_stage);
      submit_info.setCommandBufferCount(1);
      submit_info.setPCommandBuffers(&command_buffers.at(image_index.value));
      submit_info.setSignalSemaphoreCount(1);
      submit_info.setPSignalSemaphores(&render_finished_semaphore);

      instance.queue.submit(1, &submit_info, fence);

      vk::PresentInfoKHR present_info;
      present_info.setWaitSemaphoreCount(1);
      present_info.setPWaitSemaphores(&render_finished_semaphore);
      present_info.setSwapchainCount(1);
      present_info.setPSwapchains(&swapchain);
      present_info.setPImageIndices(&image_index.value);
      instance.queue.presentKHR(present_info);
    }
  done:

    instance.device.waitIdle();
    for (size_t i = 0; i < num_in_flight; i++) {
      instance.device.destroy(image_available_semaphores[i]);
      instance.device.destroy(render_finished_semaphores[i]);
      instance.device.destroy(fences[i]);
    }
    instance.device.destroy(command_pool);
    for (auto& swapchain_framebuffer : swapchain_framebuffers)
      instance.device.destroy(swapchain_framebuffer);
    instance.device.destroy(vertex_buffer);
    instance.device.free(vertex_buffer_memory);
    instance.device.destroy(pipeline);
    instance.device.destroy(pipeline_cache);
    instance.device.destroy(render_pass);
    instance.device.destroy(pipeline_layout);
    instance.device.destroy(descriptor_set_layout);
    instance.device.destroy(vert_shader);
    instance.device.destroy(frag_shader);
    for (vk::ImageView& image_view : swapchain_image_views)
      instance.device.destroy(image_view);
    instance.device.destroy(swapchain);
  } catch (std::exception& e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
  }
}
