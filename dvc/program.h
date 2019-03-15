#pragma once

#include <string>
#include <vector>

namespace dvc {

struct program {
  std::vector<std::string> args;
  program(int& argc, char**& argv);
  ~program();
};

}  // namespace dvc
