# Offline futures core — milestone 1

A C++20 library for deterministic synthetic trade calculations. It provides MES/MNQ
tick arithmetic, exact price/quantity parsing, cost-inclusive sizing, ordered market
events, FIFO position accounting, and atomic rejection of invalid fills.

This milestone is a library and test suite. It contains no entry strategy, trained model,
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
