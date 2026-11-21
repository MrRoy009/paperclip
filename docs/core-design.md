# Offline core milestone v1.1

The accepted specification is unchanged in engineer-milestone-1.md (source Quant
commit f16a014870d2d8a5a60e4c99fe62cb94dc57d52e). All fixtures are synthetic.
No account rules have been calibrated or verified. MES/MNQ integer constants are
copied from the accepted specification; no market data or live connection is used.

`fnq_core` exposes checked signed integer arithmetic, exact quarter-point parsing,
positive whole-contract parsing, total-loss-budget sizing, an admitted event stream,
and a one-instrument FIFO ledger. Errors carry no arithmetic result. The sizing
result instead carries zero size on error, as required by the specification.
BUY/SELL express direction; order quantity and fill price must be strictly positive.
Zero fees and zero sizing slippage are explicit synthetic scenario assumptions.

Validation precedence follows the specification's listed order:

1. Sizing checks positive budget, stop, tick value and cap before negative costs,
   then computes stop cost, both fees, slippage reserve and sums with checked math.
2. Events check sequence, timestamp, positive bid/ask, then crossed quote. Errors
   include the offending sequence. Only admitted events affect subsequent validation.
3. Fills check inputs (side, positive quantity/configuration, positive price, costs),
   candidate position cap, admitted event reference, strict decision/fill causality,
   then FIFO gross P&L, full-fill fee, cumulative fees and net arithmetic.
   Cap is checked before event causality in the contract's cap/events/ledger order.
   Unknown event references use INVALID_EVENT; invalid sides use INVALID_SIDE.

Position candidates use int64 arithmetic for int32 quantities, so signed-minimum
magnitudes cannot overflow. FIFO lots and all accounting are calculated in a local
candidate; successful fills alone publish state and last-fill sequence. The external
admitted stream is read-only during fills. `last_fill_seq` records the most recent
successful fill; the contract does not impose additional fill-sequence uniqueness
or fill-to-fill chronology beyond admission and decision causality.

Parsing uses no rounding and no binary noninteger arithmetic. Price syntax is an
optional minus, decimal digits, and optional nonempty decimal fraction; whitespace,
exponents and leading plus are rejected. Parsing may represent signed ticks; event
and fill boundaries enforce positive prices. Quantity fractions report FRACTIONAL_QTY;
nonpositive integer quantities report NON_POSITIVE_INPUT.

One test executable covers each area: arithmetic, sizing, events, ledger. CTest
registers the 30 names from the frozen specification. The no-float check examines
all public core headers at test time. No application data files are read by core
or numeric tests. Build tools and the static source check access ordinary source
and build files. Allocation failure uses standard C++ exception behavior rather
than a domain error; candidate construction still precedes state publication.

Limitations remain exactly those of the bounded milestone: no calendar/account-day
logic, execution adapter, real data, strategy, slippage fill model or compliance or
profitability claim. Slippage is only a sizing reserve. Independent QA is pending.
