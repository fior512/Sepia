#include "catch_amalgamated.hpp"
#include "sepia.hpp"

using namespace Sepia;
using namespace Sepia::rendering;

static void check_in_range(const std::vector<Tick>& ticks, double lo, double hi) {
    double pad = (hi - lo) * 0.05;
    for (auto& t : ticks) {
        CHECK(t.value >= lo - pad);
        CHECK(t.value <= hi + pad);
    }
}

TEST_CASE("TickEngine linear [0,100] non-empty and in range") {
    auto ticks = TickEngine::compute(0, 100, 6, false, ScaleType::Linear);
    REQUIRE_FALSE(ticks.empty());
    check_in_range(ticks, 0, 100);
    for (auto& t : ticks)
        if (!t.is_minor) CHECK_FALSE(t.label.empty());
}

TEST_CASE("TickEngine linear [-50,50] non-empty") {
    auto ticks = TickEngine::compute(-50, 50, 6, false, ScaleType::Linear);
    REQUIRE_FALSE(ticks.empty());
    check_in_range(ticks, -50, 50);
}

TEST_CASE("TickEngine linear [0,1] non-empty") {
    auto ticks = TickEngine::compute(0, 1, 6, false, ScaleType::Linear);
    REQUIRE_FALSE(ticks.empty());
    check_in_range(ticks, 0, 1);
}

TEST_CASE("TickEngine linear [1e6,2e6] large values with labels") {
    auto ticks = TickEngine::compute(1e6, 2e6, 6, false, ScaleType::Linear);
    REQUIRE_FALSE(ticks.empty());
    check_in_range(ticks, 1e6, 2e6);
    for (auto& t : ticks)
        if (!t.is_minor) CHECK_FALSE(t.label.empty());
}

TEST_CASE("TickEngine linear [1e-9,2e-9] tiny values") {
    auto ticks = TickEngine::compute(1e-9, 2e-9, 6, false, ScaleType::Linear);
    REQUIRE_FALSE(ticks.empty());
    check_in_range(ticks, 1e-9, 2e-9);
}

TEST_CASE("TickEngine lo==hi returns empty") {
    // hi <= lo guard in compute_linear
    auto ticks = TickEngine::compute(5, 5, 6, false, ScaleType::Linear);
    CHECK(ticks.empty());
}

TEST_CASE("TickEngine lo>hi returns empty") {
    auto ticks = TickEngine::compute(10, 0, 6, false, ScaleType::Linear);
    CHECK(ticks.empty());
}

TEST_CASE("TickEngine log [1,1000] powers of 10") {
    auto ticks = TickEngine::compute(1, 1000, 6, false, ScaleType::Log);
    // exp_lo=floor(log10(1))=0, exp_hi=ceil(log10(1000))=3 -> 4 major ticks
    REQUIRE(ticks.size() == 4);
    std::vector<double> expected = {1.0, 10.0, 100.0, 1000.0};
    for (std::size_t i = 0; i < 4; ++i)
        CHECK(ticks[i].value == Catch::Approx(expected[i]).epsilon(expected[i] * 1e-9));
}

TEST_CASE("TickEngine log [0.001,1] powers of 10") {
    auto ticks = TickEngine::compute(0.001, 1, 6, false, ScaleType::Log);
    // exp_lo=floor(log10(0.001))=-3, exp_hi=ceil(log10(1))=0 -> 4 major ticks
    REQUIRE(ticks.size() == 4);
    std::vector<double> expected = {0.001, 0.01, 0.1, 1.0};
    for (std::size_t i = 0; i < 4; ++i)
        CHECK(ticks[i].value == Catch::Approx(expected[i]).epsilon(expected[i] * 1e-9));
}

TEST_CASE("TickEngine minor ticks flagged and labels empty") {
    auto ticks = TickEngine::compute(0, 100, 6, true, ScaleType::Linear);
    bool has_minor = false;
    for (auto& t : ticks) {
        if (t.is_minor) { has_minor = true; CHECK(t.label.empty()); }
    }
    CHECK(has_minor);
}

TEST_CASE("TickEngine higher target_ticks gives more or equal major ticks") {
    auto t5  = TickEngine::compute(0, 100, 5,  false, ScaleType::Linear);
    auto t10 = TickEngine::compute(0, 100, 10, false, ScaleType::Linear);
    CHECK(t10.size() >= t5.size());
}
