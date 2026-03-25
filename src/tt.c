/**
 * @file tt.c
 * @brief Implementation of Zobrist hashing and Transposition Tables.
 */
#include <stdlib.h>
#include "tt.h"

U64 piece_keys[12][64];
U64 side_key;
U64 castle_keys[16];

TTEntry *tt_table = NULL;
int tt_num_entries = 0;

static U64 random_u64(U64 *state) {
    *state ^= *state >> 12;
    *state ^= *state << 25;
    *state ^= *state >> 27;
    return *state * 2685821657736338717ULL;
}

void init_zobrist(void) {
    U64 state = 1804289383ULL;
    for (int p = 0; p < 12; p++) {
        for (int sq = 0; sq < 64; sq++) {
            piece_keys[p][sq] = random_u64(&state);
        }
    }
    side_key = random_u64(&state);
    for (int i = 0; i < 16; i++) {
        castle_keys[i] = random_u64(&state);
    }
}

U64 generate_hash_key(Board *pos) {
    U64 key = 0ULL;
    for (int p = P; p <= k; p++) {
        U64 bb = pos->bitboards[p];
        while (bb) {
            int sq = get_lsb(bb);
            key ^= piece_keys[p][sq];
            pop_bit(bb, sq);
        }
    }
    if (pos->side_to_move == BLACK) {
        key ^= side_key;
    }
    key ^= castle_keys[pos->castling_rights];
    return key;
}

void init_tt(int mb) {
    size_t target_bytes = (size_t)mb * 1024 * 1024;
    int hash_size = 0x10000;
    while ((size_t)hash_size * 2 * sizeof(TTEntry) <= target_bytes) {
        hash_size *= 2;
    }
    tt_num_entries = hash_size;
    if (tt_table) free(tt_table);
    tt_table = (TTEntry*)malloc(tt_num_entries * sizeof(TTEntry));
    clear_tt();
}

void free_tt(void) {
    if (tt_table) {
        free(tt_table);
        tt_table = NULL;
    }
}

void clear_tt(void) {
    if (!tt_table) return;
    for (int i = 0; i < tt_num_entries; i++) {
        tt_table[i].key = 0;
        tt_table[i].depth = 0;
        tt_table[i].flag = 0;
        tt_table[i].score = 0;
        tt_table[i].best_move = 0;
    }
}

int probe_tt(U64 key, int depth, int alpha, int beta, int *best_move, int ply) {
    if (!tt_table) return TT_UNKNOWN;
    TTEntry *entry = &tt_table[key & (tt_num_entries - 1)];
    if (entry->key == key) {
        *best_move = entry->best_move;
        if (entry->depth >= depth) {
            int score = entry->score;
            if (score > 48000) score -= ply;
            if (score < -48000) score += ply;
            
            if (entry->flag == TT_EXACT) return score;
            if (entry->flag == TT_ALPHA && score <= alpha) return alpha;
            if (entry->flag == TT_BETA && score >= beta) return beta;
        }
    }
    return TT_UNKNOWN;
}

void store_tt(U64 key, int depth, int flag, int score, int best_move, int ply) {
    if (!tt_table) return;
    TTEntry *entry = &tt_table[key & (tt_num_entries - 1)];
    
    if (score > 48000) score += ply;
    if (score < -48000) score -= ply;

    entry->key = key;
    entry->depth = depth;
    entry->flag = flag;
    entry->score = score;
    entry->best_move = best_move;
}