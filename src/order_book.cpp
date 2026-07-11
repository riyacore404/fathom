#include "order_book.hpp"

namespace fathom {

void OrderBook::insert_limit_order(OrderId id, Price price, Qty qty, Side side) {
    if (side == Side::Buy) {
        // marketable if our buy price >= the current best ask
        auto it = asks_.begin();
        while (qty > 0 && it != asks_.end() && price >= it->first) {
            auto& dq = it->second.orders;
            while (qty > 0 && !dq.empty()) {
                Order& resting = dq.front();
                Qty traded = std::min(qty, resting.qty);
                resting.qty -= traded;
                qty -= traded;
                if (resting.qty == 0) {
                    order_index_.erase(resting.id);
                    dq.pop_front();
                }
            }
            if (dq.empty()) it = asks_.erase(it);
            else ++it;
        }
    } else {
        // marketable if our sell price <= the current best bid
        auto it = bids_.begin();
        while (qty > 0 && it != bids_.end() && price <= it->first) {
            auto& dq = it->second.orders;
            while (qty > 0 && !dq.empty()) {
                Order& resting = dq.front();
                Qty traded = std::min(qty, resting.qty);
                resting.qty -= traded;
                qty -= traded;
                if (resting.qty == 0) {
                    order_index_.erase(resting.id);
                    dq.pop_front();
                }
            }
            if (dq.empty()) it = bids_.erase(it);
            else ++it;
        }
    }

    if (qty > 0) {
        Order o{id, price, qty, side};
        if (side == Side::Buy) {
            bids_[price].orders.push_back(o);
        } else {
            asks_[price].orders.push_back(o);
        }
        order_index_[id] = OrderLocation{price, side};
    }

    enforce_level_cap();
}

void OrderBook::cancel_order(OrderId id) {
    auto it = order_index_.find(id);
    if (it == order_index_.end()) return;

    auto [price, side] = it->second;

    if (side == Side::Buy) {
        auto level_it = bids_.find(price);
        if (level_it == bids_.end()) return;
        auto& dq = level_it->second.orders;
        dq.erase(std::remove_if(dq.begin(), dq.end(),
                                 [id](const Order& o) { return o.id == id; }),
                 dq.end());
        if (dq.empty()) bids_.erase(level_it);
    } else {
        auto level_it = asks_.find(price);
        if (level_it == asks_.end()) return;
        auto& dq = level_it->second.orders;
        dq.erase(std::remove_if(dq.begin(), dq.end(),
                                 [id](const Order& o) { return o.id == id; }),
                 dq.end());
        if (dq.empty()) asks_.erase(level_it);
    }

    order_index_.erase(it);
}

std::vector<Fill> OrderBook::match_market_order(Qty qty, Side side) {
    std::vector<Fill> fills;

    if (side == Side::Buy) {
        // a buy market order matches against resting asks
        auto it = asks_.begin();
        while (qty > 0 && it != asks_.end()) {
            auto& dq = it->second.orders;
            while (qty > 0 && !dq.empty()) {
                Order& resting = dq.front();
                Qty traded = std::min(qty, resting.qty);

                fills.push_back(Fill{0, resting.id, it->first, traded});

                resting.qty -= traded;
                qty -= traded;

                if (resting.qty == 0) {
                    order_index_.erase(resting.id);
                    dq.pop_front();
                }
            }
            if (dq.empty()) it = asks_.erase(it);
            else ++it;
        }
    } else {
        // a sell market order matches against resting bids
        auto it = bids_.begin();
        while (qty > 0 && it != bids_.end()) {
            auto& dq = it->second.orders;
            while (qty > 0 && !dq.empty()) {
                Order& resting = dq.front();
                Qty traded = std::min(qty, resting.qty);

                fills.push_back(Fill{0, resting.id, it->first, traded});

                resting.qty -= traded;
                qty -= traded;

                if (resting.qty == 0) {
                    order_index_.erase(resting.id);
                    dq.pop_front();
                }
            }
            if (dq.empty()) it = bids_.erase(it);
            else ++it;
        }
    }

    return fills;
}

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

Qty OrderBook::depth_at_price(Price price, Side side) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it == bids_.end()) return 0;
        return it->second.total_qty();
    } else {
        auto it = asks_.find(price);
        if (it == asks_.end()) return 0;
        return it->second.total_qty();
    }
}

void OrderBook::reduce_order_qty(OrderId id, Qty amount) {
    auto it = order_index_.find(id);
    if (it == order_index_.end()) return;  // unknown id, ignore

    auto [price, side] = it->second;

    if (side == Side::Buy) {
        auto level_it = bids_.find(price);
        if (level_it == bids_.end()) return;
        auto& dq = level_it->second.orders;
        for (auto& o : dq) {
            if (o.id == id) {
                o.qty -= amount;
                if (o.qty <= 0) {
                    dq.erase(std::remove_if(dq.begin(), dq.end(),
                                             [id](const Order& x) { return x.id == id; }),
                             dq.end());
                    order_index_.erase(it);
                    if (dq.empty()) bids_.erase(level_it);
                }
                return;
            }
        }
    } else {
        auto level_it = asks_.find(price);
        if (level_it == asks_.end()) return;
        auto& dq = level_it->second.orders;
        for (auto& o : dq) {
            if (o.id == id) {
                o.qty -= amount;
                if (o.qty <= 0) {
                    dq.erase(std::remove_if(dq.begin(), dq.end(),
                                             [id](const Order& x) { return x.id == id; }),
                             dq.end());
                    order_index_.erase(it);
                    if (dq.empty()) asks_.erase(level_it);
                }
                return;
            }
        }
    }
}

OrderBook::OrderBook(size_t max_levels) : max_levels_(max_levels) {}

void OrderBook::enforce_level_cap() {
    // bids_ is sorted best-to-worst (highest price first), so the worst
    // level is always the LAST one in iteration order.
    while (bids_.size() > max_levels_) {
        auto worst = std::prev(bids_.end());
        for (const auto& o : worst->second.orders) {
            order_index_.erase(o.id);   // stop tracking these orders entirely
        }
        bids_.erase(worst);
    }

    // asks_ is sorted best-to-worst (lowest price first), so the worst
    // level is also always the LAST one in iteration order.
    while (asks_.size() > max_levels_) {
        auto worst = std::prev(asks_.end());
        for (const auto& o : worst->second.orders) {
            order_index_.erase(o.id);
        }
        asks_.erase(worst);
    }
}

} // namespace fathom