#pragma once
#include "order_book.hpp"
#include <vector>

namespace fathom {

enum class ActionType { PlaceLimit, Cancel };

struct StrategyAction {
    ActionType type;
    OrderId    order_id;   // for Cancel: the id to cancel. for PlaceLimit: the new order's id
    Price      price;      // ignored for Cancel
    Qty        qty;        // ignored for Cancel
    Side       side;       // ignored for Cancel
};

class Strategy {
public:
    virtual ~Strategy() = default;
    virtual std::vector<StrategyAction> on_book_update(const OrderBook& book) = 0;
    virtual void on_fill(const Fill& fill) = 0;
    virtual void set_current_time(double time) {}  
};

} // namespace fathom