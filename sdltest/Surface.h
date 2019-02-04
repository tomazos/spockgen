#pragma once

#include <vulkan/vulkan.hpp>

#include "Instance.h"
#include "Surface.h"
#include "Utils.h"
#include "Window.h"

#include "SDL2/SDL_vulkan.h"

namespace spk {

class Surface {
 public:
  SPK_DECL_IMMOVABLE(Surface);

  Surface(Instance& instance, Window& window) : instance(instance) {
    VkSurfaceKHR surface_;
    if (!SDL_Vulkan_CreateSurface(window.to_sdl_window(),
                                  instance.to_vk_instance(), &surface_))
      sdlerror("SDL_Vulkan_CreateSurface");
    surface = surface_;
  }
  ~Surface() { instance.to_cpp_instance().destroySurfaceKHR(surface); }

  Instance& instance;
  vk::SurfaceKHR surface;
};

}  // namespace spk
