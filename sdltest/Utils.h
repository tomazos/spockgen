#pragma once

#include <iostream>
#include <string_view>

#include "SDL2/SDL.h"

#define SPK_DECL_IMMOVABLE(classname)              \
  classname(const classname&) = delete;            \
  classname(classname&&) = delete;                 \
  classname& operator=(const classname&) = delete; \
  classname& operator=(classname&&) = delete

[[noreturn]] inline void sdlerror(std::string_view context) {
  std::cerr << "SDL ERROR: " << SDL_GetError() << " (context: " << context
            << ")" << std::endl;
  std::exit(EXIT_FAILURE);
}
