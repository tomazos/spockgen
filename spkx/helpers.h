#pragma once

#include <filesystem>

#include "spk/spock.h"
#include "spkx/presenter.h"

namespace spkx {

spk::shader_module create_shader(spk::device& device,
                                 const std::filesystem::path& path);

struct pipeline_config {
  std::filesystem::path vertex_shader;
  std::vector<spk::vertex_input_binding_description>
      vertex_binding_descriptions;
  std::vector<spk::vertex_input_attribute_description>
      vertex_attribute_descriptions;
  std::filesystem::path fragment_shader;
  spk::primitive_topology topology;
};

spk::pipeline create_pipeline(spk::device& device, spkx::presenter& presenter,
                              const pipeline_config& pipeline_config);

}  // namespace spkx
