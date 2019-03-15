#include "dvc/program.h"

#include "dvc/terminate.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

namespace dvc {

program::program(int& argc, char**& argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  dvc::install_terminate_handler();
  google::InstallFailureFunction(dvc::log_stacktrace);
  args = std::vector<std::string>(argv + 1, argv + argc);
}

program::~program() {
  gflags::ShutDownCommandLineFlags();
  google::ShutdownGoogleLogging();
}

}  // namespace dvc
