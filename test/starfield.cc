#include <algorithm>
#include <map>
#include <random>

#include "Eigen/Eigen"
#include "makeimage/makeimage.h"

DEFINE_double(cutoff, 0.55, "cutoff");
DEFINE_double(expansion, 10, "expansion");

int main(int argc, char** argv) {
  ImageMaker image_maker(argc, argv);

  std::random_device rd;
  std::mt19937 gen{rd()};

  std::uniform_real_distribution<float> dist;

  auto rand = [&]() -> float {
    return std::clamp(std::abs(dist(gen)), 0.0f, 1.0f);
  };

  Eigen::MatrixXf matrix(1005, 1005);

  for (int i = 0; i < 1005; ++i)
    for (int j = 0; j < 1005; ++j) matrix(i, j) = rand();

  for (int i = 0; i < 1000; ++i)
    for (int j = 0; j < 1000; ++j) {
      float f = matrix.block(i, j, 5, 5).mean();
      matrix(i, j) = f;
    }

  auto thin = [](float x) -> float {
    if (x < FLAGS_cutoff) return 0;

    x -= FLAGS_cutoff;
    x *= FLAGS_expansion;
    x = std::clamp(x, 0.0f, 1.0f);
    return x;
  };

  auto sample = [&](glm::vec2 p) -> float {
    return thin(matrix(int(1000 * p.x), int(1000 * p.y)));
  };

  image_maker.render({0, 0}, {1, 1}, [&](glm::vec2 p) -> glm::vec4 {
    return {sample(p), sample(p), sample(p), 1};
  });
}
