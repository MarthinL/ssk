# SSK: Canonical Encoding of Sparse Abstract Bit Vectors

> **WARNING: Development Implementation Only**  
> The current CDU implementation in the codebase is a development placeholder that does not fully align with the original design intent described below. It is functional for testing and development with disposable data but **MUST NOT** be used in production for Format 0. A proper CDU implementation conforming to this specification will be required before production deployment to ensure canonical bijection and efficiency.

## The Problem

Representing a subset of 64-bit integer IDs requires an **abstract bit vector** (abvector)—conceptually, one bit per possible ID (0 to 2^64-1 ≈ 18.4 exabits). Each bit indicates presence (1) or absence (0) in the subset.

Storing this literally requires **2,048 petabytes** per SSK. Impossible.

Yet real-world subsets are astronomically sparse—like matter in space. A subset of 1,000 IDs out of 2^64 has the same density ratio as a single grain of sand compared to the entire Earth.

## The Solution: Hierarchical Compaction with Canonical Bijection

SSK exploits two fundamental enablers of compaction: **sparsity** (most bits are zero) and **known structure** (exploitable patterns in the data) through hierarchical compaction while maintaining **perfect bijection**:

$$\text{Subset of IDs} \longleftrightarrow \text{Encoded Bytes}$$

- **Same subset → identical bytes** (regardless of construction history)
- **Different subsets → different bytes** (no collisions)
- **One encoding → one subset** (unique decoding)

**This bijection is not just important—it IS the entire SSK concept.** An SSK value contains ALL subset membership information within itself, serving as a stable, unique identifier for a SUBSET—something that has never been given identity before in relational databases.

### Why Bijection Matters: SSK is NOT a Hash

Without canonical bijection, SSKs would be useless:

- **Equality**: Two SSKs with same members encode to identical bytes (enables `memcmp` equality)
- **Hashing**: SSKs can be used as hash keys or database indexes (no collisions)
- **Determinism**: Encoding is reproducible across systems and time
- **Production Safety**: Data stored in Format 0 can be reliably read forever

**But crucially, SSKs are fundamentally different from hashes:**

- **Hashes have clashes**: Same hash value means potential clash (different inputs, same output)
- **SSKs have no clashes**: Same SSK value means identical subsets (bijection guarantees this)
- **Hashes are not reversible**: They don't contain the original data, only a fingerprint
- **SSKs are self-contained and reversible**: The encoded value contains all subset membership information and can be fully decoded back to the exact subset
- **Hashes are lossy**: Information is discarded for efficiency
- **SSKs preserve information**: Perfect reconstruction through defined, stable format management

**Breaking canon breaks SSK.** Everything else is secondary to maintaining this bijection—the foundation that enables subsets to have stable, queryable identities.

## The Hierarchy

The 64-bit ID space is compressed through four levels:

```
Domain (2^64 possible IDs)
  ├─ Partitions (2^32 IDs each)
  │   ├─ Segments (contiguous runs or compressed gaps)
  │   │   ├─ Chunks (64-bit fixed-size pieces)
  │   │   │   └─ Tokens (ENUM/RAW/RAW_RUN encoding)
```

### Level 1: Partitions

**64-bit ID** = `(partition_num: 32 bits high, offset: 32 bits low)`

- Partition 0: IDs 0 to 2^32-1
- Partition 1: IDs 2^32 to 2^64-1
- **Empty partitions are never stored** (saves space for sparse subsets)

### Level 2: Segments

Within a partition, IDs are grouped into **segments**—runs of consecutive or nearly-consecutive IDs where different compaction strategies apply.

Segments are determined by **mandatory split points** at dominant-bit gaps (see Canon Rules).

### Level 3: Chunks

Segments are divided into fixed-size **chunks** (64 bits each, final chunk may be partial).

Each chunk is a mini-abvector. Within the chunk:
- Bit position = chunk-relative offset
- Bit value = presence/absence

### Level 4: Tokens

Each chunk is encoded as a **token**—the smallest efficient representation:
- **ENUM**: Sparse chunks (few 1-bits) → combinadic rank encoding
- **RAW**: Dense chunks (many 1-bits) → raw 64-bit value
- **RAW_RUN**: Consecutive RAW chunks → coalesced for efficiency
- **ENUM_RUN**: Consecutive identical ENUM chunks → coalesced for efficiency

