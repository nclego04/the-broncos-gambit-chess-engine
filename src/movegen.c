/**
 * @file movegen.c
 * @brief Pseudo-legal move generator.
 *
 * Uses magic bitboards and pre-calculated attack tables to rapidly generate
 * all pseudo-legal moves for a given board state. Moves are added to a
 * statically sized MoveList array to avoid heap allocations during search.
 */
#include <stdio.h>
#include "types.h"
#include "magics.h"
#include "movegen.h"

U64 knight_attacks[64]; /**< Pre-calculated attack bitboards for knights on every square. */
U64 king_attacks[64];   /**< Pre-calculated attack bitboards for kings on every square. */

// --- Bitboard Constants for Masks ---
const U64 FILE_A = 0x0101010101010101ULL;
const U64 FILE_B = 0x0202020202020202ULL;
const U64 FILE_G = 0x4040404040404040ULL;
const U64 FILE_H = 0x8080808080808080ULL;
const U64 RANK_4 = 0x00000000FF000000ULL;
const U64 RANK_1 = 0x00000000000000FFULL;
const U64 RANK_8 = 0xFF00000000000000ULL;

/**
 * @brief Generates the relevant blocker mask for a bishop on a specific square.
 *
 * @param sq The square index (0-63).
 * @return A bitboard mapping the relevant interior diagonal squares.
 */
U64 mask_bishop_attacks(int sq) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = sq / 8, tf = sq % 8;
    
    for (r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++) attacks |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++) attacks |= (1ULL << (r * 8 + f));
    for (r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--) attacks |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--) attacks |= (1ULL << (r * 8 + f));
    return attacks;
}

/**
 * @brief Generates the relevant blocker mask for a rook on a specific square.
 *
 * @param sq The square index (0-63).
 * @return A bitboard mapping the relevant interior orthogonal squares.
 */
U64 mask_rook_attacks(int sq) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = sq / 8, tf = sq % 8;
    
    for (r = tr + 1; r <= 6; r++) attacks |= (1ULL << (r * 8 + tf));
    for (r = tr - 1; r >= 1; r--) attacks |= (1ULL << (r * 8 + tf));
    for (f = tf + 1; f <= 6; f++) attacks |= (1ULL << (tr * 8 + f));
    for (f = tf - 1; f >= 1; f--) attacks |= (1ULL << (tr * 8 + f));
    return attacks;
}

/**
 * @brief Calculates bishop attacks on the fly using slow raycasting.
 *
 * @param sq The square index of the bishop.
 * @param block A bitboard representing the currently occupied squares.
 * @return A bitboard mapping all attacked squares until blocked.
 */
U64 bishop_attacks_on_the_fly(int sq, U64 block) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = sq / 8, tf = sq % 8;
    
    for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if (block & (1ULL << (r * 8 + f))) break;
    }
    for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if (block & (1ULL << (r * 8 + f))) break;
    }
    for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if (block & (1ULL << (r * 8 + f))) break;
    }
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if (block & (1ULL << (r * 8 + f))) break;
    }
    return attacks;
}

/**
 * @brief Calculates rook attacks on the fly using slow raycasting.
 *
 * @param sq The square index of the rook.
 * @param block A bitboard representing the currently occupied squares.
 * @return A bitboard mapping all attacked squares until blocked.
 */
U64 rook_attacks_on_the_fly(int sq, U64 block) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = sq / 8, tf = sq % 8;
    
    for (r = tr + 1; r <= 7; r++) {
        attacks |= (1ULL << (r * 8 + tf));
        if (block & (1ULL << (r * 8 + tf))) break;
    }
    for (r = tr - 1; r >= 0; r--) {
        attacks |= (1ULL << (r * 8 + tf));
        if (block & (1ULL << (r * 8 + tf))) break;
    }
    for (f = tf + 1; f <= 7; f++) {
        attacks |= (1ULL << (tr * 8 + f));
        if (block & (1ULL << (tr * 8 + f))) break;
    }
    for (f = tf - 1; f >= 0; f--) {
        attacks |= (1ULL << (tr * 8 + f));
        if (block & (1ULL << (tr * 8 + f))) break;
    }
    return attacks;
}

