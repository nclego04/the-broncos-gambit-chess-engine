#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "types.h"
#include "benchmark.h"
#include "board.h"
#include "movegen.h"

// --- High Precision Timer ---
static double get_time_ms() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (time.tv_sec * 1000.0) + (time.tv_usec / 1000.0);
}

static uint64_t perft(Board *pos, int depth) {
    if (depth == 0) return 1ULL;
    
    MoveList list;
    generate_all_moves(pos, &list);
    
    uint64_t nodes = 0;
    int side = pos->side_to_move;
    int enemy = (side == WHITE) ? BLACK : WHITE;
    int our_king = (side == WHITE) ? K : k;
    
    for (int i = 0; i < list.count; i++) {
        Board next_pos = *pos;
        make_move(&next_pos, list.moves[i]);
        
        if (next_pos.bitboards[our_king] == 0) continue; 
        int king_sq = get_lsb(next_pos.bitboards[our_king]);
        if (is_square_attacked(king_sq, enemy, &next_pos)) continue;
        
        nodes += perft(&next_pos, depth - 1);
    }
    return nodes;
}

static void bench_position(const char* name, const char* fen, int max_depth, uint64_t *total_nodes, double *total_time_ms) {
    Board pos;
    parse_fen(&pos, fen);
    
    printf("\n--- Benchmarking: %s ---\n", name);
    printf("FEN: %s\n", fen);
    
    uint64_t prev_nodes = 0;
    
    for (int depth = 1; depth <= max_depth; depth++) {
        double start_time = get_time_ms();
        uint64_t nodes_searched = perft(&pos, depth);
        
        double ebf = (prev_nodes > 0) ? (double)nodes_searched / prev_nodes : 0.0;
        prev_nodes = nodes_searched;
        
        double elapsed_ms = get_time_ms() - start_time;
        double elapsed_sec = elapsed_ms / 1000.0;
        uint64_t nps = (elapsed_sec > 0.001) ? (uint64_t)(nodes_searched / elapsed_sec) : 0;
        
        printf("Depth %d | Nodes: %8llu | EBF: %5.2f | Time: %6.0f ms | NPS: %llu\n", 
               depth, (unsigned long long)nodes_searched, ebf, elapsed_ms, (unsigned long long)nps);
               
        if (total_nodes) *total_nodes += nodes_searched;
        if (total_time_ms) *total_time_ms += elapsed_ms;
    }
}

void run_benchmark() {
    uint64_t total_nodes = 0;
    double total_time_ms = 0.0;

    printf("====================================================\n");
    printf("               ENGINE BENCHMARK TOOL                \n");
    printf("====================================================\n");
    bench_position("Starting Position", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, &total_nodes, &total_time_ms);
    bench_position("Complex Midgame (Kiwipete)", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, &total_nodes, &total_time_ms);
    bench_position("Deep Endgame", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7, &total_nodes, &total_time_ms); 
    
    printf("\n====================================================\n");
    double total_time_sec = total_time_ms / 1000.0;
    uint64_t average_nps = (total_time_sec > 0.001) ? (uint64_t)(total_nodes / total_time_sec) : 0;
    printf("Total Nodes: %llu\n", (unsigned long long)total_nodes);
    printf("Total Time : %.0f ms\n", total_time_ms);
    printf("Average NPS: %llu\n", (unsigned long long)average_nps);
    printf("====================================================\n");
}