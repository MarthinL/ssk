# Implementation Concerns Model (IMP)

**IDEF0 Model Report**  
**Repository**: Open Source SSK  
**Date**: December 24, 2025  
**Creator**: Marthin Laubscher

---

## Model Summary

**Name**: Implementation Concerns  
**Purpose**: Manage the complexity of retaining bijection semantics at BIGINT scale

### Overview

As a repository and C language project, SSK is misleadingly lopsided. The core concept is incredibly simple once grasped, with full implementation of the trivial case probably not even worth adding a second source file. Meanwhile the code base is dominated by the complexities arising from a secondary problem—scaling that super-simple core to the required domain.

This model seeks to create stable, useful separation of concerns to manage that complexity better by drawing clear lines with well-defined interactions between them.

**Important**: Do not expect to see a source file per concern. IDEF0 concerns are logical boundaries, not physical file organization.

---

## Context Diagram: IMP/A0

### The SSK Extension

The PostgreSQL extension that implements SSK functionality—transforming SSK values between their byte representation and the in-memory Abstract bit Vector (AbV) form needed for operations.

![Implementation Concerns Diagram](diagrams/Implementation%20Concerns.svg)

**Context**: The SSK Extension operates within the PostgreSQL server environment, governed by formal type definitions and encoding specifications, receiving function calls with parameters and returning computed values.

---

## Activities in Context Diagram

### A0: The SSK Extension

**Creator**: Marthin Laubscher  
**Purpose**: Provide SSK data type functionality within PostgreSQL

The SSK Extension is the complete implementation boundary—everything SSK does from PostgreSQL's perspective. It accepts extension function calls with various parameter types (SSK values, BIGINTs, BIGINT arrays) and returns computed results (values, booleans, arrays).

**Decomposition**: [View detailed decomposition](diagrams/A0_%20The%20SSK%20Extension.svg)

---

## Concepts in Context Diagram

### Boundary Concepts

These concepts cross the system boundary at the A0 level—they represent the interface between PostgreSQL and the SSK extension.

#### Inputs

- **Call Parameter(s)** — Function parameters provided by SQL query
- **Extension Function Call** — Invocation from PostgreSQL query executor

#### Controls

- **Formal SSK Type Definition** — Mathematical definition of SSK semantics and bijection properties
- **Encoding Specification (Format 0, CDU)** — Canonical encoding rules that preserve bijection

#### Outputs

- **Call Return Value** — Computed result returned to PostgreSQL query

#### Mechanisms

- **PostgreSQL Server** — Runtime environment providing memory management, type system, and SQL integration

---

## Decomposition: A0 → The SSK Extension

![A0 Decomposition Diagram](diagrams/A0_%20The%20SSK%20Extension.svg)

The SSK Extension decomposes into three parallel/reactive activities that collectively implement the value transformation pipeline.

---

## Activities in Decomposition

### A1: Value Decoder

**Creator**: Marthin Laubscher  
**Purpose**: Transform SSK byte representation into in-memory Abstract bit Vector (AbV) form

**Description**: Value Decoder accepts SSK values (either as byte-encoded values or token references) and produces the AbV—an in-memory representation optimized for set operations. It manages a transaction-local cache to avoid redundant decoding of the same value.

**Key Responsibilities**:
- Recognize and retrieve cached AbV representations
- Decode byte-encoded SSK values per Format 0 specification
- Decode token values using combinadic algorithms
- Maintain AbV cache for transaction duration

**Diagram**: [View A1 decomposition](diagrams/A1_%20Value%20Decoder.svg)

**Sub-activities**:
- **A11**: Use existing AbV — Retrieve cached representation
- **A12**: Decode AbV — Parse byte-encoded hierarchical structure
- **A13**: Decode Token — Expand token to bit positions using combinadics
- **A14**: Remember AbV — Cache decoded representation

---

### A2: Function Processor

**Creator**: Marthin Laubscher  
**Purpose**: Execute SSK operations on AbV representation

**Description**: Function Processor is where the actual SSK semantics live—union, intersection, difference, containment, cardinality. It operates entirely on the AbV representation, never touching byte encodings. The processor handles fragmentation for large domains and normalizes results to canonical form.

**Key Responsibilities**:
- Execute SSK/AGG functions (union, intersection, difference, etc.)
- Determine output type based on input AbV characteristics
- Fragment large inputs for efficient processing
- Apply operations to fragmented data
- Normalize results (clean duplicates, defragment)

**Diagram**: [View A2 decomposition](diagrams/A2_%20Function%20Processor.svg)

