#include "common.h"
#include "badlib.h"

#include <stdio.h>
#include <string.h>
#include "../../../utils/utils.h"

/* Trusted target function: prints the string argument. */
static int printf_wrapper(const char *s) {
    printf("printf prints: %s\n", s);
    return 0;
}

/*
 * copy_fn_arg copies the "argument" field from src to dst, up to len bytes,
 * truncating if necessary. This is used to copy one struct’s argument to another.
 */
static void copy_fn_arg(CallCtx *tgt, CallCtx *src, size_t len) {
    memcpy(tgt->argument, src->argument, len);
}

int main(void) {
    /* Initialize trusted context with printf and empty argument. */
    CallCtx trusted = { .argument = "", .fn_ptr = printf_wrapper };
    print_cap_info_short("&trusted", &trusted);
    print_cap_info_short("&(trusted->argument)", &(trusted.argument));
    print_cap_info_short("&(trusted->fn_ptr)", &(trusted.fn_ptr));
    printf("\n");

    /* Obtain untrusted pointer and user-specified length from the buggy library. */
    size_t len = 0;
    CallCtx *untrusted = badlib_get_ctx(&len);

    /* Trusted copy: only the argument is copied, with possible truncation. */
    copy_fn_arg(&trusted, untrusted, len);

    /* Execute the trusted function with the copied argument. */
    printf("\nRunning trusted function..\n\n");
    trusted.fn_ptr(trusted.argument);
    printf("\n");
    return 0;
}
