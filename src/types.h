#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

// A Bitboard is just a 64-bit unsigned integer
typedef uint64_t U64;

// Colors and Pieces mapped automatically to 0, 1, 2...
enum { WHITE, BLACK, BOTH };
enum { P, N, B, R, Q, K, p, n, b, r, q, k };

// --- Elite Bitwise Macros ---
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))

// --- Hardware Accelerated Helpers ---
static inline int get_lsb(U64 bitboard) {
    if (bitboard == 0) return 0; // Prevent fatal segfaults on empty bitboards
    return __builtin_ctzll(bitboard);
}

static inline int count_bits(U64 bitboard) {
    return __builtin_popcountll(bitboard);
}

// The New Master Board Struct
typedef struct {
    U64 bitboards[12];     // Array of 12 bitboards
    U64 occupancies[3];    // Array of 3 occupancies
    
    int side_to_move;      
    int en_passant;        
    int castling_rights;   
} Board;

// Global function to test if a square is attacked
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

// Helper to quickly push a move into the list
static inline void add_move(MoveList *move_list, int move) {
    if (move_list->count >= 256) return; // Prevent Stack Overflow crashes
    move_list->moves[move_list->count] = move;
    move_list->count++;
}

#endif
