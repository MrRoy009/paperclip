# Exploratory MNQ bar study — October 2026 sample

**Result: zero trades.** The strategy generated 32 eligible breakout signals,
but every one failed the frozen position-sizing rule. This is a completed
approximate bar-based screen, not quote-level validation or a profitability test.

## Why no trades occurred
The unchanged synthetic risk budget is $50 per trade. Stop distances for the
32 signals ranged from 200 to 903 ticks. With $0.50/tick and $6 of fee/slippage
reserve, the required per-contract risk was $106 to $457.50. Even the smallest
signal therefore sized to zero. The budget, stop multiplier and strategy
parameters were not changed after seeing these results.

| Adverse fill assumption | Signals rejected by sizing | Trades | Net P&L |
|---|---:|---:|---:|
| 1 tick per side | 32 | 0 | $0 (no exposure) |
| 2 ticks per side — base case | 32 | 0 | $0 (no exposure) |
| 4 ticks per side | 32 | 0 | $0 (no exposure) |

Win rate and profit factor are undefined. Zero drawdown reflects no exposure;
it does not establish strategy safety. These scenarios cannot measure cost
sensitivity when no orders were filled. The flat B0 baseline also earns zero.

## Data and coverage
- Freely downloaded MNQ 10-second trade OHLCV sample: 166,756 rows.
- 46,800 rows fall within 20 complete synthetic regular-day sessions, October
  1–28. October 29 and 30 regular sessions are absent from the sample. Across
  all 22 expected weekdays, 4,680 ten-second buckets / 52 fifteen-minute bars
  are missing. All 20 observed regular sessions contain every expected bucket.
- Source file spans 2026-10-01 00:00 UTC through 2026-10-29 03:12:50 UTC.
  This is a partial-month file, not a complete October performance study.
- All timestamps, ordering, uniqueness, bucket alignment and OHLC containment
  passed validation. No null/nonfinite/nonpositive prices or fractional-tick
  OHLC rows were observed in this sample.
- Provider documents adjusted continuous-series trade bars. They are not a
  dated-contract BBO feed. No actual spread filter, queue, quote-event ordering
  or exchange/account calendar was validated. Fees and fill costs are assumed.

## Checks and reproducibility
- Frozen plan: mnq-bar-study-plan.md, hash
  6ca0e7dd17448395cb8ee0e8517e244e8511863cdac8a42a7eb29e01eda39473.
- Source SHA-256:
  dadae7d14298c599677a481ac8a4b522b21a20741169aad7928a603da43e0d6a.
- 25 synthetic unit checks pass: long/short symmetry, stops/gaps including
  entry-bar hits, ties, TTL, timer, missing windows, sizing, adjusted prices,
  opposite-exit priority, cooldown and entry cutoff.
- Independent pandas resampling/rolling calculations matched the exact time,
  stop distance and risk amount for all 32 rejected signals. No affordable
  signal was found independently.
- Python3.12.7, pyarrow17.0.0, IANA tzdata2024.2 on Windows. The study uses the
  pinned Python timezone package rather than the separate WSL2026c database;
  October 2026 Chicago session conversion was also independently checked.
- The initial output directory is retained as an intermediate artifact. The
  checked directory supersedes it: missing-day summary validity was corrected
  and rejection diagnostics added. Strategy parameters/outcomes did not change.

## Interpretation and next decision
The $50 synthetic fixture prevents this baseline from trading in the observed
sample. That is a configuration finding, not evidence for or against its edge.
An account risk specification must be established before any deliberate strategy
revision, then frozen before a fresh evaluation. This study did not choose a
larger budget or tune a tighter stop to manufacture trades.

Quote-level historical replay (QUA-15) remains blocked on suitable free MNQ BBO
history, the documented scalability cap and verified feed/calendar semantics.
No live trading, purchases, ML training, account-rule evaluation or holdout test
was performed. One exploratory partial month cannot establish profitability.

Official provider format: https://historicalfuturesdata.com/docs.
