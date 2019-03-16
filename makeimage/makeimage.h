#pragma once

#include <glm/glm.hpp>
#include <png++/png.hpp>

#include "dvc/log.h"

class ImageMaker {
 public:
  ImageMaker(int argc, char**);
  ~ImageMaker();

  template <typename F>
  void render(glm::vec2 topleft, glm::vec2 extent, F f) {
    const glm::ivec2 image_extent{width, height};
    DVC_ASSERT_GT(multisample, 0);

    png::image<png::rgba_pixel> image(image_extent.x, image_extent.y);

    const glm::vec2 pixel_extent = extent / glm::vec2(image_extent);
    const glm::vec2 sample_extent = pixel_extent / glm::vec2(multisample);
    const glm::vec2 halfsample_extent = sample_extent / 2.0f;

    const float pixelk = 255.0 / (double(multisample) * double(multisample));

    for (int y = 0; y < image_extent.y; ++y)
      for (int x = 0; x < image_extent.x; ++x) {
        glm::ivec2 pos{x, y};
        glm::vec4 pixel{0.0f};
        glm::vec2 sample_origin =
            topleft + pixel_extent * glm::vec2(pos) + halfsample_extent;
        for (png::uint_32 sx = 0; sx < multisample; sx++)
          for (png::uint_32 sy = 0; sy < multisample; sy++) {
            glm::ivec2 sample_pos(sy, sx);
            glm::vec2 sample_center =
                sample_origin + sample_extent * glm::vec2(sample_pos);
            pixel += f(sample_center);
          }

        pixel *= pixelk;
        pixel = floor(pixel + 0.5f);

        image[y][x] = png::rgba_pixel(pixel.r, pixel.g, pixel.b, pixel.a);
      }

    image.write(filename);
  }

 private:
  uint32_t width, height, multisample;
  std::string filename;
};
