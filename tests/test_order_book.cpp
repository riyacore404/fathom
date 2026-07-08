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