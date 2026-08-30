#include "../sepia.hpp"
#include <cmath>
#include <chrono>
#include <cstdio>
#include <vector>

// ---------------------------------------------------------------------------
// Stress test: measure Sepia render latency across dataset sizes
// with and without LTTB decimation, then plot the results.
// ---------------------------------------------------------------------------

struct BenchResult {
  size_t n;
  double ms_with_lttb;
  double ms_without_lttb;
};

static double bench_render(size_t N, bool lod_enable) {
  // Generate synthetic signal
  Sepia::AlignedBuffer<double> x(N), y(N);
  for (size_t i = 0; i < N; ++i) {
    double t = static_cast<double>(i) / static_cast<double>(N) * 100.0;
    x[i] = t;
    y[i] = std::sin(t * 0.1) + 0.3 * std::sin(t * 1.7) + 0.1 * std::cos(t * 13.0);
  }

  Sepia::plot2d::Figure figure(800.0, 500.0);
  figure.set_title("Bench");

  figure.perf({
    .lod_enable = lod_enable,
    .lod_target_points = 2000,
  });

  figure.plot(Sepia::data::Series(std::move(x), std::move(y)))
    .data({.color = Sepia::Color::blue(), .width = 1.0, .label = "signal"});

  figure.grid({.show = true});

  // Warm-up render (first call may trigger lazy init,  < -O2)
  figure.render();

  // Timed render
  constexpr int RUNS = 3;
  double total = 0.0;
  for (int r = 0; r < RUNS; ++r) {
    auto t0 = std::chrono::high_resolution_clock::now();
    figure.render();
    auto t1 = std::chrono::high_resolution_clock::now();
    total += std::chrono::duration<double, std::milli>(t1 - t0).count();
  }
  return total / RUNS;
}

int main() {
  // Dataset sizes to test
  std::vector<size_t> sizes = {
    100, 500, 1000, 5000, 10000, 50000,
    100000, 500000, 1000000, 5000000, 10000000, 50000000,
    100000000, 500000000, 1000000000
  };

  std::vector<BenchResult> results;
  results.reserve(sizes.size());

  std::printf("%-15s  %15s  %15s\n", "Dataset Size", "With LTTB[2k] (ms)", "Without LTTB (ms)");
  std::printf("%-15s  %15s  %15s\n", "------------", "------------------", "-----------------");

  for (size_t n : sizes) {
    double with_lttb    = bench_render(n, true);
    double without_lttb = bench_render(n, false);

    results.push_back({n, with_lttb, without_lttb});

    std::printf("%-15zu  %15.2f  %15.2f\n", n, with_lttb, without_lttb);
    std::fflush(stdout);
  }

  // ---- Plot the results using Sepia itself ----
  const size_t R = results.size();
  std::vector<double> x_vals(R), y_with(R), y_without(R);

  for (size_t i = 0; i < R; ++i) {
    x_vals[i]    = static_cast<double>(results[i].n);  // actual dataset size
    y_with[i]    = results[i].ms_with_lttb;
    y_without[i] = results[i].ms_without_lttb;
  }

  Sepia::plot2d::Figure fig(1200.0, 600.0);
  fig.set_title("Sepia Render Latency: LTTB On vs Off");
  fig.set_xlabel("Dataset Size (points)");
  fig.set_ylabel("Render Latency (ms)");

  fig.perf({.lod_enable = false}); 

  fig.plot(x_vals.data(), y_with.data(), R)
    .data({
      .color   = Sepia::Color::blue(),
      .width   = 2.5,
      .marker  = Sepia::MarkerStyle::Circle,
      .marker_size = 5.0,
      .label   = "With LTTB[2k]"
    });

  fig.plot(x_vals.data(), y_without.data(), R)
    .data({
      .color   = Sepia::Color::red(),
      .width   = 2.5,
      .marker  = Sepia::MarkerStyle::Diamond,
      .marker_size = 5.0,
      .label   = "Without LTTB"
    });

  fig.axis({
    .x_scale = Sepia::ScaleType::Log,
    .y_scale = Sepia::ScaleType::Log
  });

  fig.grid({.show = true, .show_minor = true});
  fig.layout({
    .margin_top    = 55.0,
    .margin_bottom = 65.0,
    .margin_left   = 85.0,
    .margin_right  = 25.0,
    .background    = Sepia::Color::white()
  });

  fig.render();
  fig.save_ppm("stresstest_results.ppm");

  std::printf("\nResults plotted to stresstest_results.ppm\n");

  // Print summary
  std::printf("\n--- Summary ---\n");
  std::printf("At 1B points:\n");
  std::printf("  With LTTB[2k]:  %.2f ms\n", results.back().ms_with_lttb);
  std::printf("  Without LTTB:   %.2f ms\n", results.back().ms_without_lttb);
  std::printf("  Speedup:        %.1fx\n", results.back().ms_without_lttb / results.back().ms_with_lttb);

  return 0;
}
