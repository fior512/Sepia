#include "catch_amalgamated.hpp"
#include "sepia.hpp"
#include <vector>

using namespace Sepia;

TEST_CASE("StreamDecimator bounds output to target") {
  const std::size_t n = 100000, target = 100;
  std::vector<double> x(n), y(n);
  for (std::size_t i = 0; i < n; ++i) { x[i] = (double)i; y[i] = (double)(i % 50); }
  data::StreamDecimator<double> dec(target);
  dec.push(x.data(), y.data(), n);
  auto s = dec.finish();
  CHECK(s.size() <= target);
}

TEST_CASE("StreamDecimator preserves first and last points") {
  const std::size_t n = 50000, target = 80;
  std::vector<double> x(n), y(n);
  for (std::size_t i = 0; i < n; ++i) { x[i] = (double)i; y[i] = (double)(i % 7); }
  data::StreamDecimator<double> dec(target);
  dec.push(x.data(), y.data(), 10000);
  dec.push(x.data() + 10000, y.data() + 10000, n - 10000);
  auto s = dec.finish();
  CHECK(s.x_data()[0] == Catch::Approx(0.0));
  CHECK(s.x_data()[s.size() - 1] == Catch::Approx((double)(n - 1)));
}

TEST_CASE("StreamDecimator preserves latency spikes") {
  const std::size_t n = 200000, target = 200;
  std::vector<double> x(n), y(n);
  for (std::size_t i = 0; i < n; ++i) { x[i] = (double)i; y[i] = 50.0; }
  y[n / 2] = 1e6; // outlier
  data::StreamDecimator<double> dec(target);
  dec.push(x.data(), y.data(), n); // one huge chunk exercises compact-inside-push
  auto s = dec.finish();
  double maxy = s.y_data()[0];
  for (std::size_t i = 1; i < s.size(); ++i) maxy = std::max(maxy, s.y_data()[i]);
  CHECK(maxy > 1e5);
}
