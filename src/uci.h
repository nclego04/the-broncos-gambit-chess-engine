/**
 * @file uci.h
 * @brief UCI protocol loop declarations.
 *
 * This header defines the interface for the Universal Chess Interface (UCI)
 * protocol loop. It handles the standard I/O communication between the
 * chess engine and the GUI, interpreting commands and dispatching operations
 * like position setup, search execution, and diagnostic benchmarking.
 */
#ifndef UCI_H
#define UCI_H

/**
 * @brief Starts the main Universal Chess Interface (UCI) loop.
 *
 * The uci_loop function continuously reads standard input (stdin) for UCI 
 * commands and processes them. Supported commands typically include:
 * - "uci": Identify the engine.
 * - "isready": Synchronization check.
 * - "position": Set the internal board state (from startpos or FEN) and apply moves.
 * - "go": Initiate the search algorithm to find the best move.
 * - "quit": Terminate the engine process.
 * 
 * This function blocks the main thread and runs until the "quit" command
 * is received or the input stream is closed.
 */
void uci_loop();

#endif