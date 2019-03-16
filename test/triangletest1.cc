#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <gflags/gflags.h>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <set>

#include "dvc/file.h"
#include "dvc/log.h"
#include "dvc/terminate.h"
#include "spk/loader.h"
#include "spk/spock.h"

#include <iostream>

namespace spk {
using debug_utils_messenger_callback =
    std::function<void(spk::debug_utils_message_severity_flags_ext,
                       spk::debug_utils_message_type_flags_ext,
                       const spk::debug_utils_messenger_callback_data_ext*)>;

}  // namespace spk

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageType,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
               void* pUserData) {
  auto debug_utils_messenger_callback =
      (spk::debug_utils_messenger_callback*)pUserData;
  (*debug_utils_messenger_callback)(
      (spk::debug_utils_message_severity_flags_ext)messageSeverity,
      (spk::debug_utils_message_type_flags_ext)messageType,
      (const spk::debug_utils_messenger_callback_data_ext*)pCallbackData);
  return VK_FALSE;
}

spk::debug_utils_messenger_callback debug_utils_messenger_callback =
    [](spk::debug_utils_message_severity_flags_ext severity,
       spk::debug_utils_message_type_flags_ext type,
       const spk::debug_utils_messenger_callback_data_ext* data) {
      std::cerr << data->message() << std::endl;
    };

std::vector<std::string> get_sdl_extensions(SDL_Window* window) {
  unsigned int count;

  if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr))
    DVC_FATAL(SDL_GetError());

  std::vector<const char*> extensions_cstr(count);

  if (!SDL_Vulkan_GetInstanceExtensions(window, &count, extensions_cstr.data()))
    DVC_FATAL(SDL_GetError());

  std::vector<std::string> extensions(count);
  for (unsigned int i = 0; i < count; i++) extensions[i] = extensions_cstr[i];
  return extensions;
}

void setup_sdl() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) DVC_FATAL(SDL_GetError());
}

DEFINE_bool(fullscreen, false, "use a fullscreen window");

SDL_Window* create_window() {
  SDL_Window* window =
      FLAGS_fullscreen
          ? SDL_CreateWindow("triangletest", SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED, 0, 0,
                             SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_VULKAN)
          : SDL_CreateWindow("triangletest", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, 800, 600,
                             SDL_WINDOW_VULKAN);
  if (!window) DVC_FATAL(SDL_GetError());
  return window;
}

void destroy_window(SDL_Window* window) { SDL_DestroyWindow(window); }

spk::debug_utils_messenger_create_info_ext
create_debug_utils_messenger_create_info() {
  spk::debug_utils_messenger_create_info_ext debug_utils_messenger_create_info;
  debug_utils_messenger_create_info.set_message_severity(
      spk::debug_utils_message_severity_flags_ext::warning_ext |
      spk::debug_utils_message_severity_flags_ext::error_ext |
      spk::debug_utils_message_severity_flags_ext::verbose_ext |
      spk::debug_utils_message_severity_flags_ext::info_ext);
  debug_utils_messenger_create_info.set_message_type(
      spk::debug_utils_message_type_flags_ext::general_ext |
      spk::debug_utils_message_type_flags_ext::performance_ext |
      spk::debug_utils_message_type_flags_ext::validation_ext);
  debug_utils_messenger_create_info.set_pfn_user_callback(debug_callback);

  debug_utils_messenger_create_info.set_p_user_data(
      &debug_utils_messenger_callback);

  return debug_utils_messenger_create_info;
}

spk::instance create_instance(spk::loader& loader,
                              const std::vector<std::string>& extensions) {
  spk::instance_create_info instance_create_info;
  const char* layers[] = {"VK_LAYER_LUNARG_standard_validation"};
  instance_create_info.set_pp_enabled_layer_names({layers, 1});
  std::vector<const char*> cextensions = {spk::ext_debug_utils_extension_name,
                                          spk::ext_debug_report_extension_name};
  for (const std::string& extension : extensions)
    cextensions.push_back(extension.c_str());
  instance_create_info.set_pp_enabled_extension_names(
      {cextensions.data(), cextensions.size()});

  spk::debug_utils_messenger_create_info_ext debug_utils_messenger_create_info =
      create_debug_utils_messenger_create_info();
  instance_create_info.set_next(&debug_utils_messenger_create_info);

  spk::instance instance = loader.create_instance(instance_create_info);
  return instance;
}

