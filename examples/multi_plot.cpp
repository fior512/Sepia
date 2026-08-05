#include "../sepia.hpp"
#include <cmath>
#include <vector>
 // Sepia   main  !? ❯ g++ -std=c++20 -O2 -march=native examples/multi_plot.cpp src/plot2d/figure.cpp && ./a.out && loupe multi_plot.ppm 
int main() {
  const size_t N = 300;
  std::vector<Sepia::f64> x(N), y1(N), y2(N), y3(N);

  for (size_t i = 0; i < N; ++i) {
    x[i]  = static_cast<double>(i) * 0.04;
    y1[i] = std::sin(x[i]);
    y2[i] = std::cos(x[i]) * 0.8;
    y3[i] = std::sin(x[i] * 2.0) * 0.5;
  }

  Sepia::plot2d::Figure figure(280, 180);
  figure.set_title("Multi-Plot Parameters");
  figure.set_xlabel("Time");
  figure.set_ylabel("Signal");

  figure.plot(x.data(), y1.data(), N)
    .data({.color = Sepia::Color::blue(), .width = 5.0, .label = "sin(x)"});

  figure.plot(x.data(), y2.data(), N)
    .data({.color = Sepia::Color::red(), .width = 3.0, .alpha = 0.7, .label = "cos(x)"});

  figure.plot(x.data(), y3.data(), N)
    .data({.color = Sepia::Color::green(), .width = 1.0,
      .line_style = Sepia::LineStyle::Dashed, .label = "sin(2x)"});

  figure.grid({.show = true, .show_minor = true});
  figure.layout({.background = Sepia::Color::white()});

  figure.render();
  figure.save_ppm("bin/plots/multi_plot.ppm");

  return 0;
}
