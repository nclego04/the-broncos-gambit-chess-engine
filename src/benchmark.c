/**
 * @file benchmark.c
 * @brief Performance testing and benchmarking suite.
 *
 * Runs Alpha-Beta search simulations to calculate NPS and EBF across various depths.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "types.h"
#include "benchmark.h"
#include "board.h"
#include "movegen.h"
#include "search.h"
#include "logger.h"

/**
 * @brief Gets the current system time in milliseconds with high precision.
 *
 * @return The time in milliseconds since an unspecified epoch (monotonic clock).
 */
static double get_time_ms() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (time.tv_sec * 1000.0) + (time.tv_nsec / 1000000.0);
}

/**
 * @brief Benchmarks the engine against a standardized suite of positions (EPD/FEN file).
 *
 * @param filename The name of the EPD/FEN suite file.
 * @param depth The target depth to search each position.
 */
void bench_nps_ebf(const char *filename, int depth) {
    FILE *f = logger_open_file(filename, "r");
    if (!f) {
        printf("Error: Could not open suite file '%s'\n", filename);
        return;
    }

    char line[2048];
    uint64_t total_nodes = 0;
    double total_time_ms = 0.0;
    int pos_count = 0;
    
    FILE *out = logger_open_file("benchmark_nps_ebf.txt", "w");
    if (!out) {
        printf("Note: Could not open tests/benchmark_nps_ebf.txt for writing.\n");
    }

    printf("Running suite '%s' at Depth %d...\n", filename, depth);
    if (out) fprintf(out, "Running suite '%s' at Depth %d...\n", filename, depth);

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || line[0] == '\0') continue;
        
        Board pos;
        parse_fen(&pos, line); 
        
        double start_time = get_time_ms();
        total_nodes += search_for_benchmark(&pos, depth);
        total_time_ms += (get_time_ms() - start_time);
        pos_count++;
    }
    fclose(f);

    if (pos_count == 0) {
        if (out) fclose(out);
        return;
    }

    double avg_nodes = (double)total_nodes / pos_count;
    double overall_ebf = pow(avg_nodes, 1.0 / depth);
    uint64_t overall_nps = (total_time_ms > 1.0) ? (uint64_t)((total_nodes * 1000.0) / total_time_ms) : 0;

    printf("\n--- Suite Benchmark Results ---\n");
    if (out) fprintf(out, "\n--- Suite Benchmark Results ---\n");
    printf("Positions  : %d\n", pos_count);
    if (out) fprintf(out, "Positions  : %d\n", pos_count);
    printf("Total Nodes: %llu\n", (unsigned long long)total_nodes);
    if (out) fprintf(out, "Total Nodes: %llu\n", (unsigned long long)total_nodes);
    printf("Total Time : %.0f ms\n", total_time_ms);
    if (out) fprintf(out, "Total Time : %.0f ms\n", total_time_ms);
    printf("Overall NPS: %llu\n", (unsigned long long)overall_nps);
    if (out) fprintf(out, "Overall NPS: %llu\n", (unsigned long long)overall_nps);
    printf("Overall EBF: %5.2f\n", overall_ebf);
    if (out) fprintf(out, "Overall EBF: %5.2f\n", overall_ebf);
    printf("-------------------------------\n");
    if (out) fprintf(out, "-------------------------------\n");
    
    if (out) fclose(out);
}

/**
 * @brief Benchmarks the engine by running a fixed-time search on a suite of positions.
 *
 * @param filename The name of the EPD/FEN suite file.
 * @param time_ms The maximum time limit per position in milliseconds.
 */
void bench_avg_depth(const char *filename, int time_ms) {
    FILE *f = logger_open_file(filename, "r");
    if (!f) {
        printf("Error: Could not open suite file '%s'\n", filename);
        return;
    }

    FILE *out = logger_open_file("benchmark_avg_depth.txt", "w");
    if (!out) {
        printf("Note: Could not open tests/benchmark_avg_depth.txt for writing.\n");
    }

    char line[2048];
    int total_depth = 0;
    int pos_count = 0;

    printf("Running time suite '%s' at %d ms per move...\n", filename, time_ms);
    if (out) fprintf(out, "Running time suite '%s' at %d ms per move...\n", filename, time_ms);

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || line[0] == '\0') continue;
        
        Board pos;
        parse_fen(&pos, line); 
        
        // Use the real game search with a strict time limit
        search_position(&pos, time_ms);
        total_depth += get_search_depth();
        pos_count++;
    }
    fclose(f);

    if (pos_count == 0) {
        if (out) fclose(out);
        return;
    }

    printf("\n--- Time Benchmark Results ---\n");
    if (out) fprintf(out, "\n--- Time Benchmark Results ---\n");
    printf("Positions    : %d\n", pos_count);
    if (out) fprintf(out, "Positions    : %d\n", pos_count);
    printf("Time per move: %d ms\n", time_ms);
    if (out) fprintf(out, "Time per move: %d ms\n", time_ms);
    printf("Average Depth: %5.2f\n", (double)total_depth / pos_count);
    if (out) fprintf(out, "Average Depth: %5.2f\n", (double)total_depth / pos_count);
    printf("------------------------------\n");
    if (out) fprintf(out, "------------------------------\n");
    
    if (out) fclose(out);
}