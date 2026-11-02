# Independent-QA repair: fill-event ordering

Ledger::fill now rejects a fill with seq lower than last_fill_seq as FILL_EVENT_REGRESSION,
without changing any logical state. Equal sequence numbers remain allowed for legitimate
same-event partial fills. Every fill must still reference an admitted event and have a
sequence strictly greater than its own decision's information boundary.

An older pending order may fill on a later event after another order has filled. The
decision_seq parameter belongs to that individual order; it is not the ledger's global
current time. No stale-order TTL or order-ID deduplication exists in milestone 1. The
caller must manage cancellations/deduplication in a later execution layer. Supplied fill
prices are validated as positive integer ticks but are not constrained to a quote: the
fill/slippage model remains outside this core milestone.

Regression R01 covers backward sequences 5->3 and 9->1, unchanged state on rejection,
same-event partials and a later fill for an older pending order. It supplements the 30
accepted milestone tests; it does not replace or weaken any of them.