spk::debug_utils_messenger_ext create_debug_utils_messenger(
    spk::instance& instance) {
  return instance.create_debug_utils_messenger_ext(
      create_debug_utils_messenger_create_info());
}

spk::surface_khr create_surface(SDL_Window* window, spk::instance& instance) {
  spk::surface_khr_ref surface_ref;
  if (!SDL_Vulkan_CreateSurface(window, instance, &surface_ref))
    DVC_FATAL(SDL_GetError());
  return {surface_ref, instance, instance.dispatch_table(), nullptr};
}

bool queue_family_suitable(spk::physical_device& physical_device,
                           spk::surface_khr& surface,
                           uint32_t queue_family_index,
                           const spk::queue_family_properties& properties) {
  spk::bool32_t is_supported;
  physical_device.surface_support_khr(queue_family_index, surface,
                                      is_supported);
  if (!is_supported) return false;

  return (properties.queue_count() > 0 &&
          properties.queue_flags() & spk::queue_flags::graphics);
}

struct SurfaceInfo {
  spk::surface_capabilities_khr capabilities;
  std::vector<spk::surface_format_khr> formats;
  std::vector<spk::present_mode_khr> present_modes;
};

SurfaceInfo get_surface_info(spk::physical_device& physical_device,
                             spk::surface_khr_ref surface) {
  return {physical_device.surface_capabilities_khr(surface),
          physical_device.surface_formats_khr(surface),
          physical_device.surface_present_modes_khr(surface)};
}

int evaluate_physical_device(spk::physical_device& physical_device,
                             spk::surface_khr& surface) {
  spk::physical_device_properties properties = physical_device.properties();

  std::set<std::string> available_extensions;
  for (const spk::extension_properties& extension :
       physical_device.enumerate_device_extension_properties(nullptr))
    available_extensions.insert(extension.extension_name().data());

  if (!available_extensions.count(spk::khr_swapchain_extension_name)) return 0;
  if (properties.device_type() != spk::physical_device_type::discrete_gpu)
    return 0;

  SurfaceInfo surface_info = get_surface_info(physical_device, surface);
  if (surface_info.formats.empty() || surface_info.present_modes.empty())
    return 0;

  std::vector<spk::queue_family_properties> queue_family_properties_vector =
      physical_device.queue_family_properties();
  for (uint32_t queue_family_index = 0;
       queue_family_index < queue_family_properties_vector.size();
       queue_family_index++) {
    if (queue_family_suitable(
            physical_device, surface, queue_family_index,
            queue_family_properties_vector.at(queue_family_index)))
      return 1;
  }

  return 0;
}

spk::physical_device select_physical_device(spk::instance& instance,
                                            spk::surface_khr& surface) {
  std::vector<spk::physical_device> physical_devices =
      instance.enumerate_physical_devices();

  std::vector<int> physical_device_evaluations;

  for (auto& physical_device : physical_devices)
    physical_device_evaluations.push_back(
        evaluate_physical_device(physical_device, surface));

  auto& e = physical_device_evaluations;
  size_t idx = std::distance(e.begin(), std::max_element(e.begin(), e.end()));
  DVC_ASSERT_GT(physical_device_evaluations.at(idx), 0);
  return std::move(physical_devices.at(idx));
}

uint32_t select_queue_family(spk::physical_device& physical_device,
                             spk::surface_khr& surface) {
  std::vector<spk::queue_family_properties> properties_vector =
      physical_device.queue_family_properties();
  for (size_t i = 0; i < properties_vector.size(); ++i) {
    if (queue_family_suitable(physical_device, surface, i,
                              properties_vector.at(i)))
      return i;
  }
  DVC_FATAL("no suitable queue family");
}

