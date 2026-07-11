#pragma once
#include "strategy.hpp"
#include <iostream>

// Places a single resting buy order once, then does nothing else.
// This exists purely to prove the injection/fill plumbing works end to end.
class PassiveQuoteStrategy : public fathom::Strategy {
public:
    std::vector<fathom::StrategyAction> on_book_update(const fathom::OrderBook& book) override {
        std::vector<fathom::StrategyAction> actions;
        if (!placed_) {
            auto bid = book.best_bid();
            if (bid.has_value()) {
                actions.push_back({fathom::ActionType::PlaceLimit, 0, *bid, 10, fathom::Side::Buy});
                placed_ = true;
            }
        }
        return actions;
    }

    void on_fill(const fathom::Fill& fill) override {
        fill_count_++;
        std::cout << "STRATEGY FILL #" << fill_count_
                  << " qty=" << fill.qty << " price=" << fill.price << "\n";
    }

    int fill_count() const { return fill_count_; }

private:
    bool placed_ = false;
    int fill_count_ = 0;
};