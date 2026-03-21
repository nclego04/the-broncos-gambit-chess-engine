/**
 * @file magics.h
 * @brief Declarations for magic bitboard generation and tables.
 */
#ifndef MAGICS_H
#define MAGICS_H

#include "types.h"

extern const int rook_relevant_bits[64];
extern const int bishop_relevant_bits[64];

extern U64 rook_magic_numbers[64];
extern U64 bishop_magic_numbers[64];

#endif