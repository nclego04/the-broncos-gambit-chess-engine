#include <stdio.h>
#include <string.h>
#include "types.h"
#include "uci.h"
#include "benchmark.h"

extern void init_leapers(); 
extern void init_sliders(); 

int main(int argc, char *argv[]) {
    // Disable output buffering BEFORE any printf calls happen!
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    // 1. Boot up the Magic Bitboards and Attack Tables
    init_leapers();
    init_sliders(); 
    
    // Check if the user passed the "bench" argument
    if (argc > 1 && strcmp(argv[1], "bench") == 0) {
        run_benchmark();
        return 0;
    }

    // 2. Hand control over to the communication protocol
    uci_loop();
    
    return 0;
}