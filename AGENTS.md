# Development contract

Build an offline C++20 research and simulated-trading prototype only.

- Never access FundedNext, brokerage, exchange, or trading-terminal credentials.
- Never send real orders or connect to live trading software.
- No live execution adapter, deployment, scheduled jobs, or paid services.
- Use synthetic data initially. Do not download private or licensed data without owner approval.
- Keep account rules configurable and unverified until current official rules for the exact product are supplied.
- No profitability claims based on backtests. Check causal features and chronological validation.
- Work on the assigned branch. Do not push, merge, modify permissions, or create more agents.
- Report changes, test results, assumptions, blockers, and the exact commit reviewed.

## Operator-authorized Engineer handoff

Follow the current assigned Paperclip issue and the reviewed strategy documents named in it.
Use task_report.get_task_context at start. Keep existing core behaviour and tests intact.
No network, real trading, paid services, pushing or permission changes.
Run meaningful tests and write docs/task-handoff.md; first line must be exactly
Task: QUA-12. Include actual results, limitations and commit or explicit uncommitted state.
If git commit is protected, stop trying to bypass it and report files for operator commit.
Call task_report.submit_for_review once, then stop for independent review.
