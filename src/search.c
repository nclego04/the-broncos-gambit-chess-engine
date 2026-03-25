/**
 * @file search.c
 * @brief The Alpha-Beta (Negamax) search algorithm.
 *
 * Implements Iterative Deepening and Time Management to find the best move 
 * within a given time limit. Computes live performance metrics (NPS, EBF)
 * and logs them to disk.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <time.h>
#include "search.h"
#include "evaluate.h"
#include "movegen.h"
#include "board.h"
#include "logger.h"
#include "tt.h"

// --- Alpha-Beta Search Constants ---
#define INF 50000
#define MATE 49000

// --- Engine Search State ---
static double search_start_time = 0.0;     /**< Time when the current search started. */
static int search_time_limit = 0;          /**< Allowed time for the current search in ms. */
static int stop_search = 0;                /**< Flag to aggressively abort search tree traversal. */
static uint64_t nodes_searched = 0;        /**< Running count of total evaluated nodes. */
static int current_search_depth = 0;       /**< Current depth level being processed by iterative deepening. */

/**
 * @brief Gets the current high-precision time in milliseconds.
 *
 * @return The time in milliseconds since an unspecified epoch (monotonic).
 */
static double get_time_ms() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (time.tv_sec * 1000.0) + (time.tv_nsec / 1000000.0);
}

/**
 * @brief Checks if the allocated time limit has been exceeded.
 *
 * Modifies the global `stop_search` flag if the engine runs out of time.
 */
static void check_time() {
    if (get_time_ms() - search_start_time >= search_time_limit) {
        stop_search = 1;
    }
}

/**
 * @brief Gets the highest fully completed depth of the last search.
 *
 * @return The depth reached. If the search aborted mid-way, returns depth - 1.
 */
int get_search_depth(void) {
    // If the search was aborted by time, the current depth didn't finish!
    return stop_search ? (current_search_depth - 1) : current_search_depth;
}

/**
 * @brief Simple base piece values for move ordering.
 */
static const int piece_scores[12] = {
    100, 300, 320, 500, 900, 20000, // WHITE: P, N, B, R, Q, K
    100, 300, 320, 500, 900, 20000  // BLACK: p, n, b, r, q, k
};

/**
 * @brief Assigns a heuristic score to a move for MVV-LVA move ordering.
 *
 * @param pos The current board position.
 * @param move The encoded move to score.
 * @param hash_move The best move found in a previous iterative deepening depth.
 * @return An integer score representing the move's estimated strength.
 */
static int score_move(Board *pos, int move, int hash_move) {
    if (move == hash_move) return 2000000; 
    
    int score = 0;
    int to = get_to(move);
    int piece = get_piece(move);
    int side = pos->side_to_move;
    int enemy = (side == WHITE) ? BLACK : WHITE;
    
    if (get_bit(pos->occupancies[enemy], to)) {
        int victim = P; 
        int start_piece = (side == WHITE) ? p : P;
        int end_piece = (side == WHITE) ? k : K;
        for (int p_idx = start_piece; p_idx <= end_piece; p_idx++) {
            if (get_bit(pos->bitboards[p_idx], to)) {
                victim = p_idx;
                break;
            }
        }
        // MVV-LVA: Most Valuable Victim - Least Valuable Attacker
        score = 1000000 + piece_scores[victim] * 10 - piece_scores[piece];
    }
    
    if (get_promotion(move)) {
        score += piece_scores[get_promoted(move)] * 1000; 
    }
    
    return score;
}

/**
 * @brief Sorts the next best move to the current index via selection sort.
 */
static inline void sort_next_move(MoveList *list, int *scores, int current_idx) {
    int best_score = -10000000;
    int best_idx = current_idx;
    for (int j = current_idx; j < list->count; j++) {
        if (scores[j] > best_score) {
            best_score = scores[j];
            best_idx = j;
        }
    }
    int temp_move = list->moves[current_idx];
    list->moves[current_idx] = list->moves[best_idx];
    list->moves[best_idx] = temp_move;
    
    int temp_score = scores[current_idx];
    scores[current_idx] = scores[best_idx];
    scores[best_idx] = temp_score;
}

/**
 * @brief Quiescence Search to resolve noisy positions (captures/promotions) 
 *        and eliminate the horizon effect.
 */
