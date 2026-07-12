#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "metrics.hpp"

TEST_CASE("fill_rate reflects quantity filled vs quantity placed, not order count") {
    MetricsTracker m;
    m.record_placement(1, 100.0, 10000, 20, 10000);  // placed 20 shares
    m.record_fill(1, 105.0, 5, 10000);                // only 5 filled

    REQUIRE(m.fill_rate() == Catch::Approx(0.25));    // 5/20
}

TEST_CASE("average_time_to_fill computes correctly across multiple fills") {
    MetricsTracker m;
    m.record_placement(1, 100.0, 10000, 10, 10000);
    m.record_fill(1, 103.0, 4, 10000);   // 3 seconds later
    m.record_fill(1, 107.0, 6, 10000);   // 7 seconds later

    REQUIRE(m.average_time_to_fill() == Catch::Approx(5.0));  // (3+7)/2
}

TEST_CASE("average_slippage_ticks is signed, reflects fill price vs arrival mid") {
    MetricsTracker m;
    m.record_placement(1, 100.0, 10000, 10, 9990);   // mid was 9990 at placement
    m.record_fill(1, 101.0, 10, 10000);               // filled at 10000

    REQUIRE(m.average_slippage_ticks() == Catch::Approx(10.0));  // 10000 - 9990
}

TEST_CASE("fill for an untracked placement is ignored, not counted") {
    MetricsTracker m;
    m.record_fill(999, 100.0, 5, 10000);  // order 999 was never placed
    REQUIRE(m.total_fills() == 0);
}