// --- Global Tables for Sliding Piece Attacks ---
U64 rook_masks[64];             /**< Masks of relevant blocker squares for rooks. */
U64 bishop_masks[64];           /**< Masks of relevant blocker squares for bishops. */
U64 rook_attacks[64][4096];     /**< Pre-calculated rook attacks mapped by magic index. */
U64 bishop_attacks[64][512];    /**< Pre-calculated bishop attacks mapped by magic index. */

/**
 * @brief Generates a specific blocker occupancy configuration from an index.
 *
 * @param index The integer index of the permutation.
 * @param bits_in_mask The number of relevant squares in the attack mask.
 * @param attack_mask The mask defining the valid squares to place blockers on.
 * @return A bitboard representing the generated occupancy combination.
 */
U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask) {
    U64 occupancy = 0ULL;
    for (int count = 0; count < bits_in_mask; count++) {
        int square = get_lsb(attack_mask);
        pop_bit(attack_mask, square);
        if (index & (1 << count)) {
            occupancy |= (1ULL << square);
        }
    }
    return occupancy;
}

unsigned int random_state = 1804289383;

/**
 * @brief Generates a pseudo-random 32-bit integer using an XOR shift algorithm.
 *
 * @return A pseudo-random 32-bit unsigned integer.
 */
unsigned int get_random_U32() {
    unsigned int number = random_state;
    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;
    random_state = number;
    return number;
}

/**
 * @brief Generates a pseudo-random 64-bit integer.
 *
 * @return A pseudo-random 64-bit unsigned integer.
 */
