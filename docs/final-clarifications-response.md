# Final Engineer handoff: two clarifications resolved

Research context: plan revision 6. Authoritative first implementation contract:
engineer-milestone-1.md version 1.1. Implementation has not started.

1. Sizing now includes stop loss, both sides' fees and an explicitly supplied stressed
   round-trip slippage reserve. It rejects invalid costs and overflow and rounds down.
   MES example: 8 ticks × 125 cents + 2 × 50-cent fees = 1100 cents; a 1000-cent
   budget returns zero contracts. With two extra slippage ticks, reserve is 1350 cents.
   The reserve is not separately debited in the ledger and is not a real loss guarantee.
2. FIFO entry-lot accounting now defines reductions and reversals, the residual entry price,
   fee attribution and atomic rejection. The worked MES reversal closes at +500 cents,
   opens the residual short at 20004 ticks and finishes at +1000 gross less 200 fees =
   800 cents net. The milestone now names 30 acceptance tests with explicit expected results.

Only illustrative arithmetic was checked by the operator. No C++ implementation, build,
backtest or statistical validation has occurred. Account-specific calibration remains later
work. The Engineer should implement only the bounded synthetic core milestone; QA should
test the exact resulting commit independently. All live trading and paid services remain
outside scope.
