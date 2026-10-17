// test_main.cpp - runs the unit tests on every implementation that exists so far.
#include "book_a.h"
#include "unit_tests.h"

int main() {
    int fails = run_common_tests();
    fails += run_unit_tests<BookA>("A");
    // fails += run_unit_tests<BookB>("B");   // uncomment as you add them
    // fails += run_unit_tests<BookC>("C");
    return fails ? 1 : 0;
}
