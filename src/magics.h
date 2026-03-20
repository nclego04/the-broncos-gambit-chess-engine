#ifndef MAGICS_H
#define MAGICS_H

#include "types.h"

// The number of bits in the relevant blocker mask for each square
extern const int rook_relevant_bits[64];
extern const int bishop_relevant_bits[64];

// Notice: The "const" is gone! The engine will fill these at runtime.
extern U64 rook_magic_numbers[64];
extern U64 bishop_magic_numbers[64];

#endif