# Fathom

A tick-by-tick limit order book (LOB) replay engine for realistic execution backtesting, built from scratch in C++.

Most free backtesting tools assume you get filled instantly at the quoted price with no queue position, no market impact, and no execution risk. That assumption is false, and it's why strategies that look profitable in a naive backtest routinely fail live. Fathom replays real historical order flow through a price-time-priority matching engine, so a strategy's orders compete for queue position against genuine historical activity — and shows, with real numbers on real data, exactly what that costs and what it buys you.

## Headline Result

The same intended trade (buy 10 shares of AMZN), backtested two ways against the same real trading day:

| | Fill rate | Fills | Avg. time-to-fill | Avg. slippage vs. arrival mid |
|---|---|---|---|---|
| **Naive** (instant fill at the ask) | 100% | 1 | 0s | +750 ticks (worse) |
| **Realistic** (resting order, real queue) | 100% | 4 (partial) | 112.72s | −750 ticks (better) |

Resting passively instead of crossing the spread bought **1,500 ticks ($0.15/share) of price improvement**, at the cost of **112.72 seconds of execution-time risk** and four separate partial fills instead of one clean fill. This is the core, quantified thesis of the project.

## What's in this repo

- **Order book core** (`src/order_book.*`) — price-time-priority matching engine: limit order insertion (with marketable-order matching), cancellation, partial reduction, market order matching, and price-level depth capping.
- **LOBSTER data ingestion** (`src/lobster_parser.*`, `src/replay.*`) — parses real historical tick data and replays it through the engine, validated message-by-message.
- **Strategy framework** (`src/strategy.hpp`, `src/strategies/`) — pluggable strategies that get injected into the real historical replay and compete for queue position.
- **Metrics** (`src/metrics.*`) — fill rate, time-to-fill, and slippage, computed from real fills, not assumptions.
- **Market impact model** (`python/`) — a LightGBM classifier predicting short-horizon price direction from pre-trade order book state, trained on features logged directly from the C++ engine.

## Validation, honestly reported

Fathom's book reconstruction is **internally consistent** — zero bid/ask crossings across a full trading day (269,748 real messages, AMZN, 2012-06-21, LOBSTER level-10 data).

It does **not** achieve exact agreement with LOBSTER's own ground-truth snapshot at every message — we measured and fully root-caused a 17.2% message-level mismatch rate. This is a structural property of leveled LOBSTER data (any order that exits the tracked depth becomes invisible to message-file-only reconstruction, even though the exchange's real book keeps tracking it), not a defect in the matching engine. Every mismatch was classified by root cause with concrete traced examples, and zero were left unexplained.

**Full investigation, including the three real bugs found and fixed along the way, is documented in [`docs/INVESTIGATION.md`](docs/INVESTIGATION.md).**

## Performance

Replays a full trading day of level-10 order flow — 269,748 events — in **23ms** (Release build, Apple M-series). No optimization work was needed beyond compiling with optimizations enabled; the data structure choices (sorted maps for price levels, FIFO deques for queue order, a hash map for O(1) order lookup/cancellation) were sufficient on their own.

## Market Impact Model

A LightGBM classifier predicts the **direction** (up/down) of price movement over a 20-message horizon following a real execution, using pre-trade order book state (spread, top-3 depth on each side, order flow imbalance, trade size).

| | Accuracy | ROC-AUC |
|---|---|---|
| Model (chronological holdout) | **87.9%** | **0.891** |
| Majority-class baseline | 53.5% | — |

**Why a classifier, not a regressor:** a regression model targeting exact price-move magnitude was tried first. It showed real, non-outlier-driven signal (R² of 0.14 even after excluding the most extreme 1% of moves) but its mean absolute error did not meaningfully beat a naive zero-move baseline on typical-sized moves — it wasn't well-calibrated for magnitude. Directional accuracy, however, was strong and robust (88%). This matches published market microstructure research: direction from order-flow imbalance is a substantially more tractable signal than precise impact magnitude. The model was deliberately reframed to match where the real, defensible signal lives, rather than reporting a magnitude-prediction number that looked fine in aggregate but didn't hold up under scrutiny.

Train/test split is **strictly chronological** (first 70% of the trading day for training, last 30% for testing) — never randomly shuffled, since a random split on time-series data leaks future information backward into training.

## Building (C++)

Requires CMake 3.20+ and Catch2:
```
brew install catch2
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix catch2)
cmake --build .
```

Run the tests (46 test cases, 91 assertions):
```
./fathom_tests
```

Run the naive-vs-realistic comparison against the included AMZN sample data:
```
cd .. && ./build/fathom
```

Other CLI options:
```
./build/fathom --help
./build/fathom --ticker AMZN --date 2012-06-21 --level 10
./build/fathom --log-impact-data   # regenerates the market impact training dataset
```

## Building (Python — market impact model)

LightGBM requires the OpenMP runtime on macOS, not included by default:
```
brew install libomp
```

Then:
```
cd python
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python3 train_impact_model.py ../data/AMZN_impact_features.csv
```

## Data

Sample data (AMZN, 2012-06-21, level 10) sourced from [LOBSTER](https://lobsterdata.com), an academic limit order book dataset derived from NASDAQ TotalView-ITCH.

## Architecture Notes

- **Prices are integer ticks (`int64_t`), never floating point.** Floating-point price comparisons after arithmetic aren't guaranteed exact; LOBSTER's native format (dollars × 10,000) is already integer-friendly.
- **Order lookup is O(1) via a hash map** from order ID to (price, side), avoiding a linear scan through price levels on every cancel.
- **The matching engine is deterministic and contains no AI or ML components**, by design — correctness and auditability come first; the AI layer is a separate, optional analysis pipeline that reads engine output, never the other way around.
- **The book is explicitly bounded to the source data's tracked depth** (level 10 in this dataset), rather than growing unbounded — see `docs/INVESTIGATION.md` for why this specific design decision was necessary, not optional.
