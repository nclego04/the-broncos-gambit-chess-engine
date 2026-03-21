/**
 * @file logger.c
 * @brief Implements file logging functionalities for engine metrics.
 */
#include <stdio.h>
#include <string.h>
#include "logger.h"

static FILE *search_log = NULL;

/**
 * @brief Safely opens a log file by checking multiple fallback directory paths.
 *
 * @param filename The name of the log file to open.
 * @param mode The mode string (e.g., "w" or "a") to pass to fopen.
 * @return A valid FILE pointer, or NULL if all paths fail.
 */
FILE* logger_open_file(const char *filename, const char *mode) {
    char path[256];
    FILE *f = NULL;
    
    snprintf(path, sizeof(path), "tests/%s", filename);
    f = fopen(path, mode);
    if (!f) {
        snprintf(path, sizeof(path), "../tests/%s", filename);
        f = fopen(path, mode);
    }
    if (!f) {
        f = fopen(filename, mode);
    }
    return f;
}

/**
 * @brief Initializes a log file with a starting message.
 *
 * Opens the file in write ("w") mode to wipe previous contents, writes 
 * the provided message, and safely closes the file.
 *
 * @param message The initialization message to log.
 */
void logger_init(const char *message) {
    FILE *f = logger_open_file("live_nps.txt", "w");
    if (f) {
        fprintf(f, "%s\n", message);
        fclose(f);
    }
}

/**
 * @brief Opens the live search log file in append mode.
 *
 * Intended to be called before an iterative deepening search begins.
 */
void logger_start_search(void) {
    search_log = logger_open_file("live_nps.txt", "a");
    if (search_log) {
        fprintf(search_log, "\n--- Searching Move ---\n");
    }
}

/**
 * @brief Logs the performance metrics of a completed search depth.
 *
 * @param depth The completed search depth.
 * @param nodes The cumulative number of nodes evaluated.
 * @param ebf The calculated Effective Branching Factor for this depth.
 * @param time_ms The elapsed search time in milliseconds.
 * @param nps The Nodes Per Second calculated for this depth.
 */
void logger_log_depth(int depth, uint64_t nodes, double ebf, int time_ms, uint64_t nps) {
    if (search_log) {
        fprintf(search_log, "Depth %2d | Nodes: %8llu | EBF: %5.2f | Time: %5d ms | NPS: %8llu\n", 
                depth, (unsigned long long)nodes, ebf, time_ms, (unsigned long long)nps);
    }
}

/**
 * @brief Closes the active live search log file.
 */
void logger_end_search(void) {
    if (search_log) {
        fclose(search_log);
        search_log = NULL;
    }
}