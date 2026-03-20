#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "types.h"
#include "board.h"

// Helper function to map a character to our enum index
int char_to_piece(char c) {
    switch(c) {
        case 'P': return P; case 'N': return N; case 'B': return B;
        case 'R': return R; case 'Q': return Q; case 'K': return K;
        case 'p': return p; case 'n': return n; case 'b': return b;
        case 'r': return r; case 'q': return q; case 'k': return k;
        default: return -1;
    }
}

void parse_fen(Board *pos, const char *fen) {
    // 1. Wipe the board completely clean (All bits to 0)
    memset(pos, 0, sizeof(Board));
    pos->en_passant = -1;

    int rank = 7;
    int file = 0;
    int i = 0;

    // 2. Parse the piece placement data
    while (fen[i] != ' ' && fen[i] != '\0') {
        char c = fen[i];
        
        if (c == '/') {
            rank--;
            file = 0;
        } else if (isdigit((unsigned char)c)) {
            // A number means empty squares, just skip the file index forward
            file += (c - '0');
        } else {
            int piece = char_to_piece(c);
            if (piece != -1) {
                // Calculate square index (a1 is 0, h8 is 63)
                int sq = rank * 8 + file;
                
                // Toggle the 1 bit on the specific piece bitboard!
                set_bit(pos->bitboards[piece], sq);
                
                // Toggle the 1 bit on the occupancy bitboards!
                int color = isupper((unsigned char)c) ? WHITE : BLACK;
                set_bit(pos->occupancies[color], sq);
                set_bit(pos->occupancies[BOTH], sq);
            }
            file++;
        }
        i++;
    }

    // 3. Parse side to move
    i++; // skip the space
    if (fen[i] == 'b') pos->side_to_move = BLACK;
    else pos->side_to_move = WHITE;
    
    i++; // move to the space after w/b
    i++; // move to the start of castling rights
    pos->castling_rights = 0;
    while (fen[i] != ' ' && fen[i] != '\0') {
        if (fen[i] == 'K') pos->castling_rights |= 1;
        else if (fen[i] == 'Q') pos->castling_rights |= 2;
        else if (fen[i] == 'k') pos->castling_rights |= 4;
        else if (fen[i] == 'q') pos->castling_rights |= 8;
        i++;
    }
}

// --- Move Execution ---
void make_move(Board *pos, int move) {
    int from = get_from(move);
    int to = get_to(move);
    int piece = get_piece(move);
    
    int side = pos->side_to_move;
    int enemy = (side == WHITE) ? BLACK : WHITE;

    // 1. Pick up the moving piece
    pop_bit(pos->bitboards[piece], from);
    pop_bit(pos->occupancies[side], from);
    pop_bit(pos->occupancies[BOTH], from);

    // 2. Handle Captures (Is there an enemy piece on the target square?)
    if (get_bit(pos->occupancies[enemy], to)) {
        // Find exactly which enemy piece we captured and remove it
        int start_piece = (side == WHITE) ? p : P; // p=6, P=0
        int end_piece = (side == WHITE) ? k : K;   // k=11, K=5
        
        for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++) {
            if (get_bit(pos->bitboards[bb_piece], to)) {
                pop_bit(pos->bitboards[bb_piece], to);
                break;
            }
        }
        pop_bit(pos->occupancies[enemy], to);
    }

    // 3. Put the moving piece down on the target square
    if (get_promotion(move)) {
        int promoted = get_promoted(move);
        set_bit(pos->bitboards[promoted], to);
    } else {
        set_bit(pos->bitboards[piece], to);
    }
    set_bit(pos->occupancies[side], to);
    set_bit(pos->occupancies[BOTH], to);

    // 4. Handle Castling (Move the Rook!)
    if (get_castling(move)) {
        int rook = (side == WHITE) ? R : r;
        int r_from = 0, r_to = 0;
        
        if (to == 6)       { r_from = 7;  r_to = 5;  } // White Kingside (h1 -> f1)
        else if (to == 2)  { r_from = 0;  r_to = 3;  } // White Queenside (a1 -> d1)
        else if (to == 62) { r_from = 63; r_to = 61; } // Black Kingside (h8 -> f8)
        else if (to == 58) { r_from = 56; r_to = 59; } // Black Queenside (a8 -> d8)
        
        // Pick up the Rook
        pop_bit(pos->bitboards[rook], r_from);
        pop_bit(pos->occupancies[side], r_from);
        pop_bit(pos->occupancies[BOTH], r_from);
        
        // Put the Rook down
        set_bit(pos->bitboards[rook], r_to);
        set_bit(pos->occupancies[side], r_to);
        set_bit(pos->occupancies[BOTH], r_to);
    }

    // 5. Update Castling Rights (If King or Rook moves, or a Rook is captured)
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

    // 6. Swap whose turn it is
    pos->side_to_move = enemy;
}