#pragma once

#include "spkx/presenter.h"
#include "spkx/program.h"
#include "spkx/rendering.h"

namespace spkx {

class game : public program {
 public:
  game(int argc, char** argv);

  spkx::presenter& presenter() { return *presenter_; }

  size_t num_renderings() { return renderings_.size(); }

  virtual void tick() = 0;

  virtual void prepare_rendering(
      spk::command_buffer& command_buffer, size_t rendering_index,
      spk::render_pass_begin_info& render_pass_begin_info) = 0;

 private:
  void work() override;

  size_t rendering_index_ = 0;
  std::unique_ptr<spkx::presenter> presenter_;
  std::vector<spkx::rendering> renderings_;
};

}  // namespace spkx
