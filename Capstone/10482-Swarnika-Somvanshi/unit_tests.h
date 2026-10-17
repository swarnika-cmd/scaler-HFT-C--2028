// unit_tests.h - edge-case tests, templated so A, B and C all reuse them.
#pragma once
#include <cstdio>
#include "common.h"

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::printf("  FAIL line %d: %s\n", __LINE__, #cond);        \
            ++fails;                                                     \
        }                                                                \
    } while (0)

template <class Book>
int run_unit_tests(const char* name) {
    int fails = 0;

    {   // cancel unknown / cancel twice
        Book b;
        CHECK(!b.cancel_order(42));
        b.add_order(1, Side::Buy, 99.98, 10);
        CHECK(b.cancel_order(1));
        CHECK(!b.cancel_order(1));
    }
    {   // empty book
        Book b;
        BBO x = b.get_bbo();
        CHECK(!x.has_bid && !x.has_ask);
        CHECK(b.get_bids(10).empty() && b.get_asks(10).empty());
    }
    {   // insufficient depth: 3 levels, ask for 10 -> get 3
        Book b;
        b.add_order(1, Side::Buy, 99.99, 5);
        b.add_order(2, Side::Buy, 99.98, 6);
        b.add_order(3, Side::Buy, 99.97, 7);
        auto v = b.get_bids(10);
        CHECK(v.size() == 3);
        CHECK(v[0].price == 99.99 && v[1].price == 99.98 && v[2].price == 99.97);  // best first
        CHECK(b.get_bids(2).size() == 2);
    }
    {   // same price sums; partial then full cancel
        Book b;
        b.add_order(1, Side::Sell, 100.01, 10);
        b.add_order(2, Side::Sell, 100.01, 25);
        CHECK(b.get_bbo().ask_qty == 35);
        CHECK(b.cancel_order(1));
        CHECK(b.get_bbo().ask_qty == 25);
        CHECK(b.cancel_order(2));
        CHECK(!b.get_bbo().has_ask);
    }
    {   // best moves when the best level empties; asks sorted low -> high
        Book b;
        b.add_order(1, Side::Sell, 100.01, 1);
        b.add_order(2, Side::Sell, 100.05, 2);
        b.add_order(3, Side::Buy, 99.99, 3);
        b.add_order(4, Side::Buy, 99.50, 4);
        BBO x = b.get_bbo();
        CHECK(x.has_bid && x.bid_price == 99.99 && x.bid_qty == 3);
        CHECK(x.has_ask && x.ask_price == 100.01 && x.ask_qty == 1);
        CHECK(b.cancel_order(1));
        CHECK(b.cancel_order(3));
        x = b.get_bbo();
        CHECK(x.bid_price == 99.50 && x.bid_qty == 4);
        CHECK(x.ask_price == 100.05 && x.ask_qty == 2);
        auto a = b.get_asks(10);
        CHECK(a.size() == 1 && a[0].price == 100.05);
    }
    {   // invalid adds are ignored (wrong band, 100.00, qty <= 0)
        Book b;
        b.add_order(1, Side::Buy, 100.00, 10);
        b.add_order(2, Side::Buy, 100.50, 10);
        b.add_order(3, Side::Sell, 99.00, 10);
        b.add_order(4, Side::Buy, 49.99, 10);
        b.add_order(5, Side::Sell, 150.01, 10);
        b.add_order(6, Side::Buy, 80.00, 0);
        CHECK(!b.get_bbo().has_bid && !b.get_bbo().has_ask);
        CHECK(!b.cancel_order(1));
    }

    {   // duplicate id is ignored (first order wins), cancel works once
        Book b;
        b.add_order(1, Side::Buy, 99.90, 10);
        b.add_order(1, Side::Buy, 99.90, 20);
        CHECK(b.get_bbo().bid_qty == 10);
        CHECK(b.cancel_order(1));
        CHECK(!b.cancel_order(1));
        CHECK(!b.get_bbo().has_bid);
    }
    {   // band boundaries are accepted: 50.00, 99.99, 100.01, 150.00
        Book b;
        b.add_order(1, Side::Buy, 50.00, 1);
        b.add_order(2, Side::Buy, 99.99, 2);
        b.add_order(3, Side::Sell, 100.01, 3);
        b.add_order(4, Side::Sell, 150.00, 4);
        auto bd = b.get_bids(10);
        auto ak = b.get_asks(10);
        CHECK(bd.size() == 2 && bd[0].price == 99.99 && bd[1].price == 50.00);
        CHECK(ak.size() == 2 && ak[0].price == 100.01 && ak[1].price == 150.00);
    }
    {   // a level that emptied can be refilled and is found again
        Book b;
        b.add_order(1, Side::Sell, 100.10, 5);
        CHECK(b.cancel_order(1));
        CHECK(!b.get_bbo().has_ask);
        b.add_order(2, Side::Sell, 100.10, 9);
        CHECK(b.get_bbo().has_ask && b.get_bbo().ask_qty == 9);
    }
    {   // depth <= 0 returns nothing
        Book b;
        b.add_order(1, Side::Buy, 99.00, 1);
        CHECK(b.get_bids(0).empty());
        CHECK(b.get_bids(-5).empty());
    }
    {   // best bid must fall back through several levels as they empty
        Book b;
        for (int i = 0; i < 5; ++i) b.add_order(100 + i, Side::Buy, 99.99 - 0.01 * i, 10 + i);
        for (int i = 0; i < 5; ++i) {
            BBO x = b.get_bbo();
            CHECK(x.has_bid && x.bid_price == to_price(9999 - i) && x.bid_qty == 10 + i);
            CHECK(b.cancel_order(100 + i));
        }
        CHECK(!b.get_bbo().has_bid);
    }

    std::printf("%s unit tests: %s\n", name, fails ? "FAIL" : "PASS");
    return fails;
}

// Tests for the shared helpers in common.h (tick conversion + validation).
inline int run_common_tests() {
    int fails = 0;
    // every legal tick must survive tick -> price -> tick exactly
    for (int t = BID_MIN_TICK; t <= BID_MAX_TICK; ++t) CHECK(to_tick(to_price(t)) == t);
    for (int t = ASK_MIN_TICK; t <= ASK_MAX_TICK; ++t) CHECK(to_tick(to_price(t)) == t);
    // classic floating-point traps: 100.01 * 100 is 10000.999..., llround must fix it
    CHECK(to_tick(99.98) == 9998);
    CHECK(to_tick(100.01) == 10001);
    CHECK(to_tick(150.00) == 15000);
    CHECK(to_tick(50.00) == 5000);
    // validation rules
    CHECK(valid_order(Side::Buy, 9999, 1));
    CHECK(!valid_order(Side::Buy, 10000, 1));
    CHECK(!valid_order(Side::Buy, 4999, 1));
    CHECK(valid_order(Side::Sell, 10001, 1));
    CHECK(!valid_order(Side::Sell, 10000, 1));
    CHECK(!valid_order(Side::Sell, 15001, 1));
    CHECK(!valid_order(Side::Buy, 9000, 0));
    CHECK(!valid_order(Side::Buy, 9000, -3));
    std::printf("common helpers unit tests: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
