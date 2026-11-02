# Exploratory bar screen (separate from quote engine)

See docs/mnq-bar-study-plan.md and reports/mnq-bar-study-report.md.
Runs on Python with pyarrow and pandas; available on the owner's Windows host.
The study does not change the C++ strategy or its tests. Historical bars are not
included in Git. Use the exact already downloaded free file whose hash is in
the plan. No credentials, downloads, services or order transmission in this code.

```
python research/test_bar_study.py
python research/bar_study.py --data /path/to/cme.mnq.bars.10s.2026-10.parquet --plan docs/mnq-bar-study-plan.md --out /path/to/new-study-result
python research/independent_bar_check.py --data /path/to/cme.mnq.bars.10s.2026-10.parquet --sessions /path/to/new-study-result/sessions.json --out /path/to/new-study-result/independent-check.json
```

The frozen study is deliberately tied to October2026 and the exact file hash.
Revising it for a fresh sample is a new, explicitly recorded study, not silent
parameter tuning. Current result:32 rejected signals, no trades, budget50USD
versus minimum required risk106USD; only20 of22 expected sessions observed.
