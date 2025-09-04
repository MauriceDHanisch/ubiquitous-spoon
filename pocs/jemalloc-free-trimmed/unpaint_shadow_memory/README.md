# jemalloc-free-trimmed: unpaint_shadow_memory (looped_threaded_10_step)

## Overview
This PoC demonstrates a race/logic issue where `free` on a "trimmed" capability causes CHERIvoke to track/paint the wrong ranges. With carefully timed revocation cycles and frees across two allocations, the quarantine is driven to unpaint a region to which a capability points, that should get invalidated, producing a use-after-free window with a still-tagged `shadow_cap`.

Target file: `looped_threaded_10_step.c`.

## High-level steps (mapped to code)
1. Allocate `buf1_cap = malloc(0x1000)` and set `trimmed = cheri_bounds_set(buf1_cap + 0x20, 0)`.
2. Allocate `buf2_cap = malloc(0x1000)`.
3. Free `buf2_cap`.
4. Start first revocation sweep in a thread (`revoker_thread` calls `malloc_revoke_quarantine_force_flush()`).
5. Shortly after revoker begins (nanosleep), `free(trimmed)` to paint `[0x0020, 0x1020)` relative to `buf1_cap`. This should happen exactly between the revoke and the flushing of the first `revoker_thread`.
6. Join first revoker: it unpaints `[0x1000, 0x2000)` from `buf2_cap`, thus unpainting the end of the painted region from step 5 `[0x0020, 0x1020)`. 
7. Allocate `shadow_cap = malloc(0x100)` hoping it lands at `0x1000-0x1100`.
8. Launch second revoke in a thread. This will start revoking the quarantine containing `0x0020-0x1020`.
9. Shortly after revoke starts, `free(shadow_cap)` painting `[0x1000, 0x1100)`. This should happen exactly between the revoke and the flushing of the second `revoker_thread`.
10. Join second revoker: it unpaints `[0x0020, 0x1020)` including `[0x1000, 0x1020)`. If we run the revoke algorithm again, it will leave `shadow_cap` tagged.

Finally, a forced quarantine flush occurs and the code checks if `shadow_cap` remains tagged. If so, it allocates `sensible_memory_chunk` (which will reallocate shadow_cap's chunk because it thinks it is freed and unquarantined) and demonstrates a read via `shadow_cap`.

## Why this works
- `free` accepts `trimmed` (non-chunk-base) and paints based on an incorrect region derived from it.
- Two overlapping revoke cycles are orchestrated so that an older painted region gets unpainted after a later paint that created the `shadow_cap` region.
- The end state yields a still-tagged capability to freed memory (`shadow_cap`), producing a UAF window.

## Key code excerpts
```c
void *buf1_cap = malloc(0x1000);
void *trimmed  = cheri_bounds_set((char *)buf1_cap + 0x20, 0);
void *buf2_cap = malloc(0x1000);
free(buf2_cap);
// start revoke 1
nanosleep(&ts, NULL);
free(trimmed); // paints 0x0020-0x1020
// join revoke 1 (unpaints 0x1000-0x2000)
void *shadow_cap = malloc(0x100);
// start revoke 2
nanosleep(&ts, NULL);
free(shadow_cap); // paints 0x1000-0x1100
// join revoke 2 (unpaints 0x0020-0x1020 including 0x1000-0x1020)
```

If `cheri_tag_get(shadow_cap)` is true after these steps, the PoC uses it to access the same physical memory as a fresh allocation.

## Run
```bash
make run BIN=looped_threaded_10_step
```
The process timing uses `struct timespec ts = { .tv_sec = 0, .tv_nsec = 9000 }` by default; adjust if needed for your platform.

## Behavior
- On success, prints:
  - "[child] shadow_cap still tagged; exploiting"
  - Capability info for `sensible_memory_chunk` and `shadow_cap`
  - A byte read via `shadow_cap`
- Parent reports the attempt number where success occurred.

## Inputs
- No user input.

## Notes
- This is the "unpaint a capability" variant. The choreography of two revokes is critical to ensure the painted region from `trimmed` is undone after painting `shadow_cap`’s region, leaving a valid tag.
- The `nanosleep` intervals are platform/allocator sensitive; small adjustments may improve success rates.
