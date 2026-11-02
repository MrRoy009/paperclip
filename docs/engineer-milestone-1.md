# Engineer milestone 1 — offline C++20 core types and synthetic P&L

Status: **Ready for assignment — final focused specification, version 1.1. Implementation not started.**
Plan reference: `docs/research-plan.md` revision 6, §9 step 2 and §13.3. Base commit
`61dcc12b6ee4d248060985d69e4b2a692e86b620` on `codex/quant`.

## Scope

In scope (all deliverables of this milestone, none is a prerequisite):

1. CMake (≥ 3.20) project, C++20, warnings as errors (`-Wall -Wextra -Wpedantic -Werror` or MSVC
   equivalent), CTest enabled; one static library `fnq_core` and one test executable per area.
2. Checked integer types: `Ticks` (int64 price in ticks), `Cents` (int64 cash), `Contracts`
   (int32 whole contracts). No floating point anywhere in price, quantity or cash paths.
3. Instrument spec table (exchange specification, not an account rule): MES and MNQ.
4. Ordered synthetic event stream with sequence numbers and validation.
5. Position/P&L ledger for one instrument at a time; market fills at a supplied integer-tick price.
6. Synthetic position cap, default 1 contract.

Out of scope (later steps): calendars, tzdata, DST, account-day logic, MLL/consistency/benchmark
rules, order types beyond an immediate fill at a given price, slippage model, generators, PRNGs,
manifests, statistics, strategies, ML, file/network I/O, any trading connection, real-account
calibration, real or paid data. No real-account compliance claim may be made from this code.

## Contracts

### Instrument specs (SOURCED: CME product pages, accessed 2026-11-01)

| Symbol | Tick size | Point value | `tick_value_cents` |
|---|---|---|---|
| MES | 0.25 index point | $5 | **125** |
| MNQ | 0.25 index point | $2 | **50** |

Stored as integers; `ticks_per_point = 4` for both.

### Checked arithmetic

- `checked_add`, `checked_sub`, `checked_mul` on int64 (and int32 for contracts) return a result or
  an error (`std::expected`-style or a result struct; C++23 `std::expected` not required). No
  wraparound, no saturation. Overflow is an error value that the ledger propagates; it aborts the
  operation and leaves state unchanged.
- Parsing a decimal price string to `Ticks`: exact only. `"5000.25"` → 20001 ticks; `"5000.10"` →
  error `OFF_GRID`; `"abc"` → error `PARSE`. Parsing quantity: `"1"` → 1; `"1.5"`, `"-0.5"` →
  error `FRACTIONAL_QTY`.

### Sizing (ASSUMPTION, plan §4.3)

Use a total-loss budget, including costs, not price-risk-only sizing:

`risk_per_contract = checked(stop_distance_ticks * tick_value_cents + 2 * fee_per_contract_per_side_cents + stressed_round_trip_slippage_ticks * tick_value_cents)`.

`size = min(max_abs_position, floor(risk_budget_cents / risk_per_contract))`.

All terms use checked integer arithmetic. Budget, stop distance, tick value and cap must be
positive; otherwise return zero with `NON_POSITIVE_INPUT`. Fees and stressed slippage may be
zero (explicit synthetic assumptions); negative values return `INVALID_COST`. Overflow returns
`OVERFLOW`, never a tradable size. Never round zero up to one. The slippage input is the total
additional adverse round-trip allowance beyond the stated stop distance, not a per-side value;
it is a sizing reserve only, not an implemented fill/slippage model or an extra ledger debit.
Every fixture supplies fees and slippage explicitly. This is a modelled loss allowance, not a
guarantee that real gaps or fills cannot exceed it. Milestone 1 has no real-market use.

### Synthetic cap (ASSUMPTION; never derived from FundedNext published caps)

`max_abs_position` default 1. An order whose fill would make `|position| > max_abs_position` is
**rejected whole** with reason `CAP_EXCEEDED` (no silent partial clamp); state is unchanged. An
order that reduces or flattens `|position|` passes the cap check, but still must pass all
input, causality and checked-arithmetic validation. Compute magnitudes without overflowing
at signed minimum values.

### Events

`Event { uint64 seq; int64 t_utc_ns; Ticks bid; Ticks ask; }`. Validator accepts the stream only if:
`seq` strictly increasing; `t_utc_ns` non-decreasing (equal timestamps allowed, ordered by `seq`);
`bid > 0`, `ask > 0`, `bid ≤ ask`. Violations are reported with the offending `seq` and a reason
(`SEQ_NOT_INCREASING`, `TIME_DECREASING`, `NON_POSITIVE_PRICE`, `CROSSED_QUOTE`) and the event is
not admitted. Fills reference an admitted event `seq` that must be **strictly greater** than the
decision's last-seen `seq` (else `SAME_OR_PAST_EVENT`).

### Ledger

`realised_pnl_cents = Σ (exit_ticks − entry_ticks) × side × qty × tick_value_cents − fees`, where
`fee_cents = fee_per_contract_per_side_cents × qty` charged on every fill. Fees are scenario inputs
(synthetic; zero allowed only when labelled).

Keep a FIFO queue of entry lots `(side, positive quantity, entry_ticks)` for the one selected
instrument. Same-direction fills append a lot; no rounded weighted-average entry price.
Opposite-direction fills close oldest lots first. For each closed quantity, add
`(fill_ticks - lot.entry_ticks) * lot.side * closed_quantity * tick_value_cents` to cumulative
gross realised P&L. Retain an unchanged entry price for any remaining quantity of a lot.
If a fill reverses the position, close all old lots and open only its residual quantity at the
same fill price. Charge the fee once on the entire filled quantity, not once per conceptual
close/open operation. Track cumulative gross P&L, cumulative fees and net P&L separately;
net equals gross minus fees after every fill, including entry-only fills.
Validate and calculate the complete candidate state first. On any error, position, lots,
P&L, fees and event bookkeeping remain logically identical to their previous state.

