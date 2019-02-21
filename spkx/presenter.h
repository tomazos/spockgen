#pragma once

#include <SDL2/SDL.h>

#include "spk/spock.h"
#include "spkx/canvas.h"

namespace spkx {

class presenter {
 public:
  presenter(spk::physical_device& physical_device, SDL_Window* window,
            spk::surface_khr& surface, spk::device& device);

  spk::extent_2d extent() { return extent_; }
  spk::render_pass& render_pass() { return render_pass_; }
  std::vector<canvas>& canvases() { return canvases_; }
  spk::swapchain_khr& swapchain() { return swapchain_; }
  size_t num_images() { return num_images_; }

 private:
  spk::surface_capabilities_khr surface_capabilities_;
  spk::surface_format_khr surface_format_;
  spk::extent_2d extent_;
  uint32_t num_images_;
  spk::swapchain_khr swapchain_;
  spk::render_pass render_pass_;
  std::vector<canvas> canvases_;
};

}  // namespace spkx
