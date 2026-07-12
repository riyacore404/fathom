#include "lobster_parser.hpp"
#include "replay.hpp"
#include "order_book.hpp"
#include "config.hpp"
#include "strategies/passive_quote_strategy.hpp"
#include <iostream>
#include <chrono>
#include <string>

Config parse_args(int argc, char** argv) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ticker" && i + 1 < argc) cfg.ticker = argv[++i];
        else if (arg == "--date" && i + 1 < argc) cfg.date = argv[++i];
        else if (arg == "--level" && i + 1 < argc) cfg.level = std::stoi(argv[++i]);
        else if (arg == "--data-dir" && i + 1 < argc) cfg.data_dir = argv[++i];
        else if (arg == "--help") {
            std::cout << "Usage: fathom [--ticker SYM] [--date YYYY-MM-DD] "
                         "[--level N] [--data-dir PATH]\n"
                      << "Defaults: --ticker AMZN --date 2012-06-21 --level 10 --data-dir data\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("unrecognized argument: " + arg);
        }
    }
    return cfg;
}

int main(int argc, char** argv) {
    try {
        Config cfg = parse_args(argc, argv);
        std::string path = message_file_path(cfg);   // <-- now comes from config.hpp/config.cpp

        std::cout << "loading: " << path << "\n";
        auto messages = parse_lobster_messages(path);
        std::cout << "loaded " << messages.size() << " messages\n";

        fathom::OrderBook book(cfg.level);
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