// common.h - shared types, tick helpers and validation for all three books.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

enum class Side { Buy, Sell };

struct Level {
    double price;
    long qty;
};

struct BBO {
    bool has_bid;
    double bid_price;
    long bid_qty;
    bool has_ask;
    double ask_price;
    long ask_qty;
};

// Price bands in integer ticks (1 tick = 0.01).
constexpr int BID_MIN_TICK = 5000;   // 50.00
constexpr int BID_MAX_TICK = 9999;   // 99.99
constexpr int ASK_MIN_TICK = 10001;  // 100.01
constexpr int ASK_MAX_TICK = 15000;  // 150.00
constexpr int LEVELS_PER_SIDE = 5000;

// The public API takes doubles (as the spec says). Internally everything is
// integer ticks. llround avoids 100.01*100 = 10000.999... style bugs.
inline int to_tick(double price) { return static_cast<int>(std::llround(price * 100.0)); }
inline double to_price(int tick) { return tick / 100.0; }

// Shared validation so all three books reject bad input identically.
// Invalid adds (wrong side/band, 100.00, qty <= 0) are silently ignored.
inline bool valid_order(Side side, int tick, long qty) {
    if (qty <= 0) return false;
    if (side == Side::Buy) return tick >= BID_MIN_TICK && tick <= BID_MAX_TICK;
    return tick >= ASK_MIN_TICK && tick <= ASK_MAX_TICK;
}

// What every book remembers per live order, so cancel knows what to undo.
struct Order {
    Side side;
    int tick;
    long qty;
};
