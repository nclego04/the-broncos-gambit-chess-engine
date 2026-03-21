/**
 * @file board.h
 * @brief Board state management declarations.
 */
#ifndef BOARD_H
#define BOARD_H

#include "types.h"

void parse_fen(Board *pos, const char *fen);
void make_move(Board *pos, int move);

#endif