**Sub-activities**:
- **A21**: SSK/AGG Function Exec — Route to appropriate operation handler
- **A22**: Determine output by AbV — Analyze input characteristics to predict output
- **A23**: Fragment Input(s) — Break large domains into processable chunks
- **A24**: Determine Output per Fragment — Apply operation to each fragment
- **A25**: Normalise (Clean & Defrag) — Ensure canonical AbV form

---

### A3: Value Encoder

**Creator**: Marthin Laubscher  
**Purpose**: Transform AbV back into byte-encoded SSK value for storage

**Description**: Value Encoder reverses the decoding process—it takes the canonical AbV result from Function Processor and produces the byte representation according to Format 0 and CDU specifications. Like the decoder, it manages caching to avoid redundant encoding.

**Key Responsibilities**:
- Encode AbV structure per Format 0 (partitions, segments, chunks)
- Encode tokens using CDU variable-length encoding
- Conditionally cache AbV for potential reuse
- Ensure canonical encoding (bijection property)

**Diagram**: [View A3 decomposition](diagrams/A3_%20Value%20Encoder.svg)

**Sub-activities**:
- **A31**: Encode AbV — Serialize hierarchical structure to bytes
- **A32**: Encode Token — Compress token representation using CDU
- **A33**: Conditionally Remember AbV — Cache for transaction scope

---

## Concepts in Decomposition

### Internal Concepts

These concepts flow between activities within the A0 decomposition—they are visible only to the three main activities (A1, A2, A3) and represent the internal data transformations.

#### Between A1 → A2

- **Given as AbV** — Decoded parameter in Abstract bit Vector form
- **Given BIGINT** — Integer parameter for operations (e.g., singleton insertion)
- **Given BIGINT[]** — Array parameter for multi-element operations

#### Between A2 → A3

- **Canonical Result AbV** — Normalized operation result in AbV form
- **Resulting BIGINT** — Integer result for scalar-returning operations
- **Resulting BOOLEAN** — Boolean result for predicate operations
- **Resulting BIGINT[]** — Array result for enumeration operations

#### Between A1/A3 ↔ Storage

- **Given Value** — Input SSK value (byte-encoded or token)
- **Resulting Value** — Output SSK value (byte-encoded)

### Boundary Concepts (inherited from A0)

These concepts continue to flow through the decomposition from the context level:

#### Controls

- **Formal SSK Type Definition** — Governs A2 (Function Processor) semantics
- **Encoding Specification (Format 0, CDU)** — Governs A1 (Value Decoder) and A3 (Value Encoder)

#### Mechanisms

- **PostgreSQL Server** — Provides memory management and runtime environment for all activities

---

## Detailed Activity Decompositions

### A1: Value Decoder — Detailed View

![A1 Diagram](diagrams/A1_%20Value%20Decoder.svg)

**Sub-activities**:

1. **A11: Use existing AbV**
   - Check transaction cache for previously decoded value
   - Return cached AbV if available
   - Exit early to avoid redundant work

2. **A12: Decode AbV**
   - Parse Format 0 byte structure (partition deltas, segment headers, chunk metadata)
   - Reconstruct hierarchical AbV representation
   - Use CDU types for variable-length field decoding

3. **A13: Decode Token**
   - Apply combinadic unranking algorithm
   - Expand token to explicit bit positions
   - Maintain token-to-AbV mapping

4. **A14: Remember AbV**
   - Store decoded AbV in transaction-local cache
   - Associate with original value identifier
   - Enable O(1) retrieval for subsequent uses

**Key Internal Concepts**:
- **Value Bytes** — Raw byte representation from storage
- **Token Value** — Compact token identifier
- **Token Type** — Token encoding variant (Format 0 defines multiple)
- **Decoded AbV** — Fully expanded in-memory representation
- **Value Buffer** — Temporary decode workspace
- **CDU Types** — Variable-length encoding type specifications

---

### A2: Function Processor — Detailed View

![A2 Diagram](diagrams/A2_%20Function%20Processor.svg)

**Sub-activities**:

1. **A21: SSK/AGG Function Exec**
   - Route function call to appropriate handler (union, intersection, etc.)
   - Manage aggregate state for multi-row operations
   - Coordinate sub-activities

2. **A22: Determine output by AbV**
   - Analyze input characteristics (cardinality, fragmentation, domain coverage)
   - Predict output form (token vs. explicit, fragmentation needed)
   - Optimize processing strategy

3. **A23: Fragment Input(s)**
   - Break large domains into partition-aligned fragments
   - Enable parallel/pipelined processing
   - Maintain fragment metadata

4. **A24: Determine Output per Fragment**
   - Apply operation to individual fragments
   - Produce partial results
   - Maintain operation-specific state

5. **A25: Normalise (Clean & Defrag)**
   - Remove duplicate bit positions
   - Merge fragments where beneficial
   - Ensure canonical AbV form (required for bijection)

