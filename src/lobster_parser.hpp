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