## Acceptance tests (CTest; each named test must exist and pass)

Fee in cases T1–T4 is a synthetic 50 cents per contract per side unless stated.

| Test | Input | Expected output |
|---|---|---|
| `T01_mes_long_win` | MES buy 1 @ 20000, sell 1 @ 20004 | gross +500; fees 100; net **+400**; position 0 |
| `T02_mes_long_loss` | MES buy 1 @ 20000, sell 1 @ 19992 | gross −1000; net **−1100** |
| `T03_mnq_short_win` | MNQ sell 1 @ 80000, buy 1 @ 79990 | gross +500; net **+400** |
| `T04_mnq_short_loss` | MNQ sell 1 @ 80000, buy 1 @ 80010 | gross −500; net **−600** |
| `T05_zero_fee_flat` | MES buy 1 @ 20000, sell 1 @ 20000, fee 0 | net **0** exactly |
| `T06_one_tick_values` | 1-tick move, 1 contract, fee 0 | MES **125**, MNQ **50** |
| `T07_parse_on_grid` | `"5000.25"`, `"5000.00"`, `"0.25"` | 20001, 20000, 1 |
| `T08_parse_off_grid` | `"5000.10"`, `"5000.125"` | `OFF_GRID` both |
| `T09_parse_fractional_qty` | `"1.5"`, `"-0.5"` | `FRACTIONAL_QTY` both |
| `T10_overflow_mul` | `checked_mul(INT64_MAX/2 + 1, 2)` | error; no value returned |
| `T11_overflow_add` | `checked_add(INT64_MAX, 1)`, `checked_sub(INT64_MIN, 1)` | error both |
| `T12_ledger_overflow_unchanged` | fill whose P&L product overflows | error; ledger state bit-identical to before |
| `T13_sizing_round_down` | MES, stop 8 ticks, fee 50/side, round-trip slippage 0, cap 1; budgets 1000, 1099, 1100, 2500 | sizes **0, 0, 1, 1**; per-contract risk 1100 cents |
| `T14_sizing_mnq` | MNQ, budget 1000, stop 8 ticks, fee 50/side, round-trip slippage 0, cap 1 | risk 500 cents; raw 2 → capped to **1** |
| `T15_cap_reject_open` | position 0, buy 2, cap 1 | `CAP_EXCEEDED`; position 0 |
| `T16_cap_reject_add` | position +1, buy 1, cap 1 | `CAP_EXCEEDED`; position +1 |
| `T17_cap_allow_reduce` | position +1, sell 1, cap 1 | accepted; position 0 |
| `T18_cap_allow_reversal` | MES buy 1 @ 20000, sell 2 @ 20004, buy 1 @ 20000; fee 50/side, cap 1 | positions +1, −1, 0; gross P&L 0, 500, 1000; cumulative fees 50, 150, 200; net −50, 350, **800** cents; short entry after reversal 20004 |
| `T19_events_ok` | seq 1,2,3; t 10,10,11 | all admitted |
| `T20_events_bad_seq` | seq 1,3,2 | third rejected `SEQ_NOT_INCREASING`; seq 1,3 admitted |
| `T21_events_bad_time` | t 10, 9 | second rejected `TIME_DECREASING` |
| `T22_events_crossed` | bid 101, ask 100 | `CROSSED_QUOTE` |
| `T23_events_nonpositive` | bid 0 | `NON_POSITIVE_PRICE` |
| `T24_fill_causality` | decision last-seen seq 5; fill on seq 5 / seq 6 | `SAME_OR_PAST_EVENT` / accepted |
| `T25_no_float` | build check | a test or static check that no `float`/`double` appears in `fnq_core` price/cash/quantity headers |
| `T26_sizing_stressed_cost` | MES stop 8, fee 50/side, round-trip slippage 2 ticks, cap 1; budgets 1349 / 1350 | risk 1350 cents; sizes **0 / 1** |
| `T27_sizing_overflow` | stop × tick value overflows, and separately fee × 2 overflows | `OVERFLOW`; no tradable size |
| `T28_sizing_invalid_cost` | negative fee or negative slippage; separately explicit zero fee and zero slippage | negatives `INVALID_COST`; zeros valid when other inputs positive |
| `T29_fifo_partial_close` | MES cap 3, buy 1 @ 20000, buy 2 @ 20004, sell 2 @ 20008; fee 50/side | FIFO gross 1500; fees 250; net 1250 cents; remaining long 1 @ 20004 |
| `T30_rejected_fill_atomic` | invalid quantity, cap violation, invalid event, or arithmetic overflow during a reversal | error; all logical ledger and event-bookkeeping fields unchanged |

All numeric expectations above are specification examples, not executed test results.
Determinism: two runs produce the same pass/fail results and calculated values; ignore CTest's
wall-clock timings. Test logic runs offline, with no application data files, network or
environment-dependent inputs; CMake/CTest may create ordinary build and test-log files.

## Definition of done

- `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure` passes
  with all 30 tests listed above, on the exact commit submitted.
- The Engineer reports the commit hash, compiler and version, CMake version, and the full `ctest`
  output. QA reviews that exact commit.
- No item from "out of scope" appears in the diff.
