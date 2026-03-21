/**
 * @file search.h
 * @brief Search algorithm declarations.
 */
#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

int search_position(Board *pos, int time_limit_ms);
uint64_t search_for_benchmark(Board *pos, int depth);
int get_search_depth(void);

#endif