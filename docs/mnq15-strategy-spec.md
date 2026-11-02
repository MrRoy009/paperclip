# MNQ 15-minute baseline strategy specification — `MNQ15-BRK-v1`

Status: **Proposed specification for owner review. Not implemented, not backtested, not trained.**
Task: QUA-11. Plan reference: `docs/research-plan.md` revision 6 (rule family `R1`/`R4`, §4.2;
bar convention §3.2; three clocks §3.3; fill convention §6.3). Core reference: tested offline core
commit `803828adf176f5ca26bf4e86c4ec5a4853bfdaae` (milestone 1). Neither the plan nor the core is
changed by this document. Test vectors: `docs/mnq15-strategy-test-cases.md`.

This is **one** deterministic rule with constants pinned before any data is seen. It is a baseline
to be tested against nulls, not a claim of edge. No result exists for it. Every constant below is
fixed by this document; changing any of them after looking at any evaluation output creates a new
trial under plan §7.7 and must be counted.

## 0. Parameter table (all pinned; units explicit)

| Id | Parameter | Value | Label |
|---|---|---|---|
| P1 | Instrument | MNQ outright, one dated contract per run, no roll inside a run | Exchange spec (§4.1) |
| P2 | Tick size / tick value | 0.25 index point / **50 cents** | SOURCED (CME, see §8) |
| P3 | Bar length Δ | 900 s (15 min), labels at UTC epoch multiples of 900 s | Pinned |
| P4 | Bar price series | `m2 = bid + ask` (integer, units of half-ticks) of each admitted event | Pinned |
| P5 | Channel lookback N | 8 completed prior bars (excludes decision bar) | Pinned |
| P6 | ATR window L | 8 bars (TR of the decision bar and 7 before it) | Pinned |
| P7 | Stop multiplier K | 2 × ATR | Pinned |
| P8 | Minimum stop distance | 8 ticks | Pinned |
| P9 | Stop formula | `D = max(8, ceil(sumTR_m2 / 8))` ticks (= ceil(K·ATR) with K=2, L=8, half-tick units) | Derived |
| P10 | Max entry spread | 4 ticks (`ask − bid ≤ 4` at the last event before the decision) | Pinned |
| P11 | Session template (SYNTHETIC) | Mon–Fri, 08:30:00–15:00:00 America/Chicago wall clock | Synthetic |
| P12 | First possible entry decision | Bar label 10:45 CT (9th bar of the session) | Derived from P5/P6 |
| P13 | Last entry decision | Bar label 14:00 CT inclusive | Pinned |
| P14 | Flat-by timer | 14:50:00 CT (clock timer, not a bar) | Synthetic |
| P15 | Hard session end | 15:00:00 CT | Synthetic |
| P16 | Entry order type / TTL | Market; expires if no eligible event within 60 s of decision time | Pinned |
| P17 | Exit order types | Fixed stop-market; market on opposite signal or flat-by; no TTL | Pinned |
| P18 | Cooldown | 2 bar decisions after the bar containing an exit fill | Pinned |
| P19 | Max entries per session | 3 | Pinned |
| P20 | Position cap | 1 contract (core `max_abs_position`) | Synthetic |
| P21 | Fee | **100 cents per contract per side** | **SYNTHETIC — not an actual fee** |
| P22 | Stressed round-trip slippage reserve | 8 ticks (= 400 cents for MNQ) | Synthetic stress |
| P23 | Risk budget per trade | 5000 cents | **SYNTHETIC fixture, not an account limit** |
| P24 | Reversal | Prohibited | Pinned |
| P25 | Pyramiding / partial fills | Prohibited / disabled (quantity 1 is indivisible) | Pinned |

## 1. Time, bars and information availability

- **Storage clock.** Every event and timer is a UTC epoch nanosecond integer (`t_utc_ns`). No
  local time is stored.
- **Session clock.** P11, P13–P15 are America/Chicago wall-clock times converted to UTC with a
  pinned IANA tzdata version recorded in the run manifest (plan §3.3). Under CST (UTC−6) 08:30 CT =
  14:30 UTC; under CDT (UTC−5) 08:30 CT = 13:30 UTC. Because the session is anchored to CT wall
  clock, every full session has exactly 26 bars in both DST regimes. CT offsets are whole hours, so
  the UTC 900 s grid and the CT quarter-hour grid coincide.
