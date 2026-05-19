#pragma once

#include <cstdint>

namespace lob {

// Price in ticks (fixed-point integer) — 1 tick = 0.01
// Ex: 150.25€ = 15025 ticks
using Price = int64_t;
using Quantity = uint32_t;
using OrderId = uint64_t;
using Timestamp = uint64_t;

enum class Side : uint8_t {
    BID = 0,  // achat
    ASK = 1   // vente
};

enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    IOC = 2,   // Immediate or Cancel
    FOK = 3    // Fill or Kill
};

struct Trade {
    OrderId buyer_id;
    OrderId seller_id;
    Price price;
    Quantity qty;
    Timestamp timestamp;
};

} // namespace lob
