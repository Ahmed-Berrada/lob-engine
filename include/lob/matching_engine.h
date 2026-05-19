#pragma once

#include "order_book.h"
#include <chrono>

namespace lob {

// Thin wrapper around OrderBook — could manage multiple symbols later
class MatchingEngine {
public:
    MatchingEngine() = default;

    std::vector<Trade> submit_order(OrderId id, Side side, Price price, Quantity qty) {
        auto ts = now();
        return book_.add_order(id, side, price, qty, ts);
    }

    bool cancel_order(OrderId id) {
        return book_.cancel_order(id);
    }

    std::vector<Trade> modify_order(OrderId id, Price new_price, Quantity new_qty) {
        auto ts = now();
        return book_.modify_order(id, new_price, new_qty, ts);
    }

    Price best_bid() const { return book_.best_bid(); }
    Price best_ask() const { return book_.best_ask(); }
    Price spread() const { return book_.spread(); }
    size_t order_count() const { return book_.order_count(); }
    bool empty() const { return book_.empty(); }

    const OrderBook& book() const { return book_; }

private:
    static Timestamp now() {
        return static_cast<Timestamp>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }

    OrderBook book_;
};

} // namespace lob
