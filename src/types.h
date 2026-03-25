/**
 * @file types.h
 * @brief Core data structures and macros for the engine.
 *
 * Defines the 64-bit Bitboard representation, piece/color enumerations,
 * and the bit-packed move encoding logic used throughout the engine.
 */
#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef uint64_t U64;

/**
 * @brief Enumeration representing player colors and combined occupancy.
 */
enum { WHITE, BLACK, BOTH };

/**
 * @brief Enumeration representing all chess piece types.
 * Uppercase represents White pieces, lowercase represents Black pieces.
 */
enum { P, N, B, R, Q, K, p, n, b, r, q, k };

/**
 * @brief Retrieves the state of a specific bit (square) from a bitboard.
 */
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))

/**
 * @brief Sets a specific bit (square) in a bitboard to 1.
 */
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))

/**
 * @brief Clears (sets to 0) a specific bit (square) in a bitboard.
 */
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))

/**
 * @brief Gets the index of the least significant bit (LSB).
 *
 * @param bitboard The 64-bit integer to scan.
 * @return The index (0-63) of the lowest set bit. Returns 0 if the bitboard is empty.
 */
static inline int get_lsb(U64 bitboard) {
    if (bitboard == 0) return 0;
    return __builtin_ctzll(bitboard);
}

/**
 * @brief Counts the total number of set bits in a bitboard.
 *
 * @param bitboard The 64-bit integer to evaluate.
 * @return The number of bits set to 1.
 */
static inline int count_bits(U64 bitboard) {
    return __builtin_popcountll(bitboard);
}

/**
 * @brief Represents the complete internal state of a chess board.
 */
typedef struct {
    U64 bitboards[12];     /**< Individual bitboards for each piece type and color. */
    U64 occupancies[3];    /**< Combined occupancies for WHITE, BLACK, and BOTH. */
    
    int side_to_move;      /**< The color of the player whose turn it is to move. */
    int en_passant;        /**< The square index where an en passant capture is possible, or -1 if none. */
    int castling_rights;   /**< Bitmask storing current castling rights (1=WK, 2=WQ, 4=BK, 8=BQ). */
    U64 hash_key;          /**< Zobrist hash key of the position. */
} Board;

/**
 * @brief Determines if a specific square is attacked by the given side.
 *
 * @param sq The square index (0-63) to check.
 * @param attacker_side The color (WHITE or BLACK) of the potential attacker.
 * @param pos Pointer to the current board state.
 * @return 1 if the square is attacked by the specified side, 0 otherwise.
 */
int is_square_attacked(int sq, int attacker_side, Board *pos);

// --- Move Encoding (Bit-Packing) ---
// We pack the move data into an integer like this:
// Bits 0-5: from_square (0-63)
// Bits 6-11: to_square (0-63)
// Bits 12-15: piece (0-11)
// Bits 16-19: promoted_piece (0-11)
// Bit 20: is_castling flag (0 or 1)
// Bit 21: is_promotion flag (0 or 1)

// Macro to smash the data together
#define encode_move(from, to, piece, promoted, castling, promotion) \
    ((from) | ((to) << 6) | ((piece) << 12) | ((promoted) << 16) | ((castling) << 20) | ((promotion) << 21))

// Macros to extract the data back out using bitwise AND masks
#define get_from(move) ((move) & 0x3f)
#define get_to(move) (((move) >> 6) & 0x3f)
#define get_piece(move) (((move) >> 12) & 0xf)
#define get_promoted(move) (((move) >> 16) & 0xf)
#define get_castling(move) (((move) >> 20) & 1)
#define get_promotion(move) (((move) >> 21) & 1)

// --- The Move List ---
// A simple array to hold all generated moves for a specific board state
typedef struct {
    int moves[256]; // 256 is the absolute max number of legal moves in any chess position
    int count;
} MoveList;

/**
 * @brief Appends a packed move to a MoveList.
 *
 * @param move_list Pointer to the MoveList to populate.
 * @param move The bit-packed integer representing the move.
 */
static inline void add_move(MoveList *move_list, int move) {
    if (move_list->count >= 256) return; // Prevent Stack Overflow crashes
    move_list->moves[move_list->count] = move;
    move_list->count++;
}

#endif
