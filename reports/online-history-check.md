# Online historical-data connection check

The owner requested an internet-connected historical-data workflow, retaining
the prior free-data-only requirement. Online here means read-only historical
downloads. No live trading, market-order connection, public hosting, or network
permission changes to the agents were enabled.

Added tools/online_history.py: a fixed-provider HTTPS GET for the documented
free MNQ 10-second monthly sample. No keys or paid endpoints; redirects are
rejected. Download is limited to 16 MiB, with 30-second network timeout, exact
byte-count check when supplied, SHA-256 and Parquet container/footer checks.
Columns are not decoded. Existing output directories are not overwritten.
This is an online data acquisition check, not a historical strategy backtest.

Actual 2026-11-02 run: public October 2026 sample downloaded, 2,951,591 bytes.
SHA-256: dadae7d14298c599677a481ac8a4b522b21a20741169aad7928a603da43e0d6a.
Provider docs classify these samples as adjusted continuous OHLCV bars, not
the unadjusted dated-contract event BBO required by the accepted strategy.
The sample was not converted to invented quotes or fed to the strategy.

10 isolated transfer/validation tests pass, covering failure to pay (HTTP402),
redirects, truncated/oversized files, bad container/footer, path injection and
overwrite rejection. These are mocked checks; the live download above separately
verifies actual free network access. No strategy code changed in this addition.

Run from the checkout:
```
python3 tools/online_history.py --month 2026-10 --out results/new-online-sample
python3 tools/test_online_history.py
```

Plugin discovery did not identify a suitable connected historical MNQ provider.
If "historical tool" means Historical Futures Data's MCP, its official docs
require an existing paid API key for every tool. Its run_sma_backtest is a
different long/flat SMA strategy, not this MNQ15 breakout. Neither a paid key
nor permission to change strategy or spend money was supplied. A question is
pending for the exact intended tool name/link.

Source: https://historicalfuturesdata.com/docs (checked 2026-11-02).
Free sample: https://historicalfuturesdata.com/samples/MNQ/10s/cme.mnq.bars.10s.2026-10.parquet

QUA-15 remains blocked on suitable free event-BBO data, scaling beyond the
25,000-row prototype cap and verified calendar/feed semantics. No historical
strategy result exists. All earlier offline test results retain their scope.
