#include <map>

#include "makeimage/makeimage.h"

#include "dvc/opts.h"

uint64_t DVC_OPTION(numiters, n, 0, "num ran walk iters");

int main(int argc, char** argv) {
  ImageMaker image_maker(argc, argv);

  std::map<int64_t, double> m0 = {{0, 1}};

  DVC_ASSERT(numiters > 0, "set --numiters");

  for (uint64_t i = 0; i < numiters; ++i) {
    std::map<int64_t, double> m1;
    for (const auto& [pos, weight] : m0) {
      m1[pos + 1] += weight / 2;
      m1[pos - 1] += weight / 2;
    }
    m0 = m1;
  }

  double maxh = 0;
  double lbound = std::numeric_limits<double>::max(),
         rbound = std::numeric_limits<double>::min();

  for (const auto& [pos, weight] : m0) {
    if (double(pos) < lbound) lbound = pos;
    if (double(pos) > rbound) rbound = pos;
    if (weight > maxh) maxh = weight;
  }

  std::map<double, double> m;

  for (const auto& [pos, weight] : m0) {
    m[((pos - lbound) / (rbound - lbound))] = (weight / maxh);
  }

  for (const auto& [k, v] : m) {
    std::cout << k << " " << v << std::endl;
  }

  image_maker.render({0, 1}, {1, -1}, [&](glm::vec2 p) -> glm::vec4 {
    //    LOG(ERROR) << p.x << " " << p.y;
    auto r = m.upper_bound(p.x);
    if (r == m.begin()) return {0, 1, 0, 1};
    if (r == m.end()) return {1, 0, 0, 1};
    auto l = r;
    l--;

    auto dist = [](double x, double y) { return std::abs(x - y); };

    auto it = (dist(l->first, p.x) < dist(r->first, p.x)) ? l : r;

    float g = (it->second > p.y);
    return {g, g, g, 1};
  });
}
