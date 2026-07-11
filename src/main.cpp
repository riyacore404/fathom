#include "lobster_parser.hpp"
#include "order_book.hpp"
#include <iostream>

int main() {
    auto messages = parse_lobster_messages(
        "data/AMZN_2012-06-21_34200000_57600000_message_10.csv");

    std::cout << "loaded " << messages.size() << " messages (expect 269748 for level 10)\n";

    fathom::OrderBook book(10);  

    for (size_t i = 0; i < messages.size(); ++i) {
        const auto& msg = messages[i];
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

        auto bid = book.best_bid();
        auto ask = book.best_ask();

        if (bid.has_value() && ask.has_value() && *bid >= *ask) {
            std::cout << "CROSSED BOOK detected at message index " << i << "\n";
            std::cout << "  time=" << msg.time
                      << " type=" << msg.type
                      << " order_id=" << msg.order_id
                      << " size=" << msg.size
                      << " price=" << msg.price
                      << " direction=" << msg.direction << "\n";
            std::cout << "  best_bid=" << *bid << " best_ask=" << *ask << "\n";
            return 0;
        }
    }

    std::cout << "no crossing detected across all " << messages.size() << " messages\n";
    return 0;
}