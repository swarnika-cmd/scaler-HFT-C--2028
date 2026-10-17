// main.cpp - entry point. Right now: --verify. The --impl benchmark is the next phase.
#include <cstdio>
#include <cstring>
#include "verify.h"

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], "--verify")) return run_verify();
    std::printf("usage: ./bench --verify\n       (--impl A|B|C comes with the benchmark phase)\n");
    return 2;
}
