# SSK Architecture: Dual IDEF0 Concern Models

This document presents the formal concern models that govern SSK's architecture. All SSK documentation, source organization, and design decisions trace back to these models.

## Understanding Concern Models

SSK is organized using **IDEF0 notation**—a formal language for describing how complex systems separate concerns. Each concern has clear inputs, outputs, controls, and mechanisms.

**Key principle**: Concerns operate in parallel/reactive mode, not sequentially. When inputs, controls, and mechanisms are present, outputs are generated instantly.

---

## Project Concerns Model (PRJ)

**Context**: What SSK must achieve and what enables/constrains it.

### PRJ/A0: Subset Identity from Zero to Hero

The overarching goal: give subsets stable, queryable identities in relational databases.

![Project Concerns Diagram](diagrams/Project%20Concerns.svg)

**Decomposition**:

| Activity | Reference | Purpose | Description |
|----------|-----------|---------|-------------|
| **A1** | PRJ/A1 | **Formulation: The Bijection Affair** | Recognize prior art, formulate SSK as scalar value bijecting subset onto unique identifier |
| **A2** | PRJ/A2 | **Exposition: Nothing without Value** | Demonstrate SSK in action—capturing, storing, combining, filtering subsets as scalar data |
| **A3** | PRJ/A3 | **Implementation: Rubber Hits Road** | Build reference implementation in PostgreSQL (trivial domain first, then target domain) |
| **A4** | PRJ/A4 | **Expansion: Preserving Semantics at BIGINT Scale** | The heavy lifting—engineering for correctness, completeness, bijection at scale |
| **A5** | PRJ/A5 | **Vitalisation: Expanding Horizons** | Capture derivative ideas, extended scope, adjacent domains for future work |

**Critical Observation**: PRJ/A4:Expansion is where most code lives, but PRJ/A1:Formulation is where the concept lives. Documentation must not conflate these.

---

## Implementation Concerns Model (IMP)

**Context**: How to retain bijection semantics at BIGINT scale.

### IMP/A0: The SSK Extension

The PostgreSQL extension implementing SSK functionality.

![Implementation Concerns Diagram](diagrams/Implementation%20Concerns.svg)

**Decomposition**:

| Activity | Reference | Purpose | Description |
|----------|-----------|---------|-------------|
| **A1** | IMP/A1 | **Value Decoder** | Decode SSK bytes → Abstract bit Vector (AbV) in memory |
| **A2** | IMP/A2 | **Function Processor** | Apply SSK operation on AbV representation |
| **A3** | IMP/A3 | **Value Encoder** | Encode AbV → SSK bytes for storage |

**Detailed Decompositions**:

- **IMP/A1:Value Decoder** →
  - IMP/A11: Use existing AbV
  - IMP/A12: Decode AbV  
  - IMP/A13: Decode Token
  - IMP/A14: Remember AbV

- **IMP/A2:Function Processor** →
  - IMP/A21: SSK/AGG Function Exec
  - IMP/A22: Determine output by AbV
  - IMP/A23: Fragment Input(s)
  - IMP/A24: Determine Output per Fragment
  - IMP/A25: Normalise (Clean & Defrag)

- **IMP/A3:Value Encoder** →
  - IMP/A31: Encode AbV
  - IMP/A32: Encode Token
  - IMP/A33: Conditionally Remember AbV

---

## Control Objects (What Governs the System)

| Control | Type | Governs | Description |
|---------|------|---------|-------------|
| **Formal SSK Type Definition** | Cost Object | PRJ/A3:Implementation, PRJ/A4:Expansion, IMP/A0 | Formal mathematical definition of SSK semantics |
| **Target Domain: BIGINT DBID** | Resource | PRJ/A1:Formulation, PRJ/A4:Expansion | 2^64 ID space requirement |
| **Scalar Requirement** | Resource | PRJ/A3:Implementation, PRJ/A4:Expansion | Must work as PostgreSQL scalar type |
| **Encoding Specification (Format 0, CDU)** | Resource | IMP/A1:Value Decoder, IMP/A3:Value Encoder | Canonical encoding preserving bijection |

---

## Source Code Mapping

| Directory | Concern Layer | IDEF0 Reference | Purpose |
|-----------|---------------|-----------------|---------|
| `src/udt/` | Integration | IMP/A0:The SSK Extension | PostgreSQL type system integration |
| `src/agg/` | Integration | IMP/A21:SSK/AGG Function Exec | Aggregate functions |
| `src/codec/` | Persistence | IMP/A1:Value Decoder, IMP/A3:Value Encoder | Encoding/decoding |
| `src/keystore/` | Caching | Support | AbV transaction cache |
| (missing) | Function Processing | IMP/A2:Function Processor | Set operations (currently distributed) |

---

## Key Architectural Insights

### 1. **Separation of Concerns**

SSK addresses three distinct concerns:

- **Representation**: The bijection between abstract bit vectors and subsets (everyone cares)
- **Persistence**: Encoding/decoding for storage (contributors working ON persistence layer)  
- **Operations**: Set algebra on in-memory representation (contributors working ON function implementation)

### 2. **Bijection is Everything**

SSK is NOT compression, NOT hashing. It's a bijection:
- Same subset → identical bytes (always)
- Different subsets → different bytes (no collisions)
- Perfectly reversible (contains all subset information)

### 3. **Format Immutability**

Format 0 parameters are immutable after production. Changes require Format 1+.

### 4. **Scale Strategy**

Handles 2^64 ID space through:
- Hierarchical partitioning (Partitions → Segments → Chunks → Tokens)
- Sparsity exploitation (empty regions never stored)
- Domain-specific compaction (structure + combinadics + raw bits)

---

## Using This Architecture

### For Contributors

1. **Know your concern**: Identify which IDEF0 activity you're modifying
2. **Respect boundaries**: Don't mix persistence with PostgreSQL integration  
3. **Consult inputs/outputs**: Understand what your concern receives/produces
4. **Test in isolation**: Each concern should be testable independently

### For Architects

- Use IDEF0 references in design documents (e.g., "This change affects IMP/A2:Function Processor")
- Changes within a concern are safe; changes crossing concerns need review
- Future enhancements (GIN indexing, additional formats) map to specific concerns

### For Users

- You interact with the representation layer through SQL
- Persistence and operations concerns are invisible to you
- The architecture enables guarantees about performance and correctness

---

This architecture serves as the authoritative framework for all SSK development decisions. When in doubt, trace your question back to the concern model.