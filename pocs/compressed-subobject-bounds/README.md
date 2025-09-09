# Compressed Subobject Bounds

## Introduction

CHERI capabilities provide spatial memory safety by enforcing bounds on pointers.  
On Morello (128-bit capabilities), bounds are **compressed** [1], which introduces a  
precision trade-off. While this design allows efficient representation of bounds,  
it also leads to exploitable situations in the context of **subobject bounds**.  
This document describes the mechanism, examples of imprecision, why fixing it is  
difficult, and current mitigations.

---

## Bounds Compression in Morello

Morello capabilities use a compressed bounds format. The encoding allocates at  
most **15 bits** for representing bounds precisely. For allocations smaller than  
a threshold, bounds are exact. Beyond that, they are imprecise and rounded, in a  
manner similar to floating-point exponent/mantissa encoding.

On the heap and stack, compilers and allocators typically insert sufficient  
spacing between adjacent objects so that bounds rounding does not cause a  
capability for one allocation to overlap the next allocation. This ensures  
that imprecision does not permit cross-object accesses between independently  
allocated heap or stack objects.  

The LLVM implementation of the alignment requirement is shown below [2]:
```cpp
uint64_t getMorelloRequiredAlignment(uint64_t length) {
  // FIXME: In the specification l is a 65-bit value to permit the encoding
  // of 2^64. We don't have a case where length can be 2^64 at the moment.
  // Using formula E = 50 - CountLeadingZeros(length[64:15])
  uint64_t E = 0;
  if (length >> 15)
    // Add 1 to account for our length being 64-bit not 65-bit
    E = 50 - (countLeadingZeros(length) + 1);
  // Ie = 0 if E == 0 and Length[14] == 0; 1 otherwise
  if (E == 0 && ((length & 0x4000) == 0))
    // InternalExponent Ie = 0 no additional alignment requirements
    return 1;
  // Rounding up the length may carry into the next bit and require a one
  // higher exponent, so iterate once. This is guaranteed to converge.
  length = alignTo(length, uint64_t(1) << (E + 3));
  if (length >> 15)
      E = 50 - (countLeadingZeros(length) + 1);
  // InternalExponent Ie = 1 Alignment Requirement is 2 ^ E+3
  return uint64_t(1) << (E + 3);
}
```

### Precision Thresholds

- **< 16 KiB**: Bounds are exact.  
- **> 16 KiB**: Bounds are rounded to alignments determined by exponent `E`.  
- Alignment grows with size, for example:
  - ~32 KiB → alignment of 16 bytes  
  - ~1 MB   → alignment of 512 bytes  
  - Larger allocations → increasingly coarse rounding

#### Measuring worst-case inaccuracy locally

- You can measure the required alignment and worst-case bounds inaccuracy for a  
  given allocation size using the helper binary:  
  - `make run BIN=bounds_misalignment`  
  - or `make run-bounds_misalignment`  
  When prompted, enter the size in bytes (decimal or `0xHEX`). The tool will  
  print `required_alignment` and `worst_case_inaccuracy_bytes` for that size.

This imprecision means bounds may extend beyond the intended allocation, covering  
adjacent memory.

---

## Subobject Bounds

### Why structs are different

For structs, the default is that each field inherits the bounds of the entire  
enclosing struct. This preserves long-standing C/C++ layout and access patterns  
that many data structures rely on, such as intrusive linked lists and other  
container patterns where code computes addresses of neighboring fields or uses  
`offsetof`-based arithmetic across members. With whole-struct bounds, these  
patterns remain compatible across compilation units and libraries [4].  

A consequence is that CHERI does **not** prevent accesses between adjacent  
fields inside a struct by default. An out-of-bounds access within the struct  
can therefore reach other fields.

### Enabling per-field bounds: `cheri-bounds=subobject-safe`

Enabling this compiler flag narrows bounds to individual fields [4]:

- Each struct member receives its own capability bounds.  
- However, the compiler does not insert padding between fields to ensure those  
  bounds remain precise under compressed-bounds rounding.  
