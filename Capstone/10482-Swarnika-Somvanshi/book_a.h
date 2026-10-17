// book_a.h - Implementation A: std::map levels + std::unordered_map orders.
// Baseline. Pointer-chasing tree nodes, one heap allocation per level/order.
#pragma once
#include <algorithm>
#include <functional>
#include <map>
#include <unordered_map>
#include "common.h"

class BookA {
public:
    // Duplicate ids are ignored (spec says ids are never reused).
    void add_order(uint64_t id, Side side, double price, long qty) {
        const int tick = to_tick(price);
        if (!valid_order(side, tick, qty)) return;
        if (!orders_.emplace(id, Order{side, tick, qty}).second) return;
        if (side == Side::Buy) bids_[tick] += qty;
        else                   asks_[tick] += qty;
    }

    bool cancel_order(uint64_t id) {
        auto it = orders_.find(id);
        if (it == orders_.end()) return false;
        const Order& o = it->second;
        if (o.side == Side::Buy) reduce(bids_, o.tick, o.qty);
        else                     reduce(asks_, o.tick, o.qty);
        orders_.erase(it);
        return true;
    }

    BBO get_bbo() const {
        BBO b{false, 0.0, 0, false, 0.0, 0};
        if (!bids_.empty()) {
            b.has_bid = true;
            b.bid_price = to_price(bids_.begin()->first);
            b.bid_qty = bids_.begin()->second;
        }
        if (!asks_.empty()) {
            b.has_ask = true;
            b.ask_price = to_price(asks_.begin()->first);
            b.ask_qty = asks_.begin()->second;
        }
        return b;
    }

    std::vector<Level> get_bids(int depth = 10) const { return top(bids_, depth); }
    std::vector<Level> get_asks(int depth = 10) const { return top(asks_, depth); }

private:
    // bids: highest tick first. asks: lowest tick first. begin() is always the best.
    std::map<int, long, std::greater<int>> bids_;
    std::map<int, long> asks_;
    std::unordered_map<uint64_t, Order> orders_;

    template <class M>
    static void reduce(M& m, int tick, long qty) {
        auto it = m.find(tick);  // always present for a live order
        it->second -= qty;
        if (it->second == 0) m.erase(it);
    }

    template <class M>
    static std::vector<Level> top(const M& m, int depth) {
        std::vector<Level> out;
        if (depth <= 0) return out;
        out.reserve(std::min<size_t>(static_cast<size_t>(depth), m.size()));
        for (const auto& kv : m) {
            if (static_cast<int>(out.size()) >= depth) break;
            out.push_back({to_price(kv.first), kv.second});
        }
        return out;
    }
};
