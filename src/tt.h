/**
 * @file tt.h
 * @brief Zobrist hashing and Transposition Table definitions.
 */
#ifndef TT_H
#define TT_H

#include "types.h"

#define TT_EXACT 0
#define TT_ALPHA 1
#define TT_BETA  2
#define TT_UNKNOWN -1000000

typedef struct {
    U64 key;
    int depth;
    int flag;
    int score;
    int best_move;
} TTEntry;

void init_zobrist(void);
U64 generate_hash_key(Board *pos);
void init_tt(int mb);
void free_tt(void);
void clear_tt(void);
int probe_tt(U64 key, int depth, int alpha, int beta, int *best_move, int ply);
void store_tt(U64 key, int depth, int flag, int score, int best_move, int ply);

extern U64 piece_keys[12][64];
extern U64 side_key;
extern U64 castle_keys[16];

#endif