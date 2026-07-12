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

    for (const auto& msg : messages) {
        apply_lobster_message(book, msg);
        strategy.set_current_time(msg.time);

        auto actions = strategy.on_book_update(book);
        for (const auto& action : actions) {
            if (action.type == fathom::ActionType::PlaceLimit) {
                book.insert_limit_order(action.order_id, action.price, action.qty,
                                         action.side, /*is_strategy_order=*/true);
            } else if (action.type == fathom::ActionType::Cancel) {
                book.cancel_order(action.order_id);
            }
        }
    }
}