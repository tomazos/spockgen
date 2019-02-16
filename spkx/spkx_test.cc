#include <glog/logging.h>
#include "spkx/program.h"
#include "spkx/swapchain.h"

int main(int argc, char** argv) {
  spkx::program program(argc, argv);
  spkx::swapchain swapchain(program.physical_device(), program.window(),
                            program.surface(), program.device());
}
