#pragma once

#include "SDL2/SDL.h"

#include "Utils.h"

namespace spk {

class Engine {
 public:
  SPK_DECL_IMMOVABLE(Engine);

  Engine() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) sdlerror("SDL_Init");
  }

  ~Engine() { SDL_Quit(); }
};

}  // namespace spk
