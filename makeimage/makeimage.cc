#include "makeimage/makeimage.h"

#include <glm/glm.hpp>
#include <png++/png.hpp>
#include "dvc/opts.h"
#include "dvc/terminate.h"

namespace {

uint32_t DVC_OPTION(width, W, 0, "output image width");
uint32_t DVC_OPTION(height, H, 0, "output image height");
uint32_t DVC_OPTION(multisample, -, 1, "multisample (x^2 samples per pixel)");
std::string DVC_OPTION(filename, o, "", "output filename");

void check_flags() {
  DVC_ASSERT(width > 0, "set --width");
  DVC_ASSERT(height > 0, "set --height");
  DVC_ASSERT(!filename.empty(), "set --filename");
}

}  // namespace

ImageMaker::ImageMaker(int argc, char** argv) {
  dvc::init_options(argc, argv);
  dvc::install_terminate_handler();
  check_flags();
  width = ::width;
  height = ::height;
  multisample = ::multisample;
  filename = ::filename;
}

ImageMaker::~ImageMaker() {}
