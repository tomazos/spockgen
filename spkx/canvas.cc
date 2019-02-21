#include "spkx/canvas.h"
#include "spk/spock.h"

namespace {

spk::image_view create_image_view(spk::device& device, spk::image& image,
                                  spk::format format) {
  spk::image_view_create_info info;
  info.set_image(image);
  info.set_view_type(spk::image_view_type::n2d);
  info.set_format(format);
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

spk::framebuffer create_framebuffer(spk::device& device,
                                    spk::render_pass& render_pass,
                                    spk::image_view& image_view,
                                    spk::extent_2d extent) {
  spk::framebuffer_create_info info;
  info.set_render_pass(render_pass);
  spk::image_view_ref attachment = image_view;
  info.set_attachments({&attachment, 1});
  info.set_width(extent.width());
  info.set_height(extent.height());
  info.set_layers(1);
  return device.create_framebuffer(info);
}

}  // namespace

namespace spkx {

canvas::canvas(spk::image image, spk::device& device,
               spk::render_pass& render_pass, spk::format format,
               spk::extent_2d extent)
    : image_(std::move(image)),
      image_view_(create_image_view(device, image_, format)),
      framebuffer_(
          create_framebuffer(device, render_pass, image_view_, extent)) {}

canvas::~canvas() { image_.release(); }

}  // namespace spkx
