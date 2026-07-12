# Fathom, Explained From Scratch

This document assumes you know nothing about finance, trading, or how this project works internally. By the end, you should understand what Fathom does, why it matters, how every file fits together, and how it was actually built — bugs and all.

---

# Part 1: The Idea

## What problem does this solve?

Imagine you want to buy a used car. You see it listed for $10,000. You assume that's what you'll pay. But in reality — maybe there are other buyers ahead of you, maybe the seller changes their mind, maybe by the time you actually show up with cash, the price has moved. The "listed price" and the "price you actually pay" can be very different things, and *how* different depends on timing, competition, and how patient you're willing to be.

Buying and selling stocks works the same way. There's a "quoted price," but what you actually pay depends on:
- How many other people want to buy/sell at the same moment
- Whether you're willing to wait for a good price, or want it *right now*
- What's happening in the market in the seconds after you place your order

Most tools that let people test trading strategies ("if I had done X, would I have made money?") completely ignore this. They assume: "you wanted to buy, so you instantly got exactly the listed price." That's not realistic, and it means those tools can tell you a strategy is profitable when it actually wouldn't be in real life.

**Fathom is a tool that replays real, historical stock trading activity — second by second — and simulates what *actually* would have happened if you tried to place an order, instead of just assuming you got the price you wanted.**

## The core finding, in plain terms

We ran an experiment: "I want to buy 10 shares of Amazon stock." We tested it two ways using the exact same real historical trading day:

1. **The naive way** (what most tools assume): "I'll pay whatever the current asking price is, right now, guaranteed." Result: instant purchase, but you pay a slightly *worse* price than the "fair" middle price at that moment.

2. **The realistic way** (what Fathom simulates): "I'll place an order and wait in line at a price I prefer, like everyone else does." Result: you get a *better* price than the fair middle price — but it takes almost two minutes to actually go through, and it happens in four separate small purchases instead of one, because other real buyers and sellers were doing their own thing at the same time and only sometimes matched up with you.

**The takeaway: being patient got a better price, but "better price" isn't free — it costs you time and certainty.** That trade-off is real, it's measurable, and Fathom measures it using actual historical market data, not made-up numbers.

---

# Part 2: The Finance Concepts You Need (No Prior Knowledge Assumed)

## The "order book"

Picture a whiteboard at a farmers market stall selling apples. On one side, people write down "I'll pay $2/apple" or "I'll pay $1.90/apple" — these are **buyers**. On the other side, people write "I'll sell for $2.10/apple" or "I'll sell for $2.20/apple" — these are **sellers**.

- The highest price any buyer has offered is called the **best bid**.
- The lowest price any seller is asking for is called the **best ask**.
- The gap between them is called the **spread** (in our apple example, $2.00 to $2.10 = a 10-cent spread).

A stock exchange keeps exactly this kind of whiteboard, except electronically, with thousands of entries, updating constantly. This whiteboard is called the **order book** (or **limit order book**, LOB — that's what the "L" and "O" and "B" in Fathom's subtitle mean).

## Two ways to place an order

- **A limit order** is you writing on the whiteboard: "I'll buy at $2.00, no higher." You might not get an apple immediately — you're waiting for a seller to accept your price, or for the price to drop to meet you. But if you do get one, you got your price or better.
- **A market order** is you walking up and saying "I'll pay whatever the current asking price is, just give me an apple now." You get it instantly, but you pay whatever sellers are currently asking — which might be a worse deal than being patient.

## "Queue position"

If three people all write "$2.00" on the whiteboard for the same apple, and only one apple becomes available at that price, **whoever wrote it down first gets it first.** This is called **time priority**, and combined with "best price wins," it's called **price-time priority** — which is literally the rule Fathom's matching engine enforces.

## Why this all matters for testing a trading strategy

If a tool assumes you *always* get instant, perfect execution, it's like assuming you always get the first apple in line for free, every time. That's not how it works, and a strategy that looks great under that fantasy assumption might actually lose money in reality once you account for the real waiting, real competition, and real price movement. Fathom exists to strip that fantasy out and show the real picture.

---

# Part 3: The Big Picture — What Fathom Actually Is

Fathom has three main pieces:

1. **A matching engine** (written in C++) — a program that keeps track of a real order book (like the whiteboard) and correctly figures out who trades with whom, at what price, in what order. This is the mathematical/logical core, and it has to be *perfectly* correct — no fuzziness allowed, since it's simulating real rules.

2. **A historical data replay system** — feeds real, recorded stock market activity from one actual trading day (Amazon stock, June 21, 2012) through the matching engine, message by message, second by second, so we can watch a real trading day unfold and inject a hypothetical strategy into it.

3. **An AI/machine learning layer** (written in Python, separate from the C++ engine) — trained on patterns from the replayed data to predict whether the price is likely to go up or down in the next few seconds, based on what the order book looks like right before a trade happens.

