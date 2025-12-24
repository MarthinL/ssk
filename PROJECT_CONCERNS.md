# Project Concerns Model (PRJ)

**IDEF0 Model Report**  
**Repository**: Open Source SSK  
**Date**: December 24, 2025  
**Creator**: Marthin Laubscher

---

## Model Summary

**Name**: Project Concerns  
**Purpose**: Define the strategic scope and governance of the SSK open-source initiative

### Overview

This model addresses project-level concerns—the "why" and "what" of SSK before the "how." It separates the intellectual property (the bijection concept itself) from the engineering work (scaling to production domains), and acknowledges both the demonstrated viability and the future horizons.

The SubSet Key bijection has enormous potential in the wider database industry and its countless customers, end-users, their customers, and the plethora of ventures that form and service the entire value chain. This model ensures that conceptual clarity, demonstration value, implementation quality, production scalability, and future vision all receive appropriate attention without conflation.

---

## Context Diagram: PRJ/A0

### Subset Identity from Zero to Hero

Give subsets stable, queryable identities in relational databases—from initial concept through production-ready implementation.

![Project Concerns Diagram](diagrams/Project%20Concerns.svg)

**Context**: The SSK project operates within the open-source database ecosystem, constrained by the BIGINT domain requirement (2^64 identifiers) and enabled by PostgreSQL's extensibility. It produces both conceptual artifacts (the formulation) and concrete implementations (the extension).

---

## Activities in Context Diagram

### A0: Subset Identity from Zero to Hero

**Creator**: Marthin Laubscher  
**Purpose**: Establish SSK as a viable, usable solution for subset identity in production databases

The complete SSK project—from recognizing the mathematical foundation through demonstrating viability, implementing a reference solution, engineering for scale, and identifying future applications.

**Decomposition**: [View detailed decomposition](diagrams/Decomposition%20of%20SSK%20Concerns.svg)

---

## Concepts in Context Diagram

### Boundary Concepts

These concepts define what the project consumes, produces, and operates within.

#### Inputs

- **Database Identity Requirements** — The universal need for unique, stable subset identifiers in relational systems
- **Prior Art & Combinatorics** — Existing mathematical foundations (combinadic algorithms, bijections)

#### Controls

- **Target Domain: BIGINT DBID** — The 2^64 identifier space requirement (non-negotiable for production databases)
- **Scalar Requirement** — Must work as native database scalar type (no auxiliary tables, no external dependencies)

#### Outputs

- **SSK Concept & Definition** — The formalized bijection between subsets and scalar identifiers
- **Reference Implementation** — Working PostgreSQL extension demonstrating the concept
- **Production-Ready Extension** — Scalable implementation handling BIGINT domain
- **Future Directions** — Identified opportunities for extended scope and adjacent domains

#### Mechanisms

- **PostgreSQL Ecosystem** — The platform providing extensibility, type system, and user base
- **Open Source Community** — Contributors, adopters, and maintainers

---

## Decomposition: A0 → Subset Identity from Zero to Hero

![PRJ/A0 Decomposition Diagram](diagrams/Decomposition%20of%20SSK%20Concerns.svg)

The SSK project decomposes into five sequential phases, each building on the previous:

1. **Formulation** — Recognize and formalize the core concept
2. **Exposition** — Demonstrate viability with working examples
3. **Implementation** — Build reference implementation
4. **Expansion** — Engineer for production scale
5. **Vitalisation** — Capture future potential

**Critical Observation**: Most code lives in A4 (Expansion), but the concept lives in A1 (Formulation). Documentation must not conflate these.

---

## Activities in Decomposition

### A1: Formulation — The Bijection Affair

**Creator**: Marthin Laubscher  
**Purpose**: Recognize prior art and formulate SSK as a scalar value bijecting subset onto unique identifier

**Description**: This is where the intellectual property resides—the recognition that subsets can be treated as first-class scalar values through a bijection to unique identifiers. It's not compression (lossy), not hashing (collisions), but a true bijection: same subset → identical ID (always), different subsets → different IDs (no collisions), perfectly reversible.

**Key Insights**:
- Subsets of enumerable domains have natural ordinal positions
- Combinadic algorithms provide bijections for k-subsets of n-sets
- Hierarchical partitioning extends to arbitrary domains
- Token representation exploits sparsity

**Outputs**:
- Formal definition of SSK semantics
- Mathematical proof of bijection properties
- Conceptual framework for subset identity

