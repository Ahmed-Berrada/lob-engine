#include "lob/matching_engine.h"
#include <cassert>
#include <iostream>

using namespace lob;

void test_add_and_cancel() {
    MatchingEngine engine;

    // Add a bid — no match, book should have 1 order
    auto trades = engine.submit_order(1, Side::BID, 10000, 100);
    assert(trades.empty());
    assert(engine.order_count() == 1);
    assert(engine.best_bid() == 10000);

    // Cancel it
    assert(engine.cancel_order(1));
    assert(engine.order_count() == 0);
    assert(engine.cancel_order(1) == false); // already cancelled

    std::cout << "[PASS] test_add_and_cancel\n";
}

void test_no_match_spread() {
    MatchingEngine engine;

    // Bid at 100, Ask at 101 — no cross
    engine.submit_order(1, Side::BID, 10000, 50);
    engine.submit_order(2, Side::ASK, 10100, 50);

    assert(engine.order_count() == 2);
    assert(engine.best_bid() == 10000);
    assert(engine.best_ask() == 10100);
    assert(engine.spread() == 100);

    std::cout << "[PASS] test_no_match_spread\n";
}

void test_exact_match() {
    MatchingEngine engine;

    // Resting ask at 100
    engine.submit_order(1, Side::ASK, 10000, 100);

    // Incoming bid at 100 — should match completely
    auto trades = engine.submit_order(2, Side::BID, 10000, 100);

    assert(trades.size() == 1);
    assert(trades[0].buyer_id == 2);
    assert(trades[0].seller_id == 1);
    assert(trades[0].price == 10000);
    assert(trades[0].qty == 100);
    assert(engine.order_count() == 0); // both fully filled

    std::cout << "[PASS] test_exact_match\n";
}

void test_partial_fill() {
    MatchingEngine engine;

    // Resting ask of 200 shares at 50
    engine.submit_order(1, Side::ASK, 5000, 200);

    // Incoming bid for 80 shares at 50
    auto trades = engine.submit_order(2, Side::BID, 5000, 80);

    assert(trades.size() == 1);
    assert(trades[0].qty == 80);
    assert(engine.order_count() == 1); // resting ask still has 120 left

    // Another bid for 120
    trades = engine.submit_order(3, Side::BID, 5000, 120);
    assert(trades.size() == 1);
    assert(trades[0].qty == 120);
    assert(engine.order_count() == 0);

    std::cout << "[PASS] test_partial_fill\n";
}

void test_fifo_priority() {
    MatchingEngine engine;

    // Two asks at same price — order 1 first, order 2 second
    engine.submit_order(1, Side::ASK, 10000, 50);
    engine.submit_order(2, Side::ASK, 10000, 50);

    // Incoming bid should match order 1 first (FIFO)
    auto trades = engine.submit_order(3, Side::BID, 10000, 50);
    assert(trades.size() == 1);
    assert(trades[0].seller_id == 1); // FIFO: first in, first matched

    // Next bid should match order 2
    trades = engine.submit_order(4, Side::BID, 10000, 50);
    assert(trades.size() == 1);
    assert(trades[0].seller_id == 2);

    std::cout << "[PASS] test_fifo_priority\n";
}

void test_price_priority() {
    MatchingEngine engine;

    // Asks at different prices
    engine.submit_order(1, Side::ASK, 10200, 100); // worse price
    engine.submit_order(2, Side::ASK, 10000, 100); // best price
    engine.submit_order(3, Side::ASK, 10100, 100); // middle

    // Aggressive bid at 10200 for 250 shares — should fill best prices first
    auto trades = engine.submit_order(4, Side::BID, 10200, 250);

    assert(trades.size() == 3);
    assert(trades[0].price == 10000); // best ask first
    assert(trades[1].price == 10100); // then middle
    assert(trades[2].price == 10200); // then worst

    std::cout << "[PASS] test_price_priority\n";
}

void test_multi_level_partial() {
    MatchingEngine engine;

    // Build order book: asks at 100, 101
    engine.submit_order(1, Side::ASK, 10000, 50);
    engine.submit_order(2, Side::ASK, 10100, 80);

    // Bid sweeps through first level and partially fills second
    auto trades = engine.submit_order(3, Side::BID, 10100, 70);

    assert(trades.size() == 2);
    assert(trades[0].price == 10000);
    assert(trades[0].qty == 50);
    assert(trades[1].price == 10100);
    assert(trades[1].qty == 20);
    assert(engine.order_count() == 1); // 60 shares left at ask 10100

    std::cout << "[PASS] test_multi_level_partial\n";
}

void test_empty_book_operations() {
    MatchingEngine engine;

    assert(engine.best_bid() == 0);
    assert(engine.empty());
    assert(!engine.cancel_order(999));

    // Add and immediately match
    engine.submit_order(1, Side::BID, 10000, 100);
    auto trades = engine.submit_order(2, Side::ASK, 9000, 100);
    assert(trades.size() == 1);
    assert(engine.empty());

    std::cout << "[PASS] test_empty_book_operations\n";
}

int main() {
    test_add_and_cancel();
    test_no_match_spread();
    test_exact_match();
    test_partial_fill();
    test_fifo_priority();
    test_price_priority();
    test_multi_level_partial();
    test_empty_book_operations();

    std::cout << "\n=== ALL TESTS PASSED ===\n";
    return 0;
}