- **Account clock.** Not used by this strategy. FundedNext public terms (15:10 CT flat-by, 17:00 CT
  day boundary; plan §2.3) are **unconfirmed for the exact product** and are not encoded as rules.
  The synthetic flat-by P14 is merely earlier than that public time; no compliance claim follows.
- **Bar definition.** Bar `t` (label = close time `t`) aggregates admitted events with
  `t − 900 s ≤ t_utc_ns < t`. OHLC are over `m2 = bid + ask` of those events: open = first, high =
  max, low = min, close = last, in event `seq` order. Using `bid + ask` keeps odd-tick spreads on an
  integer grid without a fractional mid. Only bars whose interval lies wholly within
  [session open, session end] are formed. Session bar index `i = 1..26`; bar 1 has label 08:45 CT.
- **Missing bar.** A session bar interval with zero admitted events is *missing*. Bars are never
  synthesised or forward-filled. A bar whose interval contains a calendar halt is flagged
  `spans_halt` and treated as missing (plan §3.2).
- **Observability.** Bar `t` is observable only at instant `t`. A decision at `t` sees events with
  `t_utc_ns < t` only; its *last-seen seq* is the largest such admitted `seq`. An event with
  `t_utc_ns = t` belongs to bar `t + 900 s` and is the first event eligible to fill.
- **Next-event execution.** An order created by a decision (bar or timer) at time `T` with last-seen
  seq `s` may fill only at an admitted event `e` with `seq(e) > s` and `t(e) ≥ T` (core
  `SAME_OR_PAST_EVENT` enforces the seq half). No fill uses the decision bar's close.

## 2. Entry (family: 8-bar closed-bar channel breakout, session-restricted — plan `R1`+`R4`)

At each bar label `t` with session index `i`, compute, from closed bars only:

- `HN = max(high_{i−8} … high_{i−1})`, `LN = min(low_{i−8} … low_{i−1})` — the decision bar `i`
  is **excluded** from the channel.
