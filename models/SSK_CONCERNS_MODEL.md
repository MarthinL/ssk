# SSK: Formal Separation of Concerns (IDEF0/ICOM Model)
**Date:** 19 December 2025  
**Purpose:** Define distinct concerns in SSK's architecture using IDEF0/ICOM (Inputs, Controls, Outputs, Mechanisms) to enable contributors and users to understand where they are in the system and what they can safely ignore.

---

## Top-Level: SSK as Subset Identity System

```
┌─────────────────────────────────────────────────────────────────┐
│ SSK: Representation of Subsets as Persistent Scalar Identities  │
├─────────────────────────────────────────────────────────────────┤
│ Inputs:      A subset of database IDs (e.g., {5, 10, 15})        │
│ Controls:    Bijection requirement (unique, reversible)          │
│ Outputs:     A scalar SSK value (opaque binary)                  │
│ Mechanisms:  Representation layer, encoding layer, query layer   │
└─────────────────────────────────────────────────────────────────┘
```

**What SSK Is:** A system that maps each possible subset of database IDs to a unique, canonical scalar value. This scalar:
- Encodes the exact membership of the subset (bijection).
- Can be stored, indexed, and compared in databases.
- Enables direct subset operations without materializing the underlying IDs.

**What SSK Is Not:** A compression algorithm, a hash function, or an encoding scheme (though it uses both).

---

## The Four Core Concerns

### Concern 1: Representation Layer
**What:** The abstract mapping of "subset of IDs" ↔ "unique scalar value."

```
┌─────────────────────────────────────────────────────────────────┐
│ CONCERN: Subset Representation                                  │
├─────────────────────────────────────────────────────────────────┤
│ Inputs:      A subset of 64-bit database IDs (arbitrary size)    │
│ Controls:    - Bijection guarantee (1:1 mapping)                 │
│              - Canonicity (same subset → same scalar)            │
│              - Reversibility (scalar can be fully decoded)       │
│ Outputs:     An abstract scalar value                            │
│ Mechanisms:  Mathematical foundation (combinadics, sparsity)     │
├─────────────────────────────────────────────────────────────────┤
│ Scale:       2^64 possible ID values → 2^(2^64) possible subsets │
│              (impossibly large → requires hierarchy + encoding)  │
│                                                                  │
│ Dependency:  Does NOT depend on encoding; is purely conceptual. │
│              Different encodings (e.g., CDU vs. VLQ-P) can      │
│              represent the same subset.                          │
└─────────────────────────────────────────────────────────────────┘
```

**Contributors working ON this concern:**
- Prove or validate bijection properties.
- Extend the abstract model (e.g., from BIGINT to other domains).
- Design new query operations that preserve canonicity.

**Contributors working WITH this concern:**
- Can assume bijection is guaranteed; encoding internals are irrelevant.
- Build query functions knowing that subset identity is stable.

**Key Principle:** The representation layer is independent of how the subset is encoded. A different encoding scheme could replace CDU+Format 0 without changing the representation.

---

### Concern 2: Encoding Layer (Persistence)
**What:** The hierarchical encoding strategy that compacts abstract bit vectors into persistent bytes while preserving bijection.

```
┌─────────────────────────────────────────────────────────────────┐
│ CONCERN: Hierarchical Encoding for Persistence                  │
├─────────────────────────────────────────────────────────────────┤
│ Inputs:      Abstract bit vector (subset represented as bits)    │
│ Controls:    - Bijection preservation (canonical mapping)        │
│              - Compaction efficiency (minimize bytes)            │
│              - Determinism (same input → same output)           │
│              - Decoding unambiguity (one encoding → one subset)  │
│ Outputs:     Persisted bytes (opaque scalar in database)         │
│ Mechanisms:  Partitions → Segments → Chunks → Tokens (CDU)      │
├─────────────────────────────────────────────────────────────────┤
│ Strategy:    Exploit sparsity and known structure               │
│              - Empty partitions omitted (sparsity)               │
│              - Segments break at dominant-bit gaps (structure)   │
│              - Chunks use combinadic (ENUM) or raw (RAW) based   │
│                on density (adaptation)                           │
│              - CDU codec for meta-parameter encoding             │
│                (canonical integer representation)                │
│                                                                  │
│ Dependencies: - Depends ON representation (must preserve)        │
│              - CDU codec (parameterized, swappable)              │
│              - Format parameters (CHUNK_BITS=64, etc.)           │
│                                                                  │
│ Tuning:      Format 0 is frozen; alternative formats possible   │
│              (e.g., Format 1 with different thresholds).         │
└─────────────────────────────────────────────────────────────────┘
```

