#include "../sepia.hpp"
#include <cmath>
#include <chrono>
#include <cstdio>

int main() {
  constexpr size_t N = 10'000'000;

  std::printf("Allocating %zu points...\n", N);
  Sepia::AlignedBuffer<Sepia::f64> x(N), y(N);

  auto t0 = std::chrono::high_resolution_clock::now();

  for (size_t i = 0; i < N; ++i) {
    double t = static_cast<double>(i) / static_cast<double>(N) * 100.0;
    x[i] = t;
    y[i] = std::sin(t * 0.1) + 0.3 * std::sin(t * 1.7) + 0.1 * std::cos(t * 13.0);
  }

  auto t1 = std::chrono::high_resolution_clock::now();
  double gen_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  std::printf("Data generation: %.1f ms\n", gen_ms);

  Sepia::plot2d::Figure figure(280, 180, 5);
  figure.set_title("10M Points without LTTB Decimation");
  figure.set_xlabel("Time (s)");
  figure.set_ylabel("Signal");

  figure.perf({.lod_enable= false, .lod_target_points = 3000});

  figure.plot(Sepia::data::Series(std::move(x), std::move(y)))
    .data({.color = Sepia::Color::blue(), .width = 1.5, .label = "10M signal"});

  figure.grid({.show = true});

  auto t2 = std::chrono::high_resolution_clock::now();
  figure.render();
  auto t3 = std::chrono::high_resolution_clock::now();

  double render_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();
  std::printf("Render: %.1f ms (with LOD decimation)\n", render_ms);

  figure.save_ppm("bin/plots/large_dataset.ppm");
  std::printf("Saved large_dataset.ppm\n");

  return 0;
}