spk::device create_device(spk::physical_device& physical_device,
                          uint32_t queue_family_index) {
  spk::device_create_info create_info;

  spk::device_queue_create_info queue_create_info;
  queue_create_info.set_queue_family_index(queue_family_index);
  float queue_priority = 1.0;
  queue_create_info.set_queue_priorities({&queue_priority, 1});
  create_info.set_queue_create_infos({&queue_create_info, 1});

  spk::physical_device_features physical_device_features;
  create_info.set_p_enabled_features(&physical_device_features);

  const char* layers[] = {"VK_LAYER_LUNARG_standard_validation"};
  create_info.set_pp_enabled_layer_names({layers, 1});

  const char* extensions[] = {spk::khr_swapchain_extension_name};
  create_info.set_pp_enabled_extension_names({extensions, 1});

  return physical_device.create_device(create_info);
}

spk::queue create_queue(spk::device& device, uint32_t queue_family_index) {
  return device.queue(queue_family_index, 0);
}

spk::surface_format_khr select_surface_format(
    spk::physical_device& physical_device, spk::surface_khr& surface) {
  SurfaceInfo info = get_surface_info(physical_device, surface);

  if (info.formats.size() == 1 &&
      info.formats.at(0).format() == spk::format::undefined) {
    spk::surface_format_khr result;
    result.set_format(spk::format::b8g8r8a8_unorm);
    result.set_color_space(
        spk::color_space_khr::color_space_srgb_nonlinear_khr);
    return result;
  }

  for (spk::surface_format_khr format : info.formats) {
    if (format.format() == spk::format::b8g8r8a8_unorm &&
        format.color_space() ==
            spk::color_space_khr::color_space_srgb_nonlinear_khr)
      return format;
  }

  DVC_FATAL("TODO: no good format");
}

spk::extent_2d get_swap_extent(const SurfaceInfo& info, SDL_Window* window) {
  spk::extent_2d extent = info.capabilities.current_extent();
  if (extent.width() == std::numeric_limits<uint32_t>::max()) {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    std::clamp(w, int(info.capabilities.min_image_extent().width()),
               int(info.capabilities.max_image_extent().width()));
    std::clamp(h, int(info.capabilities.min_image_extent().height()),
               int(info.capabilities.max_image_extent().height()));
    extent.set_width(w);
    extent.set_height(h);
  }
  return extent;
}

struct Swapchain : spk::swapchain_khr {
  spk::format format;
  spk::extent_2d extent;
};

Swapchain create_swapchain(spk::physical_device& physical_device,
                           SDL_Window* window, spk::surface_khr& surface,
                           spk::device& device) {
  SurfaceInfo surface_info = get_surface_info(physical_device, surface);

  spk::surface_format_khr surface_format =
      select_surface_format(physical_device, surface);

  spk::present_mode_khr present_mode = spk::present_mode_khr::fifo_khr;

  spk::extent_2d swap_extent = get_swap_extent(surface_info, window);
  DVC_ASSERT_LE(surface_info.capabilities.min_image_count(),
                surface_info.capabilities.max_image_count());
  uint32_t num_images = surface_info.capabilities.min_image_count() + 2;
  if (surface_info.capabilities.max_image_count() < num_images)
    num_images = surface_info.capabilities.max_image_count();

  spk::swapchain_create_info_khr swapchain_create_info;
  swapchain_create_info.set_surface(surface);
  swapchain_create_info.set_min_image_count(num_images);
  swapchain_create_info.set_image_format(surface_format.format());
  swapchain_create_info.set_image_color_space(surface_format.color_space());
  swapchain_create_info.set_image_extent(swap_extent);
  swapchain_create_info.set_image_array_layers(1);
  swapchain_create_info.set_image_usage(
      spk::image_usage_flags::color_attachment);
  swapchain_create_info.set_image_sharing_mode(spk::sharing_mode::exclusive);
  swapchain_create_info.set_pre_transform(
      surface_info.capabilities.current_transform());
  swapchain_create_info.set_composite_alpha(
      spk::composite_alpha_flags_khr::opaque_khr);
  swapchain_create_info.set_present_mode(present_mode);
  swapchain_create_info.set_clipped(true);

  return {{device.create_swapchain_khr(swapchain_create_info)},
          surface_format.format(),
          swap_extent};
}

