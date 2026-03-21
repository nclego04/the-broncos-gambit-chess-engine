/**
 * @file main.c
 * @brief Main entry point for the Broncos Gambit chess engine.
 */
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "uci.h"

extern void init_leapers(); 
extern void init_sliders(); 

/**
 * @brief Main entry point for the chess engine.
 *
 * @return 0 on successful exit.
 */
int main(void) {
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    init_leapers();
    init_sliders(); 
    
    uci_loop();
    
    return 0;
}