#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static inline uint64_t align_to(uint64_t value, uint64_t alignment) {
    if (alignment == 0) return value;
    uint64_t remainder = value % alignment;
    if (remainder == 0) return value;
    return value + (alignment - remainder);
}

// Port of the LLVM Morello alignment requirement logic shown in README
static uint64_t get_morello_required_alignment(uint64_t length) {
    uint64_t E = 0;
    if (length >> 15) {
        // Using formula E = 50 - CountLeadingZeros(length[64:15])
        // CountLeadingZeros on 64-bit value -> use builtin; add 1 like LLVM note
        E = 50 - ((uint64_t)__builtin_clzll(length) + 1);
    }
    if (E == 0 && ((length & 0x4000ULL) == 0)) {
        return 1; // no additional alignment requirement
    }
    // Round length up to alignment 2^(E+3) and recompute once
    uint64_t alignment = 1ULL << (E + 3);
    length = align_to(length, alignment);
    if (length >> 15) {
        E = 50 - ((uint64_t)__builtin_clzll(length) + 1);
    }
    return 1ULL << (E + 3);
}

static int parse_length_arg(const char *s, uint64_t *out) {
    if (s == NULL || *s == '\0') return -1;
    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(s, &end, 0); // base 0 -> auto-detect 0x for hex
    if (errno != 0) return -1;
    // allow whitespace around; ensure something was consumed
    while (*end == ' ' || *end == '\t' || *end == '\n') end++;
    if (*end != '\0') return -1;
    *out = (uint64_t)v;
    return 0;
}

int main(int argc, char **argv) {
    uint64_t length = 0;

    if (argc >= 2) {
        if (parse_length_arg(argv[1], &length) != 0) {
            fprintf(stderr, "Invalid length: '%s'\n", argv[1]);
            return 2;
        }
    } else {
        char buf[256];
        fputs("Enter length (decimal or 0xHEX): ", stdout);
        if (!fgets(buf, sizeof(buf), stdin)) {
            fprintf(stderr, "Failed to read input\n");
            return 2;
        }
        // strip trailing newline
        size_t n = strlen(buf);
        if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = '\0';
        if (parse_length_arg(buf, &length) != 0) {
            fprintf(stderr, "Invalid length: '%s'\n", buf);
            return 2;
        }
    }

    uint64_t alignment = get_morello_required_alignment(length);
    uint64_t worst_case_inaccuracy = (alignment == 0) ? 0 : (alignment - 1);

    printf("length: %llu (0x%llx)\n", (unsigned long long)length, (unsigned long long)length);
    printf("required_alignment: %llu\n", (unsigned long long)alignment);
    printf("worst_case_inaccuracy_bytes: %llu\n", (unsigned long long)worst_case_inaccuracy);

    return 0;
}