spk::image_view create_image_view(spk::device& device, spk::image& image,
                                  Swapchain& swapchain) {
  spk::image_view_create_info info;
  info.set_image(image);
  info.set_view_type(spk::image_view_type::n2d);
  info.set_format(swapchain.format);
  spk::component_mapping component_mapping;
  component_mapping.set_r(spk::component_swizzle::identity);
  component_mapping.set_g(spk::component_swizzle::identity);
  component_mapping.set_b(spk::component_swizzle::identity);
  component_mapping.set_a(spk::component_swizzle::identity);
  info.set_components(component_mapping);
  spk::image_subresource_range range;
  range.set_aspect_mask(spk::image_aspect_flags::color);
  range.set_base_mip_level(0);
  range.set_level_count(1);
  range.set_base_array_layer(0);
  range.set_layer_count(1);
  info.set_subresource_range(range);
  return device.create_image_view(info);
}

spk::shader_module create_shader(spk::device& device,
                                 const std::filesystem::path& path) {
  DVC_ASSERT(exists(path), "file not found: ", path);
  std::string code = dvc::load_file(path);
  spk::shader_module_create_info create_info;
  create_info.set_code_size(code.size());
  create_info.set_p_code((uint32_t*)code.data());
  return device.create_shader_module(create_info);
}

struct Pipeline {
  spk::shader_module vertex_shader, fragment_shader;
  spk::pipeline_layout pipeline_layout;
  spk::render_pass render_pass;
  spk::pipeline pipeline;
};

