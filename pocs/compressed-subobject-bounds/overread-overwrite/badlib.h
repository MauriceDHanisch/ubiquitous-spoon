#ifndef BADLIB_H
#define BADLIB_H
#include <stddef.h>
#include "common.h"

/*
 * Buggy API: exposes a pointer to the library’s internal CallCtx and a length
 * gathered from the user. The length may be entered in decimal or hexadecimal
 * (prefix 0x/0X). The caller (trusted main) passes both to copy_fn_arg.
 */
CallCtx *badlib_get_ctx(size_t *out_len);

#endif
