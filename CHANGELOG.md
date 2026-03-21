# Changelog

All notable changes to the Broncos Gambit engine will be documented in this file.

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
