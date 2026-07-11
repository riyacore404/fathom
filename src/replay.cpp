#include "replay.hpp"

void replay_messages(fathom::OrderBook& book, const std::vector<LobsterMessage>& messages) {
    for (const auto& msg : messages) {
        fathom::Side side = (msg.direction == 1) ? fathom::Side::Buy : fathom::Side::Sell;

        switch (msg.type) {
            case 1:  // new limit order
                book.insert_limit_order(msg.order_id, msg.price, msg.size, side);
                break;
            case 2:  // partial cancellation
                book.reduce_order_qty(msg.order_id, msg.size);
                break;
            case 3:  // full deletion
                book.cancel_order(msg.order_id);
                break;
            case 4:  // visible execution
                book.reduce_order_qty(msg.order_id, msg.size);
                break;
            case 5:  // hidden execution — no resting order to update, skip
                break;
            case 7:  // trading halt indicator — no book state change
                break;
            default:
                break;  // unknown type, ignore rather than crash
        }
    }
}