**Key Internal Concepts**:
- **AbV Param** / **AbV Param2** — Input parameters in AbV form
- **BIGINT Param** / **BIGINT[] Param** — Additional operation parameters
- **Function Callback** — Operation-specific logic (union, intersection, etc.)
- **Dirty AbV** — Unnormalized operation result
- **Fresh FragmentSet** — Input fragments ready for processing
- **Dirty FragmentSet** — Output fragments needing normalization

---

### A3: Value Encoder — Detailed View

![A3 Diagram](diagrams/A3_%20Value%20Encoder.svg)

**Sub-activities**:

1. **A31: Encode AbV**
   - Serialize AbV hierarchy to Format 0 byte structure
   - Apply CDU encoding to variable-length fields
   - Maintain canonical encoding rules

2. **A32: Encode Token**
   - Compress explicit bit positions to token form where beneficial
   - Apply combinadic ranking algorithm
   - Select appropriate token type per CDU specification

3. **A33: Conditionally Remember AbV**
   - Decide whether to cache based on value characteristics
   - Store AbV→bytes mapping for transaction scope
   - Enable encoder-side cache hits

**Key Internal Concepts**:
- **Token to Encode** — Intermediate token representation
- **Encoded Token** — Compressed CDU-encoded token bytes
- **Encoded Value** — Complete Format 0 byte representation
- **CDU Types** — Encoding type specifications

---

## Source Code Mapping

This model maps to the following source code locations:

| IDEF0 Activity | Primary Source | Purpose |
|----------------|----------------|---------|
| **IMP/A1** Value Decoder | `src/codec/` | Format 0 decoding, CDU parsing |
| **IMP/A11-A14** | `src/keystore/` | AbV caching and retrieval |
| **IMP/A2** Function Processor | Distributed across `src/` | Set operations (needs consolidation) |
| **IMP/A21** | `src/agg/` | Aggregate function handlers |
| **IMP/A3** Value Encoder | `src/codec/` | Format 0 encoding, CDU serialization |
| **IMP/A0** Integration | `src/udt/` | PostgreSQL type system interface |

**Observation**: The Function Processor (A2) is currently scattered across the codebase. The IDEF0 model suggests it should be consolidated into a dedicated module for better maintainability.

---

## Architectural Principles

### 1. Bijection Preservation

Every transformation maintains the bijection property:
- **A1** (Decode): bytes → AbV is injective (same bytes → same AbV)
- **A2** (Process): AbV → AbV preserves semantics (same input → same output)
- **A3** (Encode): AbV → bytes is canonical (same AbV → same bytes)

Result: Same subset input → identical byte output (always), different subsets → different bytes (no collisions).

### 2. Separation of Representation and Persistence

- **AbV** is the representation layer—how we think about subsets in memory
- **Format 0** is the persistence layer—how we store subsets on disk
- **A1/A3** manage the boundary; **A2** never sees bytes

This separation allows:
- Format evolution (Format 1+) without changing operation semantics
- Operation optimization without breaking encoding compatibility
- Clear testing boundaries (mock AbV, test operations)

### 3. Cache Management

Both A1 and A3 manage transaction-local caches:
- Avoids redundant decoding when same value used multiple times in query
- Avoids redundant encoding when intermediate results reused
- Scoped to transaction (automatic cleanup)

---

## Using This Model

### For Contributors

**Before modifying code**:
1. Identify which activity (A1, A2, or A3) your change affects
2. Understand the inputs, outputs, and controls for that activity
3. Check if changes cross activity boundaries (requires extra care)
4. Ensure bijection properties are preserved

**Examples**:
- Adding a new set operation → modify **A2** only
- Optimizing CDU encoding → modify **A3** only (test with existing values)
- Adding Format 1 → modify **A1/A3** together (requires version detection)

### For Architects

**Design questions to ask**:
- Which activity owns this functionality?
- What are the inputs, outputs, controls, and mechanisms?
- Does this change affect the bijection?
- Will this impact other activities?

**References in documentation**:
Use IDEF0 notation: "This optimization affects IMP/A31:Encode AbV performance by 40%"

---

## Related Documents

- **[Project Concerns Model](PROJECT_CONCERNS.md)** — Strategic/governance concerns (PRJ/A0-A5)
- **[Architecture Guide](ARCHITECTURE.md)** — Technical integration and contributor guidelines
- **[Specification](SPECIFICATION.md)** — Formal SSK semantics and Format 0 definition

---

*This model serves as the authoritative decomposition of SSK implementation complexity. When debugging, refactoring, or extending functionality, trace your work back to these activities and their defined interactions.*