## Encoded Format

An encoded SSK is a sequence of CDU encoded integers and bit fields:

```
[format_version]                    CDU:SSK_FORMAT (CDU_DEFAULT)

For each partition (ascending order):
  [partition_delta]                 CDU:SSK_PARTITION_DELTA (CDU_LARGE_INT): (H_i - H_{i-1} - 1)
  [n_segments]                      CDU:SSK_N_SEGMENTS (CDU_SMALL_INT)
  
  For each segment (ascending offset order):
    [seg_kind_tag]                  1 bit: 0=RLE, 1=MIX
    [initial_delta]                 CDU:SSK_SEGMENT_START_BIT (CDU_INITIAL_DELTA): gap from previous segment
    [length_bits]                   CDU:SSK_SEGMENT_N_BITS (CDU_MEDIUM_INT): segment length in bits
    
    If RLE:
      [membership_bit]              1 bit: the repeated bit value (0 or 1)
    
    If MIX:
      [token_stream]                Sequence of tokens (last_chunk_nbits derivable from length_bits):
        
        ENUM token:
          [token_tag]               2 bits: 00
          [k]                       6 bits: popcount (n_bits_for_k)
          [rank]                    variable: combinadic rank (rank_bits for n,k)
        
        RAW token:
          [token_tag]               2 bits: 01
          [bits]                    n bits: raw chunk bits
        
        RAW_RUN token:
          [token_tag]               2 bits: 10
          [run_len]                 CDU:SSK_RAW_RUN_LEN (CDU_SMALL_INT): number of chunks
          [bits]                    run_len * 64 + optional final bits
        
        ENUM_RUN token:
          [token_tag]               2 bits: 11
          [run_len]                 CDU:SSK_RAW_RUN_LEN (CDU_SMALL_INT): number of identical chunks
          [k]                       6 bits: popcount
          [rank]                    variable: combinadic rank (once, repeated)
```

### Example: Encoding `{5, 10, 15}`

All IDs in partition 0 (high 32 bits = 0):

```
format_version = 0
partition_id = 0                   // partition_delta = 0 (first)
n_segments = 1
segment_type = MIX                 // Not a pure rare run
start_bit = 5                      // initial_delta = 5
n_bits = 11                        // bits for IDs 5-15
rare_bit = 1 (3 ones in 11 bits)

last_chunk_nbits = 11              // Single chunk, 11 bits
token_type = ENUM                  // k=3 ≤ 18
k = 3, rank = combinadic_rank(abits_5_15, 11, 3)
```

Result: ~10 bytes instead of 2,048 petabytes.

## Canon Rules: Deterministic Encoding Decisions

**Canon is maintained through deterministic rules with NO encoder discretion.** Every encoding choice is predetermined by the abvector itself and the canon parameters.

### Rule 1: Partition Ordering
- Partitions are encoded in **strictly ascending** order by partition ID
- Gap encoding: `partition_delta = (H_i - H_{i-1} - 1)`
- Empty partitions are **never encoded** (skipped entirely)

**Canon requirement:** If encoder could choose partition order, same subset might encode differently. Strict ascending order + gap encoding ensures one encoding per subset.

### Rule 2: Segment Ordering
- Segments within partitions are encoded in **strictly ascending** order by initial offset
- Gap encoding: `initial_delta = (initial_i - (prev_initial + prev_length))`

**Canon requirement:** Reordering segments would produce different bytes for same abvector. Strict order ensures bijection.

### Rule 3: Segment Boundaries (Mandatory Split Points)

Segments are split at **dominant-bit gaps** ≥ `DOMINANT_RUN_THRESHOLD`.

**Algorithm:**
```c
for (offset = 0; offset < partition_size; offset++) {
    // Identify next contiguous span with mixed bits
    span_start = find_next_mixed_region(offset);
    if (span_start >= partition_size) break;
    
    // Scan for dominant-bit gaps >= threshold
    abits = extract_abits(span_start, ...);
    popcount = count_ones(abits);
    dominant_bit = (popcount > span_length / 2) ? 1 : 0;
    
    // Split at dominant gaps >= threshold
    for each maximal run of dominant_bit:
        if (run_length >= DOMINANT_RUN_THRESHOLD):
            // Segment boundary BEFORE this gap
            emit_segment(span_start, gap_start - span_start);
            span_start = gap_start + run_length;
}
```

