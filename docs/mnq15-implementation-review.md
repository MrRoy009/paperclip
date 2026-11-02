# Operator review and implementation clarifications — MNQ15-BRK-v1.1

Reviewed entry/exit/stop/position rules and S01–S30. Long/short arithmetic, D=43,
fee-inclusive risk=2750 cents, D=88/89 sizing boundary and gap-loss values check out.
Approved for synthetic offline implementation only, with these binding clarifications:

1. Validate an incoming quote's sequence, timestamp, positive prices and bid<=ask
before allowing it to advance timers or change state. Invalid input changes nothing.
For a valid quote, run due clock decisions against strictly earlier admitted quotes,
then admit the quote and execute eligible fills. Quotes at a bar boundary cannot
contribute to the just-closed bar. Distinct quotes at the same timestamp remain
ordered by sequence; boundary decisions execute once before the first such quote.
2. A caller must expose advance_time(T) so TTL, flat-by and session end run even
when no quotes arrive. Time cannot regress; a quote older than advanced time is
rejected. At exactly entry deadline entry expires before fills (half-open TTL).
3. Session end 15:00 is a hard execution boundary. An open position at end records
FLAT_BY_FAILURE even if the next quote is exactly 15:00. Cancel all outstanding
orders, freeze failed session and retain the unclosed position/ledger for reporting;
never fill at or after end or silently reset/carry into another session. Require an
explicit fresh strategy instance for another session after this failure. S21 means
no eligible quote in [14:50,15:00), including the exact-end boundary test.
4. At flat-by cancel pending entry. If a position already has an opposite-signal
market exit pending, retain that order and its original reason/causal sequence;
do not duplicate it. Otherwise create FLAT_BY market exit. Either pending market
exit has priority over a resting stop on the same quote. At/after flat-by no new
bar decisions. Remove the unsupported assertion that a quote necessarily arrives
well before 15:00: that is contingent, not guaranteed.
5. Bars i-8..i must be present for entry/opposite exits; earlier gaps outside that
window do not permanently disqualify the session. Cooldown follows half-open
bar containment: an exit exactly at 11:15 belongs to the bar labelled 11:30.
6. All half-tick OHLC/TR/sums, time arithmetic, stop offsets and P&L use checked
integers. Overflow fails without partial mutation. Input prices can cause bid+ask
overflow even though each individually fits; test this. Halts are supplied as
explicit synthetic UTC intervals, invalidate every intersecting bar, and are not
inferred from missing quotes. Missing bars never forward-fill.
7. Support a documented, bounded session calendar. Use a pinned, locally available
America/Chicago timezone database if practical; otherwise accept explicit UTC
session boundaries with validated offset and include a small pinned 2026 CT
calendar/offset fixture supporting S24. Reject unsupported dates rather than
guessing offsets. No network dependency or claim of a full exchange calendar.
8. The economic rationale is an untested hypothesis. Remove the unverified paper
citation from the implementation's accepted rationale rather than rely on memory.
A positive finite random-walk sample does not by itself prove a bug: inspect
sampling uncertainty. B2 matching, B3/control experiments, training, real-data
backtesting and account compliance remain deferred; they do not block this
deterministic implementation. No claim of edge or verified account suitability.

Acceptance: S01–S30 executable tests (split variants where needed) plus the boundary,
invalid-event/no-mutation and checked-overflow cases above; existing 31 tests pass.
Use actual quote sequences to exercise bar builder and state machine, not merely
precomputed expected objects or tests that duplicate implementation formulas.
