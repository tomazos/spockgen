#include "spkx/game.h"

namespace {

constexpr size_t num_renderings = 2;

std::vector<spkx::rendering> create_renderings(spk::device& device,
                                               uint32_t graphics_queue_family) {
  std::vector<spkx::rendering> renderings;
  for (size_t i = 0; i < num_renderings; ++i)
    renderings.emplace_back(device, graphics_queue_family);
  return renderings;
}

void begin_command_buffer(spk::command_buffer& command_buffer) {
  spk::command_buffer_begin_info command_buffer_begin_info;
  command_buffer_begin_info.set_flags(
      spk::command_buffer_usage_flags::simultaneous_use);
  command_buffer.begin(command_buffer_begin_info);
}

void end_command_buffer(spk::command_buffer& command_buffer) {
  command_buffer.end();
}

}  // namespace

namespace spkx {

game::game(int argc, char** argv)
    : spkx::program(argc, argv),
      presenter_(std::make_unique<spkx::presenter>(physical_device(), window(),
                                                   surface(), device())),
      renderings_(create_renderings(device(), graphics_queue_family())) {}

void game::work() {
  tick();

  spkx::rendering& rendering = renderings_[rendering_index_];

  uint32_t image_index;
  if (!rendering.try_begin(device(), presenter().swapchain(), image_index))
    return;

  begin_command_buffer(rendering.command_buffer());

  spk::render_pass_begin_info render_pass_info;
  render_pass_info.set_render_pass(presenter().render_pass());
  render_pass_info.set_framebuffer(
      presenter().canvases()[image_index].framebuffer());
  spk::rect_2d render_area;
  render_area.set_extent(presenter().extent());
  render_pass_info.set_render_area(render_area);
  prepare_rendering(rendering.command_buffer(), rendering_index_,
                    render_pass_info);
  end_command_buffer(rendering.command_buffer());

  rendering.end(graphics_queue(), present_queue(), presenter().swapchain(),
                rendering.command_buffer(), image_index);

  rendering_index_ = (rendering_index_ + 1) % num_renderings();
}
}  // namespace spkx
