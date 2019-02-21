#include <glog/logging.h>

#include "spkx/presenter.h"
#include "spkx/program.h"

int main(int argc, char** argv) {
  spkx::program program(argc, argv);
  spkx::presenter presenter(program.physical_device(), program.window(),
                            program.surface(), program.device());
}
