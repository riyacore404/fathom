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

TEST_CASE("fill callback fires when a strategy order is consumed by an incoming order") {
    fathom::OrderBook book;
    std::vector<fathom::Fill> received;
    book.set_fill_callback([&received](const fathom::Fill& f) { received.push_back(f); });

    book.insert_limit_order(1, 10000, 10, fathom::Side::Buy, /*is_strategy_order=*/true);
    book.insert_limit_order(2, 10000, 4, fathom::Side::Sell, false);   // real order hits ours

    REQUIRE(received.size() == 1);
    REQUIRE(received[0].qty == 4);
    REQUIRE(received[0].price == 10000);
}

TEST_CASE("fill callback does not fire for non-strategy orders") {
    fathom::OrderBook book;
    std::vector<fathom::Fill> received;
    book.set_fill_callback([&received](const fathom::Fill& f) { received.push_back(f); });

    book.insert_limit_order(1, 10000, 10, fathom::Side::Buy, false);   // not ours
    book.insert_limit_order(2, 10000, 4, fathom::Side::Sell, false);

    REQUIRE(received.empty());
}

TEST_CASE("OrderBook constructor rejects zero max_levels") {
    REQUIRE_THROWS_AS(fathom::OrderBook(0), std::invalid_argument);
}

TEST_CASE("insert_limit_order rejects zero or negative quantity") {
    fathom::OrderBook book;
    REQUIRE_THROWS_AS(book.insert_limit_order(1, 10000, 0, fathom::Side::Buy), std::invalid_argument);
    REQUIRE_THROWS_AS(book.insert_limit_order(1, 10000, -5, fathom::Side::Buy), std::invalid_argument);
}

TEST_CASE("insert_limit_order rejects zero or negative price") {
    fathom::OrderBook book;
    REQUIRE_THROWS_AS(book.insert_limit_order(1, 0, 5, fathom::Side::Buy), std::invalid_argument);
    REQUIRE_THROWS_AS(book.insert_limit_order(1, -100, 5, fathom::Side::Buy), std::invalid_argument);
}

TEST_CASE("order reduced to exactly zero via multiple partial cancellations is fully removed") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 10, fathom::Side::Buy);
    book.reduce_order_qty(1, 4);   // 10 -> 6
    book.reduce_order_qty(1, 6);   // 6 -> 0, should be fully removed
    REQUIRE_FALSE(book.best_bid().has_value());

    // a subsequent reduce on the now-gone order should be a safe no-op, not a crash
    book.reduce_order_qty(1, 1);
    REQUIRE_FALSE(book.best_bid().has_value());
}

TEST_CASE("reduce_order_qty amount exceeding remaining quantity does not go negative or crash") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.reduce_order_qty(1, 100);   // way more than the 5 available
    REQUIRE_FALSE(book.best_bid().has_value());
    REQUIRE(book.depth_at_price(10000, fathom::Side::Buy) == 0);
}

TEST_CASE("level-cap eviction racing with a same-price insert keeps the book consistent") {
    fathom::OrderBook book(2);  // cap at 2 levels
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.insert_limit_order(2, 10010, 5, fathom::Side::Buy);
    // now at cap (2 levels). Insert a THIRD order at an EXISTING price -- should not
    // trigger eviction, since no new price level is being created
    book.insert_limit_order(3, 10010, 5, fathom::Side::Buy);
    REQUIRE(book.depth_at_price(10010, fathom::Side::Buy) == 10);  // 5 + 5, no eviction
    REQUIRE(book.depth_at_price(10000, fathom::Side::Buy) == 5);   // untouched
}

TEST_CASE("self-crossing orders at the exact same price on both sides match fully") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Sell);
    book.insert_limit_order(2, 10000, 5, fathom::Side::Buy);  // exact same price, opposite side
    REQUIRE_FALSE(book.best_bid().has_value());
    REQUIRE_FALSE(book.best_ask().has_value());
}

TEST_CASE("marketable order that partially crosses multiple levels then rests the remainder at its own limit price") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 3, fathom::Side::Sell);
    book.insert_limit_order(2, 10010, 3, fathom::Side::Sell);
    // buy order crosses both levels (6 total) but wants 10 -- remaining 4 should
    // rest at ITS OWN limit price (10020), not at 10010 (the last level it touched)
    book.insert_limit_order(3, 10020, 10, fathom::Side::Buy);

    REQUIRE_FALSE(book.best_ask().has_value());          // both ask levels fully consumed
    REQUIRE(book.best_bid().value() == 10020);            // remainder rests at the order's own limit
    REQUIRE(book.depth_at_price(10020, fathom::Side::Buy) == 4);  // 10 - 3 - 3
}

TEST_CASE("cancelling an order twice in a row is a safe no-op the second time") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.cancel_order(1);
    book.cancel_order(1);  // already gone, should not crash or affect anything
    REQUIRE_FALSE(book.best_bid().has_value());
}

TEST_CASE("match_market_order on a completely empty book returns no fills and does not crash") {
    fathom::OrderBook book;
    auto fills = book.match_market_order(100, fathom::Side::Buy);
    REQUIRE(fills.empty());
}

TEST_CASE("depth_at_price for a price level that was never used returns zero") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    REQUIRE(book.depth_at_price(99999, fathom::Side::Buy) == 0);
}

TEST_CASE("aggregate_depth sums quantity across the requested number of best price levels") {
    fathom::OrderBook book;
    book.insert_limit_order(1, 10000, 5, fathom::Side::Buy);
    book.insert_limit_order(2, 9990, 3, fathom::Side::Buy);
    book.insert_limit_order(3, 9980, 7, fathom::Side::Buy);

    REQUIRE(book.aggregate_depth(fathom::Side::Buy, 1) == 5);
    REQUIRE(book.aggregate_depth(fathom::Side::Buy, 2) == 8);
    REQUIRE(book.aggregate_depth(fathom::Side::Buy, 10) == 15);  // more levels requested than exist
}