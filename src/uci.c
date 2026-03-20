#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "types.h"
#include "uci.h"
#include "board.h"
#include "movegen.h"
#include "benchmark.h"

// Helper to parse a UCI string (e.g. "e2e4") and match it to a valid packed integer move
static int parse_move(Board *pos, char *move_string) {
    int from = (move_string[0] - 'a') + (move_string[1] - '1') * 8;
    int to = (move_string[2] - 'a') + (move_string[3] - '1') * 8;

    MoveList list;
    generate_all_moves(pos, &list);

    for (int i = 0; i < list.count; i++) {
        int move = list.moves[i];
        if (get_from(move) == from && get_to(move) == to) {
            // If it's a promotion, we must match the exact piece the GUI requested!
            if (get_promotion(move)) {
                int promoted = get_promoted(move);
                char promo_char = move_string[4]; // 'q', 'r', 'b', 'n'
                
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
    return 0; // Illegal move
}

// --- The UCI Communication Loop ---
void uci_loop() {
    char line[2048]; 
    Board pos;

    parse_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    srand(time(NULL));

    while (fgets(line, sizeof(line), stdin)) {
        // VERY IMPORTANT: Check longer strings first!
        // "ucinewgame" must be checked before "uci" so it doesn't get accidentally intercepted.
        if (strncmp(line, "isready", 7) == 0) {
            printf("readyok\n");
            fflush(stdout); // Force output to Java
        } 
        else if (strncmp(line, "ucinewgame", 10) == 0) {
            parse_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
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
            
            // Parse the "moves" section
            char *moves = strstr(line, "moves ");
            if (moves != NULL) {
                moves += 6; // Skip over the "moves " part
                
                // Tokenize the string by spaces to get each move (e.g. "e2e4", "e7e5")
                char *token = strtok(moves, " \n\r");
                while (token != NULL) {
                    int move = parse_move(&pos, token);
                    if (move) make_move(&pos, move);
                    token = strtok(NULL, " \n\r");
                }
            }
        } 
        else if (strncmp(line, "go", 2) == 0) {
            MoveList list;
            generate_all_moves(&pos, &list);
            
            int legal_moves[256];
            int legal_count = 0;
            int side = pos.side_to_move;
            int enemy = (side == WHITE) ? BLACK : WHITE;
            int our_king = (side == WHITE) ? K : k;

            for (int i = 0; i < list.count; i++) {
                Board next_pos = pos;
                make_move(&next_pos, list.moves[i]);
                
                if (next_pos.bitboards[our_king] == 0) continue; 
                int king_sq = get_lsb(next_pos.bitboards[our_king]);
                if (is_square_attacked(king_sq, enemy, &next_pos)) continue;
                
                legal_moves[legal_count++] = list.moves[i];
            }
            
            if (legal_count > 0) {
                int random_index = rand() % legal_count;
                int move = legal_moves[random_index];
                int from = get_from(move);
                int to = get_to(move);
                
                if (get_promotion(move)) {
                    int p = get_promoted(move);
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
        else if (strncmp(line, "bench", 5) == 0) {
            run_benchmark();
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
