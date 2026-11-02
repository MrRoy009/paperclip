# MNQ15 offline implementation acceptance

Candidate reviewed: 51ebcd84b160ae7a8c37c733e52ff505055249fa.
Independent QA QUA-13: PASS with observations; no blocking implementation defect.
Accepted by the operator after the owner's request to review, repair and commit.

Verification:
- Independent QA: Debug 62/62, ASan+UBSan 62/62, Release 62/62.
- QA diagnostic suite: 24/24 executed; Q20, Q22 and Q23 were informational or
  inconclusive and are not counted as independent validation of those claims.
- Operator clean Release: 62/62; final clean Debug: 62/62; final clean
  ASan+UBSan: 62/62, with leak detection disabled due to the environment.
- Separate operator sizing probe closes Q20's gap using real quotes: D=88
  permits one contract, places stop at 79953 after entry at 80041 and closes at
  net -4600 cents; D=89 creates no order and records SIZE_ZERO. Both pass under
  ASan+UBSan. Source: qa-diagnostics/strategy-boundary/probe.cpp.

Reproduce the extra boundary check after building the sanitizer libraries:
```
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I include qa-diagnostics/strategy-boundary/probe.cpp build-san/libfnq_strategy.a build-san/libfnq_core.a -o /tmp/mnq15-boundary
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /tmp/mnq15-boundary
```
QA probes have their own CMake project in qa-diagnostics/strategy/.

Limitations: only the two explicitly pinned CT session dates are supported. No
complete calendar, historical evaluation, ML, profitability evidence or verified
account rules. Overflow is a terminal session failure: callers must stop and
report it rather than keep retrying the unchanged state. Tests cover Linux/WSL
with GCC 15.2, not native Windows/MSVC. Leak checking remains unverified.

No core/strategy code changes were needed after independent review. The final
commit adds audit/probe evidence and this acceptance disposition to the exact
reviewed implementation. Nothing was merged to main, pushed or connected to
trading. Agent scheduling remains disabled.
