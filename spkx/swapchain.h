#pragma once

#include <SDL2/SDL.h>

#include "spk/spock.h"

#include "spkx/frame.h"

namespace spkx {

class swapchain {
 public:
  swapchain(spk::physical_device& physical_device, SDL_Window* window,
            spk::surface_khr& surface, spk::device& device);

  spk::extent_2d extent() { return extent_; }
  spk::render_pass& render_pass() { return render_pass_; }
  std::vector<frame>& frames() { return frames_; }
  spk::swapchain_khr& get() { return swapchain_; }

 private:
  spk::surface_capabilities_khr surface_capabilities_;
  spk::surface_format_khr surface_format_;
  spk::extent_2d extent_;
  uint32_t num_images_;
  spk::swapchain_khr swapchain_;
  spk::render_pass render_pass_;
  std::vector<frame> frames_;
};

}  // namespace spkx