U64 get_random_U64() {
    U64 n1 = (U64)(get_random_U32()) & 0xFFFF;
    U64 n2 = (U64)(get_random_U32()) & 0xFFFF;
    U64 n3 = (U64)(get_random_U32()) & 0xFFFF;
    U64 n4 = (U64)(get_random_U32()) & 0xFFFF;
    return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

/**
 * @brief Generates a candidate magic number.
 *
 * @return A sparse 64-bit unsigned integer (achieved by ANDing three randoms).
 */
U64 generate_magic_candidate() {
    return get_random_U64() & get_random_U64() & get_random_U64();
}

/**
 * @brief Brute-forces a magic number for a specific square and sliding piece type.
 *
 * @param sq The square index (0-63).
 * @param relevant_bits The number of bits in the relevant blocker mask.
 * @param is_bishop Non-zero if finding a magic number for a bishop, zero for a rook.
 * @return A perfect magic number hash, or 0 if one could not be found.
 */
U64 find_magic_number(int sq, int relevant_bits, int is_bishop) {
    U64 occupancies[4096];
    U64 attacks[4096];
    U64 used_attacks[4096];
    
    U64 mask = is_bishop ? mask_bishop_attacks(sq) : mask_rook_attacks(sq);
    int num_indices = 1 << relevant_bits;
    
    for (int i = 0; i < num_indices; i++) {
        occupancies[i] = set_occupancy(i, relevant_bits, mask);
        attacks[i] = is_bishop ? bishop_attacks_on_the_fly(sq, occupancies[i]) : rook_attacks_on_the_fly(sq, occupancies[i]);
    }
    
    for (int random_attempts = 0; random_attempts < 100000000; random_attempts++) {
        U64 magic = generate_magic_candidate();
        
        if (count_bits((mask * magic) & 0xFF00000000000000ULL) < 6) continue;
        
        int index, fail = 0;
        for (int i = 0; i < 4096; i++) used_attacks[i] = 0ULL;
        
        for (int i = 0; i < num_indices && !fail; i++) {
            index = (int)((occupancies[i] * magic) >> (64 - relevant_bits));
            
            if (used_attacks[index] == 0ULL) {
                used_attacks[index] = attacks[i];
            } else if (used_attacks[index] != attacks[i]) {
                fail = 1;
            }
        }
        
        if (!fail) return magic;
    }
    printf("ERROR: Failed to find magic for square %d\n", sq);
    return 0ULL;
}

/**
 * @brief Initializes sliding piece masks, magic numbers, and lookup tables.
 *
 * This function must be called once at startup. It brute-forces magics and 
 * pre-calculates the attack tables for all possible blocker permutations.
 */
void init_sliders() {
    printf("Brute-forcing Magic Numbers (This takes a fraction of a second)...\n");
    for (int sq = 0; sq < 64; sq++) {
        bishop_masks[sq] = mask_bishop_attacks(sq);
        rook_masks[sq] = mask_rook_attacks(sq);
        
        int b_bits = bishop_relevant_bits[sq];
        int r_bits = rook_relevant_bits[sq];
        
        bishop_magic_numbers[sq] = find_magic_number(sq, b_bits, 1);
        rook_magic_numbers[sq] = find_magic_number(sq, r_bits, 0);
        
        int b_indices = (1 << b_bits);
        for (int i = 0; i < b_indices; i++) {
            U64 occupancy = set_occupancy(i, b_bits, bishop_masks[sq]);
            int magic_index = (occupancy * bishop_magic_numbers[sq]) >> (64 - b_bits);
            bishop_attacks[sq][magic_index] = bishop_attacks_on_the_fly(sq, occupancy);
        }
        
        int r_indices = (1 << r_bits);
        for (int i = 0; i < r_indices; i++) {
            U64 occupancy = set_occupancy(i, r_bits, rook_masks[sq]);
            int magic_index = (occupancy * rook_magic_numbers[sq]) >> (64 - r_bits);
            rook_attacks[sq][magic_index] = rook_attacks_on_the_fly(sq, occupancy);
        }
    }
    printf("Magic Numbers successfully generated and locked into memory!\n\n");
}

/**
 * @brief Initializes attack lookup tables for leaping pieces.
 *
 * This function must be called once at startup to populate knight and king moves.
 */
void init_leapers() {
    for (int sq = 0; sq < 64; sq++) {
        U64 bitboard = 0ULL;
        set_bit(bitboard, sq);
        
        U64 k_attacks = 0ULL;
        k_attacks |= (bitboard & ~(FILE_G | FILE_H)) >> 6;
        k_attacks |= (bitboard & ~FILE_H) >> 15;
        k_attacks |= (bitboard & ~FILE_A) >> 17;
        k_attacks |= (bitboard & ~(FILE_A | FILE_B)) >> 10;
        
        k_attacks |= (bitboard & ~(FILE_G | FILE_H)) << 10;
        k_attacks |= (bitboard & ~FILE_H) << 17;
        k_attacks |= (bitboard & ~FILE_A) << 15;
        k_attacks |= (bitboard & ~(FILE_A | FILE_B)) << 6;
        knight_attacks[sq] = k_attacks;

        U64 king_att = 0ULL;
        king_att |= (bitboard & ~FILE_A) >> 1;
        king_att |= (bitboard & ~FILE_H) << 1;
        king_att |= (bitboard) >> 8;
        king_att |= (bitboard) << 8;
        king_att |= (bitboard & ~FILE_A) >> 9;
        king_att |= (bitboard & ~FILE_H) >> 7;
        king_att |= (bitboard & ~FILE_A) << 7;
        king_att |= (bitboard & ~FILE_H) << 9;
        king_attacks[sq] = king_att;
    }
}

/**
 * @brief Prints a visual representation of a bitboard to the console.
 *
 * @param bitboard The 64-bit integer to print.
 */
void print_bitboard(U64 bitboard) {
    printf("\n");
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            printf(" %d", get_bit(bitboard, sq) ? 1 : 0);
        }
        printf("\n");
    }
    printf("\n");
}

/**
 * @brief Determines if a specific square is attacked by the given side.
 *
 * @param sq The square index (0-63) to check.
 * @param attacker_side The color (WHITE or BLACK) of the potential attacker.
 * @param pos Pointer to the current board state.
 * @return 1 if the square is attacked by the specified side, 0 otherwise.
 */