**Why this rule ensures canon:**
- If encoder could choose segment boundaries arbitrarily, same abvector could split differently
  ```
  Example:
  IDs:    [100-199] gap(100 zeros) [300-399]
  Option A: Two segments [100-199] and [300-399]
  Option B: One segment [100-399] with 100-zero gap
  → Different bytes for same subset!
  ```
- Mandatory split at gaps ≥ 96 bits ensures exactly one valid segmentation

**Special case:** Gaps < 96 bits are included in segment, not split. This is deterministic—no encoder choice.

### Rule 4: Segment Kind Selection (RLE vs MIX)

```c
uint64_t popcount = count_ones(segment_abits);
uint8_t dominant_bit = (popcount > segment_length / 2) ? 1 : 0;
uint8_t rare_bit = ~dominant_bit & 1;

uint64_t rare_run_length = longest_consecutive_run(segment_abits, rare_bit);

if (rare_run_length >= RARE_RUN_THRESHOLD && 
    rare_run_length == segment_length) {
    // Pure rare-bit run: use RLE
    segment_kind = RLE;
    encode_rle(rare_bit);
} else {
    // Mixed bits or dominant-heavy: use MIX
    segment_kind = MIX;
    encode_mix_tokens(segment_abits);
}
```

**Canon requirement:**
- If encoder could choose RLE vs MIX arbitrarily:
  ```
  Example: 100 consecutive 1-bits
  Option A: RLE segment (stores as "rare_bit=0, run_length=100")
  Option B: MIX segment (stores as 2 RAW tokens)
  → Different bytes for same span!
  ```
- Algorithm ensures exactly one choice per segment

### Rule 5: Chunk Boundaries

Segments are divided into fixed-size **64-bit chunks** (final chunk may be partial):

```c
for (offset = segment_start; offset < segment_end; offset += CHUNK_BITS) {
    chunk_nbits = min(CHUNK_BITS, segment_end - offset);
    process_chunk(chunk_bits, chunk_nbits);
}
```

**Canon requirement:** Fixed chunk size ensures all encoders split the same way.

**Last chunk handling:**
- May be 1-63 bits
- Derivable from `length_bits % 64` (0 means 64)
- NOT stored separately to prevent inconsistency
- Still processed with same token rules

### Rule 6: Token Kind Selection (ENUM vs RAW)

```c
uint8_t k = popcount(chunk);

if (k <= K_CHUNK_ENUM_MAX) {
    // Sparse: use combinadic encoding
    token_kind = ENUM;
    uint8_t rank_bits = ssk_get_rank_bits(nbits, k);
    uint64_t rank = combinadic_rank(chunk, nbits, k);
    encode_bits(k, N_BITS_FOR_K);   // 6 bits for k
    encode_bits(rank, rank_bits);   // variable bits for rank
} else {
    // Dense: use raw bits
    token_kind = RAW;
    encode_raw(chunk, nbits);
}
```

**Canon requirement:**
- Threshold `K_CHUNK_ENUM_MAX = 18` is **immutable** per format
- If encoder could choose the threshold, same chunk might encode differently
- Combinadic encoding is space-efficient for k ≤ 18; raw bits for k > 18
- Fixed threshold ensures deterministic choice

**Why 18?**
- Combinadic rank for k=18, n=64 requires ~24 bits
- Combined with k (6 bits) → ~30 bits total
- Raw 64 bits for k > 18 is more efficient
- This threshold minimizes total encoded size for typical distributions
- **Once chosen for Format 0, it is immutable**

### Rule 7: Token Run Coalescing (Mandatory)

**Consecutive identical tokens must be coalesced into runs:**

1. **RAW_RUN**: Two or more consecutive RAW tokens → single RAW_RUN
2. **ENUM_RUN**: Two or more consecutive ENUM tokens with identical k and rank → single ENUM_RUN