- `TR_j = max(high_j, close_{j−1}) − min(low_j, close_{j−1})` for `j ≥ 2`; `TR_1 = high_1 − low_1`
  (no cross-session carry; the prior session's close is never used). Units: half-ticks.
- `sumTR = TR_{i−7} + … + TR_i` (8 values, including the decision bar, which is closed).
- `D = max(8, ceil(sumTR / 8))` ticks; integer form `max(8, (sumTR + 7) / 8)` with `sumTR ≥ 0`.

**Eligibility (all must hold, else no entry, with the stated reason):**

1. Flat, no pending order, no open stop (else no entry evaluation).
2. `i ≥ 9`, i.e. bars `1..i` all present and none missing/`spans_halt` (`WARMUP` if `i < 9`,
   `MISSING_DATA` if any of bars `i−8..i` is missing; bars `i−8..i` cover both windows because
   `TR_{i−7}` needs `close_{i−8}`).
3. Label `t` ≤ 14:00 CT (P13) and `t` < flat-by (`AFTER_ENTRY_CUTOFF`).
4. Entries already filled this session < 3 (`MAX_ENTRIES`).
5. Not in cooldown (§2.2) (`COOLDOWN`).
6. Last event before `t` has `ask − bid ≤ 4` ticks (`SPREAD_WIDE`).

**Signal:**

- **Long** iff `close_i > HN` (strict).
- **Short** iff `close_i < LN` (strict).
- **Tie:** `close_i = HN` or `close_i = LN` → no signal (`NO_SIGNAL`). Both conditions cannot
  hold together since `HN ≥ LN`.

**Sizing (§4) then order:** if `size = 1`, release one market order (buy for long, sell for short)
at time `t` with last-seen seq as in §1. If `size = 0`, no order (`SIZE_ZERO`); the signal is not
carried forward.

### 2.1 Entry order lifetime

The entry market order fills at the first admitted event `e` with `seq(e) > s` and
`t ≤ t(e) < t + 60 s`, buy at `ask(e)`, sell at `bid(e)`. If none exists it is cancelled
(`ENTRY_EXPIRED`): no fill, no fee, no position, does not count toward P19, no cooldown.
The flat-by timer (§3) also cancels any unfilled entry order.

### 2.2 Cooldown and re-entry

Let `b_x` be the label of the bar whose interval contains the exit fill time. Entry decisions at
labels `b_x` and `b_x + 900 s` are skipped; the earliest re-entry decision is `b_x + 1800 s`.
Re-entry in either direction is then allowed subject to §2 eligibility. No pyramiding: an entry is
evaluated only when flat.

## 3. Exit

Exactly one position (|position| ≤ 1). Exit conditions:

| Priority | Condition | Order | Timing |
|---|---|---|---|
| 1 | **Flat-by timer** at 14:50:00 CT | Market (sell if long, buy if short) | Decision at the timer instant `T_f`; last-seen seq = last event with `t < T_f`; fills at first event with `t ≥ T_f` |
| 2 | **Stop** (§5) | Stop-market, resting | Triggers and fills on the triggering event |
| 3 | **Opposite signal** at a bar label: long and `close_i < LN`, or short and `close_i > HN` (same formulas, same strictness, requires bars `i−8..i` present) | Market | As entry timing, no TTL |

- The timer is a clock event, so it acts at the first event at or after 14:50:00, well before
  15:00; it never waits for the 15:00 bar. After `T_f` no bar decisions are taken in the session.
- If a timer and a bar decision fall at the same instant (not possible with the pinned values;
  possible if P14 is reconfigured to a quarter hour), the timer is processed first.
- **Missing data while in position:** the opposite-signal check is skipped (position kept); stop
  and timer remain active.
- **Exit orders have no TTL.** If an exit market order is still unfilled at 15:00:00 CT (no event
  between `T_f` and session end), the run records `FLAT_BY_FAILURE`, which invalidates the run for
  any session-rule analysis; the position is not marked out at a synthetic price.
- **One exit only (OCO).** When any exit fills, every other order of that position is cancelled.
  If an exit market order is pending and the stop also triggers on the same event `e`, the pending
  market order fills (step 1 of §6) and the stop is cancelled; the fill price is the same
  opposite quote of `e` either way, and the recorded reason is that of the market order.
- **No reversal.** An opposite signal only flattens. A new entry in the other direction requires a
  later decision after cooldown. A fill that would reverse is a specification violation.
- **Partial fills:** disabled; quantity 1 cannot be split. A later fill engine with partial fills
  enabled is out of scope for this baseline.

## 4. Position sizing (synthetic)

Uses the core formula unchanged (milestone 1 "Sizing"):

`risk_per_contract = D × 50 + 2 × 100 + 8 × 50 = 50·D + 600` cents.
`size = min(1, floor(5000 / risk_per_contract))`.

- `size = 1` iff `50·D + 600 ≤ 5000` iff `D ≤ 88` ticks. `D ≥ 89` → `SIZE_ZERO`.
- Checked: at `D = 88`, `4400 + 200 + 400 = 5000` → 1. At `D = 89`, `4450 + 600 = 5050` → 0.
  Without the 600 cents of costs `D = 89` would size 1 (`4450 ≤ 5000`), so that rejection is
  cost-caused. At the minimum `D = 8`: `400 + 600 = 1000` cents; raw `floor(5000/1000) = 5`,
  capped to 1.
- The fee (P21) is a synthetic placeholder chosen to be conservative-looking, **not** a sourced
  broker/exchange/clearing fee; replace when actual fees are supplied. The budget (P23) and cap
  (P20) are fixtures, not FundedNext account limits; no real contract cap is inferred.
- The reserve is a sizing allowance only, not a fill model; a gapped stop may lose more
  (test vector S15 shows the reserve being exceeded).

## 5. Stop

- **Placement:** anchored on the actual entry fill. Long: `stop = entry_fill − D`. Short:
  `stop = entry_fill + D`. `D` is a whole number of ticks and fills are on the tick grid, so the
  stop is on-grid; no rounding step exists.
- **Arming:** the stop is active from the first event with `seq` greater than the entry-fill event's
  seq. The entry-fill event itself cannot trigger it (prevents same-event hindsight).
- **Fixed, never trailing.** `D` is computed once at the entry decision and never updated.
- **Trigger and fill (plan §6.3):** a sell stop (long position) triggers when `bid(e) ≤ stop` and
  fills at `bid(e)`; a buy stop (short position) triggers when `ask(e) ≥ stop` and fills at
  `ask(e)`. **Gap-through:** if the quote is beyond the stop, the fill is at that quote, worse than
  the stop, never at the stop price. No price improvement.
- **Intrabar ambiguity / scope:** stops and exits execute on the quote event stream, never on bar
  OHLC, so there is no intrabar ordering ambiguity. Running this rule on bar-only data is **out of
  scope**; if ever attempted it must assume the stop is hit before any favourable price within the
  same bar and must be reported as a different, lower-fidelity experiment.
- **Unaffordable contract:** §4 `SIZE_ZERO`; no order is created.

## 6. Per-event processing order (deterministic)

Before admitting event `e`, process every timer and bar label with time `≤ t(e)` in time order
(timer before bar at equal time); each decision sees only events already admitted. Then for `e`:

1. Fill any pending market order eligible at `e` (entry or exit); cancel OCO siblings.
2. If a position remains and its stop is armed and triggered by `e`, fill the stop.
3. Expire an entry order whose 60 s TTL has passed.

## 7. Economic rationale (hypothesis, not evidence of edge)

- **Intuition (untested):** a 15-minute close beyond the prior two-hour range during regular US
  hours may coincide with order-flow imbalance or information arrival that continues for some
  minutes to hours, so that the mid-price change after entry is, on average, in the breakout
  direction.
- **Cited evidence (related, not this rule):** Gao, Han, Li and Zhou, "Market intraday momentum",
  *Journal of Financial Economics* 129(2), 2018, DOI 10.1016/j.jfineco.2018.05.009, report
  intraday return predictability in S&P 500 ETF data. **Not accessed in this run; bibliographic
  details from memory and must be verified.** It concerns a different instrument, horizon and
  signal; it does not support any claim about MNQ or this rule.
- **Falsifiable hypothesis H1:** on chronologically held-out data, mean net P&L per trade of
  `MNQ15-BRK-v1` (with P21/P22 costs) exceeds the 95th percentile of the cost-matched control
  distribution below. H1 is rejected if it does not; it is also treated as rejected if its sign
  reverses between the two chronological halves of the held-out period.
- **Where it should fail (predicted, untested):** range-bound sessions where 8-bar extremes are
  repeatedly touched and reversed; scheduled-news spikes that mean-revert inside the next bar;
  high-spread periods (partly filtered by P10); and on synthetic `G0` random walks, where expected
  net P&L per trade must be about −(fees + spread) — a positive `G0` result indicates a bug or
  lookahead, not edge.
- **Cost-matched controls:** `B3` (coin-flip direction on this rule's own entry timestamps, same
  stop distance `D`, same exits, same fees) and `B2` (random entry times matched in count and
  holding period; matching procedure still open, plan `D3`). Both incur the same per-trade fees
  and spread crossing. `B0` (flat) gives the zero floor. Report the trial count per plan §7.7.

## 8. Sources and factual status (accessed 2026-11-02 unless stated)

| Fact | Source | Status |
|---|---|---|
| MNQ: $2/index point, 0.25-point tick, $0.50 per tick | [CME MNQ](https://www.cmegroup.com/markets/equities/nasdaq/micro-e-mini-nasdaq-100.html) | Verified 2026-11-01 (plan rev 6 §2.1). Re-fetch of the contract-specs page on 2026-11-02 **timed out**; not re-verified today |
| CME Globex equity-index hours (Sun 17:00 CT open, daily 16:00–17:00 CT halt) | Plan rev 6 §3.3; [CME trading hours](https://www.cmegroup.com/trading-hours.html) | Not re-fetched in this run; **unconfirmed here**. Not needed: P11 is a synthetic template inside those hours |
| FundedNext 15:10 CT flat-by, 17:00 CT day | Plan rev 6 §2.3 (public docs) | Public terms; **exact product unconfirmed**; not encoded |
| Fees | none | **SYNTHETIC** |
| Gao et al. 2018 | journal (see §7) | **Not accessed**; to verify |

## 9. Implementation dependencies (for a later, separate Engineer task)

Based on the milestone-1 specification (`docs/engineer-milestone-1.md`). The source at commit
`803828a…` is not present in this worktree and was **not inspected** in this task.

Core already specifies (milestone 1): `Ticks`/`Cents`/`Contracts` checked integers; MNQ tick value
50; event validation with `seq`/time ordering; `SAME_OR_PAST_EVENT` causality; FIFO ledger with fees
per side; cap 1 with `CAP_EXCEEDED`; sizing formula with fee and round-trip slippage reserve.

Needed later (new modules; the milestone-1 core is not modified): pinned tzdata + CT session
template and timers; 15-min bar builder on `bid+ask` with missing/`spans_halt`; integer channel and
TR/ATR; order objects (market with TTL, stop-market trigger/gap fill, OCO cancel) per plan §6.3;
strategy state machine (flat/pending/long/short, cooldown, entry counter, reasons); per-event
processing order §6; decision/exit reason log; controls `B0`/`B3` (and `B2` after `D3`).
