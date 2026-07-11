#include <catch2/catch_test_macros.hpp>
#include "order_book.hpp"

TEST_CASE("PriceLevel total_qty sums correctly") {
    fathom::PriceLevel level;
    level.orders.push_back(fathom::Order{1, 10000, 5, fathom::Side::Buy});
    level.orders.push_back(fathom::Order{2, 10000, 3, fathom::Side::Buy});
    level.orders.push_back(fathom::Order{3, 10000, 7, fathom::Side::Buy});
    REQUIRE(level.total_qty() == 15);
}

TEST_CASE("insert adds order to correct price level") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    REQUIRE(book.best_bid().has_value());
    REQUIRE(book.best_bid().value() == 10000);
}

TEST_CASE("bids sort with highest price as best") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.insert_limit_order(2, 10050, 3, fathom::Side::Buy);
    REQUIRE(book.best_bid().value() == 10050);
}

TEST_CASE("asks sort with lowest price as best") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10100, 5, fathom::Side::Sell);
    book.insert_limit_order(2, 10050, 3, fathom::Side::Sell);
    REQUIRE(book.best_ask().value() == 10050);
}

TEST_CASE("cancel removes only the targeted order, others keep their position") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.insert_limit_order(2, 10000, 3, fathom::Side::Buy);
    book.insert_limit_order(3, 10000, 7, fathom::Side::Buy);
    book.cancel_order(2);
    REQUIRE(book.depth_at_price(10000, fathom::Side::Buy) == 12);
}

TEST_CASE("cancelling an unknown order id does nothing and does not crash") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.cancel_order(999);
    REQUIRE(book.best_bid().value() == 10000);
}

TEST_CASE("cancelling the only order at a price level removes the level entirely") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.cancel_order(1);
    REQUIRE_FALSE(book.best_bid().has_value());
}

TEST_CASE("market order exactly matches one resting order") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Sell);

    auto fills = book.match_market_order(5, fathom::Side::Buy);

    REQUIRE(fills.size() == 1);
    REQUIRE(fills[0].qty == 5);
    REQUIRE(fills[0].price == 10000);
    REQUIRE_FALSE(book.best_ask().has_value());  // level should be empty now
}

TEST_CASE("market order smaller than resting order leaves a partial fill") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 10, fathom::Side::Sell);

    auto fills = book.match_market_order(4, fathom::Side::Buy);

    REQUIRE(fills.size() == 1);
    REQUIRE(fills[0].qty == 4);
    REQUIRE(book.depth_at_price(10000, fathom::Side::Sell) == 6);  // 10 - 4 remaining
}

TEST_CASE("market order walks through multiple price levels in price order") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Sell);
    book.insert_limit_order(2, 10010, 5, fathom::Side::Sell);

    auto fills = book.match_market_order(8, fathom::Side::Buy);

    REQUIRE(fills.size() == 2);
    REQUIRE(fills[0].price == 10000);  // best (lowest ask) price consumed first
    REQUIRE(fills[0].qty == 5);
    REQUIRE(fills[1].price == 10010);
    REQUIRE(fills[1].qty == 3);        // only 3 needed to complete the order
    REQUIRE(book.depth_at_price(10010, fathom::Side::Sell) == 2);  // 5 - 3 remaining
}

TEST_CASE("market order larger than available liquidity fills what it can") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Sell);

    auto fills = book.match_market_order(20, fathom::Side::Buy);

    REQUIRE(fills.size() == 1);
    REQUIRE(fills[0].qty == 5);        // only 5 was available, so only 5 filled
    REQUIRE_FALSE(book.best_ask().has_value());
}

TEST_CASE("sell market order matches against bids, highest price first") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 9990, 5, fathom::Side::Buy);
    book.insert_limit_order(2, 10000, 5, fathom::Side::Buy);

    auto fills = book.match_market_order(5, fathom::Side::Sell);

    REQUIRE(fills.size() == 1);
    REQUIRE(fills[0].price == 10000);  // best (highest bid) price consumed first
}

TEST_CASE("reduce_order_qty shrinks an order without removing it if qty remains") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 10, fathom::Side::Buy);
    book.reduce_order_qty(1, 4);
    REQUIRE(book.depth_at_price(10000, fathom::Side::Buy) == 6);
    REQUIRE(book.best_bid().has_value());
}

TEST_CASE("reduce_order_qty removes the order entirely if it hits zero") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.reduce_order_qty(1, 5);
    REQUIRE_FALSE(book.best_bid().has_value());
}

TEST_CASE("reduce_order_qty preserves other orders' queue position") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.insert_limit_order(2, 10000, 3, fathom::Side::Buy);
    book.reduce_order_qty(1, 2);  // order 1: 5 -> 3, order 2 untouched
    REQUIRE(book.depth_at_price(10000, fathom::Side::Buy) == 6);  // 3 + 3
}

TEST_CASE("enforce_level_cap keeps only the best N price levels on the bid side") {
    fathom::OrderBook book(3);  // cap at 3 levels for this test
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.insert_limit_order(2, 10010, 5, fathom::Side::Buy);
    book.insert_limit_order(3, 10020, 5, fathom::Side::Buy);
    book.insert_limit_order(4, 10030, 5, fathom::Side::Buy);  // should push out the worst level

    REQUIRE(book.best_bid().value() == 10030);
    REQUIRE(book.depth_at_price(10000, fathom::Side::Buy) == 0);  // dropped, worst price
    REQUIRE(book.depth_at_price(10010, fathom::Side::Buy) == 5);  // kept
}

TEST_CASE("enforce_level_cap keeps only the best N price levels on the ask side") {
    fathom::OrderBook book(3);
    book.insert_limit_order(1, 10040, 5, fathom::Side::Sell);
    book.insert_limit_order(2, 10030, 5, fathom::Side::Sell);
    book.insert_limit_order(3, 10020, 5, fathom::Side::Sell);
    book.insert_limit_order(4, 10010, 5, fathom::Side::Sell);  // should push out the worst level

    REQUIRE(book.best_ask().value() == 10010);
    REQUIRE(book.depth_at_price(10040, fathom::Side::Sell) == 0);  // dropped, worst price
}

TEST_CASE("dropped level's orders no longer respond to cancel or reduce") {
    fathom::OrderBook book(1);  // cap at 1 level to force a drop immediately
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.insert_limit_order(2, 10010, 5, fathom::Side::Buy);  // drops order 1's level

    book.cancel_order(1);   // should be a silent no-op, order 1 is already forgotten
    REQUIRE(book.best_bid().value() == 10010);  // unaffected
}

TEST_CASE("marketable buy limit order matches immediately instead of resting") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Sell);   // resting ask
    book.insert_limit_order(2, 10000, 5, fathom::Side::Buy);    // buy at the touch -> should match, not rest

    REQUIRE_FALSE(book.best_ask().has_value());  // ask fully consumed
    REQUIRE_FALSE(book.best_bid().has_value());  // buy fully matched, nothing rests
}

TEST_CASE("marketable buy limit order with leftover quantity rests the remainder") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Sell);
    book.insert_limit_order(2, 10000, 8, fathom::Side::Buy);   // more than available

    REQUIRE_FALSE(book.best_ask().has_value());               // ask fully consumed
    REQUIRE(book.best_bid().value() == 10000);                // remainder (3) rests
    REQUIRE(book.depth_at_price(10000, fathom::Side::Buy) == 3);
}