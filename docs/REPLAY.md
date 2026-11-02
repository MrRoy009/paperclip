# Bounded offline MNQ quote replay

This milestone builds replay infrastructure. **No historical MNQ replay has
been performed** because no suitable free event-level MNQ bid/ask dataset was
verified. Generated fixtures are always labelled synthetic. This does not report
strategy profitability, training results, or account compliance.

## Build and run (WSL/Linux)

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build --output-on-failure
python3 tools/replay.py --manifest data/manifest.json --quotes data/quotes.csv --runner build/fnq_replay_session --out results/new-run
```

Python 3 with an installed IANA system timezone database is required. The runner
records the database version and hash of America/Chicago bytes; this environment
uses tzdata 2026c. No packages or network access are required at runtime.

Input manifest example (not itself a historical dataset):
```
{
  "kind": "historical_user_supplied",
  "contract": "MNQZ24",
  "source": "provider URL, export description and date range",
  "timestamp_basis": "UTC_UNIX_NS",
  "quote_coverage": "event_bbo",
  "adjustment": "unadjusted",
  "calendar_policy": "synthetic_regular_hours",
  "sessions": ["2024-11-05"]
}
```

Exact CSV header: `timestamp_ns,contract,bid,ask`. Prices are decimal index
points (20000.25), not ticks or provider fixed-point nanodollars. Times are
UTC Unix nanoseconds. Rows must remain in original chronological feed order;
equal timestamps preserve input order. Local replay sequence is assigned from
that order; it is not an exchange sequence or proof of feed completeness.

One dated outright contract only, matching `MNQ[HMUZ][0-9]{2}`. No continuous,
back-adjusted, candle, trade-only or proxy instrument inputs. Raw provider data
needs an explicit, verified conversion to this schema first. Do not infer BBO
from OHLC, assume a spread, or forward-fill missing bars. One run cannot roll
contracts. Source metadata is a declaration, not independent authentication.

## Session and data checks

- Select 1..60 weekday dates explicitly, years 2019..2035. Historical dates
  must be completed. UTC boundaries are generated from pinned America/Chicago.
- The **synthetic** window remains 08:30–15:00 CT, last entry 14:00, flat-by
  14:50. No exchange holiday, early-close, halt-status or account calendar is
  inferred. Exclude such sessions only with a separately verified manifest;
  this tool does not certify their suitability. Explicit holiday/early-close
  support and real-data feed/status normalization remain future work.
- All input rows must be inside the selected windows. Off-session rows cause
  rejection; filter explicitly as a separate, provenance-preserving operation.
- Missing selected days are reported as 26 missing bars, not silently dropped.
  Sparse bars and no-quote timer failures are flagged. Maximum quote gaps include
  the interval from open and the interval to end. Nonmissing bars alone never
  establish that a full event-BBO feed is present.
- Malformed rows, ordering errors, mixed symbols, off-grid/crossed quotes,
  overflow and strategy errors abort the entire run, without publishing results.
  Existing result directories are never overwritten. Identical rows are retained;
  duplicate timestamps are legal, not silently sorted or deduplicated.
- The transactional strategy copies full state per quote. **25,000 input rows
  per run is a deliberate prototype cap**, not a full-history capacity claim.
  It rejects larger files rather than sample/drop quotes. A scalable engine is
  needed for full-resolution multi-day MNQ datasets. No automatic downsampling.

## Outputs and interpretation

`summary.json`: hashes of quotes, manifest and executable; timezone provenance;
gross, fees, net, session quality, and closed-trade drawdown. `sessions.json`:
per-session state, gaps, missing bars, failure flags and deterministic decision/
fill logs. `trades.csv`: matched closed trades and fee-inclusive net amounts.
Long and short directions use the actual recorded entry position. For closed
sessions the trade report must reconcile to the core ledger before publication.

An unfilled session-end exit retains its open position and paid entry fee; no
synthetic close or imaginary profit is added. Closed-trade drawdown omits
intratrade losses, open P&L, and entry-fee excursions, and must not be used for
FundedNext drawdown compliance. Fees are the pinned synthetic $1 per side;
the 8-tick reserve is only a sizing reserve, not additional execution slippage.
No liquidity/queue/latency model is implemented.

`performance_evaluation_valid` is always false at this milestone. Even with
user-supplied history, this is an unverified-source diagnostic replay, not an
accepted historical-performance study. All generated fixtures use
`SYNTHETIC_PIPELINE_TEST`; they must never be renamed historical results.

## Free-source investigation (2026-11-02)

- Historical Futures Data's [official MNQ page](https://historicalfuturesdata.com/browse/MNQ)
  lists no-account 10-second **bars** and paid tick history. Its free bar samples
  are not event BBO and were not substituted for this strategy's quotes.
- [Kibot's official samples](https://www.kibot.com/free-historical-intraday-data.html)
  include trade-time bid/ask for IVE and WDC, not MNQ event BBO. They were not used
  as futures data.
- [Databento MBP-1 documentation](https://databento.com/docs/schemas/mbp-1)
  describes event-level top-of-book and provider price/timestamp fields; it is a
  format reference, not a verified free MNQ dataset or authorization to buy data.

No purchases, paid API fallback, new provider accounts or trading connections
were made. No suitable verified freely downloadable MNQ event-BBO history was
found in this bounded investigation; this does not claim that none exists.

## Read-only online download check

`python3 tools/online_history.py --month 2026-10 --out results/new-sample`
downloads a public free MNQ bar sample and records transfer/hash checks. It
explicitly reports incompatibility with event-BBO replay and never runs trades.
See reports/online-history-check.md. Agent network permissions are unchanged.
