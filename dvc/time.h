#pragma once

namespace dvc {

inline uint64_t now() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace dvc
