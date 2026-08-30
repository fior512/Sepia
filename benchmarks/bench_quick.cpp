#include "../sepia.hpp"
#include <chrono>
#include <cstdio>

using namespace Sepia;
using namespace Sepia::plot2d;
using Clock = std::chrono::high_resolution_clock;

int main() {
  const std::size_t n = 5'000'000ULL;
  AlignedBuffer<double> x(n), y(n);
  for (std::size_t i = 0; i < n; ++i) { x[i] = (double)i; y[i] = (double)(i % 1000); }

  Figure f(800, 500);
  f.perf({.lod_enable = true, .lod_target_points = 2000});
  f.plot_ref(x.data(), y.data(), n);
  f.render(); // warm

  auto t0 = Clock::now();
  f.render();
  double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
  std::printf("render 5M points with LTTB: %.1f ms\n", ms);
  return ms > 2000.0 ? 1 : 0;
}
