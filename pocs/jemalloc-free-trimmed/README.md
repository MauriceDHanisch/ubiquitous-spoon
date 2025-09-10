# jemalloc-free-trimmed: High-level Overview

## What this shows

These PoCs demonstrate how accepting a "trimmed" capability (a non-chunk-base pointer) in `free` can desynchronize jemalloc’s quarantine bookkeeping from CHERI temporal revocation, leading to:

- Shadow allocation: the same physical chunk ends up concurrently owned by two capabilities (attacker-held and victim-held).
- Unpainted capability: carefully interleaving `free` and revoke cycles can leave a still-tagged capability to memory that should have been invalidated, enabling a use-after-free window.

Both behaviors stem from painting/unpainting the wrong address range in the quarantine when `free` receives a capability that is not the true allocation base.

---

## Background: CHERI temporal revocation, quarantine, and the shadow map

At a high level, CHERI temporal safety (revocation) relies on a two-phase process when memory is freed:

1) Quarantine (paint): When an allocation is freed, a revocation-aware allocator records the freed range in a quarantine structure and paints its address range in a shadow map. Painted means: “capabilities into this range must be invalidated before it is considered safe for reuse.”

2) Revocation sweeps: A revoker traverses process memory/state to clear capability tags that point into painted ranges. Only after a sweep establishes there are no remaining live capabilities into a quarantined range does the allocator unpaint the range in the shadow map and release it for reuse.

Key components and invariants:

- Quarantine: Tracks freed-but-not-yet-safe ranges. Entries correspond to allocation-sized regions and their lifetimes between paint and unpaint.
- Shadow map: A 16-byte granularity bitmap indexed by address that marks whether a byte/word lies in a quarantined range. The revoker consults it to decide which capability tags to clear, and the allocator updates it on `free` (paint) and after successful revocation (unpaint).
- Ordering: Correctness requires that paint happens for the exact allocation range, stays in place while the revoker clears tags, and only then is unpainted. Any deviation (wrong address, premature unpaint, or racy interleaving) can leave stale, still-tagged capabilities.

Failure mode with trimmed caps:

- If `free` accepts a non-base (trimmed) capability, quarantine painting may be anchored at the wrong address (e.g., adjacent to the real chunk base) while size is derived from the original allocation. This mismatch corrupts quarantine/shadow-map invariants. Later sweeps can unpaint a region that does not correspond 1:1 to the original allocation, enabling shadowing or a residual tagged capability.

---

## PoC variants in this folder

### 1) shadow_alloc

- Idea: Accepting a trimmed capability in `free` causes quarantine to track the wrong region. A subsequent `malloc(S)` returns a capability to the same underlying memory as a victim’s next allocation of size `S`, creating a “shadow” allocation.
- Steps (simplified):
  - `cap = malloc(S)`
  - `trimmed = cheri_bounds_set(cap + S, 0)` (zero-length, header-adjacent)
  - `free(trimmed)` → quarantine records based on a mismatched address
  - Force a revocation flush
  - `shadow_cap = malloc(S)` now aliases the memory the victim will later receive
  - Victim allocates and writes; attacker reads/writes via `shadow_cap`
- Run: `make run BIN=shadow_alloc`
- Expected: Shows the victim’s byte via `shadow_cap`, then overwrites it; victim read reflects attacker’s write.

### 2) unpaint_shadow_memory (looped_threaded_10_step)

- Idea: Two revoke cycles are interleaved with frees so that an older painted region gets unpainted after a later paint, leaving a still-tagged capability into freed memory.
- Steps (simplified):
  - Allocate `buf1_cap`, create `trimmed = cheri_bounds_set(buf1_cap + 0x20, 0)`
  - Allocate `buf2_cap`, then free it
  - Start revoke 1; shortly after it begins, `free(trimmed)` paints a misaligned region
  - Join revoke 1: it unpaints `[buf2_cap..)` which overlaps the tail of the painted region from `trimmed`
  - Allocate `shadow_cap = malloc(0x100)` at the overlap
  - Start revoke 2; shortly after it begins, `free(shadow_cap)` paints `[shadow_cap..)`
  - Join revoke 2: it unpaints the older region, including part of `[shadow_cap..)` → leaves `shadow_cap` tagged
  - Use `shadow_cap` to read from memory that should have been invalidated
- Run: `make run BIN=looped_threaded_10_step`
- Notes: Timing (`nanosleep`) is platform/allocator sensitive; slight adjustments can improve success.

---

## Why this works

- Accepting non-base capabilities in `free` violates the assumption that quarantine/paint reflects the exact allocation range. A wrong base with the same size corrupts shadow-map tracking.
- Interleaved sweeps (paint→unpaint) rely on strict ordering. If painting/unpainting applies to misaligned ranges, a later unpaint can remove protection from an area recently repainted, or a fresh allocation can land inside a region that the system believes was safely revoked.

---