---

# Part 4: The Blueprint — How The Pieces Connect

```
                     ┌────────────────────────────-─┐
                     │   Real historical data       │
                     │   (LOBSTER CSV files)        │
                     │   "who bought/sold what,     │
                     │    at what price, when"      │
                     └───────────────┬──────────────┘
                                     │
                                     ▼
                     ┌────────────────────────────--─┐
                     │   Parser (lobster_parser)     │
                     │   reads the CSV into          │
                     │   structured C++ data         │
                     └───────────────┬──────────────-┘
                                     │
                                     ▼
                     ┌────────────────────────────--─┐
                     │   Replay engine (replay)      │
                     │   feeds each historical event │
                     │   into the order book, one    │
                     │   at a time, in order         │
                     └───────────────┬─────────────-─┘
                                     │
                     ┌───────────────┴──────────────-─┐
                     ▼                                ▼
        ┌────────────────────-─┐            ┌─────────────────────────--┐
        │   Order Book         │            │   Strategy                │
        │   (order_book)       │◄─-──────-─►│   (strategy / strategies) │
        │   the actual         │  places    │   "I want to buy 10       │
        │   whiteboard logic,  │  orders,   │   shares" — either        │
        │   matching rules     │  gets      │   patiently (realistic)   │
        │                      │  fills     │   or impatiently (naive)  │
        └───────────┬──────────┘            └───────────────────────--──┘
                    │
                    ▼
        ┌─────────────────────-┐
        │   Metrics tracker    │
        │   (metrics)          │
        │   "how well did that │
        │   actually go?"      │
        └───────────┬──────────┘
                    │
                    ▼
        ┌─────────────────────┐             ┌─────────────────────────---┐
        │   Results printed   │             │   Impact logger            │
        │   to your terminal  │             │   (impact_logger)          │
        │   (main.cpp)        │             │   exports data for the     │
        │                     │             │   AI model to learn from   │
        └─────────────────────┘             └────────────-┬────────────--┘
                                                          |
                                                          ▼
                                            ┌─────────────────────────---┐
                                            │   Python AI model          │
                                            │   (train_impact_model.py)  │
                                            │   predicts: will price     │
                                            │   go up or down next?      │
                                            └─────────────────────────---┘
```

---

# Part 5: The Data We Used — LOBSTER

**LOBSTER** is a free academic dataset (from a German university, Humboldt University of Berlin) that reconstructs exactly what a real stock exchange's order book looked like, moment by moment, on real trading days. It's built from real NASDAQ stock exchange data.

We used one real day: **Amazon stock (ticker: AMZN), June 21, 2012**, covering the regular trading hours (9:30am to 4:00pm). This came as two files:

- **The "message" file** — a giant list of every single event that happened that day: "a new order to buy X shares at $Y showed up," "an order got cancelled," "a trade happened." 269,748 of these events happened in a single day for this one stock.
- **The "orderbook" file** — a snapshot of what the top of the whiteboard looked like *after* each of those events. We used this as an answer key to check our own reconstruction was accurate.

Each event row has 6 pieces of information: **when** it happened, **what kind** of event it was (new order / cancel / trade / etc.), a unique **ID number** for that order, **how many shares**, **what price**, and whether it was a **buy or sell**.

---

# Part 6: File-by-File Walkthrough

Here's every file in the project and what it's responsible for, in the order you'd want to read them to understand the whole thing.

### `src/order_book.hpp` and `src/order_book.cpp`
**The heart of the project.** Defines what an "Order" is (id, price, quantity, buy or sell), and the `OrderBook` class, which is the actual whiteboard: it can insert new orders, cancel orders, reduce an order's quantity (partial cancels/trades), and match orders against each other following the price-time-priority rule described in Part 2. Everything else in the project depends on this being correct.

### `src/lobster_parser.hpp` and `src/lobster_parser.cpp`
Reads the raw LOBSTER CSV files (both the "message" file and the "orderbook" snapshot file) and turns each row of text into structured data the rest of the program can use. Includes validation — if a row is malformed or has a value that doesn't make sense, it stops with a clear error message instead of silently producing wrong results.

### `src/replay.hpp` and `src/replay.cpp`
Takes the parsed historical events and feeds them into an `OrderBook`, one at a time, in the correct order — recreating the real trading day's activity. Also supports "injecting" a strategy's own hypothetical orders into that same real replay, so the strategy's orders genuinely compete against real historical activity for queue position.

### `src/strategy.hpp`
Defines the *interface* (a kind of contract) that any trading strategy must follow: it needs to be able to look at the current order book and decide what to do, and it needs to be told when one of its orders actually gets filled.

### `src/strategies/passive_quote_strategy.hpp`
A simple strategy that represents the **realistic** approach: place one patient order (a limit order) and wait to see what real historical activity does to it.

