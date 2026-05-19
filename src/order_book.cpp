#include "lob/order_book.h"
#include <limits>

namespace lob {

OrderBook::OrderBook() {}
OrderBook::~OrderBook() {}

std::vector<Trade> OrderBook::add_order(OrderId id, Side side, Price price, Quantity qty, Timestamp ts) {
    // Allocate order from pool
    Order* order = pool_.allocate();
    *order = Order(id, side, price, qty, ts);

    // Try to match against opposite side
    std::vector<Trade> trades = match(order, ts);

    // If residual quantity, insert into book as resting order
    if (order->qty > 0) {
        insert_order(order);
    } else {
        // Fully filled — return to pool
        pool_.deallocate(order);
    }

    return trades;
}

bool OrderBook::cancel_order(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) return false;

    Order* order = it->second;
    remove_order(order);
    pool_.deallocate(order);
    return true;
}

std::vector<Trade> OrderBook::modify_order(OrderId id, Price new_price, Quantity new_qty, Timestamp ts) {
    cancel_order(id);
    return add_order(id, Side::BID, new_price, new_qty, ts); // Side will be passed properly
}

Price OrderBook::best_bid() const {
    if (bids_.empty()) return 0;
    return bids_.begin()->first;
}

Price OrderBook::best_ask() const {
    if (asks_.empty()) return std::numeric_limits<Price>::max();
    return asks_.begin()->first;
}

Price OrderBook::spread() const {
    return best_ask() - best_bid();
}

Quantity OrderBook::volume_at(Side side, Price price) const {
    if (side == Side::BID) {
        auto it = bids_.find(price);
        return (it != bids_.end()) ? it->second.total_qty : 0;
    } else {
        auto it = asks_.find(price);
        return (it != asks_.end()) ? it->second.total_qty : 0;
    }
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

std::vector<Trade> OrderBook::match(Order* incoming, Timestamp ts) {
    std::vector<Trade> trades;

    if (incoming->side == Side::BID) {
        // Incoming BID matches against ASKs (lowest first)
        while (incoming->qty > 0 && !asks_.empty()) {
            auto best_it = asks_.begin();
            Level& level = best_it->second;

            // BID price must be >= ASK price for a match
            if (incoming->price < level.price) break;

            // Match against head of this level (FIFO)
            Order* resting = level.head;
            Quantity fill_qty = std::min(incoming->qty, resting->qty);

            // Emit trade
            trades.push_back(Trade{
                incoming->id,    // buyer
                resting->id,     // seller
                level.price,     // trade at resting price
                fill_qty,
                ts
            });

            incoming->qty -= fill_qty;
            resting->qty -= fill_qty;

            if (resting->qty == 0) {
                // Fully filled resting order — remove from book
                OrderId resting_id = resting->id;
                level.remove(resting);
                orders_.erase(resting_id);
                pool_.deallocate(resting);

                // Remove empty level
                if (level.empty()) {
                    asks_.erase(best_it);
                }
            } else {
                // Partial fill — update level total
                level.total_qty -= fill_qty;
            }
        }
    } else {
        // Incoming ASK matches against BIDs (highest first)
        while (incoming->qty > 0 && !bids_.empty()) {
            auto best_it = bids_.begin();
            Level& level = best_it->second;

            // ASK price must be <= BID price for a match
            if (incoming->price > level.price) break;

            Order* resting = level.head;
            Quantity fill_qty = std::min(incoming->qty, resting->qty);

            trades.push_back(Trade{
                resting->id,     // buyer (the resting bid)
                incoming->id,    // seller
                level.price,
                fill_qty,
                ts
            });

            incoming->qty -= fill_qty;
            resting->qty -= fill_qty;

            if (resting->qty == 0) {
                OrderId resting_id = resting->id;
                level.remove(resting);
                orders_.erase(resting_id);
                pool_.deallocate(resting);

                if (level.empty()) {
                    bids_.erase(best_it);
                }
            } else {
                level.total_qty -= fill_qty;
            }
        }
    }

    return trades;
}

void OrderBook::insert_order(Order* order) {
    if (order->side == Side::BID) {
        // Insert into bids map — creates level if not exists
        auto& level = bids_[order->price];
        if (level.price == 0) level.price = order->price;
        level.push_back(order);
    } else {
        auto& level = asks_[order->price];
        if (level.price == 0) level.price = order->price;
        level.push_back(order);
    }
    orders_[order->id] = order;
}

void OrderBook::remove_order(Order* order) {
    if (order->side == Side::BID) {
        auto it = bids_.find(order->price);
        if (it != bids_.end()) {
            it->second.remove(order);
            if (it->second.empty()) {
                bids_.erase(it);
            }
        }
    } else {
        auto it = asks_.find(order->price);
        if (it != asks_.end()) {
            it->second.remove(order);
            if (it->second.empty()) {
                asks_.erase(it);
            }
        }
    }
    orders_.erase(order->id);
}

} // namespace lob
