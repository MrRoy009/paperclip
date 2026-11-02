Task: QUA-12

Implemented MNQ15-BRK-v1.1 for independent review; no self-approval.

Base commit inspected: 803828adf176f5ca26bf4e86c4ec5a4853bfdaae.
Branch: codex/engineer. Delivery is currently UNCOMMITTED; no implementation
commit exists at the time of this report. Review the working tree against that
exact base. Existing AGENTS.md changes and untracked operator-supplied
core-design/spec/test/review documents preceded this work and were preserved.

Changes:
- include/fnq/strategy.hpp: session, bar, features, order and deterministic log API.
- src/bars.cpp: bounded CT fixture and validated single-session construction.
- src/features.cpp: checked closed-bar channel/TR aggregation and distance.
- src/orders.cpp: next-quote execution, fixed stops, OCO, cooldown, diagnostic loss.
- src/strategy.cpp: causal bar decisions, TTL/flat/end clocks, transactional updates.
- tests/strategy.cpp and CMakeLists.txt: executable S01–S30 plus S31 review cases.
- README.md: reproducible offline build/use, sanitizer commands and scope.
Core source/API and existing test source files are unchanged.

Actual verification (GCC 15.2.0, C++20, Linux):
- Clean Debug configure/build in /tmp/qua12-clean-debug: succeeded; 62/62 passed.
- Clean Debug ASan+UBSan configure/build in /tmp/qua12-sanitized: succeeded.
  With ASAN_OPTIONS=detect_leaks=0 and UBSAN_OPTIONS=halt_on_error=1: 62/62 passed,
  no sanitizer diagnostics. All 31 original tests passed in both builds.
- LeakSanitizer attempt with detect_leaks=1 failed at runtime because LSan does
  not work under this environment's ptrace. Leak checking remains unverified.
- git diff --check passed before handoff documentation; checked again at delivery.

S01–S30 run actual quotes through bars and strategy, including ties, size 88/89,
minimum stop, cutoff, expiration, gap P&L, same-event arming, opposite flattening,
three-entry cap, missing/halt window recovery, CT offsets, no cross-session TR
and core causal rejection. Cooldown includes exact-boundary containment.
S31 also exercises duplicate/nonpositive/crossed/old quotes and bid+ask overflow
without clock/order/log mutation; TR-sum and ledger-P&L overflow rollback;
no-quote entry cancellation and timer priority when flat-by equals a bar label;
retention of an earlier opposite exit across flat-by. S12 tests deadline minus
one nanosecond and exact deadline with and without quotes. S21 tests exact end,
no-quote end and frozen unclosed ledger. No expected rule was altered to pass.

Assumptions/limitations:
- Calendar is only the explicit 2026-10-27 CDT and 2026-11-03 CST fixture.
  Unsupported dates/offsets rejected; no complete IANA/exchange calendar claimed.
- Explicit synthetic halts invalidate intersecting bars. No forward filling.
- Caller chooses one dated contract and supplies chronological integer quotes;
  fresh instance required for each session, especially after flat-by failure.
- All fees/budgets/caps/slippage allowances are synthetic. Account rules remain
  unverified. No tuning, economic experiment, chronological performance study,
  B2/B3, ML, real-data use, network, trading connection or order transmission.
- State copying and unbounded event/log retention prioritize atomicity and audit
  clarity over performance. Memory allocation failures are ordinary C++ exceptions.
- Tests verify deterministic software behavior, not edge or account compliance.

Next action: independent reviewer inspects this working tree and test evidence;
operator commits the listed implementation/document files if git protection
prevents the Engineer commit. Do not merge, push or execute trades.

Commit blocker (actual): the single git add attempt failed before staging with
"Unable to create '/home/lunar/fundednext-dev/.git/worktrees/fundednext-engineer/index.lock': Read-only file system".
No commit attempt can proceed; no permissions were changed and no bypass was
attempted. Exact reviewed base remains 803828adf176f5ca26bf4e86c4ec5a4853bfdaae;
implementation remains explicitly uncommitted. Operator commit file list is the
nine changed/new implementation and documentation paths listed above (including
this handoff); leave pre-existing operator files under their owner's control.