**Contributors working ON this concern:**
- Design new encoding formats (e.g., Format 1).
- Optimize chunk encoding (e.g., different combinadic tables).
- Prove canonicity of the encoding process.
- Implement encoder and decoder.

**Contributors working WITH this concern:**
- Use encoder/decoder as black box; assume bijection is preserved.
- Don't need to know segment sizes, chunk thresholds, or CDU details.
- Can focus on memory management, aggregate logic, or query semantics.

**Key Principle:** The encoding layer translates abstract representation into storage-optimized bytes. Its internal complexity (hierarchy, tokens, CDU) is an implementation detail that users never see and contributors should isolate.

---

### Concern 3: Decoding/In-Memory Query Layer
**What:** Reconstructing the abstract representation from persisted bytes and enabling operations on it.

```
┌─────────────────────────────────────────────────────────────────┐
│ CONCERN: Decoding and In-Memory Operations                      │
├─────────────────────────────────────────────────────────────────┤
│ Inputs:      Persisted bytes (opaque SSK scalar)                 │
│ Controls:    - Lossless reconstruction (bijection preserved)     │
│              - Efficient in-memory representation (no 2^64 bits)  │
│              - Operation correctness (union, intersection, etc.) │
│              - Offset-based pointers (realloc safety)            │
│ Outputs:     In-memory decoded structure + operation results    │
│ Mechanisms:  Decoder (CDU parser), memory model (offset-based),  │
│              operations (binary operations on decoded chunks)    │
├─────────────────────────────────────────────────────────────────┤
│ Memory Model: Decoded structure uses offset-based pointers to    │
│              partition array, segment array, chunk data. This    │
│              survives realloc without invalidation.               │
│                                                                  │
│ Operations:  Union, intersection, difference, membership test    │
│              work on decoded representation without re-encoding. │
│                                                                  │
│ Dependencies: - Depends ON encoding (must decode correctly)      │
│              - Representation concern (operations must preserve  │
│                canonicity and bijection)                         │
│              - Memory safety (offset-based, bounds checking)     │
└─────────────────────────────────────────────────────────────────┘
```

**Contributors working ON this concern:**
- Implement decoder (CDU parser).
- Design memory layout for decoded structures.
- Implement set operations (union, intersection, difference).
- Prove operation correctness.

**Contributors working WITH this concern:**
- Use decoder/memory as abstract interfaces; don't need to know memory layout.
- Call operations; let them handle internal details.
- Focus on aggregate logic or query semantics.

**Key Principle:** The decoded representation must be efficient enough to fit in memory yet flexible enough to support arbitrary operations. The memory model (offset-based) is an implementation choice; contributors working on aggregate functions don't need to know it.

---

### Concern 4: PostgreSQL Integration (UDT + Aggregates)
**What:** Exposing SSK as a user-defined type and aggregate in PostgreSQL.

```
┌─────────────────────────────────────────────────────────────────┐
│ CONCERN: PostgreSQL Extension Interface                         │
├─────────────────────────────────────────────────────────────────┤
│ Inputs:      SQL queries, user function calls, aggregate setup   │
│ Controls:    - PostgreSQL UDT contract (I/O, comparison, ops)    │
│              - Aggregate semantics (sfunc, ffunc state mgmt)     │
│              - Correctness (bijection preserved through ops)     │
│ Outputs:     SQL-queryable results, indexed SSKs, aggregated    │
│              subset values                                       │
│ Mechanisms:  Input/output functions, comparison operators,      │
│              aggregate state transitions (sfunc/ffunc),          │
│              operator definitions (@>, <@, ==, etc.)            │
├─────────────────────────────────────────────────────────────────┤
│ Current Scope: UDT I/O, basic operators, aggregate templates    │
│ Future Scope:  GIN indexes, advanced operators, RI enforcement   │
│                                                                  │
│ Dependencies: - Depends ON all lower layers                      │
│              (encoding, decoding, representation)                │
│              - PostgreSQL type system (C API)                    │
└─────────────────────────────────────────────────────────────────┘
```

**Contributors working ON this concern:**
- Implement PostgreSQL C functions (input_fn, output_fn, etc.).
- Define aggregate state machines.
- Design operator semantics (what does @> mean?).
- Test end-to-end in PostgreSQL.

**Contributors working WITH this concern:**
- Use SSK as a type in SQL; don't need to know C internals.
- Call aggregate functions; state management is abstracted.
- Write queries using SSK values.

