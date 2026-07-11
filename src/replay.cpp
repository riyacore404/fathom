#include "replay.hpp"

void apply_lobster_message(fathom::OrderBook& book, const LobsterMessage& msg) {
    fathom::Side side = (msg.direction == 1) ? fathom::Side::Buy : fathom::Side::Sell;
    switch (msg.type) {
        case 1: book.insert_limit_order(msg.order_id, msg.price, msg.size, side); break;
        case 2: book.reduce_order_qty(msg.order_id, msg.size); break;
        case 3: book.cancel_order(msg.order_id); break;
        case 4: book.reduce_order_qty(msg.order_id, msg.size); break;
        case 5: break;
        case 7: break;
        default: break;
    }
}

void replay_messages(fathom::OrderBook& book, const std::vector<LobsterMessage>& messages) {
    for (const auto& msg : messages) {
        apply_lobster_message(book, msg);
    }
}

void replay_with_strategy(fathom::OrderBook& book,
                           const std::vector<LobsterMessage>& messages,
                           fathom::Strategy& strategy) {
    book.set_fill_callback([&strategy](const fathom::Fill& f) { strategy.on_fill(f); });

    // strategy order ids live in a namespace far above LOBSTER's real order ids
    // (which run in the tens of millions in this dataset) so they never collide
    fathom::OrderId next_strategy_id = 1'000'000'000'000ULL;

    for (const auto& msg : messages) {
        apply_lobster_message(book, msg);

        auto actions = strategy.on_book_update(book);
        for (const auto& action : actions) {
            if (action.type == fathom::ActionType::PlaceLimit) {
                book.insert_limit_order(next_strategy_id++, action.price, action.qty,
                                         action.side, /*is_strategy_order=*/true);
            } else if (action.type == fathom::ActionType::Cancel) {
                book.cancel_order(action.order_id);
            }
        }
    }
}