Pipeline create_pipeline(spk::device& device, Swapchain& swapchain) {
  spk::shader_module vertex_shader =
      create_shader(device, "test/triangletest1.vert.spv");
  spk::shader_module fragment_shader =
      create_shader(device, "test/triangletest1.frag.spv");

  spk::pipeline_shader_stage_create_info vertex_shader_stage_create_info;
  vertex_shader_stage_create_info.set_module(vertex_shader);
  vertex_shader_stage_create_info.set_name("main");
  vertex_shader_stage_create_info.set_stage(spk::shader_stage_flags::vertex);

  spk::pipeline_shader_stage_create_info fragment_shader_stage_create_info;
  fragment_shader_stage_create_info.set_module(fragment_shader);
  fragment_shader_stage_create_info.set_name("main");
  fragment_shader_stage_create_info.set_stage(
      spk::shader_stage_flags::fragment);

  spk::pipeline_shader_stage_create_info pipeline_shader_stage_create_infos[2] =
      {vertex_shader_stage_create_info, fragment_shader_stage_create_info};

  spk::pipeline_vertex_input_state_create_info vertex_input_info;

  spk::pipeline_input_assembly_state_create_info input_assembly;
  input_assembly.set_topology(spk::primitive_topology::triangle_list);
  input_assembly.set_primitive_restart_enable(false);

  spk::viewport viewport;
  viewport.set_width(swapchain.extent.width());
  viewport.set_height(swapchain.extent.height());
  viewport.set_max_depth(1);

  spk::rect_2d scissor;
  scissor.set_extent(swapchain.extent);

  spk::pipeline_viewport_state_create_info viewport_state;
  viewport_state.set_viewports({&viewport, 1});
  viewport_state.set_scissors({&scissor, 1});

  spk::pipeline_rasterization_state_create_info rasterizer;
  rasterizer.set_polygon_mode(spk::polygon_mode::fill);
  rasterizer.set_line_width(1);
  rasterizer.set_cull_mode(spk::cull_mode_flags::back);
  rasterizer.set_front_face(spk::front_face::clockwise);

  spk::pipeline_multisample_state_create_info multisampling;
  multisampling.set_rasterization_samples(spk::sample_count_flags::n1);
  multisampling.set_min_sample_shading(1);

  spk::pipeline_color_blend_attachment_state color_blend_attachement;
  color_blend_attachement.set_color_write_mask(
      spk::color_component_flags::r | spk::color_component_flags::g |
      spk::color_component_flags::b | spk::color_component_flags::a);

  spk::pipeline_color_blend_state_create_info color_blend;
  color_blend.set_attachments({&color_blend_attachement, 1});

  spk::pipeline_layout_create_info pipeline_layout_create_info;

  spk::pipeline_layout pipeline_layout =
      device.create_pipeline_layout(pipeline_layout_create_info);

  spk::attachment_description color_attachment;
  color_attachment.set_format(swapchain.format);
  color_attachment.set_samples(spk::sample_count_flags::n1);
  color_attachment.set_load_op(spk::attachment_load_op::clear);
  color_attachment.set_store_op(spk::attachment_store_op::store);
  color_attachment.set_stencil_load_op(spk::attachment_load_op::dont_care);
  color_attachment.set_stencil_store_op(spk::attachment_store_op::dont_care);
  color_attachment.set_initial_layout(spk::image_layout::undefined);
  color_attachment.set_final_layout(spk::image_layout::present_src_khr);

  spk::attachment_reference color_attachment_ref;
  color_attachment_ref.set_attachment(0);
  color_attachment_ref.set_layout(spk::image_layout::color_attachment_optimal);

  spk::subpass_description subpass;
  subpass.set_pipeline_bind_point(spk::pipeline_bind_point::graphics);
  subpass.set_color_attachments({&color_attachment_ref, 1});

  spk::render_pass_create_info render_pass_create_info;
  render_pass_create_info.set_attachments({&color_attachment, 1});
  render_pass_create_info.set_subpasses({&subpass, 1});

  spk::render_pass render_pass =
      device.create_render_pass(render_pass_create_info);

  spk::graphics_pipeline_create_info pipeline_info;
  pipeline_info.set_stages({pipeline_shader_stage_create_infos, 2});
  pipeline_info.set_p_vertex_input_state(&vertex_input_info);
  pipeline_info.set_p_input_assembly_state(&input_assembly);
  pipeline_info.set_p_viewport_state(&viewport_state);
  pipeline_info.set_p_rasterization_state(&rasterizer);
  pipeline_info.set_p_color_blend_state(&color_blend);
  pipeline_info.set_layout(pipeline_layout);
  pipeline_info.set_render_pass(render_pass);
  pipeline_info.set_subpass(0);
  pipeline_info.set_p_multisample_state(&multisampling);

  spk::pipeline pipeline = std::move(
      device.create_graphics_pipelines(VK_NULL_HANDLE, {&pipeline_info, 1})
          .at(0));
  return {std::move(vertex_shader), std::move(fragment_shader),
          std::move(pipeline_layout), std::move(render_pass),
          std::move(pipeline)};
}

spk::semaphore create_semaphore(spk::device& device) {
  return device.create_semaphore(spk::semaphore_create_info{});
}

spk::fence create_signaled_fence(spk::device& device) {
  spk::fence_create_info fence_create_info;
  fence_create_info.set_flags(spk::fence_create_flags::signaled);
  return device.create_fence(fence_create_info);
}

struct Framebuffer {
  Framebuffer(Framebuffer&&) = default;

  spk::image image;
  spk::image_view image_view;
  spk::framebuffer framebuffer;

  ~Framebuffer() {
    if (spk::image_ref(image) != VK_NULL_HANDLE) image.release();
  }
};

Framebuffer create_framebuffer(spk::device& device, Swapchain& swapchain,
                               Pipeline& pipeline, spk::image image) {
  spk::image_view image_view = create_image_view(device, image, swapchain);

  spk::framebuffer_create_info info;
  info.set_render_pass(pipeline.render_pass);
  spk::image_view_ref attachment = image_view;
  info.set_attachments({&attachment, 1});
  info.set_width(swapchain.extent.width());
  info.set_height(swapchain.extent.height());
  info.set_layers(1);
  spk::framebuffer framebuffer = device.create_framebuffer(info);

  return {std::move(image), std::move(image_view), std::move(framebuffer)};
}

