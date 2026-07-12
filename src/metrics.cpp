#include "metrics.hpp"
#include <numeric>

void MetricsTracker::record_placement(fathom::OrderId id, double time, fathom::Price price, fathom::Qty qty, fathom::Price mid) {
    placements_[id] = PlacementRecord{time, price, qty, mid};
}

void MetricsTracker::record_fill(fathom::OrderId maker_id, double time, fathom::Qty qty, fathom::Price fill_price) {
    auto it = placements_.find(maker_id);
    if (it == placements_.end()) return;  // fill for an order we never tracked placement of, ignore

    const auto& placement = it->second;
    FillRecord rec;
    rec.time_filled     = time;
    rec.qty              = qty;
    rec.fill_price        = fill_price;
    rec.time_to_fill      = time - placement.time_placed;
    rec.slippage_ticks    = fill_price - placement.mid_at_placement;

    fills_.push_back(rec);
}

double MetricsTracker::fill_rate() const {
    fathom::Qty total_placed = 0;
    for (const auto& [id, p] : placements_) total_placed += p.qty_placed;

    fathom::Qty total_filled = 0;
    for (const auto& f : fills_) total_filled += f.qty;

    if (total_placed == 0) return 0.0;
    return static_cast<double>(total_filled) / static_cast<double>(total_placed);
}

double MetricsTracker::average_time_to_fill() const {
    if (fills_.empty()) return 0.0;
    double sum = 0.0;
    for (const auto& f : fills_) sum += f.time_to_fill;
    return sum / fills_.size();
}

double MetricsTracker::average_slippage_ticks() const {
    if (fills_.empty()) return 0.0;
    double sum = 0.0;
    for (const auto& f : fills_) sum += f.slippage_ticks;
    return sum / fills_.size();
}