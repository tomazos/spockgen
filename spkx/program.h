#pragma once

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <optional>
#include <string>

#include "spk/loader.h"
#include "spk/spock.h"

namespace spkx {

class program {
 public:
  program(int argc, char** argv);

  spk::physical_device& physical_device() { return physical_device_; }
  SDL_Window* window() { return window_.get(); }
  spk::surface_khr& surface() { return surface_; }
  spk::device& device() { return device_; }
  uint32_t graphics_queue_family() { return graphics_queue_family_; }
  glm::ivec2 window_size();
  spk::queue& graphics_queue() { return graphics_queue_; }
  spk::queue& present_queue() { return present_queue_; }

 private:
  struct global {
    global(int argc, char** argv);
    ~global();
  };

  global global_;
  std::string name_;
  spk::loader loader_;
  std::unique_ptr<SDL_Window, void (&)(SDL_Window*)> window_;
  spk::instance instance_;
  spk::debug_utils_messenger_ext debug_utils_messenger_;
  spk::surface_khr surface_;
  spk::physical_device physical_device_;
  uint32_t graphics_queue_family_, present_queue_family_;
  spk::device device_;
  spk::queue graphics_queue_, present_queue_;
};

}  // namespace spkx
