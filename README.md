# The Broncos Gambit 🐴♟️

A UCI-compatible Chess Engine written in C by Group 6 for ECE 4318.

## Overview
The Broncos Gambit is a custom-built chess engine featuring a 64-bit bitboard board representation, a robust pseudo-legal and legal move generator, and an implementation of the Universal Chess Interface (UCI) protocol. 

Currently at **Version 4.0**, the engine is a highly competitive intermediate-level opponent. It features an Alpha-Beta search algorithm, Transposition Tables, Quiescence Search, MVV-LVA Move Ordering, static evaluation using Piece-Square Tables, robust time management, and live performance analytics.

## Core Features (Version 4.0)

### 1. Static Evaluation
- **Material Counting:** Evaluates the board by summing up the standard values of pieces (e.g., Queens = 900, Knights = 300).
- **Piece-Square Tables (PSTs):** Uses 8x8 arrays to give positional bonuses, encouraging the engine to push pawns to the center, keep the king safe, and centralize knights.

### 2. Search Algorithm
- **Alpha-Beta Pruning (Negamax):** Mathematically proves which branches of the game tree are bad and skips them, vastly outperforming standard Minimax.
- **MVV-LVA Move Ordering:** (Most Valuable Victim - Least Valuable Attacker) Sorts generated moves to search high-value captures first, maximizing pruning efficiency and dramatically lowering the Effective Branching Factor (EBF).
- **Quiescence Search:** Extends the search past the target depth for "noisy" positions (captures and promotions), eliminating the Horizon Effect and preventing tactical blunders.
- **Transposition Tables:** Caches previously evaluated board positions to avoid redundant calculations, drastically increasing search depth and speed.
- **Zobrist Hashing:** Uses 64-bit polynomial string hashing to uniquely identify positions for the transposition table with near-zero collision probability.
- **Iterative Deepening:** Searches layer by layer (Depth 1, 2, etc.) to gracefully stop and return its best guess when time runs out.
- **Fastest-Mate Optimization:** Uses a `ply` variable to actively seek the fastest possible checkmate instead of delaying it.

### 3. Professional Time Management
- **Live Clock Polling:** Checks the `CLOCK_MONOTONIC` high-precision system clock every 2,048 nodes to avoid overstepping time boundaries.
- **Root Fallback Move:** Seeds the search with a guaranteed legal move before Iterative Deepening begins. This safely allows the engine to abort during Depth 1 in the event of a Quiescence Search explosion without returning an illegal `0000` move.
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
| **V3.0** | Move Ordering & Q-Search | ~1451 | ~6.5 | ~7.0M | ~12.5 |
| **V4.0** | Transposition Tables & Zobrist | ~1485 | ~7.6 | ~6.9M | ~11.8 |

*Note on Benchmarking Methodology:*
- *The **Estimated Elo** ratings are calculated via `cutechess-cli` against various Stockfish skill levels at a 10+0.08 time control. V3.0's ~1451 Elo comes from a +101.1 difference over Stockfish Level 1 (64.2% win rate). V4.0's ~1485 Elo comes from a +135.0 difference over the same opponent (68.5% win rate across 1,000 games).*
- *The **engine efficiency metrics** (Avg Depth, NPS, EBF) are obtained using the internal `bench` command against the 24-position **Bratko-Kopec** (`bratko_kopec.epd`) test suite. Average Depth is calculated by allocating exactly 1000ms per position. NPS and EBF are calculated by running a fixed Depth 6 search across the entire suite.*

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
- `bench [filename] [depth] [time_ms]` - Runs the engine's search algorithm across a suite of positions to measure Nodes Per Second (NPS), Effective Branching Factor (EBF), and Average Depth reached. Defaults to `bratko_kopec.epd`, Depth 6, and 1000 ms if no arguments are provided.
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
  -engine cmd=./bin/broncos_engine name="Broncos V4" \
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
