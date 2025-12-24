# IDEF0 Model Reconstruction: SSK Concerns

**Source Files:** SSK_CONCERNS.TXT, SSK_CONCERNS.IDL, SSK_CONCERNS.xml, SSK_CONCERNS.csv  
**Reconstructed:** 21 December 2025  
**Status:** Ready for verification

---

## Overview: Model Architecture

The IDEF0 model uses **4 diagram levels**:

| Diagram | ID | Title | Decomposition |
|---------|---|----|-----------------|
| **A-0** | C3 | SubSet Key (Context) | Top-level system boundary |
| **A0** | C4 | SubSet Key decomposition | Trivial→Beyond→BIGINT approach |
| **A3** | C5 | BIGINT Domain implementation | Codec + Operations + State mgmt |
| **A31** | C7 | BIGINT DB ID SSK Codec detail | Store + CDU + Language + Memory |

Each level drills into a specific concern while maintaining concept flows (inputs/controls/outputs/mechanisms).

---

## A-0: Context Diagram (The System Boundary)

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  Semantics of Bijection (CONTROL 1)                              │
│  Member Domain (CONTROL 2)                  │                    │
│                                             ↓                    │
│  Defined input parameters ───────────────→ ┌──────────────────┐  │
│                                            │  SubSet Key      │  │
│                                            │  (A0)            │  │
│  Host DB ──────────────────────────────→ │                  │  │
│                                            │                  │  │
│                                            └──────────────────┘  │
│                                             ↓ ↓ ↓ ↓              │
│                         Calculated SSK Values │                  │
│                      Decisions based on SSK   │                  │
│                      SSK Values CAST AS types │                  │
│                      SSK Documentation ──────→┘                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Observations:**
- **Controls (2):** Semantics of Bijection + Member Domain = fundamental constraints on the entire system
- **Inputs (1):** Defined input parameters (user-provided data)
- **Mechanisms (1):** Host DB (the operational environment—PostgreSQL)
- **Outputs (4):** Multiple output types (values, decisions, casts, documentation)

---

## A0: First Decomposition (The Build Strategy)

```
┌───────────────────────────────────────────────────────────────────────────┐
│                                                                           │
│  Semantics of Bijection ─────────────────────────────────────────→       │
│                                                                │           │
│  Member Domain ────┬─ "1..64 Domain" ──→ ┌──────────────────┐            │
│                   │                      │  Trivial         │  (A2 ref)  │
│                   │                      │  Implementation  │            │
│                   │                      │  1..64           │            │
│                   │                      │                  │            │
│                   │                      └──────────────────┘            │
│                   │                              │ ↑                      │
│                   │                              │ │                      │
│                   │                              │ └─ Reference code      │
│                   │                              │                        │
│                   │                       Demo SSK ↓                      │
│                   │                      ┌──────────────────┐            │
│  Defined input ──┼─→ Parameters ────────→│  Allow SSK to    │            │
│  parameters      │  (split by domain)    │  yield value     │            │
│                  │                       │  beyond original │            │
│                  │                       │  use-case        │            │
│                  │                       │                  │            │
│                  │                       └──────────────────┘            │
│                  │                              ↓ ↓                      │
│                  │                    SSK UDT Definitions                │
│                  │                    SSK Format Spec                    │
│                  │                    SSK User's Guide                   │
│                  │                              │                        │
│                  └─ "BIGINT Domain" ──→ ┌──────────────────┐             │
│                                         │  BIGINT Domain   │             │
│                                         │  Implementation  │             │
│                                         │  (A3 detail ref) │             │
│                                         │                  │             │
│                                         └──────────────────┘             │
│                                              ↓ ↑                         │
│                                              │ │                         │
│                        SSK Developer's Guide │ └─ Reference from trivial │
│                                              │                           │
│                   Host DB ────────────────────────────────────────────   │
│                                                                           │
└───────────────────────────────────────────────────────────────────────────┘
```

