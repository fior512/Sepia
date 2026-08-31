#include "catch_amalgamated.hpp"
#include "sepia.hpp"
#include <cmath>

using namespace Sepia;
using namespace Sepia::data;

// Builds x[i]=i, y[i]=sin(i*0.05) for i in [0,n)
static std::pair<AlignedBuffer<double>, AlignedBuffer<double>> make_sin_series(std::size_t n) {
    AlignedBuffer<double> x(n), y(n);
    for (std::size_t i = 0; i < n; ++i) {
        x[i] = static_cast<double>(i);
        y[i] = std::sin(static_cast<double>(i) * 0.05);
    }
    return {std::move(x), std::move(y)};
}

TEST_CASE("LttbDecimator output count equals target when n > target >= 3") {
    auto [x, y] = make_sin_series(100);
    DataView xv{x.data(), 100, 1}, yv{y.data(), 100, 1};
    // when n > target >= 3, LTTB allocates exactly target slots (first + (target-2) buckets + last)
    auto result = LttbDecimator<double>::decimate(xv, yv, 20);
    CHECK(result.size() == 20);
}

TEST_CASE("LttbDecimator first point preserved") {
    auto [x, y] = make_sin_series(100);
    DataView xv{x.data(), 100, 1}, yv{y.data(), 100, 1};
    auto result = LttbDecimator<double>::decimate(xv, yv, 20);
    CHECK(result.x_data()[0] == Catch::Approx(0.0));
}

TEST_CASE("LttbDecimator last point preserved") {
    auto [x, y] = make_sin_series(100);
    DataView xv{x.data(), 100, 1}, yv{y.data(), 100, 1};
    auto result = LttbDecimator<double>::decimate(xv, yv, 20);
    CHECK(result.x_data()[result.size() - 1] == Catch::Approx(99.0));
}

TEST_CASE("LttbDecimator identity when target >= n") {
    auto [x, y] = make_sin_series(50);
    DataView xv{x.data(), 50, 1}, yv{y.data(), 50, 1};
    auto result = LttbDecimator<double>::decimate(xv, yv, 200);
    CHECK(result.size() == 50);
}

TEST_CASE("LttbDecimator passthrough when target < 3") {
    auto [x, y] = make_sin_series(100);
    DataView xv{x.data(), 100, 1}, yv{y.data(), 100, 1};
    auto result = LttbDecimator<double>::decimate(xv, yv, 2);
    CHECK(result.size() == 100);
}

TEST_CASE("LttbDecimator output x is monotone (input x is i)") {
    auto [x, y] = make_sin_series(200);
    DataView xv{x.data(), 200, 1}, yv{y.data(), 200, 1};
    auto result = LttbDecimator<double>::decimate(xv, yv, 30);
    for (std::size_t i = 1; i < result.size(); ++i)
        CHECK(result.x_data()[i] >= result.x_data()[i - 1]);
}
