#pragma once

#include <string_view>
#include <vector>

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#include "Utils.h"

namespace spk {

class Window {
 public:
  SPK_DECL_IMMOVABLE(Window);

  Window(const std::string& name)
      : window(SDL_CreateWindow(
            name.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0,
            0, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_VULKAN)) {
    if (!window) sdlerror("SDL_CreateWindow");
  }

  ~Window() { SDL_DestroyWindow(window); }

  std::vector<std::string> extensions() {
    unsigned int count;

    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr))
      sdlerror("SDL_Vulkan_GetInstanceExtensions");

    std::vector<const char*> extensions_cstr(count);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, &extensions_cstr[0]))
      sdlerror("SDL_Vulkan_GetInstanceExtensions");

    std::vector<std::string> extensions(count);
    for (unsigned int i = 0; i < count; i++) extensions[i] = extensions_cstr[i];
    return extensions;
  }

  SDL_Window* to_sdl_window() { return window; }

 private:
  SDL_Window* window;
};

}  // namespace spk
