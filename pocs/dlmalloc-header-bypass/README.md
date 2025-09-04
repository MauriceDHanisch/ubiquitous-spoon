# dlmalloc-header-bypass

## Overview
This PoC shows that in dlmalloc, calling `free` on a "trimmed" capability can be made to succeed by crafting the chunk header so that allocator checks pass. This mirrors the initial step of the jemalloc exploit: `free` accepts a non-base pointer and the allocator proceeds using metadata influenced by the attacker.

## How it works
- Allocate a chunk and derive a trimmed capability `trimmed_cap = cheri_bounds_set(cap + 64, 64)`.
- Craft a synthetic capability in the chunk header with `CHERI_PERM_SW_VMEM` set and place it at the expected header offset. This capability will be invalid, but dlmalloc only checks the address and the permissions. Not the validity of the capability.
- Call `free(trimmed_cap)`. With the forged header, dlmalloc accepts the trimmed pointer and processes the free.

Key lines:
```c
void *cap = malloc(64 + 64);
void *trimmed_cap = cheri_bounds_set(cap + 64, 64);
// craft SW_VMEM capability and memcpy it into header at cap+16
free(trimmed_cap);
```

## Run and behavior
```bash
make run BIN=bypass_header_check
```
Expected behavior:
- Prints capability information for the base and trimmed capabilities.
- If the header is accepted, program prints confirmation (e.g., "Freeing the crafted chunk didn't throw an error!").

## Inputs
- No user input.

## Notes
- This illustrates that dlmalloc can be coerced to free a non-chunk-base capability when metadata is attacker-controlled. It's analogous to the first phase of the jemalloc PoCs but allocator-specific in header layout and checks.
