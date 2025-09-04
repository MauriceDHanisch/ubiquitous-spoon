#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <cheriintrin.h>


/* Print the low-level details of a CHERI capability. */
void print_cap_info_short(const char *label,
                    void *      __capability cap);

void print_cap_raw_bits(const char *label, void* __capability cap);

void print_shadow_idx(const char *label, void *__capability cap);
    
#endif /* UTILS_H */
