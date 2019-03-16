#include <gflags/gflags.h>
#include <png++/png.hpp>

#include "dvc/log.h"
#include "dvc/program.h"

#include "ft/face.h"
#include "ft/glyph.h"
#include "ft/library.h"

DEFINE_string(font, "", "font to use");

// struct FontImage {
//  glm::ivec2 disp;
//  glm::ivec2 extent;
//  std::vector<char> pixels;
//};

void visit_glyph(uint64_t code, uint32_t index, int top, int left,
                 uint32_t rows, uint32_t width, uint8_t* pixels) {
  std::cout << "code: " << code << std::endl;
  std::cout << "index: " << index << std::endl;
  std::cout << "bitmap.left: " << left << std::endl;
  std::cout << "bitmap.top: " << top << std::endl;
  std::cout << "bitmap.bitmap.width: " << width << std::endl;
  std::cout << "bitmap.bitmap.rows: " << rows << std::endl;

  for (unsigned int row = 0; row < rows; row++) {
    for (unsigned int col = 0; col < width; col++) {
      std::cout << (pixels[row * width + col] > 127 ? "X" : ".");
    }
    std::cout << std::endl;
  }
}

int main(int argc, char** argv) {
  dvc::program program(argc, argv);

  std::string target = program.args.at(0);

  DVC_ASSERT(!target.empty(), "usage: fonttest <outimg>");
  DVC_ASSERT(!FLAGS_font.empty(), "set --font");

  png::image<png::gray_pixel> image(1000, 1000);

  ft::library library;

  ft::face face(library, FLAGS_font, 0 /*face_index*/);

  for (const auto& [code, index] : face.get_char_code_indexs()) {
    face.set_pixel_size(20, 20);
    face.load_char(code);
    auto glyph = face.get_glyph();
    auto bitmap = glyph.bitmap();
    visit_glyph(code, index, bitmap.top, bitmap.left, bitmap.bitmap.rows,
                bitmap.bitmap.width, bitmap.bitmap.buffer);
  }

  image.write(target);
}
