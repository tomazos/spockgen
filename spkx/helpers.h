#pragma once

#include <filesystem>

#include "dvc/file.h"
#include "spk/spock.h"

namespace spkx {

spk::shader_module create_shader(spk::device& device,
                                 const std::filesystem::path& path) {
  CHECK(exists(path)) << "file not found: " << path;
  std::string code = dvc::load_file(path);
  spk::shader_module_create_info create_info;
  create_info.set_code_size(code.size());
  create_info.set_p_code((uint32_t*)code.data());
  return device.create_shader_module(create_info);
}

}  // namespace spkx
