#include "utils.h"
#include <stdio.h>
#include <string.h>  
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <cheriintrin.h>

void print_cap_info_short(const char *label, void *cap) {
    unsigned perms = cheri_perms_get(cap);

    // Short permission string
    char perm_str[8] = "------"; // rwxRWE form

    // Lowercase for basic perms
    if (perms & CHERI_PERM_LOAD)        perm_str[0] = 'r';
    if (perms & CHERI_PERM_STORE)       perm_str[1] = 'w';
    if (perms & CHERI_PERM_EXECUTE)     perm_str[2] = 'x';

    // Uppercase for privileged perms
    if (perms & CHERI_PERM_GLOBAL)      perm_str[3] = 'R'; // representative of broader rights
    if (perms & CHERI_PERM_STORE_CAP)   perm_str[4] = 'W'; // store capability
    if (perms & CHERI_PERM_EXECUTE)     perm_str[5] = 'E'; // execute (redundant but uppercase slot)

    printf("%s %p [%s, %#lx-%#lx] (%s) (%s)\n",
           label, cap, perm_str,
           (unsigned long) cheri_base_get(cap),
           (unsigned long) cheri_base_get(cap) + (unsigned long) cheri_length_get(cap),
           cheri_tag_get(cap) ? "valid" : "invalid",
           cheri_is_sealed(cap) ? "sealed" : "not sealed");
}

static void print_binary_64(uint64_t value) {
    for (int i = 63; i >= 0; --i) {
        putchar((value >> i) & 1 ? '1' : '0');
        if (i % 8 == 0 && i != 0) putchar(' ');
    }
    putchar('\n');
}

void print_cap_raw_bits(const char *label, void* __capability cap) {
    uint64_t bits[2];
    memcpy(bits, &cap, sizeof(bits));  // safely copy full 128 bits

    printf("%s\n", label);
    printf("  Raw 128-bit capability (compression bits):\n");
    printf("    Upper: ");
    print_binary_64(bits[1]);
    printf("           (0x%016lx)\n", bits[1]);

    printf("    Lower: ");
    print_binary_64(bits[0]);
    printf("           (0x%016lx)\n", bits[0]);

    printf("  Address: %#lx\n\n", (unsigned long)cheri_address_get(cap));
    puts("");
}

void print_shadow_idx(const char *label, void *__capability cap) {
    uintptr_t addr = (uintptr_t)cap;
    size_t granule = addr >> 4;
    size_t byte_idx = granule >> 3;
    size_t bit_idx = granule & 0x7;
    printf("%s\n", label);
    printf("  shadow byte offset = %zu\n", byte_idx);
    printf("  shadow bit index   = %zu\n", bit_idx);
    puts("");
}