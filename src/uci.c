/**
 * @file uci.c
 * @brief Universal Chess Interface (UCI) protocol parser.
 *
 * Handles standard I/O communication with chess GUIs (like Cutechess or Arena).
 * Parses setup commands, time controls, and triggers the search algorithm
 * while correctly formatting output data.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "types.h"
#include "uci.h"
#include "board.h"
#include "movegen.h"
#include "benchmark.h"
#include "evaluate.h"
#include "search.h"
#include "logger.h"

/**
 * @brief Parses a UCI move string and matches it to a valid engine move.
 *
 * Handles coordinate translation (e.g. "e2e4") and matches promotion pieces.
 *
 * @param pos Pointer to the current board state.
 * @param move_string The null-terminated UCI coordinate string.
 * @return The fully encoded integer move, or 0 if the move is illegal/invalid.
 */
static int parse_move(Board *pos, char *move_string) {
    int from = (move_string[0] - 'a') + (move_string[1] - '1') * 8;
    int to = (move_string[2] - 'a') + (move_string[3] - '1') * 8;

    MoveList list;
    generate_all_moves(pos, &list);

    for (int i = 0; i < list.count; i++) {
        int move = list.moves[i];
        if (get_from(move) == from && get_to(move) == to) {
            if (get_promotion(move)) {
                int promoted = get_promoted(move);
                char promo_char = move_string[4];
                
                if ((promo_char == 'q' && (promoted == Q || promoted == q)) ||
                    (promo_char == 'r' && (promoted == R || promoted == r)) ||
                    (promo_char == 'b' && (promoted == B || promoted == b)) ||
                    (promo_char == 'n' && (promoted == N || promoted == n))) {
                    return move;
                }
            } else {
                return move;
            }
        }
    }

    int side = pos->side_to_move;
    int pawn = (side == WHITE) ? P : p;
    if (get_bit(pos->bitboards[pawn], from)) {
        if ((from % 8) != (to % 8) && !get_bit(pos->occupancies[BOTH], to)) {
            return encode_move(from, to, pawn, 0, 0, 0);
        }
    }

    return 0; 
}

/**
 * @brief The main execution loop for the Universal Chess Interface (UCI).
 *
 * Continuously reads from standard input and processes GUI commands.
 */
void uci_loop() {
    char line[8192]; 
    Board pos;

    parse_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    srand(time(NULL));

    logger_init("=== ENGINE BOOTUP ===");

    while (fgets(line, sizeof(line), stdin)) {
        if (strncmp(line, "isready", 7) == 0) {
            printf("readyok\n");
            fflush(stdout);
        } 
        else if (strncmp(line, "ucinewgame", 10) == 0) {
            parse_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            
            logger_init("=== NEW GAME STARTED ===");
        } 
        else if (strncmp(line, "position", 8) == 0) {
            if (strstr(line, "startpos")) {
                parse_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            } else {
                char *fen_str = strstr(line, "fen ");
                if (fen_str != NULL) {
                    parse_fen(&pos, fen_str + 4);
                }
            }
            
            char *moves = strstr(line, "moves ");
            if (moves != NULL) {
                moves += 6; 
                
                char *token = strtok(moves, " \n\r");
                while (token != NULL) {
                    int move = parse_move(&pos, token);
                    if (move) make_move(&pos, move);
                    token = strtok(NULL, " \n\r");
                }
            }
        } 
        else if (strncmp(line, "go", 2) == 0) {
            int time_limit = 1000;
            
            char *movetime_str = strstr(line, "movetime ");
            if (movetime_str) {
                time_limit = atoi(movetime_str + 9);
            } else {
                char *wtime_str = strstr(line, "wtime ");
                char *btime_str = strstr(line, "btime ");
                int time_left = 1000;
                
                if (pos.side_to_move == WHITE && wtime_str) {
                    time_left = atoi(wtime_str + 6);
                } else if (pos.side_to_move == BLACK && btime_str) {
                    time_left = atoi(btime_str + 6);
                }
                
                time_limit = time_left / 30;
            }
            
            time_limit -= 20;
            if (time_limit < 1) time_limit = 1; 

            int best_move = search_position(&pos, time_limit);
            
            if (best_move != 0) {
                int from = get_from(best_move);
                int to = get_to(best_move);
                
                if (get_promotion(best_move)) {
                    int p = get_promoted(best_move);
                    char promo = (p == Q || p == q) ? 'q' : (p == R || p == r) ? 'r' : (p == B || p == b) ? 'b' : 'n';
                    printf("bestmove %c%c%c%c%c\n", 
                           'a' + (from % 8), '1' + (from / 8), 
                           'a' + (to % 8),   '1' + (to / 8), promo);
                } else {
                    printf("bestmove %c%c%c%c\n", 
                           'a' + (from % 8), '1' + (from / 8), 
                           'a' + (to % 8),   '1' + (to / 8));
                }
                fflush(stdout);
            } else {
                printf("bestmove 0000\n");
                fflush(stdout);
            }
        } 
        else if (strncmp(line, "bench_nps_ebf", 13) == 0) {
            char filename[256] = "bratko_kopec.epd";
            int depth = 6;
            // Optionally parse custom filename and depth from the command
            if (sscanf(line + 13, "%s %d", filename, &depth) >= 1) { }
            
            bench_nps_ebf(filename, depth);
        }
        else if (strncmp(line, "bench_avg_depth", 15) == 0) {
            char filename[256] = "bratko_kopec.epd";
            int time_ms = 1000;
            
            if (sscanf(line + 15, "%s %d", filename, &time_ms) >= 1) { }
            
            bench_avg_depth(filename, time_ms);
        }
        else if (strncmp(line, "eval", 4) == 0) {
            int score = evaluate_position(&pos);
            printf("Static Evaluation: %d centipawns\n", score);
            fflush(stdout);
        }
        else if (strncmp(line, "quit", 4) == 0) {
            break;
        }
        else if (strncmp(line, "uci", 3) == 0) {
            printf("id name The Broncos Gambit\n");
            printf("id author Group 6\n");
            printf("uciok\n");
            fflush(stdout);
        }
    }
}
