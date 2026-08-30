#include "catch_amalgamated.hpp"
#include "sepia.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

using namespace Sepia;
using namespace Sepia::plot2d;

TEST_CASE("PlotCommand commits on destruction and forwards color and label") {
    Figure fig(600, 400);
    std::vector<double> x = {1, 2, 3}, y = {4, 5, 6};
    // temporary PlotCommand is destroyed at the semicolon -> commit() fires
    fig.plot(x.data(), y.data(), 3).color(Color::red()).label("foo");
    REQUIRE(fig.entries().size() == 1);
    CHECK(fig.entries()[0].style.color.r == Color::red().r);
    CHECK(fig.entries()[0].style.label   == "foo");
}

TEST_CASE("PlotCommand forwards width and fill") {
    Figure fig(600, 400);
    std::vector<double> x = {1, 2, 3}, y = {4, 5, 6};
    fig.plot(x.data(), y.data(), 3).width(3.0).fill(true);
    REQUIRE(fig.entries().size() == 1);
    CHECK(fig.entries()[0].style.width == Catch::Approx(3.0));
    CHECK(fig.entries()[0].style.fill  == true);
}

TEST_CASE("PlotCommand forwards marker style and size") {
    Figure fig(600, 400);
    std::vector<double> x = {1, 2}, y = {3, 4};
    fig.plot(x.data(), y.data(), 2).marker(MarkerStyle::Circle, 6.0);
    REQUIRE(fig.entries().size() == 1);
    CHECK(fig.entries()[0].style.marker      == MarkerStyle::Circle);
    CHECK(fig.entries()[0].style.marker_size == Catch::Approx(6.0));
}

TEST_CASE("Figure accumulates multiple entries") {
    Figure fig(600, 400);
    std::vector<double> x = {0, 1}, y = {0, 1};
    fig.plot(x.data(), y.data(), 2);
    fig.plot(x.data(), y.data(), 2);
    CHECK(fig.entries().size() == 2);
}

TEST_CASE("Figure plot_ref creates non-owning entry with correct data") {
    Figure fig(600, 400);
    double x[] = {1, 2, 3}, y[] = {4, 5, 6};
    fig.plot_ref(x, y, 3);
    REQUIRE(fig.entries().size() == 1);
    CHECK(fig.entries()[0].size()     == 3);
    CHECK(fig.entries()[0].x_view()[1] == Catch::Approx(2.0));
}

TEST_CASE("Figure render with data does not crash") {
    Figure fig(600, 400);
    std::vector<double> x(50), y(50);
    for (std::size_t i = 0; i < 50; ++i) { x[i] = static_cast<double>(i); y[i] = static_cast<double>(i); }
    fig.plot(x.data(), y.data(), 50);
    CHECK_NOTHROW(fig.render());
}

TEST_CASE("Figure render empty figure does not crash") {
    Figure fig(600, 400);
    CHECK_NOTHROW(fig.render());
}

TEST_CASE("Figure render log scale does not crash") {
    Figure fig(600, 400);
    std::vector<double> x = {1, 10, 100}, y = {1, 10, 100};
    params::AxisStyle ax;
    ax.x_scale = ScaleType::Log;
    ax.y_scale = ScaleType::Log;
    fig.axis(ax);
    fig.plot(x.data(), y.data(), 3);
    CHECK_NOTHROW(fig.render());
}

TEST_CASE("Figure render with minor grid does not crash") {
    Figure fig(600, 400);
    std::vector<double> x = {0, 1, 2}, y = {0, 1, 4};
    params::GridStyle gs;
    gs.show_minor = true;
    fig.grid(gs);
    fig.plot(x.data(), y.data(), 3);
    CHECK_NOTHROW(fig.render());
}

TEST_CASE("Figure save_ppm produces valid PPM with correct dimensions") {
    Figure fig(200, 150);
    std::vector<double> x = {0, 1, 2}, y = {0, 1, 0};
    fig.plot(x.data(), y.data(), 3);
    fig.render();

    std::string path = (std::filesystem::temp_directory_path() / "sepia_test_figure.ppm").string();
    CHECK(fig.save_ppm(path));

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    REQUIRE(f.is_open());
    // file must contain at least the raw pixel payload
    CHECK(static_cast<int>(f.tellg()) > 200 * 150 * 3);

    f.seekg(0);
    std::string magic;
    int w, h, maxval;
    f >> magic >> w >> h >> maxval;
    CHECK(magic  == "P6");
    CHECK(w      == 200);
    CHECK(h      == 150);
    CHECK(maxval == 255);
}
