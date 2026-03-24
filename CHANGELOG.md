# Changelog

All notable changes to the Broncos Gambit engine will be documented in this file.

## [3.0.0] - Move Ordering & Quiescence Search
### Added
- **Quiescence Search:** Extended the search past the target depth for noisy positions (captures/promotions) to resolve tactical sequences and eliminate the Horizon Effect.
- **MVV-LVA Move Ordering:** Implemented a move sorting heuristic (Most Valuable Victim - Least Valuable Attacker) to search high-value captures first. This drastically lowers the Effective Branching Factor (EBF) and triggers massive Alpha-Beta cutoffs.
- **Fastest-Mate Optimization:** Added a ply metric to actively seek the fastest possible checkmate path rather than delaying it.

### Fixed
- **Time Management:** Removed the uncancellable Depth 1 restriction so the engine can safely abort and check the clock even if caught in an infinite Quiescence Search explosion.
- **Fallback Move Selection:** Assigned a guaranteed safe legal move at the root node to prevent the engine from outputting `bestmove 0000` when immediately timing out.

## [2.0.0] - Alpha-Beta Search & Evaluation
### Added
- **The "Brain":** Replaced the random mover with a recursive Negamax Alpha-Beta search algorithm.
- **Evaluation:** Implemented static evaluation using material counting and Piece-Square Tables (PSTs).
- **Time Management:** Added robust tournament time controls (polling `CLOCK_MONOTONIC`, uncancellable Depth 1, and move overhead buffers).
### Fixed
- **En Passant Reception:** Added logic to correctly receive and process incoming En Passant captures without corrupting board bitboards.

## [1.0.0] - Baseline Random Mover
### Added
- Initial 64-bit bitboard architecture and flawless pseudo-legal / legal move generation.
- Basic UCI protocol communication (functions strictly as a random legal mover).
- Internal `perft` benchmarker to measure move generation speed.
