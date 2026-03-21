/**
 * @file benchmark.h
 * @brief Definitions for the benchmarking suite.
 */
#ifndef BENCHMARK_H
#define BENCHMARK_H

void bench_nps_ebf(const char *filename, int depth);
void bench_avg_depth(const char *filename, int time_ms);

#endif