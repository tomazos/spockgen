#include "makeimage/makeimage.h"

#include <gflags/gflags.h>

#include <glm/glm.hpp>
#include <png++/png.hpp>
#include "dvc/terminate.h"

DEFINE_uint32(width, 0, "output image width");
DEFINE_uint32(height, 0, "output image height");
DEFINE_uint32(multisample, 1, "multisample (x^2 samples per pixel)");
DEFINE_string(filename, "", "output filename");

namespace {

void check_flags() {
  DVC_ASSERT(FLAGS_width > 0, "set --width");
  DVC_ASSERT(FLAGS_height > 0, "set --height");
  DVC_ASSERT(!FLAGS_filename.empty(), "set --filename");
}

}  // namespace

ImageMaker::ImageMaker(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  dvc::install_terminate_handler();
  check_flags();
  width = FLAGS_width;
  height = FLAGS_height;
  multisample = FLAGS_multisample;
  filename = FLAGS_filename;
}

ImageMaker::~ImageMaker() { gflags::ShutDownCommandLineFlags(); }