### `src/strategies/naive_instant_fill_strategy.hpp`
Represents the **naive** approach most backtesting tools assume: pretend you instantly, fully buy at the current asking price, no waiting, no queue. This strategy deliberately never places a real order in the book — it just calculates the hypothetical instant-fill outcome for comparison.

### `src/metrics.hpp` and `src/metrics.cpp`
Once a strategy's orders get filled (or don't), this calculates the numbers that actually matter: what fraction of the order got filled, how long it took, and how the price you got compares to the "fair" price at the moment you decided to trade (this comparison is called **slippage**).

### `src/config.hpp` and `src/config.cpp`
Handles settings — which stock ticker, which date, which data folder — so the program isn't hardcoded to only ever run on one specific file.

### `src/impact_logger.hpp` and `src/impact_logger.cpp`
A separate tool (not used during normal strategy backtesting) that walks through the historical data and, every time a real trade happens, writes down what the order book looked like right before it, plus what happened to the price shortly afterward. This produces a spreadsheet of examples for the AI model to learn from.

### `src/main.cpp`
The program's entry point — the file that actually runs when you type `./fathom`. Reads command-line settings, loads the data, runs the naive and realistic strategies side by side, and prints the comparison results.

### `tests/` folder
Every `.cpp` file here contains automated tests — small, specific checks that verify individual pieces of the program behave correctly (see Part 8 for more on this).

### `python/train_impact_model.py`
A separate Python program (not C++) that reads the spreadsheet produced by `impact_logger`, and trains a machine learning model to predict whether the price will go up or down shortly after a trade, based on what the order book looked like beforehand.

### `data/` folder
Where the raw LOBSTER CSV files and the AI training spreadsheet live. Not included in the code repository itself (too large, and not ours to redistribute) — has to be downloaded/generated separately.

### `README.md` and `docs/INVESTIGATION.md`
Human-readable documentation. The README is the quick overview; `INVESTIGATION.md` is the detailed story of the debugging process (see Part 9).

---

# Part 7: How It Was Actually Built, Step By Step

This is the real order things happened in, including the mistakes — because the mistakes and how they got found and fixed are a real, important part of the story.

**1. Set up the C++ toolchain.** Got a compiler, CMake (a tool that manages compiling multi-file C++ projects), and Catch2 (a testing framework) all working together, starting from an empty "hello world" program to make sure the basic machinery worked before writing any real logic.

**2. Built the order book's core data types.** Defined what a "price," a "quantity," an "order," and a "trade" even are in the code, deciding early to represent prices as whole numbers (like cents, but even more precise — ten-thousandths of a dollar) instead of using decimals, because decimal math on computers can be subtly inaccurate in ways that would corrupt financial calculations.

**3. Built the `OrderBook` class piece by piece**, writing an automated test for each piece *before* moving to the next: inserting orders, cancelling orders, and matching orders against each other. This took the most care, because it's the foundation everything else depends on.

**4. Downloaded real historical data (LOBSTER)** and wrote a parser to read it. Initially got data with very limited "depth" (only tracking the very best price on each side), which caused a real bug: orders that should have still been valid seemed to vanish from the data entirely, because the limited-depth version of the data simply stops reporting on an order once it's no longer among the very best prices. Fixed by getting more detailed data (tracking the top 10 price levels instead of just the top 1) and by explicitly telling our own program to "forget" about orders that fall out of that same tracked range — matching what the real data source does, on purpose.

**5. Found a second real bug**: the program was letting a buy order and a sell order sit at the same (or overlapping) price without ever trading with each other, when in real life they should have matched immediately. Fixed by teaching the order-insertion logic to check for this and trigger a trade automatically when it happens, before just parking the order on the whiteboard.

**6. Rigorously checked how accurate the reconstruction was** against the real data's own official record of what the order book looked like. Found it matched internally (no impossible states, like a buy price higher than a sell price) 100% of the time, but didn't perfectly match the "official record" 100% of the time — about 17% of individual moments differed. Investigated *why*, in detail, and confirmed it was a genuine, unavoidable limitation of the specific data being used (some order activity simply isn't included in this format of data at all) rather than a bug — and proved this by explaining every single mismatch's cause rather than just accepting the number.

**7. Built the ability to inject a hypothetical strategy into the real historical replay**, so a pretend order could sit in the real queue and see what real historical trading activity would have done to it.

**8. Built the naive vs. realistic strategies**, and the metrics to measure and compare them — producing the project's headline result described in Part 1.

**9. Checked performance** — confirmed the whole simulation (269,748 real events) runs in about 23 milliseconds, so speed was never a concern.

**10. Made the program more robust**: added clear error messages for bad input data instead of letting the program crash confusingly, and added command-line options so it's easy to point at different stocks/dates without editing code.

**11. Wrote a much larger set of automated tests** covering unusual situations (an order being partially cancelled down to exactly zero, two orders crossing at the exact same price, etc.) to make sure the program handles edge cases correctly, not just the "normal" cases.

**12. Built the AI layer.** First tried to predict the *exact size* of a price movement after a trade — this didn't work well (the predictions weren't meaningfully better than just guessing "no movement"). Rather than hide that, the approach was changed to predict just the *direction* (up or down) instead, which turned out to work quite well (getting it right about 88% of the time, compared to about 53% if you just guessed the more common direction every time). This kind of "the first approach didn't really work, here's the honest result, and here's the better-scoped approach that did" is a real and valuable part of the project's story.

