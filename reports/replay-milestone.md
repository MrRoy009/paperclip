# Replay milestone status — 2026-11-02

## Implemented and verified
- Offline CSV ingestion and C++ strategy replay, one dated MNQ contract per run.
- Explicit CT session dates converted with local IANA tzdata 2026c; timezone,
  input, manifest and runner hashes recorded. Default two-date strategy fixture
  remains unchanged; explicit UTC sessions require opt-in.
- Trade reconciliation, decision/fill logs, net amounts, missing-session/bar and
  quote-gap diagnostics. Data errors abort before publishing any result directory.
- Debug and ASan+UBSan: 63/63 CTest cases passed (62 existing tests plus one
  integration suite containing 23 checks). Leak checking disabled as previously.
- Deterministic generated 106-quote example covering all 26 bars passed: one
  closed trade, -2350 cents. This is an invented software fixture, not a loss on
  historical MNQ, and no economic conclusion follows. Its data and manifest are
  shipped in examples/replay/; outputs are labelled SYNTHETIC_PIPELINE_TEST.

## Historical replay is NOT completed
No suitable freely downloadable MNQ event-level bid/ask dataset was verified.
The user requires free data only. No data purchase, account creation, paid API,
provider credential use or trading connection occurred.

The official Historical Futures Data MNQ page offers free 10-second bars, which
do not establish event ordering or executable bid/ask for this strategy. Kibot's
free bid/ask samples are different instruments. Databento's MBP-1 docs establish
a compatible type of data, not a free MNQ file. URLs and findings are in REPLAY.md.
The bounded investigation does not establish that no free source exists.

## Explicit limits and remaining work
The existing strategy copies state per quote: this first runner rejects inputs
above 25,000 rows rather than silently sampling. It needs a scalable engine for
full-resolution multi-day historical data. Session hours remain a synthetic
regular-day template, not a verified exchange holiday/early-close/halt calendar.
Fee assumptions remain synthetic; no latency/liquidity model was added.
Drawdown is closed-trade only and not account-compliance drawdown.

The next real-data step needs a freely usable dated MNQ event-BBO file, verified
timestamp/price/sequence semantics and session calendar. Until those inputs and
scaling limits are addressed, performance_evaluation_valid remains false.
No historical win rate, return, Sharpe or profitability figure is reported.
