#pragma once
#include "order_book.hpp"
#include <unordered_map>
#include <vector>

struct PlacementRecord {
    double time_placed;
    fathom::Price price_placed;
    fathom::Qty   qty_placed;  
    fathom::Price mid_at_placement;
};

struct FillRecord {
    double time_filled;
    fathom::Qty qty;
    fathom::Price fill_price;
    double time_to_fill;              // time_filled - time_placed
    fathom::Price slippage_ticks;     // fill_price - mid_at_placement (signed)
};

class MetricsTracker {
public:
    void record_placement(fathom::OrderId id, double time, fathom::Price price, fathom::Qty qty, fathom::Price mid);
    void record_fill(fathom::OrderId maker_id, double time, fathom::Qty qty, fathom::Price fill_price);

    size_t total_placements() const { return placements_.size(); }
    size_t total_fills() const { return fills_.size(); }
    double fill_rate() const;              // fraction of placed orders that got at least one fill
    double average_time_to_fill() const;   // seconds, averaged across all individual fills
    double average_slippage_ticks() const; // averaged across all individual fills

    const std::vector<FillRecord>& fills() const { return fills_; }

private:
    std::unordered_map<fathom::OrderId, PlacementRecord> placements_;
    std::vector<FillRecord> fills_;
};