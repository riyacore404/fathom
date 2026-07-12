# Investigation Notes: Building an Accurate Order Book from Real Data

This document walks through the actual debugging process behind Fathom's order book reconstruction — three real bugs, all found by validating against real historical data rather than assuming correctness from passing unit tests alone. It's kept separate from the main README so the README stays scannable while this stays complete.

## Background: why unit tests weren't enough

The order book core (insert, cancel, match) passed a full suite of hand-constructed unit tests before any real data was involved. That proved the logic was *internally* correct for the cases we thought to test. It did not prove the reconstruction was *actually correct* against a real, messy trading day — and it wasn't, in three separate, genuine ways, all only found by replaying real LOBSTER data and checking the results against ground truth.

## Bug 1: Level truncation causes phantom orders

**Symptom:** replaying a full day of level-1 LOBSTER data produced a crossed book (`best_bid > best_ask`) at message index 157.

**Root cause:** LOBSTER's message file only reports events affecting the top-N tracked price levels. An order resting at price level 1 (`16797917`, a sell order) appeared exactly once in the entire file — inserted, then never mentioned again — because once a better order arrived and pushed it out of the tracked top-1 range, LOBSTER stopped reporting on it entirely. Our reconstruction had no way to know it was gone, so it sat there forever as a phantom, corrupting the book.

**Verification:** re-ran with LOBSTER's level-10 sample data. The same order's full lifecycle (insert, then a proper deletion 0.26 seconds later) was fully captured — confirming the mechanism, since more tracked depth meant the order stayed within the tracked window long enough for its removal to actually be reported.

**Fix:** bounded the `OrderBook` to the same tracked depth as the source data (`enforce_level_cap()`), evicting the worst price level — and forgetting its orders entirely — whenever the book exceeds the configured level count. This deliberately mirrors LOBSTER's own truncation boundary rather than trying to maintain unbounded depth from inherently incomplete data.

## Bug 2: Marketable limit orders weren't matched

**Symptom:** even with level-10 data and level-capping in place, a crossing still occurred — this time a **locked market** (`best_bid == best_ask`), at message index 11157.

**Root cause:** `insert_limit_order` unconditionally rested every incoming order, even when its price crossed or matched the opposite side's best price. A buy order arriving at the current best ask should trade immediately, not join the book as a second resting order at the same price.

**Verification:** traced the specific order (`40613572`) and the genuinely-still-resting order it should have traded against (`36228899`, confirmed active, not a phantom from Bug 1).

**Fix:** `insert_limit_order` now walks the opposite side first — exactly like `match_market_order` — consuming resting liquidity while the incoming price still crosses (using `>=`/`<=`, not strict `>`/`<`, since an exact price match is still marketable). Only the surviving quantity, if any, rests as a new order, at the order's own limit price.

## Bug 3 (in the reference validation script, not the C++ engine)

Worth including for completeness: while writing a Python reference implementation to cross-check the C++ logic, the reference script initially had an inverted level-cap direction (evicting the *best* price instead of the *worst* on one side of the book). It happened to produce a "no crossing detected" result — which would have been a false, misleading confirmation of Bug 2 as already fixed, had it not been re-derived and corrected before trusting it. Worth noting as a reminder that a validation tool is only as trustworthy as its own correctness, and got the same scrutiny as the production code.

## The residual gap: 17.2% mismatch, fully classified

After fixing both real bugs, the book was internally consistent (zero crossings across all 269,748 messages) but still showed a 17.2% message-level mismatch against LOBSTER's own ground-truth top-of-book snapshot. This did **not** decay over the trading day (excluding the first 50,000 messages barely moved the rate: 17.2% → 18.2%), ruling out a one-time "missing opening liquidity" explanation.

Every mismatch was classified into one of three cases:

- **Case A — invisible order:** LOBSTER's reported best price doesn't exist anywhere in our book at all. Root cause: LOBSTER only reports an order's *insertion* if it's within the tracked top-N range at the moment it's placed. An order resting deeper that later rises into the top 10 as shallower orders clear generates no "entering top 10" event — the first (and only) message we'd ever see about it is a later cancel/execution referencing an order ID we've never encountered, which is correctly (and silently) ignored as unknown.
- **Case B — partial quantity deficit:** the price level is known to us, but our tracked quantity is *less than* LOBSTER's reported size — never more. This one-directional bias (confirmed across every Case B example found) is itself evidence there's no double-counting or duplicate-insertion bug: we can only ever be missing orders we were never told about, never fabricating extras.
- **Case C — phantom depth:** the mirror image of Case A — our book holds a price level that doesn't appear anywhere in LOBSTER's real top-10 at all. This happens when our own level-cap eviction, based on *our* (necessarily incomplete) view of depth, diverges from the exchange's true depth ranking that LOBSTER's reporting boundary actually uses.

All three cases trace to the same single structural mechanism: our level-cap decisions are based on our own reconstructed depth, while LOBSTER's reporting boundary is based on the exchange's true full-depth ranking. Those two rankings drift apart over time in both directions. This is not fixable with more code — it would require a full, unrestricted-depth data feed (e.g., raw NASDAQ TotalView-ITCH, or LOBSTER's paid full-depth tier), not something available in a free academic sample.

**No mismatches were left unclassified.** The bar met here isn't "zero output error" (not achievable from this data source) — it's a codebase with a fully diagnosed, zero-unexplained-residual error profile, which is a stronger and more honest standard.
