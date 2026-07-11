#include "lobster_parser.hpp"
#include "replay.hpp"
#include "order_book.hpp"
#include "strategies/passive_quote_strategy.hpp"
#include <iostream>

int main() {
    auto messages = parse_lobster_messages(
        "data/AMZN_2012-06-21_34200000_57600000_message_10.csv");

    std::cout << "loaded " << messages.size() << " messages\n";

    fathom::OrderBook book(10);
    PassiveQuoteStrategy strategy;

    replay_with_strategy(book, messages, strategy);

    std::cout << "total fills: " << strategy.fill_count() << "\n";
    return 0;
}