std::vector<Framebuffer> create_framebuffers(spk::device& device,
                                             Swapchain& swapchain,
                                             Pipeline& pipeline) {
  std::vector<Framebuffer> framebuffers;

  std::vector<spk::image> images = swapchain.images_khr();

  for (spk::image& image : images) {
    Framebuffer framebuffer =
        create_framebuffer(device, swapchain, pipeline, std::move(image));
    framebuffers.push_back(std::move(framebuffer));
  }

  return framebuffers;
}

spk::command_pool create_command_pool(spk::device& device,
                                      uint32_t queue_family_index) {
  spk::command_pool_create_info pool_info;
  pool_info.set_queue_family_index(queue_family_index);
  return device.create_command_pool(pool_info);
}

spk::command_buffer create_command_buffer(spk::device& device,
                                          spk::command_pool& command_pool,
                                          Pipeline& pipeline,
                                          Framebuffer& framebuffer,
                                          Swapchain& swapchain) {
  spk::command_buffer_allocate_info command_buffer_allocate_info;
  command_buffer_allocate_info.set_command_pool(command_pool);
  command_buffer_allocate_info.set_level(spk::command_buffer_level::primary);
  command_buffer_allocate_info.set_command_buffer_count(1);

  spk::command_buffer command_buffer = std::move(
      device.allocate_command_buffers(command_buffer_allocate_info).at(0));

  spk::command_buffer_begin_info command_buffer_begin_info;
  command_buffer_begin_info.set_flags(
      spk::command_buffer_usage_flags::simultaneous_use);
  command_buffer.begin(command_buffer_begin_info);

  spk::render_pass_begin_info render_pass_info;
  render_pass_info.set_render_pass(pipeline.render_pass);
  render_pass_info.set_framebuffer(framebuffer.framebuffer);
  spk::rect_2d render_area;
  render_area.set_extent(swapchain.extent);
  render_pass_info.set_render_area(render_area);
  spk::clear_color_value clear_color_value;
  clear_color_value.set_float_32({0, 0, 0, 1});
  spk::clear_value clear_color;
  clear_color.set_color(clear_color_value);
  render_pass_info.set_clear_values({&clear_color, 1});
  command_buffer.begin_render_pass(render_pass_info,
                                   spk::subpass_contents::inline_);
  command_buffer.bind_pipeline(spk::pipeline_bind_point::graphics,
                               pipeline.pipeline);
  command_buffer.draw(3, 1, 0, 0);
  command_buffer.end_render_pass();
  command_buffer.end();
  return command_buffer;
}

struct Commands {
  spk::command_pool command_pool;
  std::vector<spk::command_buffer> command_buffers;
};

Commands create_commands(spk::device& device, uint32_t queue_family_index,
                         Pipeline& pipeline,
                         std::vector<Framebuffer>& framebuffers,
                         Swapchain& swapchain) {
  spk::command_pool command_pool =
      create_command_pool(device, queue_family_index);

  std::vector<spk::command_buffer> command_buffers;
  for (Framebuffer& framebuffer : framebuffers)
    command_buffers.push_back(create_command_buffer(
        device, command_pool, pipeline, framebuffer, swapchain));

  return {std::move(command_pool), std::move(command_buffers)};
}

