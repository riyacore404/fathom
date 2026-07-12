#include "lobster_parser.hpp"
#include "replay.hpp"
#include "order_book.hpp"
#include "config.hpp"
#include "strategies/passive_quote_strategy.hpp"
#include "strategies/naive_instant_fill_strategy.hpp"
#include <iostream>
#include <chrono>
#include <string>
#include <iomanip>

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

void print_metrics_row(const std::string& label, const MetricsTracker& m) {
    std::cout << std::left << std::setw(12) << label
              << " | fills: " << std::setw(3) << m.total_fills()
              << " | fill rate: " << std::setw(6) << (m.fill_rate() * 100.0) << "%"
              << " | avg time-to-fill: " << std::setw(8) << m.average_time_to_fill() << "s"
              << " | avg slippage vs mid: " << m.average_slippage_ticks() << " ticks\n";
}

int main(int argc, char** argv) {
    try {
        Config cfg = parse_args(argc, argv);
        std::string path = message_file_path(cfg);

        std::cout << "loading: " << path << "\n";
        auto messages = parse_lobster_messages(path);
        std::cout << "loaded " << messages.size() << " messages\n\n";

        // --- Realistic: passive resting order, real queue-position replay ---
        fathom::OrderBook realistic_book(cfg.level);
        PassiveQuoteStrategy realistic_strategy;
        replay_with_strategy(realistic_book, messages, realistic_strategy);

        // --- Naive: hypothetical instant fill at the ask, no real book interaction ---
        fathom::OrderBook naive_book(cfg.level);
        NaiveInstantFillStrategy naive_strategy;
        replay_with_strategy(naive_book, messages, naive_strategy);

        std::cout << "\n=== NAIVE vs REALISTIC EXECUTION COMPARISON ===\n";
        std::cout << "(same intended trade: buy 10 shares, same historical data)\n\n";
        print_metrics_row("Naive", naive_strategy.metrics());
        print_metrics_row("Realistic", realistic_strategy.metrics());

        const auto& real_m = realistic_strategy.metrics();
        const auto& naive_m = naive_strategy.metrics();

        std::cout << "\nPrice improvement from resting passively instead of crossing the spread: "
                  << (naive_m.average_slippage_ticks() - real_m.average_slippage_ticks())
                  << " ticks better execution price\n";
        std::cout << "Cost of that improvement: "
                  << real_m.average_time_to_fill() << " seconds of queue-wait risk "
                  << "(vs. 0s guaranteed instant fill for naive)\n";

    } catch (const std::exception& e) {
        std::cerr << "fathom: fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}