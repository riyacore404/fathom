#include "order_book.hpp"
#include <stdexcept>

namespace fathom {

void OrderBook::insert_limit_order(OrderId id, Price price, Qty qty, Side side, bool is_strategy_order) {
    if (qty <= 0) {
        throw std::invalid_argument("insert_limit_order: qty must be positive, got " + std::to_string(qty));
    }
    if (price <= 0) {
        throw std::invalid_argument("insert_limit_order: price must be positive, got " + std::to_string(price));
    }
    
    if (side == Side::Buy) {
        auto it = asks_.begin();
        while (qty > 0 && it != asks_.end() && price >= it->first) {
            auto& dq = it->second.orders;
            while (qty > 0 && !dq.empty()) {
                Order& resting = dq.front();
                Qty traded = std::min(qty, resting.qty);
                resting.qty -= traded;
                qty -= traded;

                if (resting.is_strategy_order && fill_callback_) {
                    fill_callback_(Fill{id, resting.id, it->first, traded});
                }

                if (resting.qty == 0) {
                    order_index_.erase(resting.id);
                    dq.pop_front();
                }
            }
            if (dq.empty()) it = asks_.erase(it);
            else ++it;
        }
    } else {
        auto it = bids_.begin();
        while (qty > 0 && it != bids_.end() && price <= it->first) {
            auto& dq = it->second.orders;
            while (qty > 0 && !dq.empty()) {
                Order& resting = dq.front();
                Qty traded = std::min(qty, resting.qty);
                resting.qty -= traded;
                qty -= traded;

                if (resting.is_strategy_order && fill_callback_) {
                    fill_callback_(Fill{id, resting.id, it->first, traded});
                }

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
        Order o{id, price, qty, side, is_strategy_order};
        if (side == Side::Buy) bids_[price].orders.push_back(o);
        else asks_[price].orders.push_back(o);
        order_index_[id] = OrderLocation{price, side};
    }

    enforce_level_cap();
}

void OrderBook::set_fill_callback(std::function<void(const Fill&)> cb) {
    fill_callback_ = std::move(cb);
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
    if (it == order_index_.end()) return;

    auto [price, side] = it->second;

    if (side == Side::Buy) {
        auto level_it = bids_.find(price);
        if (level_it == bids_.end()) return;
        auto& dq = level_it->second.orders;
        for (auto& o : dq) {
            if (o.id == id) {
                Qty traded = std::min(amount, o.qty);
                o.qty -= amount;
                if (o.is_strategy_order && fill_callback_) {
                    fill_callback_(Fill{0, o.id, price, traded});
                }
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
                Qty traded = std::min(amount, o.qty);
                o.qty -= amount;
                if (o.is_strategy_order && fill_callback_) {
                    fill_callback_(Fill{0, o.id, price, traded});
                }
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

OrderBook::OrderBook(size_t max_levels) : max_levels_(max_levels) {
    if (max_levels == 0) {
        throw std::invalid_argument("OrderBook: max_levels must be at least 1");
    }
}

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