# SSK Decision Log

Technical decisions made during SSK development. Each entry includes rationale and affected components.

---

## 2025-11-30: Bit Ordering (Little-Endian)

**Decision:** Changed from MSB-first (big-endian) to LSB-first (little-endian).

**Previous:** ID 0 → bit 63 (MSB), ID 63 → bit 0 (LSB)  
**New:** ID 0 → bit 0 (LSB), ID 63 → bit 63 (MSB)

**Rationale:**
- CTZ (count trailing zeros) directly gives lowest set ID - no conversion needed
- Growing sets extend toward MSB - natural for variable-length encoding
- CPU bit operations align with semantic meaning (bit position = element ID)
- Simpler mental model: bit N represents ID N
- Aligns with PostgreSQL's `bitmapset.c` which uses little-endian for the same reasons

**Important distinction:**

SSK encodes set membership based on an abstract bit vector (Knuth's term), NOT a SQL bitstring:

- SQL bitstrings are human-readable literal strings, hence big-endian (left-to-right reading)
- PostgreSQL's `varbit` type follows the SQL standard's human-readable semantics
- SSK's abstract bit vector is fundamentally different - a mathematical set membership structure
- The abstract bit vector is never fully realized in memory (would be 2^64 bits = 2 exabytes)
- Cannot be represented as a human-readable string (adding ~0ULL would need 8 exabytes to print)
- SSK only ever exists as compacted binary encoding

Using big-endian would have conflated SQL bitstring semantics (human readability) with bit vector semantics (machine efficiency).

**Files updated:**
- `include/bitstream.h` - All bit position logic, block masking, CLZ↔CTZ swap
- `SPECIFICATION.md` - Rule 9 and bit ordering sections

---

## 2025-11-30: Codec Validation Strategy

**Decision:** Hand-craft test structures with verbose debug tracing before building production encoder/decoder.

**Approach:**
1. Hand-craft SSKDecoded structures - test cases covering sparse, dense, RLE runs, MIX segments, multiple partitions, edge cases
2. Encode with verbose debug trace - human-readable output showing all encoding decisions and bit patterns
3. Manual validation - human confirms encoding decisions and bit patterns are correct
4. Capture golden test vectors - validated encodings become expected results
5. Unit tests - encode hand-crafted input → compare against golden output
6. Decode with similar tracing - verify bit interpretation at each step
7. Round-trip identity - decode golden → re-encode → must be byte-identical (canonical form)

**Rationale:**
- Forces all code paths with known inputs
- Makes bugs visible in trace output
- Creates regression tests from validated output
- Proves canonical form works via round-trip identity

---

## 2025-12-01: Token Type vs Chunk Metadata (2-bit Semantics)

**Decision:** Use different 2-bit semantics for encoded tokens vs in-memory chunk metadata.

**Encoded format (token_tag, 2 bits):**
- `00` = ENUM - single chunk with combinadic rank
- `01` = RAW - single chunk with raw bits
- `10` = RAW_RUN - consecutive RAW chunks (run_len + contiguous bits)
- `11` = ENUM_RUN - consecutive identical ENUM chunks (run_len + shared k/rank)

**In-memory format (chunk metadata, 2 bits):**
- Bit 0: chunk_type (0=ENUM, 1=RAW)
- Bit 1: dirty_flag (0=clean, 1=needs normalization)

**Rationale:**

*Encoded:* Variable-length tokens with run compression reduce repeated metadata. RAW_RUN coalesces consecutive RAW chunks into contiguous bits. ENUM_RUN identifies consecutive chunks with identical combinadic parameters.

*Decoded:* Array semantics for O(1) indexed access. Every chunk has its own metadata slot. RUN information is implicit:
- RAW runs = consecutive RAW flags
- ENUM runs = consecutive ENUM flags with identical block values

The dirty flag tracks chunks affected by operations (insertions, deletions) that may violate normalization invariants. Deferred normalization processes only dirty chunks.

**Files affected:**
- `include/ssk_decoded.h` - chunk_meta_pack(), chunk_meta_type(), chunk_meta_dirty()
- `src/codec/chunks/chunk_token.c` - token encoding/decoding
- `SPECIFICATION.md` - encoding format documentation

---

## 2025-12-01: Single Source of Truth for Segment Length

**Decision:** Store only `length_bits` (total bit span); derive chunk count and last_chunk_nbits.

**Previous:** MIX segment header included 6-bit `last_chunk_nbits` field

**New:** No `last_chunk_nbits` in encoded format - derivable from `length_bits`:
- `n_chunks = (length_bits + 63) / 64`
- `last_chunk_nbits = length_bits % 64` (0 means 64)

**Rationale:**

Storing redundant values (total bits AND final chunk size) creates potential inconsistency:
1. Values can become incongruent through bugs
2. No single source of truth - which to trust?
3. Either fail (unacceptable) or pick one and update others (fragile)
4. If never checked, two code paths may use different values

Integer division is deterministic. Given span L and chunk size S:
- Final chunk bits: `L % S` (0 means S)
- Chunk count: `(L + S - 1) / S`

There is NO uncertainty when deriving from a single source.

**Files affected:**
- `src/codec/segment.c` - mix_segment_header_encode/decode
- `SPECIFICATION.md` - encoding format
- `include/ssk_decoded.h` - segment_n_chunks(), segment_last_chunk_nbits() helpers

---

## 2025-12-02: VLQ-P Implementation Status

**Decision:**  
a) Acknowledge that the current VLQ-P implementation does not match the original design intent.  
b) Defer proper VLQ-P implementation until the entire project is fleshed out and tested, then rework VLQ-P and assess its impact on the whole project as a focused exercise.

**Rationale:**  
The current VLQ-P code is functional for development and testing with disposable data, allowing progress on other components without delay. However, it deviates from the intended parameterized encoding scheme, potentially affecting efficiency and canon compliance. Reworking it now would disrupt ongoing development; better to complete the full project first, then refactor VLQ-P holistically to ensure compatibility and performance.

**Important notes:**  
- Current implementation is marked as development-only in SPECIFICATION.md.  
- Must not be used in production for Format 0.  
- Future rework will require updating encoding/decoding logic, testing canon bijection, and possibly adjusting format parameters.

**Files affected:**  
- `src/codec/vlq_p.c` (current implementation)  
- `SPECIFICATION.md` (warning added)  
- `test/unit/tests/test_hand_crafted.c` (auditable expected results documented in source comments)  
- Future: Any and all tests and codec components using VLQ-P

---
