# Frozen exploratory bar-study plan — MNQ15-BAR-SCREEN-v1

Frozen before inspecting historical OHLC values or calculating outcomes, 2026-11-02.
User approved an approximate study on the already downloaded free sample.
This is a separate strategy variant, not validation of MNQ15-BRK-v1.1.

Input: provider's October 2026 10-second adjusted continuous MNQ OHLCV sample.
SHA-256 dadae7d14298c599677a481ac8a4b522b21a20741169aad7928a603da43e0d6a.
Use the entire downloaded month as an exploratory development sample; no holdout,
parameter search or significance claim. UTC bucket-start timestamps per provider
docs; convert CT regular-day synthetic window with IANA America/Chicago.
Provider: https://historicalfuturesdata.com/docs.

## Rules retained
- 15-minute closed trade-price bars, prior-8-bar channel (exclude decision bar),
  strict breakout; first possible entry 10:45 CT, last 14:00 inclusive.
- TR in whole ticks, no cross-session close. D=max(8,ceil(2*sum(TR of last8)/8)).
- Synthetic risk budget 5000 cents, fee100 cents/side, cap1, sizing risk D*50+600
  cents, 3 filled entries/session, fixed initial stop on actual modelled entry.
- Opposite breakout only flattens. Cooldown skips the bar containing the exit
  and the following 15-minute decision; no reversal or pyramiding.
- Flat-by14:50 CT, end15:00; fresh session/indicator state daily.

## Explicit approximations
- Trade OHLC substitutes for quote-mid bars; no actual bid/ask or spread filter.
  This missing filter is a model difference, not proof that spreads were narrow.
- Independently run adverse execution-cost scenarios of 1, 2 and 4 ticks per
  side. Base scenario=2 ticks/side, combining an assumed spread/slippage charge.
  Each scenario reruns the entire strategy (stop anchors/outcomes may differ).
  These are pinned sensitivity cases, not a fitted optimum or real fee estimates.
  Retain original sizing reserve8 RT ticks (at4/side it matches the assumed cost).
- Market orders fill at the first available 10-second bar open at/after decision;
  entry TTL60s, exclusive endpoint. Missing fills cancel entry; exits have no TTL.
- Long fills buy=open+adverse_ticks; short fills sell=open-adverse_ticks. Stop
  anchored on that modelled fill. If long low<=stop, exit at min(open,stop)-adverse;
  short high>=stop, exit at max(open,stop)+adverse. No better-than-stop fills.
  Include a stop hit within the entry's 10-second bar, conservatively. OHLC cannot
  establish real event ordering or stop arming; this intentionally differs from
  the quote engine. One exit only; pending market exits at open take priority.
- Every 15-minute indicator bar requires all90 distinct 10-second buckets;
  missing buckets invalidate it. No forward filling. A trade's exit can still
  act on a later available bar; report gaps and unclosed end positions.
- Each day uses a synthetic full 08:30–15:00 CT session. Holidays, early closes,
  status halts and real account calendars are not certified. Report missingness;
  this is not an exchange-calendar backtest or account-rule evaluation.
- Keep provider prices as exact Decimal values without forcing OHLC onto a
  tradable tick grid: additive adjusted continuous prices may be fractional
  ticks. Stops use whole-tick distances, execution remains in adjusted units.
  P&L per contract is $2/point. Describe synthetic continuous-series fills as
  non-executable; do not pretend this is a dated-contract replay. Ceil TR-based D
  once after the exact sum. No contract-roll trading is inferred.

## Reporting and checks
- Validate timestamp alignment/order, duplicates, OHLC containment, nonnegative
  volume, NaN/null prices, timestamp timezone, row counts and dataset hashes.
- Report sessions, missing buckets/bars, trades, wins/losses/flat, gross, fees,
  modelled execution drag, net, profit factor, worst trade, closed-trade drawdown,
  intrabar adverse-excursion bound, unclosed positions and each cost scenario.
- Drawdown/account compliance is not certified; show trade-close drawdown and a
  separately labelled conservative intrabar adverse-excursion bound.
- Flat B0 earns zero. No random-direction control/significance test in this first
  screen; absence prevents edge claims. Inspect gross-to-net deterioration.
- Independently unit-check long/short, gap and entry-bar stops, ties, TTL, timer,
  missing windows, fee arithmetic, exact equality, and price adjustment handling.
- Any failed session invalidates a complete-month economic summary; show raw
  diagnostic aggregates separately. One month alone cannot establish edge.
