# Offline futures research prototype

A C++20 library for deterministic synthetic trade calculations. It provides MES/MNQ
tick arithmetic, exact price/quantity parsing, cost-inclusive sizing, ordered market
events, FIFO position accounting, and atomic rejection of invalid fills.

The core and MNQ15-BRK-v1.1 strategy are libraries and a test suite. There is no trained model,
market-data connection, trading terminal bridge, account credentials, or order transmitter.

## Build and test in Ubuntu / WSL2

Prerequisites: CMake 3.20+, a C++20 compiler, and standard build tools. No third-party
library download, API key, market data, or network access is needed for the build.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The suite contains 30 milestone acceptance tests and the independent-QA regression
`R01_fill_event_monotonic`. All must pass. CTest timing output is not deterministic;
calculated values and pass/fail results are.

## Key contracts

- MES: 4 ticks per index point, 125 cents per tick. MNQ: 4 ticks per point, 50 cents per tick.
- Order quantity is side BUY/SELL plus a positive integer number of contracts.
- Sizing reserves stop risk, both fee sides and configured round-trip slippage. It rounds down;
  an unaffordable contract produces zero size. This is a scenario allowance, not a guarantee.
- Filled prices are supplied positive integer ticks. Quote matching/slippage is a later layer.
- A fill must reference an admitted event after its own decision boundary. Fill-event sequence
  cannot regress across ledger calls. Same-event partial fills remain valid.
- Older pending orders may fill later. TTL, order IDs, execution deduplication and cancellation
  belong to a later order-management layer and are not claimed here.
- FIFO realised P&L and per-side fees are exact integer cents. Arithmetic errors leave state unchanged.

Public API: `include/fnq/core.hpp`. Implementation: `src/core.cpp`. Tests: `tests/`.
The accepted scope and exact examples are in `docs/engineer-milestone-1.md`; the temporal
repair is documented in `docs/qa-repair-notes.md`.

## Validation limits

The build is verified on Ubuntu/WSL2 with GCC. A native Windows/MSVC build has not been
verified. Synthetic arithmetic and software tests establish no strategy edge, account-rule
compliance, or real trading performance. Fees and position caps used in tests are synthetic.

## MNQ15-BRK-v1.1 synthetic strategy

`fnq_strategy` links the unchanged `fnq_core`. The accepted rules are in
`docs/mnq15-strategy-spec.md` and `docs/mnq15-strategy-test-cases.md`, with
`docs/mnq15-implementation-review.md` taking precedence. The implementation has
no network dependencies. The economic rationale is an untested channel-breakout
hypothesis; no unverified paper citation is part of the accepted implementation.

Minimal library use (prices in ticks, times in UTC epoch nanoseconds):

```cpp
#include "fnq/strategy.hpp"
auto session = fnq::session_2026("2026-11-03");
fnq::Strategy strategy(*session.value);
auto error = strategy.quote({1, session.value->open, 80000, 80002});
// Check every Error result. Advance timers even when quotes are absent.
error = strategy.advance_time(session.value->end);
// Inspect strategy.state(), bars(), log(), pending(), stop(), and failed().
```

Each instance represents one session and one dated MNQ contract selected by the
caller. The pinned local fixture supports only 2026-10-27 (CDT, UTC-5) and
2026-11-03 (CST, UTC-6), both 26 bars. It is an explicit UTC offset fixture,
not an IANA database or exchange holiday calendar. Unsupported dates/offsets
are rejected. `Session` permits explicit synthetic UTC halt intervals, cutoff
and flat-by configuration; constructor validation fixes the open/end template.
There is no account-rule calendar. Account suitability remains unverified.

`quote` and `advance_time` return errors without partial mutation, including
clock, bars, orders, ledger and logs. Entries use half-open 60-second TTL;
market exits take priority over stops; end forbids execution and retains any
unclosed ledger position with `FLAT_BY_FAILURE`. A new session requires a fresh
instance. The implementation copies state for transactional updates and retains
all admitted quotes; it is a small research prototype, not a scalable feed engine.
No reversal, pyramiding, partial fills, controls B2/B3, training, real-data
backtest, live/demo execution or profitability/compliance claim is provided.
Fees, risk budget, slippage reserve and cap are pinned synthetic strategy fixtures;
the core sizing/ledger APIs remain configurable for separate research scenarios.

The full suite is 62 CTest cases: existing 31 plus S01–S30 and S31 review
boundaries/overflow. Each strategy case builds explicit quotes from synthetic
half-tick OHLC vectors; execution is always on quotes. To run one case:

```bash
./build/test_strategy 1
```

Sanitizer build (GCC/Linux; no downloads):

```bash
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"
cmake --build build-san -j2
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-san --output-on-failure
```

Leak detection is disabled for this managed ptrace environment: LeakSanitizer
exits with its unsupported-ptrace error. ASan and UBSan remain enabled. On a
host supporting LeakSanitizer, enable leak detection separately.