```c
i = 0;
while (i < n_chunks) {
    uint8_t k = popcount(chunks[i]);
    bool is_raw = (k > K_ENUM_MAX);
    
    if (is_raw) {
        // Count consecutive RAW chunks
        run = 1;
        while (i + run < n_chunks && popcount(chunks[i + run]) > K_ENUM_MAX)
            run++;
        
        if (run >= 2) {
            emit(RAW_RUN, run);
            emit_raw_bits(chunks[i .. i+run-1]);  // contiguous
        } else {
            emit(RAW);
            emit_raw_bits(chunks[i]);
        }
        i += run;
    } else {
        // ENUM: check for identical consecutive chunks
        uint64_t rank = combinadic_rank(chunks[i], 64, k);
        run = 1;
        while (i + run < n_chunks) {
            uint8_t next_k = popcount(chunks[i + run]);
            if (next_k > K_ENUM_MAX) break;  // switches to RAW
            uint64_t next_rank = combinadic_rank(chunks[i + run], 64, next_k);
            if (next_k != k || next_rank != rank) break;  // different ENUM
            run++;
        }
        
        if (run >= 2) {
            emit(ENUM_RUN, run, k, rank);  // one k+rank for all
        } else {
            emit(ENUM, k, rank);
        }
        i += run;
    }
}
```

**Canon requirement:**
```
Before coalescing: [ENUM] [RAW] [RAW] [RAW] [ENUM]
Option A: [ENUM] [RAW] [RAW] [RAW] [ENUM] (uncoalesced)
Option B: [ENUM] [RAW_RUN(3)] [ENUM] (coalesced)
→ Different bytes for same segment!

Before coalescing: [ENUM(k=2,r=5)] [ENUM(k=2,r=5)] [ENUM(k=2,r=5)]
Option A: Three separate ENUM tokens (uncoalesced)
Option B: [ENUM_RUN(3, k=2, r=5)] (coalesced)
→ Different bytes for same segment!
```

Mandatory coalescing ensures only the coalesced form is valid.

**Why ENUM_RUN is essentially free:**
- We already compute k (popcount) and rank for the first chunk
- Checking if next chunk matches is just integer comparison
- Patterns repeating exactly 64 bits apart are detected at near-zero cost
- Common in real data: repeated bitmasks, periodic patterns

### Rule 8: CDU Minimality

Every CDU encoded value must use the **minimum number of continuation bits** required:

```c
// Example: stage_bits = {2, 5, 8}
value = 3;
max_stage_0 = 2^2 - 1 = 3;        // Fits in stage 0
✓ Encode as: 2 bits (no continuation)
✗ Encode as: 2 + 5 bits (continuation) → NON-CANONICAL

value = 5;
max_stage_0 = 3;                  // Doesn't fit
needs_continuation = true;
max_stage_1 = 2^(2+5) - 1 = 127;  // Fits in stage 1
✓ Encode as: 2 + 5 bits (1 continuation)
✗ Encode as: 2 bits only → IMPOSSIBLE
✗ Encode as: 2 + 5 + 8 bits → NON-CANONICAL
```

**Decoder validation:**
```c
bool is_minimal(cdu_bytes, value, cdu_params) {
    // Check if value would fit in fewer stages
    for (stage = 0; stage < stages_used - 1; stage++) {
        max_value = (1ULL << stage_bits_sum[stage]) - 1;
        if (value <= max_value) {
            return false;  // Non-minimal!
        }
    }
    return true;
}
```

**Canon requirement:** Non-minimal encoding would allow different byte sequences for same value.

### Rule 9: Bit Ordering (LSB-First)

Within each chunk, bits are ordered **LSB-first** (least significant bit = lowest ID):

```
Chunk covering IDs [N, N+63]:
  Bit 0  (LSB) = presence of ID N
  Bit 1        = presence of ID N+1
  ...
  Bit 63 (MSB) = presence of ID N+63
```

**Canon requirement:** Different bit orders would produce different bytes for same subset.

## Canon Validation (Decoder Responsibility)

Decoders **MUST** validate all canon rules and reject violations:

