# `MNQ15-BRK-v1` — proposed test vectors

Status: **PROPOSED tests for a later Engineer task. None has been executed; all expected values
are hand-derived from `docs/mnq15-strategy-spec.md`, not results.**

Conventions (all vectors unless stated):

- MNQ, tick value 50 cents, synthetic fee 100 cents/side, stressed round-trip reserve 8 ticks,
  budget 5000 cents, cap 1. Prices are integer ticks (80000 ticks = 20000.00 points).
- Bars are given as `(O, H, L, C)` in `m2 = bid + ask` half-tick units. `B = 160000`.
- Default date: Tuesday 2026-11-03 (CST, UTC−6): 08:30 CT = 14:30 UTC; bar 9 label 10:45 CT =
  16:45 UTC; flat-by 14:50 CT = 20:50 UTC. Times below are CT wall clock.
- "Base bars" = bars 1–8 each `(B, B+20, B−20, B)`, so `HN = 160020`, `LN = 159980`, each
  `TR = 40` (bar 1: `H−L = 40`; bars 2–8: `max(B+20,B) − min(B−20,B) = 40`). All base-bar
  events have spread ≤ 4 ticks.
- `P&L = (exit − entry) × side × 50 − fees`; `modelled max loss = 50·D + 600`.

## Hand-checked cases

**S01 — long (positive).** Base bars; bar 9 `(B+20, B+60, B+20, B+50)`; last event before 10:45
bid 80024 / ask 80026 (`m2 = 160050`, spread 2).
- `TR_9 = max(160060, 160000) − min(160020, 160000) = 60`; `sumTR` (bars 2–9) `= 7·40 + 60 = 340`;
  `D = max(8, ceil(340/8)) = ceil(42.5) = 43` ticks.
- `close 160050 > HN 160020` → long. Risk `= 43·50 + 200 + 400 = 2150 + 600 = 2750 ≤ 5000`;
  `floor(5000/2750) = 1` → size 1.
- Entry event 10:45:00.100 bid 80025 / ask 80026 → **buy 1 @ 80026**. Stop `= 80026 − 43 = 79983`.
- Later event bid 79983 / ask 79984 → `bid ≤ 79983` → **sell 1 @ 79983**, reason `STOP`.
  Gross `(79983 − 80026)·50 = −2150`; fees 200; **net −2350** (≤ modelled 2750).

**S02 — short (positive).** Base bars; bar 9 `(B−20, B−20, B−60, B−50)`; last event before 10:45
bid 79974 / ask 79976 (`m2 = 159950`).
- `TR_9 = max(159980, 160000) − min(159940, 160000) = 60`; `sumTR = 340`; `D = 43`.
- `close 159950 < LN 159980` → short; size 1 (risk 2750).
- Entry event 10:45:00.100 bid 79974 / ask 79975 → **sell 1 @ 79974**. Stop `= 79974 + 43 = 80017`.
- Assume no later event has `ask ≥ 80017` and no later bar close exceeds its `HN`. Flat-by timer
  14:50:00; first event at 14:50:00.200 bid 79960 / ask 79961 → **buy 1 @ 79961**, reason `FLAT_BY`.
  Gross `(79961 − 79974)·(−1)·50 = +650`; fees 200; **net +450**.

**S03 — no trade (tie at HN).** Base bars; bar 9 `(B, B+20, B−10, B+20)`.
- `close 160020 = HN 160020` → not strictly greater; `160020 < LN`? no → `NO_SIGNAL`. No order,
  no fee, position 0. (`TR_9 = 30`, irrelevant.)

**S07 — cost-based size zero.** Bars 1–8 `(B, B+44, B−45, B)` (each `TR = 89`); bar 9
`(B+44, B+89, B, B+80)`, `TR_9 = max(160089,160000) − min(160000,160000) = 89`; last event bid
80039 / ask 80041.
- `sumTR = 8·89 = 712`; `D = ceil(712/8) = 89`. `close 160080 > HN 160044` → long signal.
- Risk `= 89·50 + 200 + 400 = 4450 + 600 = 5050 > 5000` → `floor = 0` → **`SIZE_ZERO`**, no order.
- Cost attribution: price risk alone `4450 ≤ 5000` would give 1. Budget 5050 → size 1.

## All vectors