int is_square_attacked(int sq, int attacker_side, Board *pos) {
    U64 sq_bb = 1ULL << sq;
    
    if (attacker_side == WHITE) {
        if ((sq_bb >> 7) & ~FILE_A & pos->bitboards[P]) return 1;
        if ((sq_bb >> 9) & ~FILE_H & pos->bitboards[P]) return 1;
    } else {
        if ((sq_bb << 9) & ~FILE_A & pos->bitboards[p]) return 1;
        if ((sq_bb << 7) & ~FILE_H & pos->bitboards[p]) return 1;
    }
    
    int knight = (attacker_side == WHITE) ? N : n;
    if (knight_attacks[sq] & pos->bitboards[knight]) return 1;
    
    int king = (attacker_side == WHITE) ? K : k;
    if (king_attacks[sq] & pos->bitboards[king]) return 1;
    
    int bishop = (attacker_side == WHITE) ? B : b;
    int queen  = (attacker_side == WHITE) ? Q : q;
    U64 b_occ = pos->occupancies[BOTH] & bishop_masks[sq];
    int b_magic = (b_occ * bishop_magic_numbers[sq]) >> (64 - bishop_relevant_bits[sq]);
    if (bishop_attacks[sq][b_magic] & (pos->bitboards[bishop] | pos->bitboards[queen])) return 1;
    
    int rook = (attacker_side == WHITE) ? R : r;
    U64 r_occ = pos->occupancies[BOTH] & rook_masks[sq];
    int r_magic = (r_occ * rook_magic_numbers[sq]) >> (64 - rook_relevant_bits[sq]);
    if (rook_attacks[sq][r_magic] & (pos->bitboards[rook] | pos->bitboards[queen])) return 1;
    
    return 0;
}

/**
 * @brief Populates the given MoveList with all pseudo-legal moves.
 *
 * @param pos Pointer to the current board state.
 * @param list Pointer to the MoveList to populate.
 */
