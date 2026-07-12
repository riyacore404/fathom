#pragma once
#include "strategy.hpp"
#include "metrics.hpp"
#include <iostream>

// Models the naive backtesting assumption: a desired trade fills completely
// and instantly at the current best ask (crossing the spread), with zero
// queue wait and zero execution risk. This deliberately does NOT place a
// real order into the book -- it's a hypothetical, not a real market action.
class NaiveInstantFillStrategy : public fathom::Strategy {
public:
    std::vector<fathom::StrategyAction> on_book_update(const fathom::OrderBook& book) override {
        if (!placed_) {
            auto bid = book.best_bid();
            auto ask = book.best_ask();
            if (bid.has_value() && ask.has_value()) {
                fathom::OrderId synthetic_id = 1;
                fathom::Qty qty = 10;
                fathom::Price mid = (*bid + *ask) / 2;

                // record the "placement" and an instantaneous full fill at the ask --
                // no action returned, since nothing actually touches the real book
                metrics_.record_placement(synthetic_id, current_time_, *ask, qty, mid);
                metrics_.record_fill(synthetic_id, current_time_, qty, *ask);

                std::cout << "NAIVE FILL (instant, hypothetical) qty=" << qty
                          << " price=" << *ask << "\n";
                placed_ = true;
            }
        }
        return {};  // never returns a real action -- this strategy never touches the book
    }

    void on_fill(const fathom::Fill&) override {
        // never called -- this strategy never places a real order, so the book
        // never generates a real Fill callback for it
    }

    void set_current_time(double t) override { current_time_ = t; }
    const MetricsTracker& metrics() const { return metrics_; }

private:
    bool placed_ = false;
    double current_time_ = 0.0;
    MetricsTracker metrics_;
};