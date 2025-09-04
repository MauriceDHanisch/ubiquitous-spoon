# Compressed Subobject Bounds: Overread–Overwrite

## Overview

This proof-of-concept (PoC) demonstrates how `-cheri-bounds=subobject-safe` can be abused in the presence of a buggy, untrusted helper library. The trusted application delegates input handling to the untrusted library, expecting to only copy the returned string regardless of the untrusted function pointer in the struct used to pass information. It assumes safety because it uses subobject-safe boundsa and copies *only* the `argument` field of the struct provided by the library. However, due to capability compression and lack of padding between large fields under subobject-safe, the representable bounds of the string field extend into the adjacent function pointer field. This allows an attacker to redirect control flow.

## Components

- **`common.h`**: Defines `CallCtx`, containing a large `argument` buffer and a function pointer `fn_ptr`.
- **`badlib.c`**: Buggy library that exposes internal state (`g_ctx`). It misinitializes `fn_ptr` to `execv_wrapper` and reuses the same `CallCtx` instance instead of returning a fresh object.
- **`main.c`**: Trusted application. It initializes its own `CallCtx` with a safe function pointer (`printf_wrapper`), but copies only the string field from the untrusted context before invoking the trusted function pointer.

## Data Types

```c
typedef int (*fn_t)(const char *);
typedef struct {
    char argument[0x10001];  /* oversized buffer */
    fn_t fn_ptr;             /* invoked with argument */
} CallCtx;
```

The oversized `argument` field causes representable bounds overlap: subobject-safe generates bounds that may extend across the struct boundary into the function pointer.

## Vulnerability and Flow

1. **Trusted side**: expects only to copy the string and invoke its safe function pointer.
2. **Untrusted side**: returns a pointer to its internal `CallCtx`, which contains `fn_ptr = execv_wrapper`.
3. **Copy operation**: `memcpy(tgt->argument, src->argument, len)` uses a user-supplied `len`.
4. **Exploit**: when `len` matches the size of the representable bounds (e.g. `0x10020`), the copy operation overwrites the trusted function pointer, redirecting execution to `execv_wrapper`.

## Inputs and Behavior

Untrusted library prompts:

- *Enter a string to print out*: attacker-controlled text.
- *Enter maximum length to copy*: decimal or hex (0x-prefixed).

### Normal Usage

- Input: `hello`
- Length: `5`
- Result: trusted side executes `printf_wrapper("hello")`.

### Exploit

- Input: `/bin/sh`
- Length: `0x10020` (representable bound length for a buffer of `0x10001`).
- Result: trusted `fn_ptr` is overwritten with `execv_wrapper`, causing `execv("/bin/sh", …)`.

## Execution

```bash
make run BIN=overread_overwrite
```

You will be prompted for the string and length. With crafted values, the trusted application’s call transitions into the untrusted `execv_wrapper`.

## Notes

- Root cause: representable bounds overlap between adjacent struct fields without padding under `-cheri-bounds=subobject-safe`.
- Buggy library API: returns a pointer to internal state and seeds `fn_ptr` with a dangerous capability (`execv_wrapper`).
- Impact: enables capability misuse by combining the unsafe API with subobject-safe’s representable bounds.
