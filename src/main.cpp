#include "lobster_parser.hpp"
#include "replay.hpp"
#include "order_book.hpp"
#include "strategies/passive_quote_strategy.hpp"
#include <iostream>
#include <chrono>

int main() {
    try {
        auto messages = parse_lobster_messages(
            "data/AMZN_2012-06-21_34200000_57600000_message_10.csv");

        std::cout << "loaded " << messages.size() << " messages\n";

        fathom::OrderBook book(10);
        PassiveQuoteStrategy strategy;

        auto start = std::chrono::steady_clock::now();
        replay_with_strategy(book, messages, strategy);
        auto end = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << "\nreplay took " << elapsed_ms << " ms for " << messages.size() << " messages\n";

        const auto& m = strategy.metrics();
        std::cout << "\n--- METRICS ---\n";
        std::cout << "total placements: " << m.total_placements() << "\n";
        std::cout << "total fills: " << m.total_fills() << "\n";
        std::cout << "fill rate: " << (m.fill_rate() * 100.0) << "%\n";
        std::cout << "average time-to-fill: " << m.average_time_to_fill() << " seconds\n";
        std::cout << "average slippage: " << m.average_slippage_ticks() << " ticks\n";

    } catch (const std::exception& e) {
        std::cerr << "fathom: fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}