**Control**: Target Domain BIGINT DBID constrains the formulation—we need 2^64 identifiers, not just theoretical elegance.

---

### A2: Exposition — Nothing without Value

**Creator**: Marthin Laubscher  
**Purpose**: Demonstrate SSK in action—capturing, storing, combining, filtering subsets as scalar data

**Description**: Proof of concept through demonstration. Show that SSK isn't just theoretically sound but practically useful. Demonstrate capturing subsets from data, storing them as scalar values, combining them with set algebra, and filtering data based on membership.

**Key Demonstrations**:
- Capture: Extract subset from query result → SSK value
- Store: SSK value in table column (no auxiliary tables)
- Combine: Union, intersection, difference as scalar operations
- Query: Filter by subset membership, compute cardinality
- Persist: SSK values survive transactions, backups, replication

**Audience**: Database developers and architects evaluating SSK adoption

**Success Criteria**: Working examples that clearly show value proposition

---

### A3: Implementation — Rubber Hits Road

**Creator**: Marthin Laubscher  
**Purpose**: Build reference implementation in PostgreSQL (trivial domain first, then target domain)

**Description**: Convert concept and demonstrations into production code. This is the first complete implementation—a working PostgreSQL extension that provides SSK functionality. Start with trivial domain (small n) for correctness verification, then extend to target domain.

**Key Deliverables**:
- PostgreSQL extension with SSK data type
- Basic set operations (union, intersection, difference, containment)
- Encoding/decoding infrastructure (Format 0)
- Unit tests for correctness
- Integration tests with PostgreSQL

**Implementation Strategy**:
1. Trivial domain first (n ≤ 1000) — proves concept without scale complexity
2. Validate semantics and user experience
3. Extend to BIGINT domain — adds scale engineering

**Control**: Scalar Requirement governs implementation—must work as native PostgreSQL type.

---

### A4: Expansion — Preserving Semantics at BIGINT Scale

**Creator**: Marthin Laubscher  
**Purpose**: Engineering for correctness, completeness, and bijection at scale

**Description**: This is where most code lives. Scaling the trivial domain to 2^64 identifiers introduces enormous engineering challenges—hierarchical partitioning, sparsity exploitation, variable-length encoding, cache management, fragmentation handling, and performance optimization.

**Engineering Challenges**:
- **Domain Scale**: 2^64 possible members → cannot enumerate
- **Cardinality Range**: 0 to 2^64 members → cannot store explicitly
- **Sparsity**: Real-world subsets are typically sparse → exploit structure
- **Encoding Efficiency**: Minimize storage while maintaining canonicality
- **Performance**: Operations must complete in reasonable time

**Solutions**:
- **Hierarchical Partitioning**: Partition → Segment → Chunk → Token structure
- **Format 0 Encoding**: Canonical encoding specification (CDU for variable-length)
- **Combinadic Tokens**: Compress sparse regions using combinatorial ranking
- **AbV Representation**: In-memory form optimized for operations
- **Cache Management**: Transaction-local caching to avoid redundant work

**Critical Property**: Bijection must be preserved—same subset → identical bytes (always).

**Success Criteria**: 
- Handles 2^64 domain correctly
- Maintains bijection property
- Performance acceptable for production use
- Memory usage reasonable

---

### A5: Vitalisation — Expanding Horizons

**Creator**: Marthin Laubscher  
**Purpose**: Capture derivative ideas, extended scope, adjacent domains for future work

**Description**: SSK opens doors to adjacent possibilities—GIN indexing for fast subset queries, support for other database systems, alternative encoding formats, subset-based access control, temporal subsets, and many others. This activity captures and documents these opportunities without committing to immediate implementation.

**Future Directions**:
- **GIN Indexing**: Enable `WHERE value @ subset` queries using inverted index
- **Multi-Database**: Port to MySQL, SQLite, Oracle, etc.
- **Format Evolution**: Format 1+ with different trade-offs (speed vs. size)
- **Ordered Subsets**: Extend to multisets or ordered collections
- **Temporal Subsets**: Subsets that evolve over time
- **Distributed Systems**: SSK in distributed databases (consistency challenges)

**Purpose**: Ensure good ideas aren't lost and provide roadmap for contributors

**Note**: Ideas here are NOT commitments—they're possibilities to be evaluated later.

---

## Relationships Between Activities

### Sequential Flow

```
A1 (Formulation)
  ↓ provides concept
A2 (Exposition)
  ↓ validates viability
A3 (Implementation)
  ↓ proves feasibility
A4 (Expansion)
  ↓ achieves production readiness
A5 (Vitalisation)
  → captures future potential
```

