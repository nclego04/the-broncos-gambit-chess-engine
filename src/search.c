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

#define INF 50000
#define MATE 49000

static double search_start_time = 0.0;
static int search_time_limit = 0;
static int stop_search = 0;
static uint64_t nodes_searched = 0;
static int current_search_depth = 0;

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
    if (current_search_depth > 1 && (nodes_searched++ & 2047) == 0) check_time();
    if (stop_search) return 0;

    if (depth == 0) {
        // TODO: Implement Quiescence Search here to resolve pending captures and 
        // eliminate the Horizon Effect before calling the static evaluation.
        return evaluate_position(pos);
    }

    MoveList list;
    generate_all_moves(pos, &list);
    
    // TODO: Implement MVV-LVA Move Ordering here. Sorting captures first will 
    // drastically lower the Effective Branching Factor (EBF) and increase depth.

    int legal_moves = 0;
    int side = pos->side_to_move;
    int enemy = (side == WHITE) ? BLACK : WHITE;
    int our_king = (side == WHITE) ? K : k;

    for (int i = 0; i < list.count; i++) {
        Board next_pos = *pos;
        make_move(&next_pos, list.moves[i]);

        if (next_pos.bitboards[our_king] == 0) continue; 
        int king_sq = get_lsb(next_pos.bitboards[our_king]);
        if (is_square_attacked(king_sq, enemy, &next_pos)) continue;

        legal_moves++;

        int score = -alpha_beta(&next_pos, -beta, -alpha, depth - 1, ply + 1);

        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
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

        for (int i = 0; i < list.count; i++) {
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

    for (int i = 0; i < list.count; i++) {
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