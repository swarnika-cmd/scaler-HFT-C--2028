// verify.h - --verify mode: same ops through every implementation, compare everything.
#pragma once
#include <array>
#include <cstdio>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>
#include "book_a.h"
// #include "book_b.h"   // <- uncomment as you add them
// #include "book_c.h"
#include "unit_tests.h"
#include "workload.h"

// To add B and C: extend these two lines. Index 0 is the reference everyone is compared to.
using Books = std::tuple<BookA>;
constexpr const char* kNames[] = {"A"};
constexpr size_t kNumBooks = std::tuple_size<Books>::value;
static_assert(sizeof(kNames) / sizeof(kNames[0]) == kNumBooks, "kNames must match Books");

template <class Tuple, class F, size_t... I>
void for_each_impl(Tuple& t, F&& f, std::index_sequence<I...>) { (f(std::get<I>(t), I), ...); }
template <class Tuple, class F>
void for_each_book(Tuple& t, F&& f) {
    for_each_impl(t, f, std::make_index_sequence<std::tuple_size<Tuple>::value>{});
}

struct Result {
    bool cancelled = false;
    BBO bbo{};
    std::vector<Level> bids, asks;
};

template <class Book>
void apply(Book& b, const Op& op, Result& r) {
    switch (op.type) {
        case OpType::Add:    b.add_order(op.id, op.side, to_price(op.tick), op.qty); break;
        case OpType::Cancel: r.cancelled = b.cancel_order(op.id); break;
        case OpType::Bbo:    r.bbo = b.get_bbo(); break;
        case OpType::Depth:  r.bids = b.get_bids(10); r.asks = b.get_asks(10); break;
    }
}

inline bool same_levels(const std::vector<Level>& x, const std::vector<Level>& y) {
    if (x.size() != y.size()) return false;
    for (size_t i = 0; i < x.size(); ++i)
        if (x[i].price != y[i].price || x[i].qty != y[i].qty) return false;
    return true;
}

inline bool same_bbo(const BBO& x, const BBO& y) {
    if (x.has_bid != y.has_bid || x.has_ask != y.has_ask) return false;
    if (x.has_bid && (x.bid_price != y.bid_price || x.bid_qty != y.bid_qty)) return false;
    if (x.has_ask && (x.ask_price != y.ask_price || x.ask_qty != y.ask_qty)) return false;
    return true;
}

inline const char* op_name(OpType t) {
    switch (t) {
        case OpType::Add: return "add";
        case OpType::Cancel: return "cancel";
        case OpType::Bbo: return "bbo";
        default: return "depth";
    }
}

// Runs one workload through every book. Marks pass[k]=false (and prints the first
// mismatch) if book k ever differs from book 0. Every `deep_every` ops it also
// compares the ENTIRE book on both sides, not just the top 10.
inline void verify_workload(const char* label, const std::vector<Op>& ops, int deep_every,
                            std::array<bool, kNumBooks>& pass) {
    long add = 0, cancel = 0, bbo = 0, depth = 0;
    for (const Op& o : ops) {
        add += o.type == OpType::Add; cancel += o.type == OpType::Cancel;
        bbo += o.type == OpType::Bbo; depth += o.type == OpType::Depth;
    }
    std::printf("  [%s] %zu ops (add %ld, cancel %ld, bbo %ld, depth %ld)\n", label, ops.size(),
                add, cancel, bbo, depth);

    auto books = std::make_unique<Books>();
    std::array<Result, kNumBooks> res;
    std::array<bool, kNumBooks> reported{};

    auto fail = [&](size_t k, size_t i, const char* what) {
        pass[k] = false;
        if (!reported[k]) {
            reported[k] = true;
            std::printf("    MISMATCH impl %s vs %s at op %zu (%s): %s\n", kNames[k], kNames[0], i,
                        op_name(ops[i].type), what);
        }
    };

    for (size_t i = 0; i < ops.size(); ++i) {
        const Op& op = ops[i];
        for_each_book(*books, [&](auto& b, size_t k) { apply(b, op, res[k]); });

        for (size_t k = 1; k < kNumBooks; ++k) {
            switch (op.type) {
                case OpType::Cancel:
                    if (res[k].cancelled != res[0].cancelled) fail(k, i, "cancel return value");
                    break;
                case OpType::Bbo:
                    if (!same_bbo(res[k].bbo, res[0].bbo)) fail(k, i, "BBO");
                    break;
                case OpType::Depth:
                    if (!same_levels(res[k].bids, res[0].bids)) fail(k, i, "top-10 bids");
                    if (!same_levels(res[k].asks, res[0].asks)) fail(k, i, "top-10 asks");
                    break;
                default: break;
            }
        }

        if (deep_every > 0 && (i + 1) % deep_every == 0) {
            std::array<std::vector<Level>, kNumBooks> fb, fa;
            for_each_book(*books, [&](auto& b, size_t k) {
                fb[k] = b.get_bids(1 << 20);
                fa[k] = b.get_asks(1 << 20);
            });
            for (size_t k = 1; k < kNumBooks; ++k) {
                if (!same_levels(fb[k], fb[0])) fail(k, i, "FULL bid side");
                if (!same_levels(fa[k], fa[0])) fail(k, i, "FULL ask side");
            }
        }
    }
}

inline int run_verify() {
    std::printf("Running correctness test...\n");
    std::array<bool, kNumBooks> pass;
    pass.fill(true);

    // 1) edge-case unit tests on every implementation
    {
        auto books = std::make_unique<Books>();
        for_each_book(*books, [&](auto& b, size_t k) {
            using B = std::decay_t<decltype(b)>;
            if (run_unit_tests<B>(kNames[k]) != 0) pass[k] = false;
        });
    }

    // 2) main workload: spec distribution, full price bands
    verify_workload("main", make_workload(WorkloadCfg{}), 10000, pass);

    // 3) stress workloads: tiny price bands so levels constantly empty and refill,
    //    heavy cancels, plus cancels of unknown / already-cancelled ids.
    WorkloadCfg narrow;
    narrow.n = 500000; narrow.seed = 7; narrow.add_pct = 50; narrow.cancel_pct = 45;
    narrow.bid_lo = 9990; narrow.bid_hi = 9999; narrow.ask_lo = 10001; narrow.ask_hi = 10010;
    narrow.bogus_cancel_pct = 15;
    verify_workload("stress-narrow", make_workload(narrow), 500, pass);

    WorkloadCfg medium = narrow;
    medium.seed = 99;
    medium.bid_lo = 9800; medium.bid_hi = 9999; medium.ask_lo = 10001; medium.ask_hi = 10200;
    verify_workload("stress-medium", make_workload(medium), 1000, pass);

    bool all = true;
    for (size_t k = 0; k < kNumBooks; ++k) {
        std::printf("Implementation %s: %s\n", kNames[k], pass[k] ? "PASS" : "FAIL");
        all = all && pass[k];
    }
    if (all) std::printf("All implementations produce identical results.\n");
    else     std::printf("MISMATCH FOUND - see messages above.\n");
    return all ? 0 : 1;
}
