#pragma once

#include "types.h"

namespace lob {

// Intrusive doubly-linked list node
// Each Order lives inside a Level's FIFO queue
struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity qty;
    Timestamp timestamp;

    // Intrusive list pointers (within the same price level)
    Order* prev = nullptr;
    Order* next = nullptr;

    Order() = default;
    Order(OrderId id_, Side side_, Price price_, Quantity qty_, Timestamp ts_)
        : id(id_), side(side_), price(price_), qty(qty_), timestamp(ts_),
          prev(nullptr), next(nullptr) {}
};

} // namespace lob
