#include "presenter.h"
#include <SDL2/SDL_vulkan.h>
#include <algorithm>

#include "dvc/log.h"

namespace {

spk::surface_format_khr select_surface_format(
    spk::physical_device& physical_device,
    const std::vector<spk::surface_format_khr>& surface_formats) {
  if (surface_formats.size() == 1 &&
      surface_formats.at(0).format() == spk::format::undefined) {
    spk::surface_format_khr format;
    format.set_format(spk::format::b8g8r8a8_unorm);
    format.set_color_space(
        spk::color_space_khr::color_space_srgb_nonlinear_khr);
    return format;
  }

  for (spk::surface_format_khr format : surface_formats) {
    if (format.format() == spk::format::b8g8r8a8_unorm &&
        format.color_space() ==
            spk::color_space_khr::color_space_srgb_nonlinear_khr)
      return format;
  }

  DVC_FATAL("TODO: no good format");
}

bool swapchain_determines_extent(spk::extent_2d extent) {
  return (extent.width() == 0xFFFFFFFF && extent.height() == 0xFFFFFFFF);
}

spk::extent_2d get_window_extent(SDL_Window* window) {
  spk::extent_2d extent;
  int width, height;
  SDL_Vulkan_GetDrawableSize(window, &width, &height);
  extent.set_width(width);
  extent.set_height(height);
  return extent;
}

spk::extent_2d clamp_extent(spk::extent_2d extent, spk::extent_2d min,
                            spk::extent_2d max) {
  spk::extent_2d result;
  result.set_width(std::clamp(extent.width(), min.width(), max.width()));
  result.set_height(std::clamp(extent.height(), min.height(), max.height()));
  return result;
}

spk::extent_2d get_extent(
    const spk::surface_capabilities_khr& surface_capabilities,
    SDL_Window* window) {
  spk::extent_2d extent = surface_capabilities.current_extent();
  if (!swapchain_determines_extent(extent))
    return extent;
  else
    return clamp_extent(get_window_extent(window),
                        surface_capabilities.min_image_extent(),
                        surface_capabilities.max_image_extent());
}

uint32_t select_num_images(
    const spk::surface_capabilities_khr& surface_capabilities) {
  uint32_t num_swap_images = 3;

  num_swap_images =
      std::max(num_swap_images, surface_capabilities.min_image_count());
  if (surface_capabilities.max_image_count() != 0)
    num_swap_images =
        std::min(num_swap_images, surface_capabilities.max_image_count());

  return num_swap_images;
}

spk::swapchain_khr create_swapchain(
    spk::surface_khr& surface, spk::device& device,
    const spk::surface_capabilities_khr& surface_capabilities,
    const spk::surface_format_khr& surface_format, spk::extent_2d extent,
    uint32_t num_images) {
  spk::swapchain_create_info_khr swapchain_create_info;
  swapchain_create_info.set_surface(surface);
  swapchain_create_info.set_min_image_count(num_images);
  swapchain_create_info.set_image_format(surface_format.format());
  swapchain_create_info.set_image_color_space(surface_format.color_space());
  swapchain_create_info.set_image_extent(extent);
  swapchain_create_info.set_image_array_layers(1);
  swapchain_create_info.set_image_usage(
      spk::image_usage_flags::color_attachment);
  swapchain_create_info.set_image_sharing_mode(spk::sharing_mode::exclusive);
  swapchain_create_info.set_pre_transform(
      surface_capabilities.current_transform());
  swapchain_create_info.set_composite_alpha(
      spk::composite_alpha_flags_khr::opaque_khr);
  swapchain_create_info.set_present_mode(spk::present_mode_khr::fifo_khr);
  swapchain_create_info.set_clipped(true);

  return device.create_swapchain_khr(swapchain_create_info);
}

spk::render_pass create_render_pass(spk ::device& device, spk::format format) {
  spk::attachment_description color_attachment;
  color_attachment.set_format(format);
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

  return device.create_render_pass(render_pass_create_info);
}

std::vector<spkx::canvas> create_canvases(spk::device& device,
                                          spk::swapchain_khr& swapchain,
                                          spk::render_pass& render_pass,
                                          spk::format format,
                                          spk::extent_2d extent) {
  std::vector<spkx::canvas> canvases;

  std::vector<spk::image> images = swapchain.images_khr();

  for (spk::image& image : images)
    canvases.emplace_back(std::move(image), device, render_pass, format,
                          extent);

  return std::move(canvases);
}

}  // namespace

namespace spkx {

presenter::presenter(spk::physical_device& physical_device, SDL_Window* window,
                     spk::surface_khr& surface, spk::device& device)
    : surface_capabilities_(physical_device.surface_capabilities_khr(surface)),
      surface_format_(select_surface_format(
          physical_device, physical_device.surface_formats_khr(surface))),
      extent_(get_extent(surface_capabilities_, window)),
      num_images_(select_num_images(surface_capabilities_)),
      swapchain_(create_swapchain(surface, device, surface_capabilities_,
                                  surface_format_, extent_, num_images_)),
      render_pass_(create_render_pass(device, surface_format_.format())),
      canvases_(create_canvases(device, swapchain_, render_pass_,
                                surface_format_.format(), extent_)) {}

}  // namespace spkx
