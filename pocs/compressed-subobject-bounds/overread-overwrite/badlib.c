#include "badlib.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>

/* Bug: function pointer is (mis)initialized to execv via a wrapper. */
static int execv_wrapper(const char *path) {
    char *const argv[] = { (char *)path,  NULL };
    return execv(path, argv);
}

static size_t parse_len_auto(const char *s) {
    /* Accept decimal or hex (0x/0X). Use base 0 for auto-detection. */
    errno = 0;
    char *end = NULL;
    uintmax_t v = strtoumax(s, &end, 0);
    if (errno != 0 || end == s) return 0;              /* error or no digits */
    /* Skip trailing spaces/newlines; treat junk as error. */
    while (*end == ' ' || *end == '\t' || *end == '\n') end++;
    if (*end != '\0') return 0;
    if (v > (uintmax_t)SIZE_MAX) return SIZE_MAX;
    return (size_t)v;
}

/* Internal state that the library wrongly exposes by pointer. */
CallCtx g_ctx = {
    .argument = "",
    .fn_ptr   = execv_wrapper    /* BUG: dangerous capability assignment */
};


CallCtx *badlib_get_ctx(size_t *out_len) {
    /* BUG: reusing g_ctx because only string is copied instead of reinit a CallCtx struct! */ 

    /* Directly fill the argument field from user input. */
    printf("Enter a string to print out: ");
    fflush(stdout);
    if (!fgets(g_ctx.argument, sizeof(g_ctx.argument), stdin)) {
        g_ctx.argument[0] = '\0';
    } else {
        size_t n = strcspn(g_ctx.argument, "\n");
        g_ctx.argument[n] = '\0';
    }

    printf("Enter maximum length to copy (decimal or 0xHEX): ");
    fflush(stdout);
    char buf[128];
    size_t len = 0;
    if (fgets(buf, sizeof(buf), stdin)) {
        /* Trim leading spaces. */
        size_t i = 0; while (buf[i] == ' ' || buf[i] == '\t') i++;
        len = parse_len_auto(buf + i);
    } else {
        len = 0;
    }

    if (out_len) *out_len = len;
    return &g_ctx;  
}