static int quiescence(Board *pos, int alpha, int beta, int ply) {
    if ((nodes_searched++ & 2047) == 0) check_time();
    if (stop_search) return 0;

    // Circuit breaker to prevent Stack Overflow in explosive tactical positions
    if (ply > 64) return evaluate_position(pos);

    int stand_pat = evaluate_position(pos);
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    MoveList list;
    generate_all_moves(pos, &list);

    int scores[256];
    for (int i = 0; i < list.count; i++) {
        scores[i] = score_move(pos, list.moves[i], 0);
    }

    int side = pos->side_to_move;
    int enemy = (side == WHITE) ? BLACK : WHITE;
    int our_king = (side == WHITE) ? K : k;

    for (int i = 0; i < list.count; i++) {
        sort_next_move(&list, scores, i);
        int move = list.moves[i];
        
        if (!get_bit(pos->occupancies[enemy], get_to(move)) && !get_promotion(move)) continue;

        Board next_pos = *pos;
        make_move(&next_pos, move);

        if (next_pos.bitboards[our_king] == 0) continue; 
        int king_sq = get_lsb(next_pos.bitboards[our_king]);
        if (is_square_attacked(king_sq, enemy, &next_pos)) continue;

        int score = -quiescence(&next_pos, -beta, -alpha, ply + 1);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

/**
 * @brief Evaluates a position using the Negamax algorithm with Alpha-Beta pruning.
 *
 * Recursively explores the game tree up to the specified remaining depth. 
 * Incorporates time-checking to abort early if the time limit is exceeded.
 *
 * @param pos   Pointer to the current board state (passed by value-copy internally).
 * @param alpha The minimum score the maximizing player is assured of.
 * @param beta  The maximum score the minimizing player is assured of.
 * @param depth The remaining number of plies to search before dropping to static eval.
 * @param ply   The distance from the root node (used to prefer faster checkmates).
 * 
 * @return The evaluation score of the position in centipawns, from the perspective
 *         of the side to move. Returns 0 immediately if search is globally stopped.
 */
static int alpha_beta(Board *pos, int alpha, int beta, int depth, int ply) {
    if ((nodes_searched++ & 2047) == 0) check_time();
    if (stop_search) return 0;

    if (depth == 0) {
        return quiescence(pos, alpha, beta, ply);
    }

    int hash_move = 0;
    int tt_score = probe_tt(pos->hash_key, depth, alpha, beta, &hash_move, ply);
    if (tt_score != TT_UNKNOWN) {
        return tt_score;
    }
    int alpha_orig = alpha;

    MoveList list;
    generate_all_moves(pos, &list);
    
    int scores[256];
    for (int i = 0; i < list.count; i++) {
        scores[i] = score_move(pos, list.moves[i], hash_move);
    }

    int legal_moves = 0;
    int side = pos->side_to_move;
    int enemy = (side == WHITE) ? BLACK : WHITE;
    int our_king = (side == WHITE) ? K : k;
    int best_move = 0;

    for (int i = 0; i < list.count; i++) {
        sort_next_move(&list, scores, i);
        
        Board next_pos = *pos;
        make_move(&next_pos, list.moves[i]);

        if (next_pos.bitboards[our_king] == 0) continue; 
        int king_sq = get_lsb(next_pos.bitboards[our_king]);
        if (is_square_attacked(king_sq, enemy, &next_pos)) continue;

        legal_moves++;

        int score = -alpha_beta(&next_pos, -beta, -alpha, depth - 1, ply + 1);

        if (score >= beta) {
            store_tt(pos->hash_key, depth, TT_BETA, beta, list.moves[i], ply);
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            best_move = list.moves[i];
        }
    }

    if (legal_moves == 0) {
        int king_sq = get_lsb(pos->bitboards[our_king]);
        if (is_square_attacked(king_sq, enemy, pos)) {
            return -MATE + ply;
        } else {
            return 0;
        }
    }

    int flag = (alpha > alpha_orig) ? TT_EXACT : TT_ALPHA;
    store_tt(pos->hash_key, depth, flag, alpha, best_move, ply);

    return alpha;
}

/**
 * @brief Initiates an Iterative Deepening search for the best move.
 *
 * @param pos Pointer to the board position to evaluate.
 * @param time_limit_ms The maximum allowed search time in milliseconds.
 * @return The encoded integer representing the best move found, or 0 if 
 *         aborted instantly or no legal moves exist.
 */
int search_position(Board *pos, int time_limit_ms) {
    search_start_time = get_time_ms();
    search_time_limit = time_limit_ms;
    stop_search = 0;
    nodes_searched = 0;
    
    int best_move = 0;
    int current_best_move = 0;
    
    // --- Fallback Move Selection ---
    // Ensure we always have at least one legal move to return in case
    // the search runs out of time immediately during Depth 1.
    MoveList root_list;
    generate_all_moves(pos, &root_list);
    int side = pos->side_to_move;
    int enemy = (side == WHITE) ? BLACK : WHITE;
    int our_king = (side == WHITE) ? K : k;
    for (int i = 0; i < root_list.count; i++) {
        Board test_pos = *pos;
        make_move(&test_pos, root_list.moves[i]);
        if (test_pos.bitboards[our_king] == 0) continue;
        int king_sq = get_lsb(test_pos.bitboards[our_king]);
        if (!is_square_attacked(king_sq, enemy, &test_pos)) {
            best_move = root_list.moves[i];
            break;
        }
    }

    logger_start_search();
    
    uint64_t prev_total_nodes = 0;
    uint64_t prev_iteration_nodes = 0;
    
    for (int depth = 1; depth <= 64; depth++) {
        current_search_depth = depth;

        MoveList list;
        generate_all_moves(pos, &list);

        int alpha = -INF;
        int beta = INF;

        int side = pos->side_to_move;
        int enemy = (side == WHITE) ? BLACK : WHITE;
        int our_king = (side == WHITE) ? K : k;

        current_best_move = 0;

        int scores[256];
        for (int i = 0; i < list.count; i++) {
            scores[i] = score_move(pos, list.moves[i], best_move);
        }

        for (int i = 0; i < list.count; i++) {
            sort_next_move(&list, scores, i);

            Board next_pos = *pos;
            make_move(&next_pos, list.moves[i]);

            if (next_pos.bitboards[our_king] == 0) continue;
            int king_sq = get_lsb(next_pos.bitboards[our_king]);
            if (is_square_attacked(king_sq, enemy, &next_pos)) continue;

            int score = -alpha_beta(&next_pos, -beta, -alpha, depth - 1, 1);
            
            if (stop_search) break;

            if (score > alpha) {
                alpha = score;
                current_best_move = list.moves[i];
            }
        }
        
        if (stop_search) {
            break; 
        }
        
        if (current_best_move != 0) {
            best_move = current_best_move;
            
            double time_spent = get_time_ms() - search_start_time;
            
            uint64_t nps = (time_spent > 1.0) ? (uint64_t)((nodes_searched * 1000.0) / time_spent) : 0;
            
            uint64_t nodes_this_iteration = nodes_searched - prev_total_nodes;
            double ebf = (prev_iteration_nodes > 0) ? (double)nodes_this_iteration / prev_iteration_nodes : 0.0;
            
            logger_log_depth(depth, nodes_searched, ebf, (int)time_spent, nps);
            
            prev_total_nodes = nodes_searched;
            prev_iteration_nodes = nodes_this_iteration;
        }
    }

    logger_end_search();

    return best_move;
}

/**
 * @brief Runs a fixed-depth Alpha-Beta search for benchmarking purposes.
 *
 * @param pos Pointer to the board position.
 * @param depth The target depth to search.
 * @return The total number of nodes evaluated.
 */
uint64_t search_for_benchmark(Board *pos, int depth) {
    search_start_time = get_time_ms();
    search_time_limit = 99999999; // Effectively infinite so it never aborts
    stop_search = 0;
    nodes_searched = 0;
    current_search_depth = depth;

    MoveList list;
    generate_all_moves(pos, &list);

    int alpha = -INF;
    int beta = INF;
    int side = pos->side_to_move;
    int enemy = (side == WHITE) ? BLACK : WHITE;
    int our_king = (side == WHITE) ? K : k;

    int scores[256];
    for (int i = 0; i < list.count; i++) {
        scores[i] = score_move(pos, list.moves[i], 0);
    }

    for (int i = 0; i < list.count; i++) {
        sort_next_move(&list, scores, i);

        Board next_pos = *pos;
        make_move(&next_pos, list.moves[i]);
        if (next_pos.bitboards[our_king] == 0) continue;
        int king_sq = get_lsb(next_pos.bitboards[our_king]);
        if (is_square_attacked(king_sq, enemy, &next_pos)) continue;
        int score = -alpha_beta(&next_pos, -beta, -alpha, depth - 1, 1);
        if (score > alpha) alpha = score;
    }
    return nodes_searched;
}