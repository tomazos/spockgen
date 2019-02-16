#pragma once

#include "spk/spock.h"

namespace spkx {

class frame {
 public:
  frame(spk::image image, spk::device& device, spk::render_pass& render_pass,
        spk::format format, spk::extent_2d extent);
  frame(frame&&) = default;
  ~frame();

  spk::framebuffer& framebuffer() { return framebuffer_; }

 private:
  spk::image image_;
  spk::image_view image_view_;
  spk::framebuffer framebuffer_;
};

}  // namespace spkx
