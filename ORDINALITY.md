# Run Length Encoded Ordinality for SSK Values

## Overview

This document describes a method for preserving the ordinality (original order) of a sequence of unique natural numbers when storing that sequence as an **SSK value**—a set encoded via a logical bit vector. The approach is designed for applications where the default order is ascending, with occasional manual reordering allowed. It introduces **Run Length Encoded Ordinality (RLEO)** as an efficient encoding for the "ordinality component"—the information lost when converting a sequence to an SSK value.

---

## Terminology

- **SSK Value:**  
  A set of unique natural numbers encoded as a logical bit vector. For a universe of N elements, bit i is set if integer i is present in the set.

- **Ordinality Component:**  
  The information about the original order of the sequence, lost when converting a sequence to an SSK value.

- **Run Length Encoded Ordinality (RLEO):**  
  A compact encoding of the ordinality component, representing the permutation of set members as a sequence of monotonic runs (ascending/descending). Each run is stored as a tuple: (start position, length, step).

---

## Motivation

- **Canonical Storage:**  
  SSK values (bit vectors) provide efficient and canonical set representation, but do not retain original sequence order.
- **Occasional Manual Reordering:**  
  For user-facing applications where list order may be tweaked by users, storing full sequences is wasteful—most lists are nearly sorted.
- **Efficient Recovery:**  
  RLEO enables reconstructing the original sequence from the SSK value and a minimal, compact record of the order changes.

---

## Encoding Method

Given:
- An original sequence σ = [x₁, x₂, ..., xₙ] of unique natural numbers.
- The corresponding set encoded as SSK value (bit vector).
- The sorted list of set members, `sorted_members[]`.

### Step 1: Compute the Ordinality List

For each element in σ, record its 1-based position in `sorted_members`.  
E.g., σ = [40, 10, 20, 30], `sorted_members` = [10, 20, 30, 40], ordinality = [4, 1, 2, 3].

### Step 2: Run Length Encode the Ordinality

Scan the ordinality list for maximal monotonic runs (step +1 or -1).  
Each run is encoded as (start_value, run_length, step).

- E.g., [4, 1, 2, 3] → two runs: (4, 1, 0), (1, 3, +1)

### Step 3: Store RLEO Alongside the SSK Value

- If ordinality is [1, 2, ..., n], the RLEO is empty (natural order).
- Otherwise, store the RLEO as an array of tuples.

### Decoding

Given the SSK value and RLEO, reconstruct the ordinality list, then map ordinals to `sorted_members` to recover the original sequence.

---

## Application Constraints

- **Default Order:**  
  Sequences are typically stored in ascending order.
- **Manual Reordering:**  
  Only occasional, manual reordering is allowed. The system may refuse or warn if RLEO size grows excessively.
- **Set Size:**  
  Set size is limited to ensure RLEO always remains compact and decoding is efficient.

---

## Literature and Prior Art

- **Permutation with Runs:**  
  Encoding permutations with monotonic runs is well-studied in combinatorics and information theory.  
  See ["Runs in Permutations"](https://en.wikipedia.org/wiki/Permutation#Runs).

- **Compressed Permutations:**  
  [Barbay, J., & Navarro, G. (2013). SIAM J. Computing, 43(1), 1–31.](https://arxiv.org/abs/1007.1163)  
  This work formalizes the use of monotonic runs for compact permutation encoding and proves its efficiency for nearly-sorted permutations.

- **Applications:**  
  RLEO is related to techniques in text editing, version control, and time-series compression, where run-based encodings are common.

---

## Acknowledgements

- The term **SSK value** follows established usage for bit vector set encoding.
- Run-length encoding of ordinality is based on work in permutation compression, especially Barbay & Navarro (2013), and adapted for practical application constraints here.

---

## References

1. Barbay, J., & Navarro, G. (2013). Compressed Representations of Permutations, and Applications. *SIAM Journal on Computing*, 43(1), 1–31. [arXiv:1007.1163](https://arxiv.org/abs/1007.1163)
2. Wikipedia: [Runs in Permutations](https://en.wikipedia.org/wiki/Permutation#Runs)
3. Knuth, D. E., *The Art of Computer Programming*, Vol. 3: Sorting and Searching.

---