**Key Insight: The Trivial-to-BIGINT Bridge**

This diagram reveals your implementation philosophy:

1. **Build Trivial First (1..64):** Proves the concept, demonstrates all SSK operations, provides testable reference.
2. **Plan for Extension:** "Allow SSK to yield value beyond original use-case" captures the commitment to generalizability.
3. **Build BIGINT:** Real implementation, but WITH reference to trivial code embedded throughout (via comments, #ifdef, etc.).

**Concept Flow Notes:**
- "Member Domain" **splits** into "1..64 Domain" and "BIGINT Domain" (part-of relationship)
- "Defined input parameters" **splits** by domain (parametric variation)
- Both implementations produce outputs that **join** for documentation (Definitions, Spec, Guides)

---

## A3: BIGINT Domain Detail (The Operational Concerns)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                         │
│  Semantics of Bijection ───────────────────────────────────────────→   │
│  (as "SSK UDT and AGG Definitions")                                 │   │
│                                                                      │   │
│  Member Domain ────────────────────────────────────────────────────→   │
│  (as "Reference Code for Trivial domain")                               │
│                                                                         │
│  Bijection by Canonisation Rules ──────→ ┌──────────────────────────┐  │
│                                           │  BIGINT DB ID SSK       │  │
│                                           │  Codec                  │  │
│                                           │  (A31 detail ref)       │  │
│                                           │                         │  │
│                                           └──────────────────────────┘  │
│  Defined BIGINT input parameters ─┬────→ │ • Encode/Decode       │  │
│  (single ID / ARRAY / aggregate) │       │ • Store Mgmt          │  │
│                                   │       │ • Format 0 Compliance │  │
│                                   │       └──────────────────────────┘  │
│                                   │                │                    │
│                                   │                │ Encoded value      │
│                                   │                │ Decoded SSKDecoded │
│                                   │                │ Format 0 spec      │
│                                   │                ↓                    │
│                                   │       ┌──────────────────────────┐  │
│                                   └──────→│  Abstract Bit Vector    │  │
│                                           │  Operations             │  │
│                                           │  (Map/Reduce)           │  │
│                                           │                         │  │
│                                           └──────────────────────────┘  │
│                                               ↓ ↑ ↑ ↑                   │
│                         Reference from trivial │ │ │ └─ Dirty flags     │
│                                                │ │ │                    │
│                           SSKDecoded pair ─────┘ │ │                    │
│                           (carries both values)  │ │                    │
│                                                  │ │                    │
│                                                  ↓ │    ┌────────────────┐  │
│                                           ┌──────────────────────────┐  │
│                                           │  Fragmentation          │  │
│                                           │  (tracks dirty areas)   │  │
│                                           │                         │  │
│                                           └──────────────────────────┘  │
│                                                  │ ↑                    │
│                                  Fragment list  │ │  (to canonise)      │
│                                                  ↓ │                    │
│                                           ┌──────────────────────────┐  │
│                                           │  Canonisation           │  │
│                                           │  (Defrag)               │  │
│                                           │                         │  │
│                                           └──────────────────────────┘  │
│                                                  │                      │
│                                 Clean SSKDecoded │                      │
│                                                  ↓                      │
│                                           (ready for encoding)           │
│                                                                         │
│  Host DB ───────────────────────────────────────────────────────────   │
│                                                                         │
│  Outputs ───────────────────→                                          │
│   • Calculated values for BIGINT domain                                 │
│   • Decisions based on membership                                       │
│   • CAST results (ARRAY / SETOF BIGINT)                                │
│   • Developer guides + Format spec                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**Key Insight: State Management Through Transformation**

This diagram reveals the operational pipeline:

1. **Codec** (A31): Translate between storage and in-memory forms
2. **Operations** (Map/Reduce): Perform set algebra on decoded form
3. **Fragmentation**: Track dirty regions (result of operations)
4. **Canonisation** (Defrag): Restore to canonical form before re-encoding

**Concept Flow Notes:**
- "SSKDecoded pair" flows from Codec → Operations, carrying both old and new values
- "Dirty flags" signal Fragmentation concern about which regions need defrag
- "Clean SSKDecoded" exits Canonisation, ready for encoding back to storage
- Outputs **join** multiple result types (values, decisions, casts, documentation)

---

## A31-A34: BIGINT Implementation Detail (A3 Decomposition)

This is **Diagram Level 3** showing the complete decomposition of A3 (BIGINT Reference Implementation). It has **four sibling activities**:

### A31: BIGINT DB ID SSK Codec (Orchestrator)

**ICOM:**
- **Inputs:** SSKDecoded * to_encode | SSK Value To Decode
- **Controls:** BIGINT Domain | Bijection by Canonisation Rules
- **Outputs:** Encoded SSK Value | Format 0 | Outer Codec API Reference | SSKDecoded * decoded
- **Mechanisms:** Host DB

### A32: Abstract Bit Vector Operations (Map/Reduce)

**ICOM:**
- **Inputs:** SSKDecoded * decoded | SSKFragments * list_to_process
- **Controls:** Reference Code for Trivial domain | SSK UDT and AGG Definitions
- **Outputs:** SSK Operations Reference | BIGINT result[] | BOOLEAN membership | SSKDecoded * dirty | SSKDecoded Pair
- **Mechanisms:** Host DB

### A33: Fragmentation

**ICOM:**
- **Inputs:** SSKDecoded Pair | BIGINT input[] | BIGINT single
- **Controls:** (none)
- **Outputs:** SSKFragments * list_to_process
- **Mechanisms:** Host DB

### A34: Canonisation (Defrag)

**ICOM:**
- **Inputs:** SSKDecoded * dirty
- **Controls:** Bijection by Canonisation Rules
- **Outputs:** SSKDecoded * clean
- **Mechanisms:** Host DB

---

## A311-A314: Codec Decomposition (A31 Children - THE CRITICAL LEVEL)

**Parent:** A31 (BIGINT DB ID SSK Codec)

This is **Diagram Level 4**, the deepest decomposition, and it contains **most of the key architectural concerns**. A31 decomposes into **four sub-activities:**

### A311: Decoding Store

**Purpose:** Stateless gateway and cache for decoded values

**ICOM:**
- **Inputs:** SSK Value To Decode | Value/Decoding Pair
- **Controls:** (none)
- **Outputs:** SSKDecoded * decoded | Encoded SSK Value | Bytes to Decode
- **Mechanism:** Host DB

**Key Role:** Acts as stateless lookup/memoization layer. Receives an SSK Value and (optionally) a cached pairing, outputs the decoded structure and bytes for further processing. No internal state—pure transformation.

---

### A312: Canonical Data Unit (CDU)

**Purpose:** Implements bijective encoding/decoding per Format 0

**ICOM:**
- **Inputs:** Bytes to Decode | Bits to Decode into Tokens | Token Values to Encode as bits
- **Controls:** BIGINT Domain
- **Outputs:** Format 0 | Bits from Token Values | Token Values from bits
- **Mechanism:** Host DB

**Key Role:** Encodes/decodes individual data units. Bidirectional transform: tokens ↔ bits. This is where the "language" of representation becomes concrete—the variable-length encoding specified by Format 0. Works at the BIGINT Domain granularity.

---

### A313: ABV Generation "Language" Processor

**Purpose:** Orchestrates the abstract bit vector representation and codec pipeline

**ICOM:**
- **Inputs:** Bits from Token Values | Token Values from bits | SSKDecoded * to_encode | SSKDecoded * resized
- **Controls:** BIGINT Domain | Bijection by Canonisation Rules
- **Outputs:** Format 0 | Outer Codec API Reference | Memory Adjustment | SSKDecoded * decoding | BYTEA value | Bits to Decode into Tokens | Token Values to Encode as bits
- **Mechanism:** Host DB

**Key Role:** This is the cognitive core of SSK encoding. It orchestrates:
1. The full encode/decode pipeline
2. State management (dirty → defrag → clean)
3. Memory sizing decisions (signals to A314)
4. Both system outputs (BYTEA, API, documentation) AND internal signals (Bits/Tokens for CDU, Memory Adjustment for manager)

This activity contains the **bijection semantics**—it's where canonisation rules meet Format 0 mechanics.

---

### A314: SSKDecode Memory Manager

**Purpose:** Handles realloc-safe offset-based memory allocation

**ICOM:**
- **Inputs:** SSKDecoded * abandoned | SSKDecoded * undersized
- **Controls:** (none)
- **Outputs:** SSKDecoded * resized
- **Mechanism:** Host DB

**Key Role:** Manages allocation/growth/cleanup of SSKDecoded structures. Uses offset-based pointers (not direct pointers) to make realloc safe. Receives signals from A313 about memory pressure, returns resized structures. Closure of the realloc-safe model.

---

## A311-A314 Data Flow Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│ Diagram A31: BIGINT DB ID SSK Codec (Orchestrator)                 │
│                                                                     │
│  A311: Decoding Store                                               │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ In: SSK Value To Decode, Value/Decoding Pair               │  │
│  │ Out: SSKDecoded* decoded, Encoded SSK Value, Bytes        │  │
│  └──────────────────────────────────────────────────────────────┘  │
│         ↓ (Bytes to Decode, SSKDecoded * decoded)                  │
│         ↓                                                            │
│  A312: Canonical Data Unit (CDU)                                    │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ In: Bytes to Decode, Bits→Tokens, Tokens→Bits              │  │
│  │ Out: Format 0, Bits from Tokens, Tokens from Bits           │  │
│  │ (Bijective transformation at unit level)                    │  │
│  └──────────────────────────────────────────────────────────────┘  │
│         ↓ ↑ (Bits ↔ Tokens, bidirectional)                         │
│         ↓ ↑                                                          │
│  A313: ABV "Language" Processor (Orchestrator)                      │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ In: Bits/Tokens (from CDU), SSKDecoded*/Resized (from Mgr) │  │
│  │ Out: BYTEA, BOOLEAN, API Refs, Memory Adjustment signal    │  │
│  │ (Manages: dirty→canonise→clean, encodes, sizes structures) │  │
│  └──────────────────────────────────────────────────────────────┘  │
│         ↓ (Memory Adjustment signal)                                │
│         ↓                                                            │
│  A314: Memory Manager                                               │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ In: Abandoned/Undersized SSKDecoded*                        │  │
│  │ Out: SSKDecoded* resized (offset-based, realloc-safe)       │  │
│  └──────────────────────────────────────────────────────────────┘  │
│         ↑ ↓ (feedback loop: resized structs back to A313)          │
│         └──┘                                                         │
│                                                                     │
│  Host DB mechanism flows through all four                          │
└─────────────────────────────────────────────────────────────────────┘
```

**Critical Flows:**

1. **A311 → A312 → A313:** Forward direction (decode):
   - SSK Value Decode → Bytes → Bits → Structure generation → BYTEA output

2. **A313 → A312:** Backward direction (encode):
   - Structure (clean) → Tokens → Bits → BYTEA

3. **A313 ↔ A314:** Feedback loop (memory):
   - A313 detects memory pressure → signals A314
   - A314 reallocates → returns resized structures → A313 continues

---

## A311-A314 Concept Breakdown

**Key Concepts Appearing at A311-A314 Level:**

| Concept | A311 | A312 | A313 | A314 |
|---------|------|------|------|------|
| **Format 0** | Output (reference) | Core input/output | Output (spec) | — |
| **Token Values** | — | Bidirectional transforms | Inputs/Outputs | — |
| **Bits** | — | Bidirectional transforms | Inputs/Outputs | — |
| **BYTEA value** | Output | — | Output | — |
| **SSKDecoded *** | Multiple flows | — | Central | Feedback |
| **Dirty/Clean** | — | — | State machine | Clean arrival |
| **Memory Adjustment** | — | — | Output signal | Input trigger |

**Concept Type-of Relationships:**

- **SSKDecoded * states:** clean, dirty, to_encode, resized, undersized, abandoned
  - Dirty: output from operations (A32), input to canonisation (A34)
  - Clean: output from canonisation, ready to encode
  - Resized: output from memory manager, ready for reuse

- **Bits/Tokens flows:** Bidirectional in A312, mediated by A313
  - Encoding: Tokens → Bits
  - Decoding: Bits → Tokens
  - Mediation: A313 decides which direction based on operation (encode vs. decode)

---

## Critical Architectural Insight: A313 is NOT a Pass-Through

**A313 (ABV "Language" Processor)** appears to route data, but it's actually the cognitive core:

1. **It embeds Reference Code for Trivial domain** (as control input from A32)
   - This means the trivial implementation guides A313's logic
   - A313 must be able to explain its decisions in terms of trivial case

2. **It orchestrates the canonisation rules** (control input)
   - It doesn't just call A34; it decides when/how canonisation applies
   - It translates structure states (dirty/clean) into actual semantics

3. **It generates BOTH outputs AND internal signals**
   - Outputs: BYTEA (encoded), BOOLEAN (membership test), API ref (documentation)
   - Signals: Memory Adjustment (to A314), Bits/Tokens (to/from A312)
   - This is orchestration, not pass-through

4. **It closes the state machine**
   - Takes SSKDecoded in various states
   - Outputs them in known states (clean before encode, resized before use)
   - Manages the complete lifecycle

---

## Cross-Diagram Concept Mapping (Key Insight: Renames, Splits, and Flows)

**Concept Trajectory Across All 4 Levels:**

| Concept | A-0 | A0 | A3 | A31-A34 | A311-A314 |
|---------|-----|----|----|---------|-----------|
| **Member Domain** | Control (splits) | "1..64" / "BIGINT" | "BIGINT Domain" | BIGINT Domain (C1) | BIGINT Domain (C1) |
| **Bijection Semantics** | Control | Implicit | Control + Output | Control (C2) | Control (C2) in A313 |
| **Canonisation Rules** | Implicit | Implicit | Explicit control | Explicit control | Explicit control → A313 |
| **Trivial Impl Ref** | Implicit | Implicit (trivial pathway) | Explicit control to A3 | Explicit control to A32 | Inherited (embedded in A313) |
| **Defined Inputs** | Splits by domain | Domain-specific | "BIGINT inputs" | Hidden (SSK Value in) | Hidden (Bytes/Tokens in) |
| **Format 0** | Not visible | Not visible | Output | Output | Output (core in A312) |
| **SSKDecoded *** | Not visible | Not visible | Flows through ops | Multiple states | Central: all states |
| **Bits/Tokens** | Not visible | Not visible | Not visible | Not visible | Core transforms (A312-A313) |

**Concept Splitting (Type-of) and State Progression:**

- **Member Domain** → "1..64 Domain" (trivial), "BIGINT Domain" (target)
  - Bifurcates at A0, A3, A31-A34 follows only BIGINT path
  
- **SSKDecoded *** → Multiple states that form a state machine:
  - Inputs as: `to_encode`, `decoded`, `resized`
  - Internal states: `dirty` (post-operation), `clean` (post-defrag)
  - Outputs as: `resized`, `undersized`, `abandoned`
  - Managed by A314, orchestrated by A313

- **Format 0** → Emerges only at A3 level, becomes central at A311-A314
  - Implicit constraint in A0/A3 organization
  - Explicit output from A31 and A312
  - Specification flows to documentation level

- **SSK Documentation** → Three separate guides emerge from outputs
  - Format Specification (from A31, A3, A1/A2)
  - User's Guide (from A2)
  - Developer's Guide (from A3)
  - Each traces back to its originating concern

---

## Critical Implementation Implications (Revised with A311-A314)

**From This Complete Model Structure (A-0 through A311-A314), I Now See:**

1. **A313 (ABV Language Processor) is the cognitive core of the entire system:**
   - It's not just orchestration; it embeds the reference code for trivial domain
   - It makes decisions about when/how canonisation applies
   - It manages the complete SSKDecoded * lifecycle (all states)
   - This is where "representation" meets "encoding"—the real semantics live here

2. **The codec (A311-A314) is NOT a simple encode/decode box:**
   - A311 (Decoding Store) is stateless memoization
   - A312 (CDU) is just one unit-level transform (bijective)
   - A313 is the orchestrator and decision-maker
   - A314 is the memory safety guarantor
   - Together they implement the full semantics, not separate concerns

3. **Format 0 emerges naturally as the specification for bits/tokens:**
   - Not a separate concept, but the outcome of decomposition
   - Appears at A31 level as an output
   - Becomes central at A312 (CDU implements it)
   - This is the "language" that A313 orchestrates

4. **SSKDecoded * is a state machine with explicit transitions:**
   - Input states: `to_encode`, `decoded`, `resized`
   - Internal states: `dirty` (marked by operations), `clean` (from canonisation)
   - Output states: `resized`, `undersized`, `abandoned`
   - A314 manages allocation; A313 manages semantic transitions

5. **Trivial implementation is embedded (not isolated):**
   - A313 receives "Reference Code for Trivial domain" as control input
   - This means A313 must be able to answer: "For the trivial case, what would this do?"
   - Suggests trivial version is either #ifdef'ed code in A313 or HIGHLY visible reference

6. **Documentation structure follows the concerns:**
   - Format Specification comes from A31 (codec detail)
   - User's Guide comes from A2 (beyond original use-case)
   - Developer's Guide comes from A3 (BIGINT implementation)
   - Each guide is rooted in a specific concern level

---

## Updated Critical Questions (With A311-A314 Visibility)

1. **A313 Embedding of Trivial Reference:**
   - Does "Reference Code for Trivial domain" mean:
     a) Parallel C functions for trivial case (called from A313)?
     b) Commented pseudocode showing trivial logic?
     c) Embedded #ifdef TRIVIAL_SSK sections in A313's code?
     d) All of the above in different contexts?

2. **State Machine Enforcement:**
   - Are the SSKDecoded * state transitions enforced at compile-time (e.g., type system)?
   - Or at runtime (e.g., flags/assertions)?
   - Or documented as a contract that A313/A314 must respect?

3. **A312 (CDU) Scope:**
   - CDU implements Format 0 at the "unit" level. What constitutes a "unit"?
   - Is it a single token-value pair? A batch? Tunable?
   - How does CDU interact with overall SSK value encoding (is CDU one layer of a multi-layer scheme)?

4. **A314 Memory Manager: Offset vs. Pointer:**
   - "Offset-based, realloc-safe" — does this mean:
     a) All internal structure references are offsets from a base pointer?
     b) Or pointers that are patched after realloc?
     c) Or both depending on context?

5. **Feedback Loop Closure (A313 ↔ A314):**
   - A313 sends "Memory Adjustment" signal; A314 returns "Resized structures"
   - Is this a streaming pipeline (process continuously) or batch (collect, resize, continue)?
   - How does this interact with operations (A32) that fragment the structure?

---

## Document Status

- **Reconstruction Complete:** All 4 diagram levels with A311-A314 detail
- **Concept Flows:** Mapped across all levels
- **State Machine:** SSKDecoded * states identified
- **Orchestration:** A313 role clarified as cognitive core
- **Open Questions:** 5 critical questions for user verification

**I have captured the full model depth. The A311-A314 decomposition was essential—it shows that the codec is not monolithic but a tightly orchestrated system where A313 is the real decision-maker, not A312 (CDU).**
