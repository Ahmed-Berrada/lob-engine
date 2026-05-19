#include "lob/matching_engine.h"
#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iomanip>

using namespace lob;
using Clock = std::chrono::high_resolution_clock;

struct BenchResult {
    double ops_per_sec;
    double median_ns;
    double p99_ns;
    double p999_ns;
    double mean_ns;
};

BenchResult run_benchmark(size_t num_ops) {
    MatchingEngine engine;
    std::mt19937_64 rng(42); // deterministic seed

    // Price distribution around mid-price 10000 (±50 ticks)
    std::uniform_int_distribution<Price> price_dist(9500, 10500);
    std::uniform_int_distribution<Quantity> qty_dist(1, 1000);
    std::uniform_int_distribution<int> action_dist(0, 99);

    std::vector<double> latencies;
    latencies.reserve(num_ops);

    std::vector<OrderId> active_orders;
    active_orders.reserve(num_ops / 2);

    OrderId next_id = 1;

    auto total_start = Clock::now();

    for (size_t i = 0; i < num_ops; ++i) {
        int action = action_dist(rng);
        auto start = Clock::now();

        if (action < 60) {
            // 60% — Add order
            Side side = (action < 30) ? Side::BID : Side::ASK;
            Price price = price_dist(rng);
            Quantity qty = qty_dist(rng);
            engine.submit_order(next_id, side, price, qty);
            active_orders.push_back(next_id);
            ++next_id;
        } else if (action < 85 && !active_orders.empty()) {
            // 25% — Cancel order
            std::uniform_int_distribution<size_t> idx_dist(0, active_orders.size() - 1);
            size_t idx = idx_dist(rng);
            engine.cancel_order(active_orders[idx]);
            // Swap-and-pop
            active_orders[idx] = active_orders.back();
            active_orders.pop_back();
        } else {
            // 15% — Aggressive order (likely to match)
            Side side = (action % 2 == 0) ? Side::BID : Side::ASK;
            Price price = (side == Side::BID) ? Price(10500) : Price(9500);
            Quantity qty = qty_dist(rng);
            engine.submit_order(next_id, side, price, qty);
            ++next_id;
        }

        auto end = Clock::now();
        double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(ns);
    }

    auto total_end = Clock::now();
    double total_sec = std::chrono::duration_cast<std::chrono::microseconds>(total_end - total_start).count() / 1e6;

    // Sort latencies for percentile calculation
    std::sort(latencies.begin(), latencies.end());

    BenchResult result;
    result.ops_per_sec = num_ops / total_sec;
    result.median_ns = latencies[num_ops / 2];
    result.p99_ns = latencies[static_cast<size_t>(num_ops * 0.99)];
    result.p999_ns = latencies[static_cast<size_t>(num_ops * 0.999)];
    result.mean_ns = std::accumulate(latencies.begin(), latencies.end(), 0.0) / num_ops;

    return result;
}

int main() {
    std::cout << "=== LOB Matching Engine Benchmark ===\n\n";
    std::cout << "Warming up...\n";

    // Warm-up run
    //run_benchmark(100000);

    std::cout << "Running benchmark...\n\n";

    // Main benchmark: 5M operations
    size_t num_ops = 50000000;
    auto result = run_benchmark(num_ops);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Operations:    " << num_ops / 1000000.0 << "M\n";
    std::cout << "Throughput:    " << result.ops_per_sec / 1e6 << " M ops/sec\n";
    std::cout << "Mean latency:  " << result.mean_ns << " ns\n";
    std::cout << "Median (P50):  " << result.median_ns << " ns\n";
    std::cout << "P99:           " << result.p99_ns << " ns\n";
    std::cout << "P99.9:         " << result.p999_ns << " ns\n";

    std::cout << "\n--- Targets ---\n";
    std::cout << "Throughput > 2M ops/sec: " << (result.ops_per_sec > 2e6 ? "PASS ✓" : "FAIL ✗") << "\n";
    std::cout << "Median < 500ns:          " << (result.median_ns < 500 ? "PASS ✓" : "FAIL ✗") << "\n";

    return 0;
}
