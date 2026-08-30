#include "catch_amalgamated.hpp"
#include "sepia.hpp"
#include <limits>
#include <vector>

using namespace Sepia;
using namespace Sepia::plot2d;

static std::size_t nonbg_count(const rendering::Canvas& c) {
  const std::uint8_t* p = c.data();
  std::size_t n = (std::size_t)c.width() * c.height();
  std::size_t cnt = 0;
  for (std::size_t i = 0; i < n; ++i) {
    std::size_t o = i * 4;
    if (p[o] != 255 || p[o+1] != 255 || p[o+2] != 255) ++cnt;
  }
  return cnt;
}

static std::size_t colored_columns(const rendering::Canvas& c) {
  std::size_t cols = 0;
  for (std::uint32_t x = 0; x < c.width(); ++x) {
    bool hit = false;
    for (std::uint32_t y = 0; y < c.height() && !hit; ++y) {
      const std::uint8_t* p = c.data() + ((std::size_t)y * c.width() + x) * 4;
      if (p[0] != 255 || p[1] != 255 || p[2] != 255) hit = true;
    }
    if (hit) ++cols;
  }
  return cols;
}

TEST_CASE("NaN splits the line leaving a gap") {
  Figure cont(400, 200), gap(400, 200);
  std::vector<double> x{0, 1, 2, 3}, y{0, 0, 0, 0};
  std::vector<double> yn = y;
  yn[2] = std::numeric_limits<double>::quiet_NaN();
  cont.grid({.show = false});
  gap.grid({.show = false});
  params::AxisStyle ax; ax.show = false;
  cont.axis(ax);
  gap.axis(ax);
  cont.plot(x.data(), y.data(), 4).width(2.0);
  gap.plot(x.data(), yn.data(), 4).width(2.0);
  CHECK_NOTHROW(cont.render());
  CHECK_NOTHROW(gap.render());
  CHECK(nonbg_count(gap.canvas()) < nonbg_count(cont.canvas()));
  CHECK(colored_columns(gap.canvas()) < colored_columns(cont.canvas()));
}

TEST_CASE("NaN in fill and marker paths does not crash") {
  Figure f(400, 200);
  std::vector<double> x{0, 1, 2, 3}, y{0, 1, std::numeric_limits<double>::quiet_NaN(), 3};
  f.plot(x.data(), y.data(), 4).fill(true).marker(MarkerStyle::Circle, 4.0);
  CHECK_NOTHROW(f.render());
}