```c
bool validate_canon(encoded_bytes, len, format_spec) {
    // 1. Partition ordering
    prev_partition_num = -1;
    for each partition:
        if (partition_num <= prev_partition_num)
            return false;  // Out of order
        prev_partition_num = partition_num;
    
    // 2. Segment ordering
    for each partition:
        prev_end = 0;
        for each segment:
            if (segment.offset < prev_end)
                return false;  // Out of order or overlapping
            prev_end = segment.offset + segment.length;
    
    // 3. CDU minimality
    for each cdu_value:
        if (!is_minimal(value, cdu_params))
            return false;
    
    // 4. Segment kind correctness
    for each segment:
        expected_kind = compute_segment_kind(segment.abits, format_spec);
        if (segment.kind != expected_kind)
            return false;
    
    // 5. Token kind correctness
    for each MIX segment:
        for each chunk:
            k = popcount(chunk);
            expected_kind = (k <= K_ENUM_MAX) ? ENUM : RAW;
            if (token.kind != expected_kind && token.kind != RAW_RUN)
                return false;
    
    // 6. RAW coalescing
    for each MIX segment:
        if (has_consecutive_raw(segment))
            return false;  // Uncoalesced
    
    // 7. Flag consistency
    for each segment:
        if (segment.rare_bit != (~segment.dominant_bit & 1))
            return false;
    
    // 8. No empty spans
    for each segment:
        if (segment.length == 0)
            return false;
    
    // 9. Bounds
    for each segment:
        if (segment.offset + segment.length > partition_size)
            return false;
    
    return true;
}
```

**Error handling:**
- Canon violations → `ERRCODE_DATA_CORRUPTED` (indicates corruption or tampering)
- Not a format error (that would be `ERRCODE_INVALID_BINARY_REPRESENTATION`)
- SSKs are immutable; non-canonical encodings should never occur in practice

## Format Identification and Versioning

### Format Version Field

Every encoded SSK begins with a **format version** encoded as CDU:

```c
[format_version]  CDU using CDU_DEFAULT parameters
```

This enables:
1. **Forward compatibility**: Decoders can identify format and apply correct rules
2. **Backward compatibility**: Support multiple formats simultaneously
3. **Format evolution**: When production data + demonstrated need emerges, introduce Format 1+

### Design Principles

1. **Decode all supported formats**: Implementation can read any format version it knows about
2. **Encode to specified format**: Can re-encode to original format or different supported format
3. **Default encoding format**: New SSKs are encoded using `SSK_DEFAULT_ENCODING_FORMAT`
4. **Format changes are rare**: Only during major upgrades with bulk re-encoding strategy

### Format 0 Status

**Format 0 is intended to remain the default indefinitely.** Only a critical flaw justifies introducing Format 1. The goal is **ONE FORMAT, FOREVER**.

### Future Format Evolution

When introducing Format 1+ (if needed after production data emerges):

1. Define new canon parameters in new `SSKFormatSpec` structure
2. Document new format specification with all 9 rules applied to new parameters
3. Implement separate encode/decode paths for new format
4. **Never mix formats in operations** (equality, hashing, etc.)
5. Provide bulk re-encoding tool if needed for format upgrade

**Cross-format comparison:**

```c
bool ssk_equals(SSK *a, SSK *b) {
    // Require same format (or convert first)
    if (a->format_version != b->format_version) {
        ereport(ERROR, (errmsg("Cannot compare SSKs with different formats")));
    }
    return memcmp(a->bytes, b->bytes, a->len) == 0;
}
```

## CDU: Parameterized Variable-Length Encoding

SSK uses **CDU** (Canonical Data Unit) for all variable-length integer fields.

### CDU Definition

```c
typedef struct CDUParam {
    uint8_t n_stages;           // Number of extension stages
    uint8_t stage_bits[16];     // Bit count for each stage
} CDUParam;
```

### Encoding Process

```c
uint64_t cdu_encode(uint64_t value, const CDUParam *params) {
    uint64_t result = 0;
    int bit_pos = 0;
    int stage = 0;
    
    while (stage < params->n_stages) {
        uint8_t bits_in_stage = params->stage_bits[stage];
        uint64_t mask = (1ULL << bits_in_stage) - 1;
        
        if (value <= mask || stage == params->n_stages - 1) {
            // Fits in this stage: no continuation bit
            result |= (value << bit_pos);
            return result;
        }
        
        // Doesn't fit: add continuation bit and extend
        result |= ((1ULL << bits_in_stage) | value) << bit_pos;
        bit_pos += bits_in_stage + 1;
        value >>= bits_in_stage;
        stage++;
    }
    
    return result;  // Overflow
}
```