void main_loop(spk::device& device, Swapchain& swapchain, spk::queue& queue,
               std::vector<spk::command_buffer>& command_buffers) {
  std::vector<spk::semaphore> image_available_semaphores;
  std::vector<spk::semaphore> render_finished_semaphores;
  std::vector<spk::fence> fences;

  size_t num_frames = 2;
  size_t current_frame = 0;

  for (size_t i = 0; i < num_frames; i++) {
    image_available_semaphores.push_back(create_semaphore(device));
    render_finished_semaphores.push_back(create_semaphore(device));
    fences.push_back(create_signaled_fence(device));
  }

  size_t niterations = 0;
  size_t nnofence = 0;
  size_t nnoacquire = 0;
  size_t npresent = 0;

  uint32_t last_report = SDL_GetTicks();

loop:
  while (1) {
    if (SDL_GetTicks() > last_report + 1000) {
      last_report = SDL_GetTicks();
      DVC_ERROR("niterations ", niterations, " nnofence ", nnofence,
                " nnoacquire ", nnoacquire, " npresent ", npresent);
      niterations = 0;
      nnofence = 0;
      nnoacquire = 0;
      npresent = 0;
    }
    niterations++;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          goto done;
        case SDL_KEYDOWN:
          if (event.key.keysym.sym == SDLK_q) goto done;
      }
    }

    spk::semaphore_ref image_available_semaphore =
        image_available_semaphores[current_frame];
    spk::semaphore_ref render_finished_semaphore =
        render_finished_semaphores[current_frame];
    spk::fence_ref current_fence = fences[current_frame];

    spk::result wait_for_fences_result =
        device.wait_for_fences({&current_fence, 1}, true /*wait_all*/, 0);
    switch (wait_for_fences_result) {
      case spk::result::success:
        break;
      case spk::result::timeout:
        nnofence++;
        goto loop;
      default:
        DVC_FATAL("unexpected result ", wait_for_fences_result,
                  " from wait_for_fences");
    }

    uint32_t image_index;
    spk::result acquire_next_image_result = swapchain.acquire_next_image_khr(
        0, image_available_semaphore, VK_NULL_HANDLE, image_index);

    switch (acquire_next_image_result) {
      case spk::result::success:
        npresent++;
        break;
      case spk::result::not_ready:
        nnoacquire++;
        goto loop;
      default:
        DVC_FATAL("acquire_next_image_khr returned ",
                  acquire_next_image_result);
    }

    device.reset_fences({&current_fence, 1});

    spk::submit_info submit_info;
    spk::pipeline_stage_flags wait_stages[] = {
        spk::pipeline_stage_flags::color_attachment_output};
    spk::semaphore_ref wait = image_available_semaphore;
    submit_info.set_wait_semaphores({&wait, 1});
    submit_info.set_wait_dst_stage_mask({wait_stages, 1});
    spk::command_buffer_ref buf = command_buffers.at(image_index);
    submit_info.set_command_buffers({&buf, 1});
    spk::semaphore_ref signal = render_finished_semaphore;
    submit_info.set_signal_semaphores({&signal, 1});
    queue.submit({&submit_info, 1}, current_fence);

    spk::present_info_khr present_info;
    present_info.set_wait_semaphores({&signal, 1});
    spk::swapchain_khr_ref sc = swapchain;
    present_info.set_swapchains({&sc, 1});
    present_info.set_image_indices({&image_index, 1});
    queue.present_khr(present_info);
    current_frame = (current_frame + 1) % num_frames;
  }
done:

  for (spk::fence_ref fence : fences) {
    DVC_ASSERT_EQ(spk::result::success,
                  device.wait_for_fences({&fence, 1}, true /*wait_all*/,
                                         std::numeric_limits<uint64_t>::max()));
  }
  queue.wait_idle();
  device.wait_idle();
}

void triangle_test() {
  setup_sdl();

  spk::loader loader;

  std::unique_ptr<SDL_Window, void (&)(SDL_Window*)> window(create_window(),
                                                            destroy_window);

  std::vector<std::string> extensions = get_sdl_extensions(window.get());

  spk::instance instance = create_instance(loader, extensions);

  spk::debug_utils_messenger_ext debug_utils_messenger =
      create_debug_utils_messenger(instance);

  spk::surface_khr surface = create_surface(window.get(), instance);

  spk::physical_device physical_device =
      select_physical_device(instance, surface);

  uint32_t queue_family_index = select_queue_family(physical_device, surface);

  spk::device device = create_device(physical_device, queue_family_index);

  spk::queue queue = create_queue(device, queue_family_index);

  Swapchain swapchain =
      create_swapchain(physical_device, window.get(), surface, device);

  Pipeline pipeline = create_pipeline(device, swapchain);

  std::vector<Framebuffer> framebuffers =
      create_framebuffers(device, swapchain, pipeline);

  Commands commands = create_commands(device, queue_family_index, pipeline,
                                      framebuffers, swapchain);

  main_loop(device, swapchain, queue, commands.command_buffers);
}

}  // namespace

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  dvc::install_terminate_handler();

  triangle_test();
}
