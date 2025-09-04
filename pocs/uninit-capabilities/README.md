# uninit-capabilities: call_execv_via_uninit_mmr

## Overview
This PoC abuses the use of uninitialized memory that may contain leftover capabilities on the stack or in memory. Normally, critical capabilities observed by user code would be sealed entry capabilities. By restarting `_start` it manipulates cleanup and the environment leaves unsealed capabilities to libc in an accessible location. The PoC then finds a capability spanning all of libc and turns it into a direct call to `execv`, spawning a shell.

Target file: `call_execv_via_uninit_mmr.c`.

## How it works
- The program scans a set of uninitialized pointers (`str[MAX_PTRS]`) as raw capabilities.
- For each candidate `cap`, it checks:
  - `cheri_tag_get(cap)` is true.
  - `CHERI_PERM_EXECUTE` is present.
  - Capability is not sealed.
- It matches either:
  - A capability whose base/length equals the libc mapping. If found, it uses `cheri_address_set` to set the offset to the `execv` symbol address and invokes it with `/bin/sh`. (the address of `execv` is predetermined).
  - Or a capability matching the `_start` mapping. If found first, it asks the user to press enter, then directly calls `_start()` to restart in a state that leaves non-sealed libc capabilities reachable on the next pass.

Key lines:
```c
if (execv_addr > base && execv_addr < end) {
    // ...
    char *argv[] = { "sh", NULL };
    execv_t execv_ptr = (execv_t)cheri_address_set(cap, execv_addr);
    execv_ptr("/bin/sh", argv);
}
```

## Run and behavior
```bash
make run BIN=call_execv_via_uninit_mmr
```
Expected behavior:
- Program iterates capabilities, printing prompts:
  - On finding `_start`: prints index and waits for x or any other key. If x is pressed it executes that `_start` capability.
  - On finding libc: prints index and waits for Enter, then resolves `execv` and spawns `/bin/sh`.

## Inputs
- The program pauses for key entries while scanning for `_start` capabilities:
  - When detecting `_start` capability choose to execute or continue searching. The 3rd `_start` capability needs to get excuted to correctly restart the program and proceed with the exploit.

## Notes
- The addresses `execv_addr` and `_start_base` are hardcoded for the target environment; adjust as needed.
- The exploit relies on finding unsealed, executable capabilities to the full libc region. The `_start` restart step is used to perturb the environment so that such an unsealed capability becomes available.
