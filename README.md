# FundedNext offline development

Scope: research, C++20 development, independent testing, and simulated execution only.

No trading credentials or live execution are authorized.

## Team

1. Quant / ML Research Lead: Claude Code with the owner's Claude Pro subscription.
2. C++ Engineer: Codex with the owner's ChatGPT Plus subscription.
3. QA / Risk Auditor: Claude Code with the same owner's Claude Pro allowance.

Run one task at a time, manually. Subscription allowances are shared, not multiplied by agent count. No API fallback or paid overage.

## First tasks

- Quant: write docs/research-plan.md covering a non-ML baseline, causal features, chronological validation, leakage checks, costs/slippage, and unresolved requirements.
- Engineer: after specification approval, implement a CMake/CTest C++20 skeleton with synthetic events, configurable risk checks, and in-memory simulated execution. Test stale data, duplicate events, invalid prices, position limits, and a latched kill switch.
- QA: independently review the exact engineer commit, produce reports/initial-audit.md, and mark findings pass, fail, or unverified. Do not change audited implementation.

## Review report

Task:
Commit/files reviewed:
Changes:
Tests and results:
Assumptions:
Unverified items:
Permission blockers:
Next proposed task:
