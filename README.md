# The Broncos Gambit 🐴♟️

A UCI-compatible Chess Engine written in C by Group 6 for ECE 4318.

## Overview
The Broncos Gambit is a custom-built chess engine featuring a 64-bit bitboard board representation, a robust pseudo-legal and legal move generator, and an implementation of the Universal Chess Interface (UCI) protocol. 

Currently at **Version 3.0**, the engine is a highly competitive intermediate-level opponent. It features an Alpha-Beta search algorithm, Quiescence Search, MVV-LVA Move Ordering, static evaluation using Piece-Square Tables, robust time management, and live performance analytics.

## Core Features (Version 3.0)

### 1. Static Evaluation
- **Material Counting:** Evaluates the board by summing up the standard values of pieces (e.g., Queens = 900, Knights = 300).
- **Piece-Square Tables (PSTs):** Uses 8x8 arrays to give positional bonuses, encouraging the engine to push pawns to the center, keep the king safe, and centralize knights.

### 2. Search Algorithm
- **Alpha-Beta Pruning (Negamax):** Mathematically proves which branches of the game tree are bad and skips them, vastly outperforming standard Minimax.
- **MVV-LVA Move Ordering:** (Most Valuable Victim - Least Valuable Attacker) Sorts generated moves to search high-value captures first, maximizing pruning efficiency and dramatically lowering the Effective Branching Factor (EBF).
- **Quiescence Search:** Extends the search past the target depth for "noisy" positions (captures and promotions), eliminating the Horizon Effect and preventing tactical blunders.
- **Iterative Deepening:** Searches layer by layer (Depth 1, 2, etc.) to gracefully stop and return its best guess when time runs out.
- **Fastest-Mate Optimization:** Uses a `ply` variable to actively seek the fastest possible checkmate instead of delaying it.

### 3. Professional Time Management
- **Live Clock Polling:** Checks the `CLOCK_MONOTONIC` high-precision system clock every 2,048 nodes to avoid overstepping time boundaries.
- **Uncancellable Depth 1:** Refuses to check the clock during the first Iterative Deepening pass to guarantee a valid fallback move.
- **Move Overhead Buffer:** Subtracts 20ms from allocated time to protect against OS thread scheduling and network lag.

### 4. Rule Handling & Analytics
- **En Passant Reception:** Successfully parses, processes, and updates its internal bitboards when the opponent plays an En Passant capture to prevent board corruption.
- **Live Analytics:** Calculates Nodes Per Second (NPS) and Effective Branching Factor (EBF) in real-time, logging them to `tests/live_nps.txt`.

## Version History & Benchmarks

Tracking the engine's playing strength and search performance across major updates:

| Version | Description / Features | Estimated Elo | Avg Depth | Avg NPS | Avg EBF |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **V1.0** | Baseline: Random Mover & Perft | N/A | N/A | ~76M | ~38.0 |
| **V2.0** | Basic Alpha-Beta Search & Eval | ~1224 | ~6.0 | ~27.9M | ~15.7 |
| **V3.0** | Move Ordering & Q-Search | ~1451 | ~7.5 | ~7.0M | ~12.5 |

*Note on Benchmarking Methodology:*
- *The **Estimated Elo** for V2.0 was calculated against Stockfish Level 0 (assumed ~1200 baseline) yielding a +24.4 Elo difference. V3.0 was tested via `cutechess-cli` in a nearly 4,000-game match against Stockfish Level 1 (assumed ~1350 baseline) at a 10+0.08 time control. A 64.2% win rate yielded a +101.1 Elo difference, resulting in ~1451 Elo.*
- *The **engine efficiency metrics** were obtained using the internal `bench_nps_ebf` and `bench_avg_depth` commands against the 24-position **Bratko-Kopec** (`bratko_kopec.epd`) test suite. Average Depth was calculated by allocating exactly 1000ms per position. NPS and EBF were calculated by running a fixed Depth 6 search across the entire suite.*

## Project Structure

- **`src/`** - The core C source code and header files for the engine.
- **`tools/`** - Additional utilities and GUI wrappers (e.g., UciBoardArena).

## Prerequisites

- A C compiler (`gcc` or `clang`)
- `make` (for building the engine)
- **Java 11+** (Optional, only required if using the included `UciBoardArena` GUI)

## Building the Engine

This project uses a standard Makefile optimized with `-O3` for maximum execution speed. To compile the engine, ensure you have `gcc` installed and run:

```bash
make
```

This will compile the source files and neatly place the executable in the `bin/` directory. To clean up the build files and start fresh:

```bash
make clean
```

## Usage

You can run the engine directly from your terminal:
```bash
./bin/broncos_engine
```

### Supported Commands
- `uci` - Initializes the engine and identifies it to the GUI.
- `isready` - Pings the engine to check if it's ready to receive commands.
- `position startpos moves <m1> <m2>...` - Sets up the board state.
- `go` - Tells the engine to calculate and return its best move using Alpha-Beta search.
- `bench_nps_ebf [filename] [depth]` - Runs the engine's search algorithm across a suite of positions to measure Nodes Per Second (NPS) and Effective Branching Factor (EBF). Defaults to the 24-position **Bratko-Kopec test suite** (`bratko_kopec.epd`) at Depth 6 if no arguments are provided.
- `bench_avg_depth [filename] [time_ms]` - Runs the engine's search algorithm across a suite of positions with a fixed time limit per move to measure the Average Depth reached. Defaults to the **Bratko-Kopec test suite** (`bratko_kopec.epd`) at 1000 ms if no arguments are provided.
- `quit` - Exits the engine process.

### Using with the Java Arena (UciBoardArena)
To connect the engine to the provided Java arena GUI:
1. Ensure the engine is compiled by running `make` in the project root.
2. Launch the Java arena application (located in the `tools/` directory).
3. Navigate to the engine configuration or management settings within the GUI.
4. Add a new engine, specify **UCI** as the protocol, and set the executable path to point to your compiled engine (e.g., `bin/broncos_engine` or its absolute path).
5. Load the engine to play against it or test it in an engine-vs-engine match.

### Testing with Cutechess
To reproduce the ~1451 Elo rating, you can pit the engine against Stockfish Level 1 using standard `cutechess-cli` commands:

```bash
cutechess-cli \
  -engine cmd=./bin/broncos_engine name="Broncos V3" \
  -engine cmd=stockfish name="Stockfish1" option."Skill Level"=1 \
  -each proto=uci tc=10+0.08 \
  -openings file=tests/bratko_kopec.epd format=epd order=random \
  -rounds 480 -games 2 \
  -concurrency 4 \
  -pgnout tests/tournament.pgn
```

## Authors
**Group 6** - ECE 4318

## License
This project is developed for academic purposes. *(Consider adding an MIT or GPL License here if open-sourcing!)*
