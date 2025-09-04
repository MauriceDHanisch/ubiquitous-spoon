#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

typedef int (*fn_t)(const char *);

/* Short container for a function and its string argument. */
typedef struct {
    char argument[0x10001];  /* large buffer for the string argument */
    fn_t fn_ptr;          /* function pointer invoked with the argument */
} CallCtx;

#endif
