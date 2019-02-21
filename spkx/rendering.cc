#include "spkx/rendering.h"

#include "spk/spock.h"

namespace {

spk::semaphore create_semaphore(spk::device& device) {
  return device.create_semaphore(spk::semaphore_create_info{});
}

spk::fence create_signaled_fence(spk::device& device) {
  spk::fence_create_info fence_create_info;
  fence_create_info.set_flags(spk::fence_create_flags::signaled);
  return device.create_fence(fence_create_info);
}

spk::command_pool create_command_pool(spk::device& device,
                                      uint32_t graphics_queue_family) {
  spk::command_pool_create_info pool_info;
  pool_info.set_queue_family_index(graphics_queue_family);
  pool_info.set_flags(spk::command_pool_create_flags::reset_command_buffer |
                      spk::command_pool_create_flags::transient);
  return device.create_command_pool(pool_info);
}

spk::command_buffer create_command_buffer(spk::device& device,
                                          spk::command_pool& command_pool) {
  spk::command_buffer_allocate_info command_buffer_allocate_info;
  command_buffer_allocate_info.set_command_pool(command_pool);
  command_buffer_allocate_info.set_level(spk::command_buffer_level::primary);
  command_buffer_allocate_info.set_command_buffer_count(1);
  spk::command_buffer command_buffer = std::move(
      device.allocate_command_buffers(command_buffer_allocate_info).at(0));
  return command_buffer;
}

}  // namespace

namespace spkx {

rendering::rendering(spk::device& device, uint32_t graphics_queue_family)
    : image_ready_semaphore_(create_semaphore(device)),
      rendering_done_semaphore_(create_semaphore(device)),
      rendering_done_fence_(create_signaled_fence(device)),
      command_pool_(create_command_pool(device, graphics_queue_family)),
      command_buffer_(create_command_buffer(device, command_pool_)) {}

[[nodiscard]] bool rendering::try_begin(spk::device& device,
                                        spk::swapchain_khr& swapchain,
                                        uint32_t& image_index) {
  spk::fence_ref fence = rendering_done_fence();

  spk::result wait_for_fences_result =
      device.wait_for_fences({&fence, 1}, true /*wait_all*/, 0);
  switch (wait_for_fences_result) {
    case spk::result::success:
      break;
    case spk::result::timeout:
      return false;
    default:
      LOG(FATAL) << "unexpected result " << wait_for_fences_result
                 << " from wait_for_fences";
  }

  spk::result acquire_next_image_result = swapchain.acquire_next_image_khr(
      0, image_ready_semaphore(), VK_NULL_HANDLE, image_index);

  switch (acquire_next_image_result) {
    case spk::result::success:
      break;
    case spk::result::not_ready:
      return false;
    default:
      LOG(FATAL) << "acquire_next_image_khr returned "
                 << acquire_next_image_result;
  }

  device.reset_fences({&fence, 1});

  return true;
}

void rendering::end(spk::queue& graphics_queue, spk::queue& present_queue,
                    spk::swapchain_khr_ref swapchain,
                    spk::command_buffer_ref command, uint32_t image_index) {
  spk::submit_info submit_info;
  spk::semaphore_ref a = image_ready_semaphore();
  submit_info.set_wait_semaphores({&a, 1});
  spk::pipeline_stage_flags wait_stages[] = {
      spk::pipeline_stage_flags::color_attachment_output};
  submit_info.set_wait_dst_stage_mask({wait_stages, 1});
  submit_info.set_command_buffers({&command, 1});
  spk::semaphore_ref b = rendering_done_semaphore();
  submit_info.set_signal_semaphores({&b, 1});
  graphics_queue.submit({&submit_info, 1}, rendering_done_fence());

  spk::present_info_khr present_info;
  present_info.set_wait_semaphores({&b, 1});
  spk::swapchain_khr_ref sc = swapchain;
  present_info.set_swapchains({&sc, 1});
  present_info.set_image_indices({&image_index, 1});
  present_queue.present_khr(present_info);
}

}  // namespace spkx