- Large members therefore remain subject to imprecision, and their bounds may  
  still overlap adjacent fields.

### Why not add padding inside structs?

Padding large members to preserve precision would change `sizeof(struct)` and  
`offsetof(member)`, breaking the Application Binary Interface (ABI). That means:  

- Code compiled with and without the flag would disagree on struct size and  
  member offsets.  
- Cross-module and system interfaces (e.g., kernel↔user, library boundaries,  
  device-driver ABIs) would misinterpret the same memory.  
- Stable on-disk formats or wire protocols that embed structs would no longer  
  match.  

Because many projects must interoperate across mixed toolchains and compiled  
artifacts, silently changing struct layout is not acceptable. As a result,  
subobject bounds are applied without layout changes; they are effective when  
precision allows, but cannot be guaranteed for all members.

---

## Security Implications

### Exploit Mechanism

Imprecision can be exploited via:

- **Overwrite attacks**: Writing past the end of one field into another.  
- **Overread attacks**: Reading from one field into a neighboring one, leaking data.  

This is particularly dangerous if developers assume that enabling  
`subobject-safe` guarantees exact field isolation [3, 4].

### Practical Impact

- Large kernel structures (e.g., per-device driver state) are frequently affected.  
- Variable-sized structs make static analysis harder, so exact bounds matter more.  
- Both kernel- and user-space software can become vulnerable if assumptions about  
  subobject safety are incorrect.

---

## Why This Is Hard to Fix

Fixing subobject bounds precision while maintaining compatibility is difficult:

- **ABI compatibility is paramount**: As explained above, adding intra-struct  
  padding changes sizes and offsets, breaking interoperability across binaries  
  and interfaces. This rules out the most direct fix.  
- **Compiler trade-offs**: A stricter mode could add padding or reject affected  
  structs, but would fragment the ecosystem and break common layouts (e.g.,  
  small metadata followed by a large in-struct buffer).  
- **Spurious faults risk**: Forcing exact bounds (e.g., via `setboundsexact`)  
  can fault benign programs unless thoroughly validated. Achieving correctness  
  at scale requires extensive testing.  
- **Prevalent real-world patterns**: Many vulnerable layouts are exactly those  
  that resist automatic fixes (e.g., “metadata + large tail buffer” within a  
  single struct). In these cases, surfacing diagnostics is often the safest  
  approach.

---

## Mitigations

1. **Allocation-domain padding (heap/stack)**  
   - Compilers/allocators insert spacing so rounded bounds for one allocation do  
     not overlap the next. This reduces cross-object risks outside of structs.  

2. **Struct design guidance**  
   - Separate large buffers from metadata (allocate buffers independently).  
   - Use flexible array members or place large arrays last to reduce overlap risk.  
   - Where you control the ABI, add explicit padding and freeze the new layout.  

3. **Diagnostics**  
   - Compiler warnings for imprecise member bounds.  
   - Static analysis (e.g., DWARF/LLVM passes) to flag risky layouts in large  
     codebases such as CheriBSD [3, 4].  

4. **Policy and defaults**  
   - Explore enabling limited subobject bounds by default where safe.  
   - Document that subobject bounds are opportunistic under compression and  
     cannot guarantee exact per-field isolation for large members.

---

## References

1. Arm Ltd., *Arm Architecture Reference Manual for A-profile Architecture (Arm ARM), DDI 0606* — `https://developer.arm.com/documentation/ddi0606/latest/`  
2. LLVM Project, *Morello bounds alignment implementation* — `https://github.com/llvm/llvm-project/blob/main/llvm/lib/Support/Morello.cpp`  
3. Alexander Richardson, *Complete spatial safety for C and C++ using CHERI capabilities*, University of Cambridge, Technical Report UCAM-CL-TR-949, 2020 — `https://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-949.pdf`  
4. CTSRD-CHERI, *CHERI C/C++ Programming Guide* — `https://github.com/CTSRD-CHERI/cheri-c-programming`  

---
