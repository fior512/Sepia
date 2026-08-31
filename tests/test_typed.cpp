#include "catch_amalgamated.hpp"
#include "sepia.hpp"
#include <type_traits>
#include <vector>

using namespace Sepia;
using namespace Sepia::plot2d;

struct f32tag {};

template <>
struct Sepia::scalar_traits<f32tag> { using type = float; };

TEST_CASE("scalar_traits default is double and tags override") {
  static_assert(std::is_same_v<Sepia::scalar_traits<>::type, double>);
  static_assert(std::is_same_v<Sepia::scalar_traits<f32tag>::type, float>);
  CHECK(true);
}

TEST_CASE("Figure<> resolves the static default scalar") {
  Figure<> f(200, 150);
  std::vector<double> x{0, 1}, y{0, 1};
  f.plot(x.data(), y.data(), 2);
  CHECK(f.entries().size() == 1);
}

TEST_CASE("Figure<float> plots and renders") {
  Figure<float> f(400, 300);
  std::vector<float> x{0.0f, 1.0f, 2.0f}, y{0.0f, 1.0f, 4.0f};
  f.plot(x.data(), y.data(), 3);
  REQUIRE(f.entries().size() == 1);
  CHECK_NOTHROW(f.render());
}

TEST_CASE("Series<float> keeps float storage and double bounds math") {
  float x[] = {1.0f, 5.0f, 3.0f}, y[] = {-2.0f, 8.0f, 4.0f};
  data::Series<float> s(x, y, 3);
  CHECK(s.size() == 3);
  CHECK(s.bounds().x_min == Catch::Approx(1.0));
  CHECK(s.bounds().x_max == Catch::Approx(5.0));
  CHECK(s.bounds().y_max == Catch::Approx(8.0));
}

TEST_CASE("Series<float> converts deduced double input") {
  double x[] = {0.5, 1.5, 2.5}, y[] = {1.25, 3.75, 9.5};
  data::Series<float> s(x, y, 3);
  CHECK(s.x_data()[1] == Catch::Approx(1.5f));
  CHECK(s.y_data()[2] == Catch::Approx(9.5f));
}

TEST_CASE("LttbDecimator<float> preserves endpoints") {
  std::vector<float> x(100), y(100);
  for (std::size_t i = 0; i < 100; ++i) { x[i] = (float)i; y[i] = (float)(i % 7); }
  data::DataView<float> xv{x.data(), 100, 1}, yv{y.data(), 100, 1};
  auto r = data::LttbDecimator<float>::decimate(xv, yv, 20);
  REQUIRE(r.size() == 20);
  CHECK(r.x_data()[0] == Catch::Approx(0.0f));
  CHECK(r.x_data()[19] == Catch::Approx(99.0f));
}

TEST_CASE("Figure<float> compresses deduced double input into float") {
  Figure<float> f(400, 300);
  std::vector<double> x{0.5, 1.5, 2.5}, y{1.25, 3.75, 9.5};
  f.plot(x.data(), y.data(), 3);
  REQUIRE(f.entries().size() == 1);
  CHECK(f.entries()[0].size() == 3);
  CHECK(f.entries()[0].x_view()[1] == Catch::Approx(1.5f));
  CHECK(f.entries()[0].y_view()[2] == Catch::Approx(9.5f));
  CHECK_NOTHROW(f.render());
}

TEST_CASE("Figure<double> widens deduced float input into double") {
  Figure<double> f(400, 300);
  std::vector<float> x{0.0f, 1.0f, 2.0f}, y{0.0f, 1.0f, 4.0f};
  f.plot(x.data(), y.data(), 3);
  REQUIRE(f.entries().size() == 1);
  CHECK(f.entries()[0].x_view()[2] == Catch::Approx(2.0));
}

TEST_CASE("Figure default stays double") {
  Figure f(400, 300);
  std::vector<double> x{0, 1, 2}, y{0, 1, 4};
  f.plot(x.data(), y.data(), 3);
  CHECK(f.entries().size() == 1);
}
