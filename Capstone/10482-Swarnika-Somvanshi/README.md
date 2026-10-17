# Capstone: Low-Latency Order Book

- **Name:** Swarnika Somvanshi
- **SST email:** swarnika.24bcs10482@sst.scaler.com
- **Roll number:** 24BCS10482  <!-- TODO: confirm -->
- **Language:** C++17, single-threaded, standard library only, built with `-O2`

## 1. Build and run

```bash
# build everything (one binary)
g++ -O2 -std=c++17 -Wall -Wextra -g -fno-omit-frame-pointer -o bench main.cpp

# unit tests (edge cases from the spec) on every implementation
g++ -O2 -std=c++17 -Wall -Wextra -o test_main test_main.cpp && ./test_main

# correctness: same workload through all implementations, compare everything
./bench --verify

# benchmark one implementation (pinned to one P-core)   <!-- TODO: --impl comes with the benchmark phase -->
taskset -c 2 ./bench --impl A
taskset -c 2 ./bench --impl B
taskset -c 2 ./bench --impl C
```

`-g -fno-omit-frame-pointer` only make `perf record -g` readable. The optimization level stays `-O2`.

## 2. Interface changes

**None.** The public API is exactly the one in the spec (`double` prices, `add_order`,
`cancel_order`, `get_bbo`, `get_bids`, `get_asks`). Prices are converted to integer ticks
(`llround(price * 100)`) inside each book using one shared helper in `common.h`, so every
implementation pays the same conversion cost.

Behaviours the spec leaves open (all three books behave identically):
- An invalid add is **silently ignored**: price outside its band, price exactly 100.00, or `qty <= 0`.
- Adding an id that is already live is ignored (the spec says ids are never reused).
- `get_bids(depth)` / `get_asks(depth)` return best level first, and fewer than `depth` levels if the book has fewer.

## 3. Files

| File | What it is |
|---|---|
| `common.h` | `Side`, `Level`, `BBO`, tick helpers, shared validation, `Order` |
| `book_a.h` | Implementation A |
| `book_b.h` | Implementation B  <!-- TODO --> |
| `book_c.h` | Implementation C  <!-- TODO --> |
| `workload.h` | Repeatable workload generator (`mt19937`, seed 42) |
| `unit_tests.h`, `test_main.cpp` | Edge-case tests, shared by all implementations |
| `verify.h` | `--verify` correctness harness |
| `main.cpp` | `--verify` and `--impl` entry point |
| `collect_sysinfo.sh` | Saves machine and toolchain info |
| `results/` | Raw outputs (see section 7) |

## 4. Machine

Intel Core i5-13500H (hybrid P-core + E-core), Ubuntu 24.04, g++ 13.3.0, native Linux (dual boot, not a VM).
The CPU is hybrid, so perf splits every counter into `cpu_core` (P-core) and `cpu_atom` (E-core).
All runs are **pinned to P-core 2 with `taskset -c 2`** and I report the `cpu_core` counters.
Full details: `results/system_info.txt`.
Counters unavailable: `cpu_atom/L1-dcache-load-misses` (E-core). <!-- TODO: list anything else that turns out missing -->

## 5. Data-structure choices

### Implementation A: `std::map` + `std::unordered_map`

1. **Data structures:** two `std::map<int,long>` (one per side) for price levels, one `std::unordered_map<uint64_t, Order>` for orders.
2. **How orders are stored:** `unordered_map` from order id to `{side, tick, qty}`.
3. **How price levels are stored:** red-black tree keyed by tick, value = total quantity at that tick. Bids use `std::greater<int>` so the tree is ordered best-first on both sides. A level is erased when its quantity reaches 0.
4. **How the best bid/ask is found:** `begin()` of each map.
5. **How cancel finds an order:** hash lookup by id gives side, tick and qty, then one tree lookup finds the level to subtract from.

| Operation | Complexity (L = levels per side, D = depth) |
|---|---|
| `add_order` | O(log L) tree update + O(1) average hash insert |
| `cancel_order` | O(log L) + O(1) average hash erase |
| `get_bbo` | O(1) |
| `get_bids` / `get_asks` | O(D) |

### Implementation B: <!-- TODO -->

### Implementation C: <!-- TODO -->

### Why the three are genuinely different

<!-- TODO: one table: order storage / level storage / find best / find order on cancel, for A, B, C -->

## 6. Correctness results

```
<!-- TODO: paste output of ./bench --verify once B and C exist -->
```

`--verify` runs three workloads (main spec workload, and two stress workloads with tiny price bands,
heavy cancels, and cancels of unknown or already-cancelled ids), compares cancel results, BBO, top-10
bids/asks every op, and the **entire book** at regular intervals.

## 7. Performance results

Raw files are in `results/`:

| File | Contents |
|---|---|
| `system_info.txt` | CPU, kernel, compiler, perf version, governor |
| `unit_tests_output.txt` | Unit test output |
| `verify_output.txt` | `--verify` output |
| `bench_A.txt`, `bench_B.txt`, `bench_C.txt` | Benchmark output (median of 5 runs, final BBO, checksum) |
| `perf_stat_A.txt`, `perf_stat_B.txt`, `perf_stat_C.txt` | `perf stat -r 5` output |
| `perf_report_A.txt`, `perf_report_B.txt`, `perf_report_C.txt` | `perf report --stdio` (top symbols) |

### Results table (median of 5 runs, pinned to P-core 2)

| Metric | A | B | C |
|---|---|---|---|
| Total time (ms) | | | |
| ns / add | | | |
| ns / cancel | | | |
| ns / BBO | | | |
| IPC | | | |
| Branch misses (%) | | | |
| L1-dcache-load-misses | | | |
| Cache misses | | | |
| Page faults | | | |

Notes for reading the table:
- Per-op timings subtract the measured cost of a `steady_clock::now()` call.
- perf counters include workload generation, which is identical for all three, so it is a constant offset.

## 8. Performance analysis

See `REPORT.md`.
