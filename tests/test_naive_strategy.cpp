#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "strategies/naive_instant_fill_strategy.hpp"
#include "order_book.hpp"

TEST_CASE("naive strategy fills instantly and fully at the ask, never touching the real book") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 100, fathom::Side::Sell);  // resting ask, real liquidity present
    book.insert_limit_order(2, 9990, 100, fathom::Side::Buy);    // resting bid

    NaiveInstantFillStrategy strategy;
    auto actions = strategy.on_book_update(book);

    REQUIRE(actions.empty());                          // never emits a real action
    REQUIRE(book.depth_at_price(10000, fathom::Side::Sell) == 100);  // real book untouched

    const auto& m = strategy.metrics();
    REQUIRE(m.total_fills() == 1);
    REQUIRE(m.fill_rate() == Catch::Approx(1.0));       // always "100%" by construction
    REQUIRE(m.average_time_to_fill() == Catch::Approx(0.0));  // always instant by construction
}