**13. Wrote the documentation** (this document, the README, and the detailed investigation notes) last, once everything else was working and verified.

---

# Part 8: How Testing Works

Every piece of logic in this project has small, automated checks called **tests**, written using a testing tool called **Catch2**. A test looks like this, in plain English: "if I do X to the order book, Y should be true afterward." For example: "if I insert an order for 10 shares and then cancel it, the order book should show zero shares resting at that price."

There are 46 of these tests in the project, covering everything from basic operations to unusual edge cases. Every time any code changes, all 46 tests get run again automatically (`./build/fathom_tests`) — if even one fails, that's an immediate signal something broke, before it has a chance to quietly cause wrong results elsewhere. This is why the project could be built up in small, confident steps: each new piece was verified before building the next piece on top of it.

Beyond these small automated tests, there's also a bigger, real-world validation: running the entire program against the actual historical Amazon trading day and checking the results make sense (Part 7, step 6) — this is less like "does 2+2 equal 4" and more like "does this whole machine work correctly on a real, messy, complicated example," which is a different and complementary kind of confidence.

---

# Part 9: The Debugging Story (Why It Matters)

It would have been possible to build this project, have all 46 small tests pass, and *still* have it be wrong in a way that only shows up on real data — and that's exactly what happened, twice, before things were fully correct. Both bugs were found not by writing more small tests, but by running the whole system against a real historical trading day and noticing the results didn't make sense (a "buy" price ended up higher than a "sell" price, which is never supposed to happen in a real market).

The bigger lesson embedded in this project: **passing your own tests proves your code does what you told it to do — it doesn't prove your code does what reality actually requires.** Only checking against real, independent data caught these problems. The full details of exactly what went wrong and how it was diagnosed are in `docs/INVESTIGATION.md`, written specifically so someone else could follow the same reasoning.

---

# Part 10: The AI Model, Explained Simply

Machine learning models learn patterns from examples. We gave the model thousands of real examples that looked like: "right before this real trade happened, here's what the order book looked like (how big is the gap between buy and sell prices, how many shares are waiting on each side, is there more buying pressure or selling pressure) — and here's what actually happened to the price shortly after."

The model's job: given a *new* situation it hasn't seen before, guess whether the price is about to go up or down.

We deliberately tested the model fairly: it was only ever trained on the *first* 70% of the trading day, then tested on the *last* 30% it had never seen — like training someone using only the morning's events, then quizzing them on the afternoon, so there's no way they could have "cheated" by already knowing the answer.

Result: the model correctly guessed the direction about 88% of the time, compared to about 53% if you just always guessed "prices tend to go up" (or whichever direction was slightly more common that day). That's a real, meaningful improvement — not a perfect crystal ball, but genuinely useful signal.

---

# Part 11: A Small Glossary

- **Order book**: the running list of everyone's current buy and sell offers for a stock.
- **Bid**: the price someone is offering to *buy* at.
- **Ask**: the price someone is offering to *sell* at.
- **Spread**: the gap between the best bid and best ask.
- **Limit order**: "I'll trade, but only at this price or better" — might not happen instantly.
- **Market order**: "I'll trade right now, whatever the current price is."
- **Fill**: when an order actually gets matched and executed.
- **Slippage**: the difference between the price you expected and the price you actually got.
- **Queue position**: your place in line among everyone else waiting at the same price.
- **Ticker**: the short code identifying a stock (AMZN = Amazon).
- **Backtesting**: testing a trading idea against historical data to see how it would have performed.

---

# Part 12: How To Actually Run It

1. Install the required tools: a C++ compiler (already built into macOS), CMake, and Catch2 (`brew install catch2`).
2. Build the project: `mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix catch2) && cmake --build .`
3. Run the automated tests: `./fathom_tests` (should show all 46 passing).
4. Run the main program: `cd .. && ./build/fathom` — this prints the naive-vs-realistic comparison using real historical data.
5. (Optional) Set up and run the AI model: instructions are in the main `README.md` under "Building (Python)."

That's the whole thing, end to end — real market rules, real historical data, real bugs found and fixed, a real measurable result, and an honestly-evaluated AI layer on top.
