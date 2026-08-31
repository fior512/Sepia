#include "catch_amalgamated.hpp"
#include "sepia.hpp"
#include <vector>

using namespace Sepia;
using namespace Sepia::plot2d;

TEST_CASE("Default series colors cycle") {
  Figure f(400, 200);
  std::vector<f64> x{0, 1}, y{0, 1};
  f.plot(x.data(), y.data(), 2);
  f.plot(x.data(), y.data(), 2);
  f.plot(x.data(), y.data(), 2);
  REQUIRE(f.entries().size() == 3);
  CHECK_FALSE(f.entries()[0].style.color == f.entries()[1].style.color);
  CHECK(f.entries()[0].style.color == Color::blue());
}

TEST_CASE("Explicit fluent color is respected") {
  Figure f(400, 200);
  std::vector<f64> x{0, 1}, y{0, 1};
  f.plot(x.data(), y.data(), 2).color(Color::red());
  f.plot(x.data(), y.data(), 2).color(Color::green());
  REQUIRE(f.entries().size() == 2);
  CHECK(f.entries()[0].style.color.r == Color::red().r);
  CHECK(f.entries()[1].style.color.r == Color::green().r);
}

TEST_CASE("Explicit aggregate color is respected") {
  Figure f(400, 200);
  std::vector<f64> x{0, 1}, y{0, 1};
  f.plot(x.data(), y.data(), 2).data({.color = Color::orange()});
  f.plot(x.data(), y.data(), 2).data({.color = Color::purple()});
  REQUIRE(f.entries().size() == 2);
  CHECK(f.entries()[0].style.color.r == Color::orange().r);
  CHECK(f.entries()[1].style.color.r == Color::purple().r);
}

TEST_CASE("Explicit series do not shift the auto palette") {
  Figure f(400, 200);
  std::vector<f64> x{0, 1}, y{0, 1};
  f.plot(x.data(), y.data(), 2).color(Color::black());
  f.plot(x.data(), y.data(), 2); // auto
  f.plot(x.data(), y.data(), 2); // auto
  REQUIRE(f.entries().size() == 3);
  CHECK(f.entries()[0].style.color == Color::black());
  CHECK(f.entries()[1].style.color == Color::blue()); // palette[0], not shifted
  CHECK_FALSE(f.entries()[1].style.color == f.entries()[2].style.color);
}