### Critical Dependencies

- **A2 depends on A1**: Can't demonstrate what hasn't been formulated
- **A3 depends on A2**: Implementation without validated concept risks building the wrong thing
- **A4 depends on A3**: Can't scale what doesn't work at trivial scale
- **A5 depends on A4**: Future directions make sense only with production foundation

### Independence

- **A1 is self-contained**: The bijection concept exists independent of implementation
- **A5 is parallel**: Future ideas can be captured during any phase

---

## Mapping to Artifacts

| Activity | Primary Artifacts | Location |
|----------|-------------------|----------|
| **A1: Formulation** | Formal definitions, mathematical proofs | `SPECIFICATION.md`, `SSK_INTRO.md` |
| **A2: Exposition** | Examples, demonstrations, tutorials | `README.md`, `examples/` |
| **A3: Implementation** | Core extension code, basic operations | Early commits, `src/udt/`, `src/codec/` |
| **A4: Expansion** | Scale engineering, optimizations | Most of `src/`, `include/`, Format 0 |
| **A5: Vitalisation** | Future ideas, roadmap | `TODO-LIST.md`, issue tracker |

---

## Common Misconceptions

### "SSK is just the PostgreSQL extension"

**Wrong**. The extension (A3/A4) is an *implementation* of SSK. The concept (A1) is the core intellectual property—it exists independent of any implementation.

### "The complex code is what makes SSK valuable"

**Wrong**. The value is the bijection (A1). The complex code (A4) is necessary engineering to make it work at production scale, but the insight is separate.

### "SSK is only for PostgreSQL"

**Wrong**. PostgreSQL is our reference platform (A3/A4). The concept (A1) applies to any database system. A5 explicitly considers other platforms.

### "All the code should be in production"

**Wrong**. A2 (Exposition) produces demonstration code that may never ship. A5 produces experimental code that explores possibilities. Only A3/A4 target production.

---

## Governance Implications

### What Changes Require Review

**A1 Changes** (Formulation):
- Require mathematical review
- Impact ALL downstream activities
- Must preserve bijection property
- Example: Changing the fundamental definition

**A2 Changes** (Exposition):
- Require usability review
- Impact adoption and comprehension
- Example: New tutorial or example

**A3/A4 Changes** (Implementation/Expansion):
- Require code review
- Impact production users
- Must maintain backward compatibility
- Example: Performance optimization, bug fix

**A5 Changes** (Vitalisation):
- Require strategic review
- Impact project roadmap
- Example: New feature proposal

### Versioning Strategy

- **Concept versioning**: SSK semantics are stable (A1)
- **Format versioning**: Format 0, Format 1, etc. (A4)
- **Extension versioning**: Semantic versioning for releases (A3/A4)

---

## Using This Model

### For Project Governance

**When evaluating changes**:
1. Identify which activity the change affects
2. Apply appropriate review process
3. Assess downstream impacts
4. Ensure activity-appropriate testing

**When planning roadmap**:
1. Check A5 for captured future directions
2. Prioritize based on user needs
3. Maintain conceptual coherence (trace back to A1)

### For Contributors

**Before contributing**:
1. Understand which activity your contribution targets
2. Read relevant artifacts for that activity
3. Follow activity-appropriate standards
4. Consider impacts on related activities

**Examples**:
- Improving documentation → A2 (Exposition)
- Adding GIN index support → A5 → A4 (move from future to expansion)
- Optimizing encoding → A4 (Expansion)
- Proposing new concept → A1 (Formulation) - rare and requires deep review

### For Adopters

**Understanding what you're getting**:
- **A1**: The guarantee (bijection semantics)
- **A2**: The examples (how to use)
- **A3/A4**: The implementation (production code)
- **A5**: The potential (future possibilities, not promises)

---

## Related Documents

- **[Implementation Concerns Model](IMPLEMENTATION_CONCERNS.md)** — Technical decomposition (IMP/A0-A3)
- **[Architecture Guide](ARCHITECTURE.md)** — Integration of both concern models
- **[Specification](SPECIFICATION.md)** — Formal SSK semantics (output of PRJ/A1)
- **[Introduction](SSK_INTRO.md)** — Conceptual overview (output of PRJ/A1/A2)

---

*This model serves as the authoritative decomposition of SSK project scope and governance. When making strategic decisions about the project's direction, trace your reasoning back to these activities and their relationships.*
