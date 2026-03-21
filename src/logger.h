/**
 * @file logger.h
 * @brief Logging utilities for the engine's performance metrics.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdint.h>

FILE* logger_open_file(const char *filename, const char *mode);
void logger_init(const char *message);
void logger_start_search(void);
void logger_log_depth(int depth, uint64_t nodes, double ebf, int time_ms, uint64_t nps);
void logger_end_search(void);

#endif