void generate_all_moves(Board *pos, MoveList *list) {
    list->count = 0;

    int side = pos->side_to_move;
    int enemy = (side == WHITE) ? BLACK : WHITE;
    
    U64 friendly = pos->occupancies[side];
    U64 enemies = pos->occupancies[enemy];
    U64 both = pos->occupancies[BOTH];
    U64 empty = ~both;

    int offset = (side == WHITE) ? 0 : 6;
    int pawn   = P + offset;
    int knight = N + offset;
    int bishop = B + offset;
    int rook   = R + offset;
    int queen  = Q + offset;
    int king   = K + offset;

    // FIXME: En Passant generation is missing. The engine can receive an EP 
    // capture from the opponent, but cannot currently generate one itself.
    U64 pawns = pos->bitboards[pawn];
    if (side == WHITE) {
        U64 pushes = (pawns << 8) & empty;
        U64 single_pushes = pushes; 
        while (single_pushes) {
            int to_sq = get_lsb(single_pushes);
            if (to_sq >= 56) {
                add_move(list, encode_move(to_sq - 8, to_sq, pawn, queen, 0, 1));
                add_move(list, encode_move(to_sq - 8, to_sq, pawn, rook, 0, 1));
                add_move(list, encode_move(to_sq - 8, to_sq, pawn, bishop, 0, 1));
                add_move(list, encode_move(to_sq - 8, to_sq, pawn, knight, 0, 1));
            } else {
                add_move(list, encode_move(to_sq - 8, to_sq, pawn, 0, 0, 0));
            }
            pop_bit(single_pushes, to_sq);
        }
        
        U64 double_pushes = (pushes << 8) & empty & RANK_4;
        while (double_pushes) {
            int to_sq = get_lsb(double_pushes);
            add_move(list, encode_move(to_sq - 16, to_sq, pawn, 0, 0, 0));
            pop_bit(double_pushes, to_sq);
        }
        
        U64 cap_west = ((pawns & ~FILE_A) << 7) & enemies;
        while (cap_west) {
            int to_sq = get_lsb(cap_west);
            if (to_sq >= 56) {
                add_move(list, encode_move(to_sq - 7, to_sq, pawn, queen, 0, 1));
                add_move(list, encode_move(to_sq - 7, to_sq, pawn, rook, 0, 1));
                add_move(list, encode_move(to_sq - 7, to_sq, pawn, bishop, 0, 1));
                add_move(list, encode_move(to_sq - 7, to_sq, pawn, knight, 0, 1));
            } else {
                add_move(list, encode_move(to_sq - 7, to_sq, pawn, 0, 0, 0));
            }
            pop_bit(cap_west, to_sq);
        }
        
        U64 cap_east = ((pawns & ~FILE_H) << 9) & enemies;
        while (cap_east) {
            int to_sq = get_lsb(cap_east);
            if (to_sq >= 56) {
                add_move(list, encode_move(to_sq - 9, to_sq, pawn, queen, 0, 1));
                add_move(list, encode_move(to_sq - 9, to_sq, pawn, rook, 0, 1));
                add_move(list, encode_move(to_sq - 9, to_sq, pawn, bishop, 0, 1));
                add_move(list, encode_move(to_sq - 9, to_sq, pawn, knight, 0, 1));
            } else {
                add_move(list, encode_move(to_sq - 9, to_sq, pawn, 0, 0, 0));
            }
            pop_bit(cap_east, to_sq);
        }
    } else {
        U64 pushes = (pawns >> 8) & empty;
        U64 single_pushes = pushes;
        while (single_pushes) {
            int to_sq = get_lsb(single_pushes);
            if (to_sq <= 7) {
                add_move(list, encode_move(to_sq + 8, to_sq, pawn, queen, 0, 1));
                add_move(list, encode_move(to_sq + 8, to_sq, pawn, rook, 0, 1));
                add_move(list, encode_move(to_sq + 8, to_sq, pawn, bishop, 0, 1));
                add_move(list, encode_move(to_sq + 8, to_sq, pawn, knight, 0, 1));
            } else {
                add_move(list, encode_move(to_sq + 8, to_sq, pawn, 0, 0, 0));
            }
            pop_bit(single_pushes, to_sq);
        }
        
        U64 double_pushes = (pushes >> 8) & empty & 0x000000FF00000000ULL; 
        while (double_pushes) {
            int to_sq = get_lsb(double_pushes);
            add_move(list, encode_move(to_sq + 16, to_sq, pawn, 0, 0, 0));
            pop_bit(double_pushes, to_sq);
        }
        
        U64 cap_west = ((pawns & ~FILE_A) >> 9) & enemies;
        while (cap_west) {
            int to_sq = get_lsb(cap_west);
            if (to_sq <= 7) {
                add_move(list, encode_move(to_sq + 9, to_sq, pawn, queen, 0, 1));
                add_move(list, encode_move(to_sq + 9, to_sq, pawn, rook, 0, 1));
                add_move(list, encode_move(to_sq + 9, to_sq, pawn, bishop, 0, 1));
                add_move(list, encode_move(to_sq + 9, to_sq, pawn, knight, 0, 1));
            } else {
                add_move(list, encode_move(to_sq + 9, to_sq, pawn, 0, 0, 0));
            }
            pop_bit(cap_west, to_sq);
        }
        
        U64 cap_east = ((pawns & ~FILE_H) >> 7) & enemies;
        while (cap_east) {
            int to_sq = get_lsb(cap_east);
            if (to_sq <= 7) {
                add_move(list, encode_move(to_sq + 7, to_sq, pawn, queen, 0, 1));
                add_move(list, encode_move(to_sq + 7, to_sq, pawn, rook, 0, 1));
                add_move(list, encode_move(to_sq + 7, to_sq, pawn, bishop, 0, 1));
                add_move(list, encode_move(to_sq + 7, to_sq, pawn, knight, 0, 1));
            } else {
                add_move(list, encode_move(to_sq + 7, to_sq, pawn, 0, 0, 0));
            }
            pop_bit(cap_east, to_sq);
        }
    }

    U64 knights = pos->bitboards[knight];
    while (knights) {
        int from_sq = get_lsb(knights);
        U64 attacks = knight_attacks[from_sq] & ~friendly;
        while (attacks) {
            int to_sq = get_lsb(attacks);
            add_move(list, encode_move(from_sq, to_sq, knight, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(knights, from_sq);
    }

    U64 kings = pos->bitboards[king];
    while (kings) {
        int from_sq = get_lsb(kings);
        U64 attacks = king_attacks[from_sq] & ~friendly;
        while (attacks) {
            int to_sq = get_lsb(attacks);
            add_move(list, encode_move(from_sq, to_sq, king, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(kings, from_sq);
    }

    U64 bishops_and_queens = pos->bitboards[bishop] | pos->bitboards[queen];
    while (bishops_and_queens) {
        int from_sq = get_lsb(bishops_and_queens);
        U64 occupancy = both & bishop_masks[from_sq];
        int magic_index = (occupancy * bishop_magic_numbers[from_sq]) >> (64 - bishop_relevant_bits[from_sq]);
        
        U64 attacks = bishop_attacks[from_sq][magic_index] & ~friendly;
        
        int moving_piece = get_bit(pos->bitboards[queen], from_sq) ? queen : bishop;
        
        while (attacks) {
            int to_sq = get_lsb(attacks);
            add_move(list, encode_move(from_sq, to_sq, moving_piece, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(bishops_and_queens, from_sq);
    }

    U64 rooks_and_queens = pos->bitboards[rook] | pos->bitboards[queen];
    while (rooks_and_queens) {
        int from_sq = get_lsb(rooks_and_queens);
        U64 occupancy = both & rook_masks[from_sq];
        int magic_index = (occupancy * rook_magic_numbers[from_sq]) >> (64 - rook_relevant_bits[from_sq]);
        
        U64 attacks = rook_attacks[from_sq][magic_index] & ~friendly;
        int moving_piece = get_bit(pos->bitboards[queen], from_sq) ? queen : rook;
        
        while (attacks) {
            int to_sq = get_lsb(attacks);
            add_move(list, encode_move(from_sq, to_sq, moving_piece, 0, 0, 0));
            pop_bit(attacks, to_sq);
        }
        pop_bit(rooks_and_queens, from_sq);
    }

    if (side == WHITE) {
        if (pos->castling_rights & 1) {
            if (!get_bit(both, 5) && !get_bit(both, 6)) {
                if (!is_square_attacked(4, BLACK, pos) && 
                    !is_square_attacked(5, BLACK, pos) && 
                    !is_square_attacked(6, BLACK, pos)) {
                    add_move(list, encode_move(4, 6, king, 0, 1, 0));
                }
            }
        }
        if (pos->castling_rights & 2) {
            if (!get_bit(both, 1) && !get_bit(both, 2) && !get_bit(both, 3)) {
                if (!is_square_attacked(4, BLACK, pos) && 
                    !is_square_attacked(3, BLACK, pos) && 
                    !is_square_attacked(2, BLACK, pos)) {
                    add_move(list, encode_move(4, 2, king, 0, 1, 0));
                }
            }
        }
    } else {
        if (pos->castling_rights & 4) {
            if (!get_bit(both, 61) && !get_bit(both, 62)) {
                if (!is_square_attacked(60, WHITE, pos) && 
                    !is_square_attacked(61, WHITE, pos) && 
                    !is_square_attacked(62, WHITE, pos)) {
                    add_move(list, encode_move(60, 62, king, 0, 1, 0));
                }
            }
        }
        if (pos->castling_rights & 8) {
            if (!get_bit(both, 57) && !get_bit(both, 58) && !get_bit(both, 59)) {
                if (!is_square_attacked(60, WHITE, pos) && 
                    !is_square_attacked(59, WHITE, pos) && 
                    !is_square_attacked(58, WHITE, pos)) {
                    add_move(list, encode_move(60, 58, king, 0, 1, 0));
                }
            }
        }
    }
}