**Key Principle:** The PostgreSQL layer is the user-facing interface. Contributors here work WITH all lower concerns; they assume encoding, decoding, and operations are correct and focus on SQL semantics.

---

## Interaction Matrix: Which Concerns Talk to Each Other?

| From → To | Representation | Encoding | Decoding | PostgreSQL |
|-----------|---|---|---|---|
| **Representation** | — | Depends (input) | — | — |
| **Encoding** | Constrains (must preserve) | — | Depends (output) | — |
| **Decoding** | Depends (correctness) | Depends (parsing) | — | Depends |
| **PostgreSQL** | — | — | Depends (input) | — |

**Flow:**
- **Writing:** User SQL → PostgreSQL layer → Encoding layer (compressed bytes) → Storage.
- **Reading:** Storage → Decoding layer (in-memory structure) → Representation layer (bijection guaranteed) → PostgreSQL layer (result to user).
- **Operations:** PostgreSQL layer → Decoding layer → Representation operations (union, etc.) → Encoding layer (canonical re-encoding) → Storage.

---

## Using This Model in Practice

### For Code Comments
When documenting a function, reference which concern it works ON or WITH:

```c
/**
 * ssk_decoded_union() - Compute union of two decoded SSKs
 * CONCERN: Works ON Decoding/In-Memory Query Layer
 *          Works WITH Representation Layer (must preserve bijection)
 * 
 * Inputs: Two decoded SSK structures
 * Output: A new decoded SSK representing the union
 * 
 * Note: Does not re-encode; operations on decoded form. 
 *       Canonicity is preserved because union operation is deterministic.
 */
```

### For Commit Messages
When committing work, state the concern:

```
[Encoding Layer] Implement CDU decoder for partition arrays
[PostgreSQL Layer] Add aggregate transition function for UNION
[Decoding Layer] Optimize offset-based pointer traversal
```

### For Design Decisions
When proposing a change, reference which concern it affects:

```
Decision: Use offset-based pointers for decoded structures
Concern: Decoding/In-Memory Query Layer
Rationale: Survives realloc; enables efficient memory growth
Implication: Bounds-checking required; decoder must validate offsets
```

---

## Boundaries and Firewall Principles

### Rock-Solid Boundaries (Never Cross Without Discussion)
1. **Representation ↔ Encoding:** Changing how representation is encoded requires proving bijection still holds.
2. **Encoding ↔ Decoding:** Changing encoding format requires new decoder; must be version-gated.
3. **Decoding ↔ PostgreSQL:** Changes to operation semantics affect SQL queries; must be documented and backward-compatible.

### Loose Boundaries (Implementation Details)
1. **Within Encoding:** Partition sizes, chunk thresholds, token schemes are tunable (Format parameters).
2. **Within Decoding:** Memory layout is an internal detail; can be optimized without external impact.
3. **Within PostgreSQL:** Operator implementations can change as long as semantics stay the same.

---

## Why This Model Matters

1. **For Contributors:** Know what you're working on and what you can safely ignore. "I'm working ON the encoding layer" means "I don't need to understand aggregate logic."

2. **For Reviewers:** Check commits against this model. "This commit touches Encoding and PostgreSQL layers" is a red flag; should be split.

3. **For Users:** Understand that SSK's complexity is managed through layers. "SSK encodes subsets" (conceptual) ≠ "here's how partitions and chunks work" (implementation).

4. **For Future Maintainers:** When questions arise ("Should we change CHUNK_BITS?"), reference which concern is affected and what that change implies.

---

## Evolution of Concerns

As SSK evolves, new concerns may emerge or existing ones may split:
- **GIN Indexing Concern** (future): How SSK values are indexed for efficient retrieval.
- **Reference Integrity Concern** (future): Enforcing consistency when parent/child subsets change.
- **Version Management Concern** (implicit): How Format 0 → Format 1 transitions work.

When these emerge, document them here with the same IDEF0 structure.

---

## References

- **Implementation Concerns:** See [../IMPLEMENTATION_CONCERNS.md](../IMPLEMENTATION_CONCERNS.md) for the complete technical decomposition (IMP model).
- **Project Concerns:** See [../PROJECT_CONCERNS.md](../PROJECT_CONCERNS.md) for the strategic decomposition (PRJ model).
- **Architecture Guide:** See [../ARCHITECTURE.md](../ARCHITECTURE.md) for integration patterns and source code mapping.
- **Formal Models:** See [README.md](README.md) for descriptions of all IDEF0 model files in this directory.
