/**
 * @file evaluate.c
 * @brief Static position evaluation.
 *
 * Calculates a heuristic score for a given board position based on material
 * advantage and Piece-Square Tables (PSTs).
 */
#include "evaluate.h"

const int piece_values[12] = {
    100, 300, 320, 500, 900, 20000, // WHITE: P, N, B, R, Q, K
    100, 300, 320, 500, 900, 20000  // BLACK: p, n, b, r, q, k
};

const int pawn_pst[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
     50, 50, 50, 50, 50, 50, 50, 50,
     10, 10, 20, 30, 30, 20, 10, 10,
      5,  5, 10, 25, 25, 10,  5,  5,
      0,  0,  0, 20, 20,  0,  0,  0,
      5, -5,-10,  0,  0,-10, -5,  5,
      5, 10, 10,-20,-20, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0
};

const int knight_pst[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

const int bishop_pst[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

const int king_pst[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20
};

/**
 * @brief Mirrors a square vertically on the chessboard.
 *
 * @param sq The square index to mirror (0-63).
 * @return The mirrored square index, essentially flipping Rank 1 to Rank 8.
 */
static inline int mirror_square(int sq) {
    return sq ^ 56;
}

/**
 * @brief Evaluates the board state from the perspective of the side to move.
 *
 * @param pos Pointer to the current board state.
 * @return A score in centipawns. Positive values favor the side to move, 
 *         satisfying the mathematical requirements of the Negamax framework.
 */
int evaluate_position(Board *pos) {
    int score = 0;

    for (int piece = P; piece <= k; piece++) {
        U64 bitboard = pos->bitboards[piece];
        
        while (bitboard) {
            int sq = get_lsb(bitboard);
            
            int material = piece_values[piece];
            int positional = 0;
            
            switch(piece) {
                case P: positional = pawn_pst[mirror_square(sq)]; break;
                case N: positional = knight_pst[mirror_square(sq)]; break;
                case B: positional = bishop_pst[mirror_square(sq)]; break;
                case K: positional = king_pst[mirror_square(sq)]; break;
                
                case p: positional = pawn_pst[sq]; break;
                case n: positional = knight_pst[sq]; break;
                case b: positional = bishop_pst[sq]; break;
                case k: positional = king_pst[sq]; break;
                
                default: break;
            }
            
            if (piece >= P && piece <= K) {
                score += material + positional;
            } else {
                score -= material + positional;
            }
            
            pop_bit(bitboard, sq);
        }
    }
    
    return (pos->side_to_move == WHITE) ? score : -score;
}
