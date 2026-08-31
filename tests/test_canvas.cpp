#include "catch_amalgamated.hpp"
#include "sepia.hpp"
#include <filesystem>
#include <fstream>
#include <string>

using namespace Sepia;
using namespace Sepia::rendering;

// Returns pointer to the first byte of pixel (x,y) in RGBA layout
static const std::uint8_t* px(const Canvas& c, int x, int y) {
    return c.data() + (static_cast<std::size_t>(y) * c.width() + static_cast<std::size_t>(x)) * 4;
}

TEST_CASE("Canvas dimensions and stride") {
    Canvas c(10, 8);
    CHECK(c.width()  == 10u);
    CHECK(c.height() == 8u);
    // stride = width * 4 bytes per pixel
    CHECK(c.stride() == 40u);
}

TEST_CASE("Canvas initialized to white") {
    Canvas c(10, 8);
    CHECK(px(c,0,0)[0] == 255u);
    CHECK(px(c,0,0)[1] == 255u);
    CHECK(px(c,0,0)[2] == 255u);
    CHECK(px(c,0,0)[3] == 255u);
}

TEST_CASE("Canvas set_pixel full-alpha RGBA byte layout") {
    Canvas c(10, 8);
    c.set_pixel(3, 2, Color::red());
    CHECK(px(c,3,2)[0] == 228u);
    CHECK(px(c,3,2)[1] == 26u);
    CHECK(px(c,3,2)[2] == 28u);
    CHECK(px(c,3,2)[3] == 255u);
}

TEST_CASE("Canvas set_pixel out-of-bounds does not crash") {
    Canvas c(10, 8);
    CHECK_NOTHROW(c.set_pixel(-1, 0,  Color::red()));
    CHECK_NOTHROW(c.set_pixel(10, 0,  Color::red()));
    CHECK_NOTHROW(c.set_pixel(0,  -1, Color::red()));
    CHECK_NOTHROW(c.set_pixel(0,  8,  Color::red()));
}

TEST_CASE("Canvas set_pixel alpha blend formula") {
    Canvas c(10, 8);
    // blend {255, 0, 0, a=128} over white {255, 255, 255}
    // formula: out[ch] = (src[ch]*sa + dst[ch]*da) / 255, where sa=128, da=255-128=127
    // r: (255*128 + 255*127) / 255 = 255*(128+127)/255 = 255
    // g: (  0*128 + 255*127) / 255 = 32385/255         = 127
    // b: same as g
    c.set_pixel(0, 0, {255, 0, 0, 128});
    CHECK(px(c,0,0)[0] == 255u);
    CHECK(px(c,0,0)[1] == 127u);
    CHECK(px(c,0,0)[2] == 127u);
}

TEST_CASE("Canvas clear overwrites previously written pixel") {
    Canvas c(10, 8);
    c.set_pixel(3, 2, Color::red());
    c.clear(Color::black());
    CHECK(px(c,3,2)[0] == 0u);
    CHECK(px(c,3,2)[1] == 0u);
    CHECK(px(c,3,2)[2] == 0u);
}

TEST_CASE("Canvas fill_rect paints corners, leaves exterior unchanged") {
    Canvas c(20, 20);
    c.fill_rect(2, 3, 4, 5, Color::red());
    // fill_rect(x=2, y=3, w=4, h=5): painted x in [2, 2+4) = [2,5], y in [3, 3+5) = [3,7]
    CHECK(px(c,2,3)[0] == 228u);
    CHECK(px(c,5,7)[0] == 228u);
    CHECK(px(c,1,3)[0] == 255u);
    CHECK(px(c,6,3)[0] == 255u);
    CHECK(px(c,2,8)[0] == 255u);
}

TEST_CASE("Canvas draw_circle paints center, leaves far pixel unchanged") {
    Canvas c(30, 30);
    c.draw_circle(15, 15, 5, Color::red());
    CHECK(px(c,15,15)[0] == 228u);
    CHECK(px(c,21,15)[0] == 255u);
}

TEST_CASE("text_width formula len*6*scale - scale") {
    // "ABC": len=3, scale=2 -> 3*6*2 - 2 = 34
    CHECK(text_width("ABC", 2) == 34);
}

TEST_CASE("text_width empty string returns zero") {
    CHECK(text_width("", 1) == 0);
    CHECK(text_width("", 3) == 0);
}

TEST_CASE("text_height formula 7*scale") {
    CHECK(text_height(1) == 7);
    CHECK(text_height(3) == 21);
}

TEST_CASE("draw_text writes at least one non-background pixel") {
    Canvas c(100, 20);
    draw_text(c, "A", 0, 0, Color::black());
    bool found = false;
    for (std::uint32_t y = 0; y < c.height() && !found; ++y)
        for (std::uint32_t x = 0; x < c.width() && !found; ++x)
            if (px(c, static_cast<int>(x), static_cast<int>(y))[0] != 255u) found = true;
    CHECK(found);
}

TEST_CASE("draw_text_vertical writes at least one non-background pixel") {
    Canvas c(20, 100);
    draw_text_vertical(c, "A", 0, 0, Color::black());
    bool found = false;
    for (std::uint32_t y = 0; y < c.height() && !found; ++y)
        for (std::uint32_t x = 0; x < c.width() && !found; ++x)
            if (px(c, static_cast<int>(x), static_cast<int>(y))[0] != 255u) found = true;
    CHECK(found);
}

TEST_CASE("write_ppm produces valid PPM header") {
    Canvas c(10, 8);
    c.set_pixel(0, 0, {10, 20, 30, 255});
    std::string path = (std::filesystem::temp_directory_path() / "sepia_test_canvas.ppm").string();
    REQUIRE(output::write_ppm(c, path));

    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.is_open());
    std::string magic;
    int w, h, maxval;
    f >> magic >> w >> h >> maxval;
    CHECK(magic   == "P6");
    CHECK(w       == 10);
    CHECK(h       == 8);
    CHECK(maxval  == 255);
}

TEST_CASE("write_ppm file size is exact") {
    Canvas c(10, 8);
    std::string path = (std::filesystem::temp_directory_path() / "sepia_test_canvas_size.ppm").string();
    REQUIRE(output::write_ppm(c, path));

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    REQUIRE(f.is_open());
    // header "P6\n10 8\n255\n" = 12 bytes; pixel data: 10*8*3 = 240 bytes
    CHECK(static_cast<int>(f.tellg()) == 12 + 10 * 8 * 3);
}

TEST_CASE("write_ppm invalid path returns false") {
    Canvas c(10, 8);
    CHECK_FALSE(output::write_ppm(c, "/nonexistent_dir/test.ppm"));
}
