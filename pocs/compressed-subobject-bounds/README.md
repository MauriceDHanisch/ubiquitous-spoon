# Compressed Subobject Bounds

## Introduction

CHERI capabilities provide spatial memory safety by enforcing bounds on pointers.  
On Morello (128-bit capabilities), bounds are **compressed**, which introduces a  
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

The LLVM implementation of the alignment requirement is shown below:
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

- **≤ 32 KB (2^15)**: Bounds are exact.  
- **> 32 KB**: Bounds are rounded to alignments determined by exponent `E`.  
- Alignment grows with size, for example:
  - ~1 MB → alignment of 64 bytes  
  - ~1 GB → alignment of 512 KB  
  - Larger allocations → increasingly coarse rounding

This imprecision means bounds may extend beyond the intended allocation, covering  
adjacent memory.

---

## Subobject Bounds

### Default Behavior

By default, struct fields inherit the bounds of the enclosing struct. This means  
CHERI’s protections do **not** prevent accesses between adjacent fields inside a  
struct. Out-of-bounds reads or writes within the struct can therefore corrupt  
other fields.

### `cheri-bounds=subobject-safe`

This compiler flag narrows bounds to the specific field:

- Each struct member has its own capability bounds.  
- However, **no padding is inserted** between fields to guarantee precision.  
- Large fields remain subject to compression imprecision, so bounds can still  
  overlap with adjacent fields.

---

## Security Implications

### Exploit Mechanism

Imprecision can be exploited via:

- **Overwrite attacks**: Writing past the end of one field into another.  
- **Overread attacks**: Reading from one field into a neighboring one, leaking data.  

This is particularly dangerous if developers assume that enabling  
`subobject-safe` guarantees exact field isolation.

### Practical Impact

- Large kernel structures (e.g., per-device driver state) are frequently affected.  
- Variable-sized structs make static analysis harder, so exact bounds matter more.  
- Both kernel- and user-space software can become vulnerable if assumptions about  
  subobject safety are incorrect.

---

## Why This Is Hard to Fix

Fixing subobject bounds precision is not straightforward:

- **ABI Compatibility**: Inserting padding or changing alignment in structs would  
  break binary interfaces between code compiled with and without subobject bounds.  
  To avoid ABI divergence, CHERI currently treats subobject bounds as  
  *opportunistic* — they are enforced when possible, but not guaranteed.

- **Compiler Trade-offs**: A stricter compiler could add padding or reject  
  imprecise structures, but this would cause widespread compatibility issues,  
  especially for large data structures (e.g., metadata + large buffers).

- **Spurious Faults**: Using exact bounds instructions (e.g., `setboundsexact`)  
  could cause false faults in normal code unless carefully tested. A complete  
  solution would require extensive test suites.

- **Real-World Structures**: Many problematic cases are precisely the ones least  
  suited to automated padding (e.g., “128 bytes metadata + 512 KB buffer”).  
  In such cases, compiler errors or warnings are preferable to silent imprecision.

---

## Mitigations

1. **Heap/Stack Padding**  
   - For heap and stack allocations, CHERI already adds padding to preserve bounds  
     precision.  

2. **Compiler Warnings**  
   - Warn when struct fields have imprecise bounds.  
   - Encourage developers to restructure affected data structures.  

3. **Static Analysis**  
   - Tools (DWARF analysis, LLVM passes) can detect imprecise struct members.  
   - Helps identify risky cases in large codebases such as CheriBSD.  

4. **Policy Changes**  
   - There is ongoing discussion about enabling limited subobject bounds by default.  
   - This has measurable security benefits, as demonstrated in kernel vulnerability  
     studies, though it cannot eliminate all risks.

---

## References

- Watson et al., *CHERI: A Hybrid Capability-System Architecture for Scalable Software Compartmentalization*, IEEE S&P 2015.  
- Richardson, *Exploring C/C++ Memory Safety with CHERI Subobject Bounds*, PhD Thesis, University of Cambridge, 2020.  
- Morello Architecture Specification, Arm Ltd., 2021.  
- LLVM Implementation: [llvm/lib/Support/Morello.cpp](https://github.com/llvm/llvm-project/blob/main/llvm/lib/Support/Morello.cpp)  
- CHERI C/C++ Programming Guide: [CTSRD-CHERI/cheri-c-programming](https://github.com/CTSRD-CHERI/cheri-c-programming)  

---
