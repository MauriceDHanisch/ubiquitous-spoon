# jemalloc-free-trimmed: shadow_alloc

## Overview
This PoC shows that `free` accepts a "trimmed" capability (non-base, non-chunk pointer) in jemalloc and that CHERIvoke/quarantine bookkeeping then tracks the wrong region. This enables a shadow-allocation scenario where the same physical chunk is concurrently owned by two distinct capabilities: an attacker-held `shadow_cap` and a victim-held "sensible" allocation.

## How it works
- Attacker allocates a buffer of size `S` and derives `trimmed_cap = cheri_bounds_set(cap + S, 0)` — a zero-length (header-adjacent) capability not equal to the original chunk base.
- Calls `free(trimmed_cap)`. Due to missing strict base checks, this is accepted and the quarantine records/paints based on the wrong address but with the same size as the original chunk. This effectively now tracks the region just after the cap.
- Forces a CHERIvoke cycle via `malloc_revoke_quarantine_force_flush()`.
- Allocates `shadow_cap = malloc(S)`. Because of the mismatched bookkeeping, this refers to the same underlying physical memory as the victim’s next `malloc(S)`.
- Victim allocates `sensible_memory_chunk = malloc(S)` and writes to it.
- Attacker reads/writes the same memory through `shadow_cap`, demonstrating capability shadowing.

Key lines:
```c
void *cap = malloc(S);
void *trimmed_cap = cheri_bounds_set(cap + S, 0);
free(trimmed_cap);
malloc_revoke_quarantine_force_flush();
void *shadow_cap = malloc(S);
```

## Run and behavior
```bash
make run BIN=shadow_alloc
```
Expected behavior:
- Program prints/leaks the victim’s byte via `shadow_cap`, then overwrites it to 'X'.
- The victim-side read reflects the attacker's write, confirming shadowing.

## Inputs
- No user input.

## Notes
- This is the "allocate same memory twice" variant caused by accepting trimmed pointers into `free` and inconsistent quarantine tracking relative to revocation.
