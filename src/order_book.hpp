#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>
#include <optional>
#include <vector>
#include <algorithm>   
#include <functional>
#include <iterator> 

namespace fathom {

using Price = int64_t;
using Qty   = int64_t;
using OrderId = uint64_t;

enum class Side { Buy, Sell };

struct Order {
    OrderId id;
    Price   price;
    Qty     qty;
    Side    side;
    bool    is_strategy_order = false; 
};

struct Fill {
    OrderId taker_id;
    OrderId maker_id;
    Price   price;
    Qty     qty;
};

struct PriceLevel {
    std::deque<Order> orders;

    Qty total_qty() const {
        Qty sum = 0;
        for (const auto& o : orders) sum += o.qty;
        return sum;
    }
};

class OrderBook {
public:
    explicit OrderBook(size_t max_levels = 10);   // default matches our level-10 data

    void insert_limit_order(OrderId id, Price price, Qty qty, Side side, bool is_strategy_order = false);
    void set_fill_callback(std::function<void(const Fill&)> cb);
    void cancel_order(OrderId id);
    void reduce_order_qty(OrderId id, Qty amount);
    std::vector<Fill> match_market_order(Qty qty, Side side);

    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    Qty depth_at_price(Price price, Side side) const;
    Qty aggregate_depth(Side side, int levels) const;

private:
    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    std::map<Price, PriceLevel> asks_;

    struct OrderLocation { Price price; Side side; };
    std::unordered_map<OrderId, OrderLocation> order_index_;
    std::function<void(const Fill&)> fill_callback_;

    size_t max_levels_;
    void enforce_level_cap();
};

} // namespace fathom