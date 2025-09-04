# CHERI Vulnerability PoCs

This repository contains proof-of-concept programs that exercise several CHERI-related vulnerability classes across allocator behavior, subobject bounds, and capability initialization/cleanup.

## Prerequisites
- CHERI Clang/LLVM toolchain capable of targeting `aarch64-unknown-freebsd` with purecap ABI
- Runtime environment capable of running CHERI purecap binaries

The central Makefile uses the following common flags:
- `clang -g -O0 -Wall -fPIC --target=aarch64-unknown-freebsd -mabi=purecap`

## Makefile usage
- List available PoCs:
  ```sh
  make list
  ```
- Build all:
  ```sh
  make
  ```
- Build one target (example):
  ```sh
  make simple_overflow
  ```
- Run one target (example):
  ```sh
  make run BIN=simple_oveflow
  ```

Targets built by default:
- `simple_overflow`
- `overread_overwrite`
- `shadow_alloc`
- `looped_threaded_10_step`
- `bypass_header_check`
- `call_execv_via_uninit_mmr`

## PoC index and behavior

### compressed-subobject-bounds/simple-overflow
- Demonstrates that `-cheri-bounds=subobject-safe` can leave large struct fields with representable bounds overlapping the next field.
- Overflows a large `char` array to overwrite an adjacent function pointer capability.
- Inputs: none.
- Behavior: prints a safe message before overflow; after overflow, hijacked function prints an unsafe message.

### compressed-subobject-bounds/overread-overwrite
- Untrusted library returns an internal struct with a powerful function pointer (execv wrapper) and large string field.
- Trusted code copies only the string, but overreads and overwrites adjacent fields under compressed subobject bounds.
- Inputs (from library):
  - String to copy (e.g., `/bin/echo`).
  - Maximum length (decimal or 0xHEX). Example: `0x1020`.
- Behavior: trusted side calls function with copied string; with crafted inputs, control flows into `execv_wrapper`.

### jemalloc-free-trimmed/shadow_alloc
- `free` accepts a trimmed capability; quarantine/revocation tracks a wrong region (offset from true base) with the same size.
- After a CHERIvoke cycle, the same physical memory can be simultaneously owned by attacker and victim allocations.
- Inputs: none.
- Behavior: attacker leaks then overwrites a byte via `shadow_cap`; victim read reflects the change.

### jemalloc-free-trimmed/unpaint_shadow_memory (looped_threaded_10_step)
- Carefully timed sequence with two revocation cycles and frees causes the quarantine to unpaint a capability that should remain invalid.
- Leaves a still-tagged `shadow_cap` to freed memory, enabling a UAF window.
- Inputs: none.
- Behavior: on success, shows tagged `shadow_cap`, capability info, and cross-allocation read.

### dlmalloc-header-bypass
- Crafts a chunk header so that `free(trimmed_cap)` is accepted by dlmalloc.
- The forged header includes a capability with `CHERI_PERM_SW_VMEM`; validity is not checked by dlmalloc (address/perms checked).
- Inputs: none.
- Behavior: prints capability info; confirms that free of crafted trimmed pointer does not error.

### uninit-capabilities/call_execv_via_uninit_mmr
- Exploits uninitialized memory to locate unsealed executable capabilities (libc or `_start`).
- `_start` is found first, restarts to perturb cleanup, making unsealed libc capability available.
- Once libc capability is found, adjusts address to known `execv` offset and invokes `/bin/sh`.
- Inputs: press Enter when prompted for `_start` or libc findings.
- Behavior: restarts once; on libc match, spawns a shell via `execv`.

## Notes
- The detailed mechanics and security considerations are documented in each PoC's README.
- Some PoCs depend on allocator/version/timing; you may need to tweak timing or run multiple attempts.
