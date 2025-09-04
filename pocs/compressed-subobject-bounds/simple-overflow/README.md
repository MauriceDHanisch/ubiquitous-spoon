# Compressed Subobject Bounds: simple-overflow

## Overview
This PoC demonstrates how `-cheri-bounds=subobject-safe` can still permit field-to-field overlap for large fields because the compiler does not insert padding between struct fields. A large `char` array followed by a function pointer leads to bounds on the array that span into the adjacent field. An overflow of the array can therefore overwrite the function pointer capability.

## Vulnerable layout
The core structure is:

```c
struct vuln{
    char buffer[0x10001];
    void (*func_ptr)(void); // capability, 16-byte aligned requirement
};
```

With subobject-safe bounds, `buffer` gets a capability bounded to the field, but due to capability compression and lack of padding, its representable bounds cover into the next field. The PoC crafts an overflow payload that aligns to the function pointer and overwrites it with a chosen capability.

## How the exploit works
- Initialize `v.func_ptr = safe_fn` and call it once.
- Compute an aligned offset corresponding to where the function pointer will sit after the large buffer, respecting capability alignment.
- Create a payload of 'A's up to that aligned offset, then append the bytes of a hijack function pointer (`unsafe_fn`).
- `memcpy` the payload into `v.buffer`, overflowing into and replacing `v.func_ptr`.
- Call `v.func_ptr()` again, which now invokes `unsafe_fn`.

Key lines:
```c
memcpy(v.buffer, payload, sizeof(payload));
```

## Run and behavior
```bash
make run BIN=simple_overflow
```
Expected output shows the safe function before overflow, then after overflow the unsafe function runs:
- Before overflow: prints "Safe function executed."
- After overflow: prints "!!! Unsafe function executed !!!"

## Inputs
- This PoC takes no user input. All payload construction is internal.

## Notes
- The demonstration relies on representable bounds compression and lack of automatic padding between adjacent fields under `-cheri-bounds=subobject-safe`.
