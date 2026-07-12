#pragma once
#include "strategy.hpp"
#include "metrics.hpp"
#include <iostream>

class PassiveQuoteStrategy : public fathom::Strategy {
public:
    std::vector<fathom::StrategyAction> on_book_update(const fathom::OrderBook& book) override {
        std::vector<fathom::StrategyAction> actions;
        current_time_ = last_seen_time_;  // updated externally, see main.cpp

        if (!placed_) {
            auto bid = book.best_bid();
            auto ask = book.best_ask();
            if (bid.has_value() && ask.has_value()) {
                fathom::OrderId id = next_id_++;
                fathom::Qty qty = 10;
                fathom::Price mid = (*bid + *ask) / 2;

                actions.push_back({fathom::ActionType::PlaceLimit, id, *bid, qty, fathom::Side::Buy});
                metrics_.record_placement(id, current_time_, *bid, qty, mid);
                placed_ = true;
            }
        }
        return actions;
    }

    void on_fill(const fathom::Fill& fill) override {
        metrics_.record_fill(fill.maker_id, current_time_, fill.qty, fill.price);
        std::cout << "STRATEGY FILL qty=" << fill.qty << " price=" << fill.price << "\n";
    }

    void set_current_time(double t) override { last_seen_time_ = t; }
    const MetricsTracker& metrics() const { return metrics_; }

private:
    bool placed_ = false;
    double current_time_ = 0.0;
    double last_seen_time_ = 0.0;
    fathom::OrderId next_id_ = 1'000'000'000'000ULL;  // stays clear of real LOBSTER ids
    MetricsTracker metrics_;
};