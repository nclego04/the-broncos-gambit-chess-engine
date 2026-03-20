# The Broncos Gambit 🐴♟️

A UCI-compatible Chess Engine written in C by Group 6 for ECE 4318.

## Overview
The Broncos Gambit is a custom-built chess engine featuring a 64-bit bitboard board representation, a robust pseudo-legal and legal move generator, and an implementation of the Universal Chess Interface (UCI) protocol. 

Currently at **Version 1.0**, the engine serves as a foundational baseline: it functions as a random legal mover and includes a high-performance `perft` (Performance Test) benchmarker to validate move generation speed and correctness without search overhead.

## Project Structure

- **`src/`** - The core C source code and header files for the engine.
- **`bin/`** - Contains the compiled executable (`broncos_engine`). Ignored by version control.
- **`tools/`** - Additional utilities and GUI wrappers (e.g., UciBoardArena).

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
- `go` - Tells the engine to calculate and return its best move (random legal move in V1).
- `bench` - Runs the internal performance test (`perft`) across standard complex positions (like Kiwipete) to measure Nodes Per Second (NPS).
- `quit` - Exits the engine process.

### Using with the Java Arena (UciBoardArena)
To connect the engine to the provided Java arena GUI:
1. Ensure the engine is compiled by running `make` in the project root.
2. Launch the Java arena application (located in the `tools/` directory).
3. Navigate to the engine configuration or management settings within the GUI.
4. Add a new engine, specify **UCI** as the protocol, and set the executable path to point to your compiled engine (e.g., `bin/broncos_engine` or its absolute path).
5. Load the engine to play against it or test it in an engine-vs-engine match.

## Authors
**Group 6** - ECE 4318