### Example: stage_bits = {2, 5, 8}

| Value Range | Encoding | Stages |
|-------------|----------|--------|
| 0-3 | 2 bits, no continuation | 0 |
| 4-131 | 2 bits (1 continuation) + 5 bits | 1 |
| 132-32,899 | 2 bits + 5 bits (1 continuation) + 8 bits | 2 |

### CDU Subtypes for Format 0

Each encoded field has a specific CDU subtype, chosen to minimize total encoded size for typical value distributions:

**Field → Subtype assignments** (frozen for Format 0):
- `format_version` → `CDU_DEFAULT` (must encode 0 in minimal space)
- `partition_delta` → `CDU_LARGE_INT` (0 frequent, large gaps occasional)
- `segment_count` → `CDU_SMALL_INT` (typically 1-10)
- `initial_delta` → `CDU_INITIAL_DELTA` (0 frequent, gaps up to 2^32)
- `length_bits` → `CDU_MEDIUM_INT` (typically 64-2048)
- `enum_combined` → `CDU_LARGE_INT` (wide range depending on k and rank)
- `raw_run_len` → `CDU_SMALL_INT` (typically 2-100)

These assignments are **immutable per format**. Changing any assignment breaks canon → requires new format version.

## Format 0 Canon Parameters (Frozen)

These parameters define Format 0 encoding rules. They are **IMMUTABLE** and captured in code:

### Structural Parameters
```c
#define PARTITION_SIZE_BITS     32      // 2^32 IDs per partition
#define CHUNK_BITS              64      // Fixed chunk size
```

### Encoding Decision Parameters
```c
#define K_CHUNK_ENUM_MAX        18      // Max popcount for combinadic encoding
#define DOMINANT_RUN_THRESHOLD  96      // Min bits to split at dominant gap
#define RARE_RUN_THRESHOLD      64      // Min bits for RLE segment
#define MAX_SEGMENT_LEN_HINT    2048    // Soft limit for MIX segment splitting
```

### Rank Encoding
```c
#define N_BITS_FOR_K            6       // Bits to encode k in ENUM
// RANK_BITS[n][k] = ceil(log2(C(n,k)))  // Precomputed table (65 × 19 array)
```

### Bit Ordering
```c
#define CHUNK_BIT_ORDER         LSB_FIRST  // Least significant bit = lowest ID
```

### Why These Specific Values?

**K_CHUNK_ENUM_MAX = 18:**
- Combinadic rank for k=18, n=64 requires ~24 bits
- With k encoding (6 bits): total ~30 bits
- Raw 64 bits for k > 18 is more efficient
- Balances encoding efficiency with rank table size (65×19 precomputed values)

**DOMINANT_RUN_THRESHOLD = 96:**
- Represents a heuristic balance point
- Gaps < 96 bits of dominant bit: include in segment (saves space)
- Gaps ≥ 96 bits: split segments (enables independent compaction)
- Chosen via empirical profiling; frozen after validation

**RARE_RUN_THRESHOLD = 64:**
- Pure rare-bit runs ≥ 64 bits are more efficient as RLE
- Shorter runs mixed with other bits: use MIX encoding
- Balances token overhead vs compaction efficiency

**CHUNK_BITS = 64:**
- Natural fit for 64-bit machines (no bit packing overhead)
- Combinadic tables precomputed for all n ≤ 64
- Balances memory locality with compaction granularity

These values are **educated guesses**, refined during development. Once frozen for Format 0 production, they become **immutable forever**. Any change requires Format 1.

## Decoded Memory Structures

Encoded SSKs are decoded into memory structures optimized for lookup and modification:

```c
typedef struct SSKDecoded {
    uint16_t format_version;        // Format identifier
    uint8_t  rare_bit;              // Global rare bit (0 or 1)
    uint8_t  _pad1;
    uint32_t n_partitions;          // Number of non-empty partitions
    uint32_t var_data_off;          // Byte offset from partition_offs[0] to var data
    uint32_t var_data_used;         // Bytes used in var data area
    uint32_t var_data_allocated;    // Bytes allocated for var data area
    uint32_t _pad2;
    uint64_t cardinality;           // Total count of set bits
    uint32_t partition_offs[];      // Flexible array member (offsets to partitions)
} SSKDecoded;
```

**Why offsets, not pointers?**
1. **Smaller**: 32 bits vs 64 bits on modern systems
2. **Realloc-safe**: When memory grows, offsets remain valid but pointers become invalid
3. **Serializable**: Offsets can be written/read directly; pointers cannot

**Memory layout:**
```
[Single contiguous allocation]
  ├─ SSKDecoded header (32 bytes)
  ├─ partition_offs[n_partitions] offset array (FAM)
  └─ Variable-length partition/segment/chunk data
      ├─ SSKPartition[n_partitions]
      │   ├─ Partition header (28 bytes)
      │   ├─ segment_offs[n_segments] offset array (FAM)
      │   └─ SSKSegment[n_segments]
      │       ├─ Segment header (24 bytes)
      │       └─ data[] (FAM) - contains chunk metadata and block bitmaps
```

Offset arrays enable O(1) access and realloc-safe growth; contiguous layout improves cache locality.

## Testing Canon Invariants

Canon must be verified through testing:

```c
void test_canon_bijection() {
    // Create subset via different construction paths
    SSK *ssk1 = create_from_ids({5, 10, 15, 20});
    SSK *ssk2 = union(create({5, 15}), create({10, 20}));
    SSK *ssk3 = except(create({1,5,10,15,20,25}), create({1, 25}));
    
    // All must encode to IDENTICAL bytes
    assert(ssk_bytes_equal(ssk1, ssk2));
    assert(ssk_bytes_equal(ssk1, ssk3));
    
    // Hash must match
    assert(ssk_hash(ssk1) == ssk_hash(ssk2));
    assert(ssk_hash(ssk1) == ssk_hash(ssk3));
    
    // Round-trip decode must recover identical subset
    assert(ssk_members_equal(ssk1, decode(encode(ssk1))));
}

void test_canon_rejection() {
    // Create intentionally non-canonical encodings
    uint8_t bad_partition_order[] = {...};   // Partitions out of order
    uint8_t bad_segment_order[] = {...};     // Segments out of order
    uint8_t bad_cdu_minimal[] = {...};       // Non-minimal CDU
    uint8_t bad_segment_kind[] = {...};      // Wrong RLE vs MIX
    uint8_t bad_token_kind[] = {...};        // Wrong token kind
    uint8_t bad_raw_uncoalesced[] = {...};   // Uncoalesced RAW tokens
    
    // Decoder must REJECT all with ERRCODE_DATA_CORRUPTED
    assert_error(decode(bad_partition_order), ERRCODE_DATA_CORRUPTED);
    assert_error(decode(bad_segment_order), ERRCODE_DATA_CORRUPTED);
    assert_error(decode(bad_cdu_minimal), ERRCODE_DATA_CORRUPTED);
    assert_error(decode(bad_segment_kind), ERRCODE_DATA_CORRUPTED);
    assert_error(decode(bad_token_kind), ERRCODE_DATA_CORRUPTED);
    assert_error(decode(bad_raw_uncoalesced), ERRCODE_DATA_CORRUPTED);
}
```

## Summary: From Sparse Abstract Bit Vector to Canonical Bytes

1. **Problem**: 2,048 petabytes per SSK (literal bitstring storage)
2. **Solution**: Hierarchical compaction + canonical bijection
3. **Hierarchy**: Partitions → Segments → Chunks → Tokens
4. **Canon**: 9 deterministic rules ensure bijection
5. **Versioning**: Format identifier + immutable per-format parameters
6. **Production Safety**: Format 0 frozen forever; all data readable indefinitely
7. **Validation**: Decoders enforce all canon rules; violations = corruption signal

**The central principle: A given subset has exactly one valid encoding under a given format version. This bijection is not just a feature—it is the foundation of SSK.**

