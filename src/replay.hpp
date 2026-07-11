#pragma once
#include "order_book.hpp"
#include "lobster_parser.hpp"
#include "strategy.hpp"
#include <vector>

void apply_lobster_message(fathom::OrderBook& book, const LobsterMessage& msg);
void replay_messages(fathom::OrderBook& book, const std::vector<LobsterMessage>& messages);

// replays historical messages while injecting a strategy's own orders into the same book
void replay_with_strategy(fathom::OrderBook& book,
                           const std::vector<LobsterMessage>& messages,
                           fathom::Strategy& strategy);