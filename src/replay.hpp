#pragma once
#include "order_book.hpp"
#include "lobster_parser.hpp"
#include <vector>

void replay_messages(fathom::OrderBook& book, const std::vector<LobsterMessage>& messages);