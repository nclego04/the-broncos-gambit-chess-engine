/**
 * @file board.c
 * @brief Board state management and move execution.
 *
 * Handles parsing FEN strings to initialize the board and applying
 * legal/pseudo-legal moves to update the bitboards and game state.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "types.h"
#include "board.h"
#include "tt.h"

/**
 * @brief Converts a character representation of a chess piece to its enum index.
 *
 * @param c The character representing the piece (e.g., 'P', 'n', 'K').
 * @return The corresponding piece enum value (0-11), or -1 if invalid.
 */
int char_to_piece(char c) {
    switch(c) {
        case 'P': return P; case 'N': return N; case 'B': return B;
        case 'R': return R; case 'Q': return Q; case 'K': return K;
        case 'p': return p; case 'n': return n; case 'b': return b;
        case 'r': return r; case 'q': return q; case 'k': return k;
        default: return -1;
    }
}

/**
 * @brief Parses a FEN (Forsyth-Edwards Notation) string and sets up the board.
 *
 * @warning MUTATES the passed Board struct. It completely wipes the current
 * board state and replaces it with the position described in the FEN string.
 *
 * @param pos Pointer to the board state to initialize.
 * @param fen The FEN string representing the desired board position.
 */
void parse_fen(Board *pos, const char *fen) {
    memset(pos, 0, sizeof(Board));
    pos->en_passant = -1;

    int rank = 7;
    int file = 0;
    int i = 0;

    while (fen[i] != ' ' && fen[i] != '\0') {
        char c = fen[i];
        
        if (c == '/') {
            rank--;
            file = 0;
        } else if (isdigit((unsigned char)c)) {
            file += (c - '0');
        } else {
            int piece = char_to_piece(c);
            if (piece != -1) {
                int sq = rank * 8 + file;
                
                set_bit(pos->bitboards[piece], sq);
                
                int color = isupper((unsigned char)c) ? WHITE : BLACK;
                set_bit(pos->occupancies[color], sq);
                set_bit(pos->occupancies[BOTH], sq);
            }
            file++;
        }
        i++;
    }

    i++; 
    if (fen[i] == 'b') pos->side_to_move = BLACK;
    else pos->side_to_move = WHITE;
    
    i++; 
    i++; 
    pos->castling_rights = 0;
    while (fen[i] != ' ' && fen[i] != '\0') {
        if (fen[i] == 'K') pos->castling_rights |= 1;
        else if (fen[i] == 'Q') pos->castling_rights |= 2;
        else if (fen[i] == 'k') pos->castling_rights |= 4;
        else if (fen[i] == 'q') pos->castling_rights |= 8;
        i++;
    }

    pos->hash_key = generate_hash_key(pos);
}

/**
 * @brief Executes a move and updates the internal board state.
 *
 * @warning MUTATES the passed Board struct. This function does not validate
 * pseudo-legality or check status. Ensure `move` is valid before calling.
 *
 * @param pos Pointer to the board state to modify.
 * @param move The bit-packed integer representing the move to apply.
 */
void make_move(Board *pos, int move) {
    int from = get_from(move);
    int to = get_to(move);
    int piece = get_piece(move);
    
    int side = pos->side_to_move;
    int enemy = (side == WHITE) ? BLACK : WHITE;

    pos->hash_key ^= piece_keys[piece][from];
    pos->hash_key ^= castle_keys[pos->castling_rights];

    pop_bit(pos->bitboards[piece], from);
    pop_bit(pos->occupancies[side], from);
    pop_bit(pos->occupancies[BOTH], from);

    if (get_bit(pos->occupancies[enemy], to)) {
        int start_piece = (side == WHITE) ? p : P; 
        int end_piece = (side == WHITE) ? k : K;   
        
        for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++) {
            if (get_bit(pos->bitboards[bb_piece], to)) {
                pop_bit(pos->bitboards[bb_piece], to);
                pos->hash_key ^= piece_keys[bb_piece][to];
                break;
            }
        }
        pop_bit(pos->occupancies[enemy], to);
    } 
    
    else if ((piece == P || piece == p) && (from % 8 != to % 8)) {
        int captured_pawn_sq = (side == WHITE) ? to - 8 : to + 8;
        int captured_pawn_piece = (side == WHITE) ? p : P;
        
        pop_bit(pos->bitboards[captured_pawn_piece], captured_pawn_sq);
        pop_bit(pos->occupancies[enemy], captured_pawn_sq);
        pop_bit(pos->occupancies[BOTH], captured_pawn_sq);
        pos->hash_key ^= piece_keys[captured_pawn_piece][captured_pawn_sq];
    }

    if (get_promotion(move)) {
        int promoted = get_promoted(move);
        set_bit(pos->bitboards[promoted], to);
        pos->hash_key ^= piece_keys[promoted][to];
    } else {
        set_bit(pos->bitboards[piece], to);
        pos->hash_key ^= piece_keys[piece][to];
    }
    set_bit(pos->occupancies[side], to);
    set_bit(pos->occupancies[BOTH], to);

    if (get_castling(move)) {
        int rook = (side == WHITE) ? R : r;
        int r_from = 0, r_to = 0;
        
        if (to == 6)       { r_from = 7;  r_to = 5;  } 
        else if (to == 2)  { r_from = 0;  r_to = 3;  } 
        else if (to == 62) { r_from = 63; r_to = 61; } 
        else if (to == 58) { r_from = 56; r_to = 59; } 
        
        pop_bit(pos->bitboards[rook], r_from);
        pop_bit(pos->occupancies[side], r_from);
        pop_bit(pos->occupancies[BOTH], r_from);
        
        set_bit(pos->bitboards[rook], r_to);
        set_bit(pos->occupancies[side], r_to);
        set_bit(pos->occupancies[BOTH], r_to);

        pos->hash_key ^= piece_keys[rook][r_from];
        pos->hash_key ^= piece_keys[rook][r_to];
    }

    if (piece == K) pos->castling_rights &= ~3;
    if (piece == k) pos->castling_rights &= ~12;
    if (piece == R && from == 0) pos->castling_rights &= ~2;
    if (piece == R && from == 7) pos->castling_rights &= ~1;
    if (piece == r && from == 56) pos->castling_rights &= ~8;
    if (piece == r && from == 63) pos->castling_rights &= ~4;
    if (to == 0) pos->castling_rights &= ~2;
    if (to == 7) pos->castling_rights &= ~1;
    if (to == 56) pos->castling_rights &= ~8;
    if (to == 63) pos->castling_rights &= ~4;

    pos->hash_key ^= castle_keys[pos->castling_rights];

    pos->side_to_move = enemy;
    pos->hash_key ^= side_key;
}