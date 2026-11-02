# MNQ15-BRK-v1.1 independent audit (QUA-13)

**Candidate commit reviewed:** 51ebcd84b160ae7a8c37c733e52ff505055249fa (branch codex/qa, no tracked-file modifications by QA)
**Verdict: PASS for the offline synthetic implementation candidate, with the non-blocking observations below. This is a QA finding only, not acceptance, and not self-approval.**

## Builds and accepted tests (ctest only)
| Configuration | Result |
|---|---|
| Debug (build-qua13-dbg) | 62/62 passed |
| Debug + ASan + UBSan (build-qua13-san) | 62/62 passed, no sanitizer reports. Leak detection was not separately verified. |
| Release (build-qua13-rel) | 62/62 passed |
| Independent probes (qa-diagnostics/strategy, ASan+UBSan) | 24/24 passed |

Accepted tests and implementation were not modified. Probes are in qa-diagnostics/strategy/probes.cpp with their own CMakeLists (Q1–Q24 via ctest).

## Independent probe results
- Q1/Q4/Q24: a boundary quote never enters the closing bar. The decision runs once before the first boundary quote, and a pre-open quote is excluded from bar 1. Pass.
- Q2: TTL is half-open. A quote at deadline−1ns fills. A quote or advance_time at exactly the deadline gives ENTRY_EXPIRED with no entry counted. Pass.
- Q3: stale or equal timestamps. A stale quote is rejected with no mutation of log, events or clock. advance_time backwards returns TIME_DECREASING. Equal timestamps with higher sequence are accepted. A non-increasing sequence returns SEQ_NOT_INCREASING. Pass.
- Q5/Q15: the stop arms only after the entry fill and is not triggered by the entry quote's own bid. A gap-through stop fills at the quote price. The short side is symmetric (buy stop at ask ≥ stop). Pass.
- Q6: an open position at exactly session end gives FLAT_BY_FAILURE, with orders cancelled, state frozen and the position retained. The opposite-signal exit is retained at flat-by and not duplicated. Pass.
- Q7: flat-by fills at the first quote ≥14:50. A quote 1ns before end still fills. Pass.
- Q8: offset and calendar validation. A wrong UTC offset, misaligned open, wrong width, cutoff ≥ flat, flat > end, and malformed or out-of-session halts are all rejected. Only the dates 2026-10-27 (CDT) and 2026-11-03 (CST) are accepted. Unsupported dates are rejected. Pass.
- Q9: overflow inside the clock rolls back (bars, log and now are unchanged). A bid+ask overflow quote is rejected with no effect. Pass.
- Q10: an exit exactly on a bar boundary belongs to the next bar. Cooldown spans exactly the specified bars. Pass.
- Q11/Q12/Q17: missing-window recovery. A gap outside the 9-bar window does not permanently disqualify the session. A gap inside the window gives MISSING_DATA for entries and for opposite signals. Pass.
- Q13: a session with no quotes at all runs clean under sanitizers. Pass.
- Q14: OCO. A pending opposite exit takes priority over a triggered stop on the same quote. Pass.
- Q16: flat-by cancels the entry, creates a FLAT_BY exit, and no bar decisions follow. Pass.
- Q18: entries stay at or below 3 (observed entries=3). Pass.
- Q19: spread is judged on the last event strictly before the bar label. Pass.
- Q21: the CDT date behaves identically to CST. Pass.

## Observations (non-blocking)
1. **Overflow wedges the strategy.** After an overflow in the feature computation, advance_time(T) returns OVERFLOW repeatedly for the same T (Q9). State is rolled back as required, but the session cannot progress past that decision. src/strategy.cpp:14. This is consistent with "fail without partial mutation", but callers must treat OVERFLOW as terminal for the session.
2. **Q18** logs MAX_ENTRIES 4 times across the session, once per suppressed signal. This is cosmetic.
3. **Q20 (D=88/89 sizing boundary) was not independently exercised.** The probe's synthetic bars always produced SIZE_ZERO, so the probe does not discriminate the boundary. The boundary relies on the accepted sizing tests (test_sizing, passing) and the operator review, which says the arithmetic checks out. This is a material gap in my independent coverage, not a failure of the candidate.
4. **Q22/Q23 were informational.** With TMAX/16 prices no overflow was reached. A non-positive stop was not reproduced, and the observed stop was 12 for a long filled at 22 with a small distance.

## Limitations
- Calendar is bounded to two pinned dates (2026-10-27 CDT, 2026-11-03 CST). It is not a full exchange calendar, and nothing was verified about holidays or early closes.
- Coverage is Linux only (WSL2, GCC/Clang as installed). Other platforms and compilers were not tested.
- All data is synthetic. There is no inference about profitability, edge or account-rule compliance. Account rules remain unverified.
- The QA run was cut short by the turn limit once earlier. This report is based on the final builds and probe runs above.
- Some of my sandboxed commands were denied partway through, so I did not run a final clean rebuild of all four trees. The existing build trees were verified up to date with `cmake --build` and then run under ctest.

## Commands
`cmake --build build-qua13-diag`; `ctest --test-dir build-qua13-{dbg,san,rel}` (62/62 each); `ctest --test-dir build-qua13-diag` (24/24).
