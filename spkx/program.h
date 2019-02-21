#pragma once

#include <SDL2/SDL.h>
#include <chrono>
#include <functional>
#include <glm/glm.hpp>
#include <optional>
#include <string>

#include "spk/loader.h"
#include "spk/spock.h"

namespace spkx {

class program {
 public:
  using quit_event = SDL_QuitEvent;
  using keyboard_event = SDL_KeyboardEvent;
  using mouse_button_event = SDL_MouseButtonEvent;
  using mouse_motion_event = SDL_MouseMotionEvent;

  program(int argc, char** argv);

  spk::physical_device& physical_device() { return physical_device_; }
  SDL_Window* window() { return window_.get(); }
  spk::surface_khr& surface() { return surface_; }
  spk::device& device() { return device_; }
  uint32_t graphics_queue_family() { return graphics_queue_family_; }
  glm::ivec2 window_size();
  spk::queue& graphics_queue() { return graphics_queue_; }
  spk::queue& present_queue() { return present_queue_; }

  virtual void quit(const quit_event&) { shutdown(); }
  virtual void key_down(const keyboard_event& event) {
    if (event.keysym.sym == SDLK_q) shutdown();
  }
  virtual void key_up(const keyboard_event&) {}
  virtual void mouse_button_down(const mouse_button_event&){};
  virtual void mouse_button_up(const mouse_button_event&){};
  virtual void mouse_motion(const mouse_motion_event&){};

  virtual void work(){};

  void run();

 protected:
  void shutdown() { shutdown_ = true; }

 private:
  void handle_event(const SDL_Event& event);

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
  bool shutdown_ = false;
};

}  // namespace spkx
