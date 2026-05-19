#pragma once

#include "order.h"

namespace lob {

// A price level: all orders at the same price, stored as FIFO doubly-linked list
struct Level {
    Price price = 0;
    Quantity total_qty = 0;
    uint32_t order_count = 0;
    Order* head = nullptr;  // oldest order (first to match)
    Order* tail = nullptr;  // newest order

    Level() = default;
    explicit Level(Price p) : price(p) {}

    // Push order at the tail (newest, last priority)
    void push_back(Order* order) {
        order->prev = tail;
        order->next = nullptr;
        if (tail) {
            tail->next = order;
        } else {
            head = order;  // list was empty
        }
        tail = order;
        total_qty += order->qty;
        ++order_count;
    }

    // Remove an order from anywhere in the list — O(1)
    void remove(Order* order) {
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head = order->next;  // was head
        }
        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail = order->prev;  // was tail
        }
        total_qty -= order->qty;
        --order_count;
        order->prev = nullptr;
        order->next = nullptr;
    }

    bool empty() const { return order_count == 0; }

    // Pop the head (oldest / highest priority) — used during matching
    Order* pop_front() {
        if (!head) return nullptr;
        Order* front = head;
        remove(front);
        return front;
    }
};

} // namespace lob
