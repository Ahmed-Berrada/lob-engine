# LOB Engine: Limit Order Book Matching Engine

[![CI](https://github.com/Ahmed-Berrada/lob-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/Ahmed-Berrada/lob-engine/actions/workflows/ci.yml)

*[Version française](README.md)*

A single-threaded limit order book matching engine in C++17 with **price-time priority (FIFO)**.

**3.46 M ops/s · 163 ns median latency · O(log n) matching · O(1) cancel**

## What is a matching engine?

A matching engine is the core of any electronic exchange: it pairs buyers and sellers who agree on a price and emits the resulting trades. It has to be deterministic (same input, same trades), fair (best price first, then first come first served) and fast.

## Benchmark

5 million operations, deterministic seed, 100K-operation warm-up before measurement. Mix: 60 % adds, 25 % cancels, 15 % aggressive orders.

| Metric | Result |
|---|---|
| Throughput | 3.46 M ops/s |
| Mean latency | 253.16 ns |
| Median (P50) | 163 ns |
| P99 | 996 ns |
| P99.9 | 5,517 ns |

The tail (P99.9) is about 34 times the median, which is the number to work on next (see Roadmap).

## Architecture

```
MatchingEngine
└── OrderBook
    ├── Bids: std::map<Price, Level, std::greater>   best bid = begin()
    ├── Asks: std::map<Price, Level, std::less>      best ask = begin()
    ├── orders_: unordered_map<OrderId, Order*>      O(1) cancel lookup
    ├── pool_:   PoolAllocator<Order, 65536>         arena allocation
    └── Level = intrusive doubly-linked list (FIFO)
```

## Design choices

1. **Integer prices (`int64_t`)**: one tick is 0.01 EUR, so 150.25 EUR is stored as 15025. Floating point breaks equality (`0.1 + 0.2 != 0.3`) and prices are discrete anyway.
2. **`std::map` for the price tree**: the sorted order gives the best price at `begin()` in O(1). A hash map would need an O(n) scan, and an array indexed by price wastes memory on a sparse range.
3. **Intrusive linked list per price level**: the `prev` and `next` pointers live inside `Order`, so there is one allocation per order, better cache locality than `std::list`, and O(1) push, pop and unlink.
4. **Pool allocator**: pre-allocated blocks and a free list make allocation cost a few nanoseconds with no `malloc` jitter and no fragmentation.
5. **Hash map for cancel**: finds any order by id in amortised O(1), then unlinks it from its level in O(1).
6. **Single thread per book**: arrival order defines priority, so a book is sequential by nature. Locks would hurt P99, and the usual scaling pattern is one thread per symbol.

## Matching algorithm

An incoming bid at price P and quantity Q walks the asks from the best price upward while P is at or above the best ask. Each fill is `min(Q, resting quantity)` and trades at the **resting order's price**. Fully filled resting orders are removed, and any remainder of the incoming order is inserted in the book.

| Operation | Complexity |
|---|---|
| Add, no match | O(log n) |
| Add, match at the best level | O(1) |
| Add, sweeping K levels | O(K + log n) |
| Cancel | O(1) |
| Best bid / best ask | O(1) |

Here n is the number of active price levels.

## Project structure

```
include/lob/   types.h, order.h, level.h, pool_allocator.h, order_book.h, matching_engine.h
src/           order_book.cpp, matching_engine.cpp
tests/         test_orderbook.cpp   (8 unit tests)
bench/         benchmark.cpp        (5M operations, latency and throughput)
```

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/tests    # unit tests
./build/bench    # benchmark
```

Release builds use `-O3 -march=native`, so benchmark numbers depend on the CPU they were measured on.

## Roadmap

- **Order types:** IOC, FOK, market orders, modify that keeps priority.
- **Interfaces:** multi-symbol engine, FIX 4.4 gateway, market data feed (L2/L3).
- **Reliability:** write-ahead log for crash recovery, nanosecond audit trail.
- **Latency:** CPU pinning, huge pages, `template <Side>` to remove branches, lock-free SPSC queues between input, engine and output, prefetching.

## License

MIT. See [LICENSE](LICENSE).
