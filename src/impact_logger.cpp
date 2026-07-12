#include "impact_logger.hpp"
#include "replay.hpp"
#include <fstream>
#include <stdexcept>

size_t log_trade_impact_features(const std::vector<LobsterMessage>& messages,
                                  int level,
                                  int horizon_messages,
                                  const std::string& output_path) {
    if (horizon_messages <= 0) {
        throw std::invalid_argument("horizon_messages must be positive");
    }

    fathom::OrderBook book(level);
    std::vector<double> mid_price_series;
    mid_price_series.reserve(messages.size());

    std::vector<TradeFeatureRow> rows;

    for (size_t i = 0; i < messages.size(); ++i) {
        const auto& msg = messages[i];

        // capture pre-trade features BEFORE applying an execution message
        if (msg.type == 4) {
            auto bid = book.best_bid();
            auto ask = book.best_ask();
            if (bid.has_value() && ask.has_value()) {
                TradeFeatureRow row;
                row.time = msg.time;
                row.message_index = i;
                row.trade_qty = msg.size;
                row.direction = msg.direction;
                row.best_bid = *bid;
                row.best_ask = *ask;
                row.spread = *ask - *bid;
                row.bid_depth_top3 = book.aggregate_depth(fathom::Side::Buy, 3);
                row.ask_depth_top3 = book.aggregate_depth(fathom::Side::Sell, 3);

                fathom::Qty total_depth = row.bid_depth_top3 + row.ask_depth_top3;
                row.imbalance = (total_depth > 0)
                    ? static_cast<double>(row.bid_depth_top3 - row.ask_depth_top3) / total_depth
                    : 0.0;

                row.pre_trade_mid = (*bid + *ask) / 2.0;
                row.label_price_move = 0.0;  // filled in on the second pass below

                rows.push_back(row);
            }
        }

        apply_lobster_message(book, msg);

        auto bid = book.best_bid();
        auto ask = book.best_ask();
        double mid = (bid.has_value() && ask.has_value()) ? (*bid + *ask) / 2.0 : mid_price_series.empty() ? 0.0 : mid_price_series.back();
        mid_price_series.push_back(mid);
    }

    // second pass: fill in the forward-looking label now that the full mid-price series exists
    size_t written = 0;
    std::ofstream out(output_path);
    if (!out.is_open()) {
        throw std::runtime_error("could not open output file: " + output_path);
    }
    out << "time,message_index,trade_qty,direction,best_bid,best_ask,spread,"
           "bid_depth_top3,ask_depth_top3,imbalance,pre_trade_mid,label_price_move\n";

    for (auto& row : rows) {
        size_t future_index = row.message_index + horizon_messages;
        if (future_index >= mid_price_series.size()) continue;  // too close to the end, skip

        row.label_price_move = mid_price_series[future_index] - row.pre_trade_mid;

        out << row.time << "," << row.message_index << "," << row.trade_qty << ","
            << row.direction << "," << row.best_bid << "," << row.best_ask << ","
            << row.spread << "," << row.bid_depth_top3 << "," << row.ask_depth_top3 << ","
            << row.imbalance << "," << row.pre_trade_mid << "," << row.label_price_move << "\n";
        written++;
    }

    return written;
}