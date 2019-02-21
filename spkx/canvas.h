#pragma once

#include "spk/spock.h"

namespace spkx {

class canvas {
 public:
  canvas(spk::image image, spk::device& device, spk::render_pass& render_pass,
         spk::format format, spk::extent_2d extent);
  canvas(canvas&&) = default;
  ~canvas();

  spk::framebuffer& framebuffer() { return framebuffer_; }

 private:
  spk::image image_;
  spk::image_view image_view_;
  spk::framebuffer framebuffer_;
};

}  // namespace spkx
