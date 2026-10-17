// workload.h - repeatable random workload, generated BEFORE any timing starts
// so RNG cost never shows up in the benchmark numbers.
#pragma once
#include <cstdint>
#include <random>
#include <vector>
#include "common.h"

enum class OpType : uint8_t { Add, Cancel, Bbo, Depth };

struct Op {
    OpType type;
    Side side;     // Add only
    int tick;      // Add only
    long qty;      // Add only
    uint64_t id;   // Add and Cancel
};

struct WorkloadCfg {
    int n = 1'000'000;      // main ops (Depth ops are extra, one per 100 main ops)
    unsigned seed = 42;
    int add_pct = 70;
    int cancel_pct = 20;    // remainder = BBO queries
    int bid_lo = BID_MIN_TICK, bid_hi = BID_MAX_TICK;
    int ask_lo = ASK_MIN_TICK, ask_hi = ASK_MAX_TICK;
    int bogus_cancel_pct = 0;  // % of cancels aimed at unknown / already-cancelled ids
};

constexpr uint64_t kUnknownIdBase = 10'000'000'000ULL;  // never added, huge on purpose

inline std::vector<Op> make_workload(const WorkloadCfg& c) {
    std::mt19937 rng(c.seed);
    auto rnd = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };

    std::vector<Op> ops;
    ops.reserve(static_cast<size_t>(c.n) + c.n / 100 + 1);
    std::vector<uint64_t> live;  // ids currently resting in the book
    std::vector<uint64_t> dead;  // already cancelled ids (only kept for bogus cancels)
    live.reserve(c.n);
    uint64_t next_id = 1;

    for (int i = 0; i < c.n; ++i) {
        const int r = rnd(0, 99);
        OpType t = r < c.add_pct ? OpType::Add
                 : r < c.add_pct + c.cancel_pct ? OpType::Cancel
                 : OpType::Bbo;

        if (t == OpType::Cancel) {
            const bool bogus = c.bogus_cancel_pct > 0 && rnd(0, 99) < c.bogus_cancel_pct;
            if (bogus) {
                uint64_t id = (!dead.empty() && rnd(0, 1))
                                  ? dead[rnd(0, static_cast<int>(dead.size()) - 1)]
                                  : kUnknownIdBase + static_cast<uint64_t>(rnd(0, 1'000'000));
                ops.push_back({OpType::Cancel, Side::Buy, 0, 0, id});
            } else if (live.empty()) {
                t = OpType::Add;  // nothing live to cancel -> turn into an add
            } else {
                const int idx = rnd(0, static_cast<int>(live.size()) - 1);
                const uint64_t id = live[idx];
                live[idx] = live.back();  // swap-remove: O(1), always a live target
                live.pop_back();
                if (c.bogus_cancel_pct > 0 && dead.size() < 10000) dead.push_back(id);
                ops.push_back({OpType::Cancel, Side::Buy, 0, 0, id});
            }
        }
        if (t == OpType::Add) {
            const Side s = rnd(0, 1) ? Side::Buy : Side::Sell;
            const int tick = (s == Side::Buy) ? rnd(c.bid_lo, c.bid_hi) : rnd(c.ask_lo, c.ask_hi);
            ops.push_back({OpType::Add, s, tick, rnd(1, 100), next_id});
            live.push_back(next_id++);
        } else if (t == OpType::Bbo) {
            ops.push_back({OpType::Bbo, Side::Buy, 0, 0, 0});
        }

        if ((i + 1) % 100 == 0) ops.push_back({OpType::Depth, Side::Buy, 0, 0, 0});
    }
    return ops;
}