| # | Category | Inputs (differences from conventions) | Expected |
|---|---|---|---|
| S01 | Long | above | Buy @80026; stop 79983; stop exit @79983; net −2350 |
| S02 | Short | above | Sell @79974; stop 80017; flat-by buy @79961; net +450 |
| S03 | Tie | close = HN | `NO_SIGNAL` |
| S04 | Tie | Base bars; bar 9 `(B, B+10, B−20, B−20)`; close = LN = 159980 | `NO_SIGNAL` |
| S05 | Warm-up | Breakout close at bar 8 (label 10:30) vs bars 1–7 | `WARMUP`; first evaluable decision is 10:45 |
| S06 | Missing data | Bar 4 (label 09:30) has zero events; breakout closes at bars 9, 12, 13 | 9 and 12 → `MISSING_DATA` (windows 1–9, 4–12 contain bar 4); 13 → evaluated (window 5–13) |
| S07 | Size zero | above | `SIZE_ZERO` |
| S08 | Size boundary | Bars 1–8 `(B, B+44, B−44, B)` TR 88; bar 9 `(B+44, B+88, B, B+80)` TR 88; sumTR 704 | `D = 88`; risk 5000; size **1** |
| S09 | Min stop | Bars 1–8 `(B, B+2, B−2, B)` TR 4; bar 9 `(B+2, B+6, B, B+4)` TR 6; sumTR 34 | `ceil(34/8) = 5` → `D = 8`; risk 1000; raw 5 → capped **1** |
| S10 | Spread | S01 but last event before 10:45 bid 80023 / ask 80028 (5 ticks); separately bid 80023 / ask 80027 (4) | 5 → `SPREAD_WIDE`; 4 → entry proceeds |
| S11 | Session cutoff | Valid long breakout at label 14:00; separately at 14:15 | 14:00 → order; 14:15 → `AFTER_ENTRY_CUTOFF` |
| S12 | Entry TTL | S01 but no event in [10:45:00, 10:46:00); next event at exactly 10:46:00.000 | `ENTRY_EXPIRED`; no fill, fee 0, no cooldown, entry count unchanged |
| S13 | Boundary event | Event at exactly 10:45:00.000 | Not part of bar 9; it is the S01 fill event if first eligible |
| S14 | Stop exact | S01 exit | Fill exactly @79983 (no improvement) |
| S15 | Gap stop long | S01 but exit event bid 79970 / ask 79972 | Sell @79970 (13 ticks through stop); gross −2800; net **−3000**; exceeds modelled 2750 by 250 → flag `RESERVE_EXCEEDED` (diagnostic, not error) |
| S16 | Gap stop short | S02 but event bid 80020 / ask 80022 before flat-by | Buy @80022; gross −2400; net −2600; ≤ 2750, no flag |
| S17 | Stop arming | S01 but entry event bid 79980 / ask 80026 (bid ≤ stop on the fill event); next event bid 80020 / ask 80022 | No stop on the fill event; position stays long |
| S18 | Opposite exit, no reversal | Long from S01; at a later label `close < LN` (window present) | Market sell at next event; position 0; **no** short order at that decision |
| S19 | Cooldown | Exit fill at 11:07:00 → `b_x = 11:15` | Breakouts at 11:15, 11:30 → `COOLDOWN`; 11:45 → evaluated |
| S20 | Flat-by timer | Long open; last event 14:49:59.900; next 14:50:00.250 bid 80100 / ask 80101 | Timer decision at 14:50:00 (last-seen = 14:49:59.900 event); sell @80100 at 14:50:00.250; not at the 15:00 bar |
| S21 | Flat-by failure | Long open; no event in [14:50:00, 15:00:00] | `FLAT_BY_FAILURE`; run invalid for session-rule analysis; no synthetic mark-out |
| S22 | Max entries | Three filled entries already in session; 4th valid breakout | `MAX_ENTRIES` |
| S23 | Same-event conflict | Long, stop 79983; opposite-signal exit pending; next event bid 79980 / ask 79982 | One sell @79980, reason `OPPOSITE_SIGNAL`; stop cancelled; position 0 (not −1) |
| S24 | DST | Same bar data on Tue 2026-10-27 (CDT) and Tue 2026-11-03 (CST) | 10:45 CT = 15:45 UTC vs 16:45 UTC; 26 bars both days; identical actions |
| S25 | Missing data in position | Long open; a bar in the current window missing | Opposite-signal check skipped; stop and flat-by still active |
| S26 | No cross-session TR | Previous session close far from today's bar 1 | `TR_1 = H_1 − L_1`; previous close unused |
| S27 | Tie on exit | Short open; `close_i = HN` | No exit (strict inequality) |
| S28 | Off-session events | Events at 08:29:59.999 and 15:00:00.000 | Excluded from bars 1 and 26 respectively |
| S29 | Halt flag | Synthetic calendar halt inside bar 5 interval | Bar 5 `spans_halt` → treated as missing (as S06) |
| S30 | Causality | Any fill whose event seq ≤ decision last-seen seq | Rejected by core `SAME_OR_PAST_EVENT` |

Control tests (proposed, separate from unit vectors): on synthetic `G0` streams, mean net P&L per
trade should be near −(fees + spread crossing); `B3` uses S01-style timestamps with random direction
and identical stop/exit/fee logic.
