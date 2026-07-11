#pragma once
#include <vector>
#include <string>
#include "order_book.hpp"   // for consistency if you want to reuse types, optional here

struct LobsterMessage {
    double  time;
    int     type;
    int64_t order_id;
    int64_t size;
    int64_t price;
    int     direction;
};

std::vector<LobsterMessage> parse_lobster_messages(const std::string& path);

struct LobsterBookSnapshot {
    std::vector<fathom::Price> ask_prices;   // index 0 = level 1 (best), index 1 = level 2, etc.
    std::vector<fathom::Qty>   ask_sizes;
    std::vector<fathom::Price> bid_prices;
    std::vector<fathom::Qty>   bid_sizes;
};

std::vector<LobsterBookSnapshot> parse_lobster_orderbook(const std::string& path, int num_levels);