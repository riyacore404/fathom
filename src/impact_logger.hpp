#pragma once
#include "order_book.hpp"
#include "lobster_parser.hpp"
#include <vector>
#include <string>

struct TradeFeatureRow {
    double time;
    size_t message_index;
    fathom::Qty trade_qty;
    int direction;              // 1 = resting buy order was hit, -1 = resting sell order was hit
    fathom::Price best_bid;
    fathom::Price best_ask;
    fathom::Price spread;
    fathom::Qty bid_depth_top3;
    fathom::Qty ask_depth_top3;
    double imbalance;           // (bid_depth - ask_depth) / (bid_depth + ask_depth), range [-1, 1]
    double pre_trade_mid;
    double label_price_move;    // future_mid - pre_trade_mid, over a fixed message-count horizon
};

// Replays messages through a fresh OrderBook, capturing book-state features before every
// real execution (type 4) and a forward-looking price-move label computed a fixed number
// of messages later. Writes the result to a CSV at output_path. Returns the row count written.
size_t log_trade_impact_features(const std::vector<LobsterMessage>& messages,
                                  int level,
                                  int horizon_messages,
                                  const std::string& output_path);