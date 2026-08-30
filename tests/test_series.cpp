#include "catch_amalgamated.hpp"
#include "sepia.hpp"

using namespace Sepia;
using namespace Sepia::data;

// --- Series ---

TEST_CASE("Series construction from raw arrays") {
    double x[] = {1, 2, 3}, y[] = {10, 20, 30};
    Series s(x, y, 3);
    CHECK(s.size() == 3);
    CHECK(s.x_data()[0] == Catch::Approx(1.0));
    CHECK(s.x_data()[2] == Catch::Approx(3.0));
    CHECK(s.y_data()[1] == Catch::Approx(20.0));
}

TEST_CASE("Series construction from AlignedBuffer move") {
    AlignedBuffer<double> x(3), y(3);
    x[0] = 5; x[1] = 6; x[2] = 7;
    y[0] = 50; y[1] = 60; y[2] = 70;
    Series s(std::move(x), std::move(y));
    CHECK(s.size() == 3);
    CHECK(s.y_data()[2] == Catch::Approx(70.0));
}

TEST_CASE("Series default construction is empty") {
    Series s;
    CHECK(s.size() == 0);
}

TEST_CASE("Series bounds computed correctly") {
    double x[] = {1, 5, 3}, y[] = {-2, 8, 4};
    Series s(x, y, 3);
    CHECK(s.bounds().x_min == Catch::Approx(1.0));
    CHECK(s.bounds().x_max == Catch::Approx(5.0));
    CHECK(s.bounds().y_min == Catch::Approx(-2.0));
    CHECK(s.bounds().y_max == Catch::Approx(8.0));
}

TEST_CASE("Series x_view and y_view indexed access") {
    double x[] = {2, 4, 6}, y[] = {1, 3, 5};
    Series s(x, y, 3);
    auto xv = s.x_view();
    auto yv = s.y_view();
    CHECK(xv.count == 3);
    CHECK(xv[1] == Catch::Approx(4.0));
    CHECK(yv[2] == Catch::Approx(5.0));
}

TEST_CASE("Series x_data and y_data pointer access") {
    double x[] = {7, 8}, y[] = {9, 10};
    Series s(x, y, 2);
    CHECK(s.x_data()[0] == Catch::Approx(7.0));
    CHECK(s.y_data()[1] == Catch::Approx(10.0));
}

// --- DataView ---

TEST_CASE("DataView default construction is empty") {
    DataView dv;
    CHECK(dv.empty());
    CHECK(dv.count == 0);
}

TEST_CASE("DataView stride=2 accesses every other element") {
    double arr[] = {10.0, 99.0, 20.0, 99.0, 30.0};
    // n=3, stride=2 -> element i is at ptr[i*2], so indices 0, 2, 4
    DataView dv{arr, 3, 2};
    CHECK_FALSE(dv.empty());
    CHECK(dv[0] == Catch::Approx(10.0));
    CHECK(dv[1] == Catch::Approx(20.0));
    CHECK(dv[2] == Catch::Approx(30.0));
}

TEST_CASE("DataView stride=2 skips interleaved element") {
    double xs[] = {1.0, 999.0, 3.0};
    // n=2, stride=2 -> reads xs[0*2]=xs[0]=1.0 and xs[1*2]=xs[2]=3.0, xs[1]=999.0 never touched
    DataView dv{xs, 2, 2};
    CHECK(dv[0] == Catch::Approx(1.0));
    CHECK(dv[1] == Catch::Approx(3.0));
}

// --- ExternalSeries ---

TEST_CASE("ExternalSeries stride=1 basic access") {
    double x[] = {1, 2, 3}, y[] = {10, 20, 30};
    ExternalSeries s(x, y, 3, 1);
    CHECK(s.size() == 3);
    CHECK(s.x_view()[2] == Catch::Approx(3.0));
    CHECK(s.bounds().x_min == Catch::Approx(1.0));
}

TEST_CASE("ExternalSeries stride=2 element access") {
    double xs[] = {5, 0, 15, 0, 25};
    double ys[] = {50, 0, 150, 0, 250};
    ExternalSeries s(xs, ys, 3, 2);
    CHECK(s.size() == 3);
    CHECK(s.x_view()[1] == Catch::Approx(15.0));
    CHECK(s.y_view()[2] == Catch::Approx(250.0));
}

TEST_CASE("ExternalSeries stride=2 bounds ignores interleaved elements") {
    double xs[] = {1.0, 999.0, 3.0};
    double ys[] = {10.0, 888.0, 30.0};
    // n=2, stride=2 -> reads xs[0]=1.0 and xs[2]=3.0, xs[1]=999.0 never touched
    ExternalSeries s(xs, ys, 2, 2);
    CHECK(s.bounds().x_min == Catch::Approx(1.0));
    CHECK(s.bounds().x_max == Catch::Approx(3.0));
    CHECK(s.bounds().y_max == Catch::Approx(30.0));
}
