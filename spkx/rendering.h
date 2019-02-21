#pragma once

#include "spk/spock.h"

namespace spkx {

class rendering {
 public:
  rendering(spk::device& device, uint32_t graphics_queue_family);

  [[nodiscard]] bool try_begin(spk::device& device,
                               spk::swapchain_khr& swapchain,
                               uint32_t& image_index);

  void end(spk::queue& graphics_queue, spk::queue& present_queue,
           spk::swapchain_khr_ref swapchain, spk::command_buffer_ref command,
           uint32_t image_index);

  spk::command_buffer& command_buffer() { return command_buffer_; }

  spk::semaphore& image_ready_semaphore() { return image_ready_semaphore_; }
  spk::semaphore& rendering_done_semaphore() {
    return rendering_done_semaphore_;
  }
  spk::fence& rendering_done_fence() { return rendering_done_fence_; }

 private:
  spk::semaphore image_ready_semaphore_;
  spk::semaphore rendering_done_semaphore_;
  spk::fence rendering_done_fence_;
  spk::command_pool command_pool_;
  spk::command_buffer command_buffer_;
};

}  // namespace spkx
