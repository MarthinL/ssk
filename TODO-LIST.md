# SSK Implementation TODO List

**Status:** Ready for implementation (constraints locked 18 Dec 2025)  
**Target:** Functional PostgreSQL demo (not production)  
**Last Updated:** 18 December 2025

---

## Task Checklist

- [ ] **Task 1:** Implement memory functions (`ssk_decoded_alloc`, `ssk_decoded_grow`, `ssk_decoded_free`)
- [ ] **Task 2:** Implement keystore (stateless decode-only)
- [ ] **Task 3:** Integrate hierarchy into aggregate sfunc/finalfunc
- [ ] **Task 4:** Write unit tests for all modules
- [ ] **Task 5:** Validate PostgreSQL demo end-to-end

---

## Implementation Constraints (Locked)

**Codec:** CDU only (no VLQ-P)  
**Keystore:** Always decode, no caching assumptions  
**Format 0:** Parameters remain tunable (CHUNK_BITS=64, K_CHUNK_ENUM_MAX=18, etc.) but finalized for this demo  
**Target Environment:** PostgreSQL demo (not production)  
**Error Handling:** Debug-mode hard failures (asserts fail, no recovery from memory exhaustion or malformed input)  
**Success Criteria:** Working UDT/AGG in PostgreSQL with unit tests per module demonstrating correct encoding/decoding round-trips

---

## Key Assumptions

### Assumption 1: Hierarchy Navigation is Mechanical
The hierarchical offset-based structure in [include/ssk_decoded.h](include/ssk_decoded.h) is complete and correct. Navigation (finding/creating partitions and segments) follows straightforward offset math without hidden edge cases.

**Implication:** Implementation proceeds directly to adding values without redesigning the hierarchy.

### Assumption 2: Memory Model is Finalized
The realloc-safe offset-based memory model is canonical. All offsets are relative, and growth is via new allocation with offset relocation.

**Implication:** No runtime pointer validation or dual-representation needed.

### Assumption 3: CDU Codec is Stable
The CDU implementation in [src/codec/cdu.c](src/codec/cdu.c) is tested and ready (workshops exist, unit tests pass).

**Implication:** Codec integration requires only calling existing `cdu_encode()` and `cdu_decode()` functions.

### Assumption 4: PostgreSQL Integration is Scaffolded
The UDT stubs in [src/udt/ssk_udt.c](src/udt/ssk_udt.c) and aggregate stubs in [src/agg/ssk_agg.c](src/agg/ssk_agg.c) are sufficient; only the core logic needs implementation.

**Implication:** No need to rewrite I/O signatures or PostgreSQL boilerplate.

### Assumption 5: No Runtime Format Negotiation
Format 0 is hard-coded; no multi-format codec logic is needed. All values encode/decode to Format 0.

**Implication:** Implementation targets a single encoding path, simplifying the aggregate state machine.

### Assumption 6: Sparse Hierarchies Are Acceptable
The hierarchy stores only non-empty partitions and segments. Empty partitions/segments are not pre-allocated.

**Implication:** Aggregate add logic must handle creating new partitions/segments on-demand and managing growth.

---

## Pre-Planning Detail per Task

### Task 1: Implement Memory Functions

**File:** `src/codec/memory.c` (new) + updates to [include/ssk_decoded.h](include/ssk_decoded.h)

**Scope:** ~100 lines of C code

**Declarations to implement:**
```c
SSKDecoded *ssk_decoded_alloc(uint16_t format_version, size_t initial_size);
SSKDecoded *ssk_decoded_grow(SSKDecoded *dec, size_t needed_bytes);
void ssk_decoded_free(SSKDecoded *dec);
```

**Design Details:**

1. **`ssk_decoded_alloc(format_version, initial_size)`**
   - Allocate single contiguous block (via `palloc` or `malloc`)
   - Initialize header fields: `format_version`, `alloc_size` (track for realloc), `used_size` (bytes written so far), `num_partitions`
   - Set `partition_offs` pointer to first offset array position (right after header)
   - Initialize `partition_offs[0] = 0` (sentinel, unused; real partitions start at index 1)
   - Return pointer to SSKDecoded root

2. **`ssk_decoded_grow(dec, needed_bytes)`**
   - Check: if `used_size + needed_bytes <= alloc_size`, return `dec` unchanged (space available)
   - If growth needed: calculate new size (double previous or use `alloc_size + needed_bytes + margin`)
   - Allocate new block, copy old data, update all internal offsets (realloc-safe: offsets survive)
   - Free old block
   - Update `alloc_size` in header
   - Return new SSKDecoded pointer (may differ from input)
   - **Critical:** Caller must replace `dec` variable with return value

3. **`ssk_decoded_free(dec)`**
   - Deallocate single block via `pfree` or `free`
   - No complex tree traversal (single allocation)

**Test Strategy:**
- Unit test: allocate, check header fields
- Unit test: grow multiple times, verify no data loss
- Unit test: realloc stress (1000 grows), verify offsets remain valid
- Unit test: free, no memory leak

**Edge Cases to Handle:**
- Initial size too small (raise error via assert)
- Growth requested when already at max (assert in debug mode, fail hard)
- Zero-byte growth request (allowed, no-op)

---

### Task 2: Implement Keystore (Stateless Decode)

**File:** `src/keystore/ssk_keystore.c` (replace stub)

**Scope:** ~150 lines of C code

**Interface to implement:**
```c
SSKDecoded *ssk_keystore_decode(bytea *encoded);
bytea *ssk_keystore_encode(SSKDecoded *dec);
```

**Design Details:**

1. **`ssk_keystore_decode(bytea *encoded)`**
   - Extract bytea payload via `VARDATA_ANY(encoded)` and `VARSIZE_ANY_EXHDR(encoded)`
   - Allocate empty SSKDecoded via `ssk_decoded_alloc()` with reasonable initial size (~1KB)
   - Parse Format 0 header (1 byte):
     - Extract format version (bits 0-3), partition count (bits 4-7)
     - Validate format == 0 (assert if not)
   - For each partition (sequential read via bit/byte offsets):
     - Decode partition header via CDU (e.g., `CDU_TYPE_INITIAL_DELTA` or `CDU_TYPE_ENUM_K`)
     - For each segment in partition:
       - Decode segment header (type: ENUM, RAW, or RLE)
       - For ENUM/RAW: decode chunk count, then chunk metadata array
       - Grow SSKDecoded as needed
       - Copy decoded data into hierarchy
   - Return populated SSKDecoded
   - **Assumption:** Encoded bytea is well-formed (no validation beyond asserts in debug mode)

2. **`ssk_keystore_encode(SSKDecoded *dec)`**
   - Allocate output bytea via `palloc(VARHDRSZ + estimated_size)`
   - Write Format 0 header
   - For each partition in SSKDecoded:
     - Encode partition header via CDU
     - For each segment:
       - Encode segment header and chunk metadata
       - Encode chunk data (64-bit blocks)
   - Update bytea size via `SET_VARSIZE(result, VARHDRSZ + bytes_written)`
   - Return bytea

**Test Strategy:**
- Unit test: decode empty SSK (Format 0 header only)
- Unit test: decode single partition with one segment
- Unit test: round-trip encode-decode, verify bit-for-bit identity
- Unit test: decode with multiple segments and partitions

**Edge Cases to Handle:**
- Empty SSK (no partitions) - valid, returns minimal structure
- Malformed bytea (truncated header) - assert in debug, undefined behavior in release
- RLE segments - decode correctly, expand to in-memory form

---

### Task 3: Integrate Hierarchy into Aggregate

**Files:** `src/agg/ssk_agg.c` (replace stubs) + hierarchy helper functions in `src/codec/hierarchy.c` (new)

**Scope:** ~200 lines of C code

**Functions to implement:**

```c
// Aggregate state functions
Datum ssk_sfunc(PG_FUNCTION_ARGS);  // (state ssk, value bigint) -> ssk
Datum ssk_ffunc(PG_FUNCTION_ARGS);  // (state ssk) -> ssk

// Hierarchy helpers (internal)
static void hierarchy_add_value(SSKDecoded *dec, uint64_t value);
static SSKPartition *hierarchy_get_or_create_partition(SSKDecoded *dec, uint16_t partition_id);
static SSKSegment *hierarchy_get_or_create_segment(SSKPartition *part, uint16_t segment_id);
```

**Design Details:**

1. **`ssk_sfunc(state, value)`**
   - If state is NULL: initialize via `ssk_decoded_alloc(0, 4096)` (empty SSK for Format 0)
   - Else: decode state bytea via `ssk_keystore_decode()`
   - Call `hierarchy_add_value(dec, value)` to append value
   - Encode dec back to bytea via `ssk_keystore_encode()`
   - Return encoded bytea
   - **Note:** Decode/encode on every call is inefficient but meets demo requirements

2. **`ssk_ffunc(state)`**
   - Return state unchanged (no special finalization needed)
   - If state is NULL, return empty SSK via `make_empty_ssk()`

3. **`hierarchy_add_value(dec, value)`**
   - Determine which partition this value belongs to (use combinadic or partition strategy TBD)
   - Get or create that partition via `hierarchy_get_or_create_partition()`
   - Determine which segment within the partition
   - Get or create that segment via `hierarchy_get_or_create_segment()`
   - Append value to segment's chunk array
   - Mark segment dirty (dirty_flag = 1)
   - Update partition and root counts

4. **`hierarchy_get_or_create_partition(dec, partition_id)`**
   - Check: does `partition_offs[partition_id]` exist and non-zero?
   - If yes: return pointer to SSKPartition at that offset
   - If no: allocate new SSKPartition in dec's free space (grow if needed)
   - Write new offset into `partition_offs[partition_id]`
   - Increment `dec->num_partitions`
   - Return new partition

5. **`hierarchy_get_or_create_segment(part, segment_id)`**
   - Similar to partition logic, but within partition's segment array
   - Allocate new SSKSegment if needed
   - Initialize segment with type (ENUM or RAW), chunk count, etc.

**Test Strategy:**
- Unit test: add single value to empty SSK, verify structure
- Unit test: add multiple values, verify partition/segment creation
- Unit test: aggregate function on PostgreSQL with small dataset (10 values)
- Unit test: verify encoded output round-trips (encode → decode → encode)

**Edge Cases to Handle:**
- First value (empty SSK state)
- Values requiring new partitions
- Values requiring new segments
- Memory growth during aggregation
- Realloc failures (assert in debug mode)

---

### Task 4: Write Unit Tests for All Modules

**Files:** 
- `test/unit/test_memory.c` (new)
- `test/unit/test_keystore.c` (new)
- `test/unit/test_hierarchy.c` (new)
- `test/unit/test_aggregate.c` (new)

**Scope:** ~300 lines of C test code

**Test Structure:**

Each test file should follow pattern:
```c
#include <stdio.h>
#include <assert.h>
#include "ssk.h"
#include "ssk_decoded.h"
#include "cdu.h"

void test_memory_alloc() { ... }
void test_memory_grow() { ... }
void test_memory_realloc_stress() { ... }
// ... more tests ...

int main() {
    test_memory_alloc();
    test_memory_grow();
    // ... run all tests ...
    printf("All tests passed.\n");
    return 0;
}
```

**Test Coverage:**

1. **Memory tests** (`test_memory.c`):
   - Allocate with various sizes, verify header fields
   - Grow multiple times, verify no data loss
   - Realloc stress (1000 grows), verify offset stability
   - Free (no crash)

2. **Keystore tests** (`test_keystore.c`):
   - Encode/decode empty SSK
   - Encode/decode single partition
   - Round-trip (encode → decode → encode), verify identity
   - Decode with multiple segments

3. **Hierarchy tests** (`test_hierarchy.c`):
   - Add single value to empty SSK
   - Add multiple values, verify partition/segment creation
   - Verify offset pointers remain valid after realloc

4. **Aggregate tests** (`test_aggregate.c`):
   - Aggregate 10 bigint values, verify output is valid SSK
   - Decode aggregated output, verify structure

**Compile & Run:**
```bash
make -C test/unit
./test/unit/test_memory
./test/unit/test_keystore
./test/unit/test_hierarchy
./test/unit/test_aggregate
```

---

### Task 5: Validate PostgreSQL Demo End-to-End

**Files:**
- `test/sql/demo.sql` (new)
- `Makefile` (update to include demo target)

**Scope:** SQL setup + demo queries

**Demo Schema:**

```sql
-- Create table with SSK column
CREATE TABLE demo_ssk (
    id SERIAL PRIMARY KEY,
    ssk_value SSK
);

-- Insert data with aggregate
INSERT INTO demo_ssk (ssk_value)
SELECT ssk_agg(x)
FROM (
    VALUES (1), (5), (10), (15), (20), (50), (100)
) AS t(x);

-- Query to verify
SELECT id, encode(ssk_value, 'hex') AS encoded FROM demo_ssk;

-- Decode and inspect (if functions exist)
SELECT ssk_describe(ssk_value) FROM demo_ssk;
```

**Steps:**

1. Build extension: `make install`
2. Start PostgreSQL, load extension: `CREATE EXTENSION ssk;`
3. Run demo schema
4. Verify output encodes/decodes without error
5. Document observed output in [priv/workfiles/demo-output.txt](priv/workfiles/demo-output.txt)

**Success Criteria:**
- No PostgreSQL errors during aggregate
- SSK output is non-empty bytea
- Round-trip decode produces consistent structure
- All asserts pass in debug build

---

## Dependencies & Preconditions

**Before starting Task 1:**
- Verify [include/ssk_decoded.h](include/ssk_decoded.h) is complete and compiles
- Verify CDU in [src/codec/cdu.c](src/codec/cdu.c) compiles and links

**Before starting Task 2:**
- Task 1 (memory functions) must be complete
- Verify [include/cdu.h](include/cdu.h) is in include path

**Before starting Task 3:**
- Task 1 and Task 2 must be complete
- Determine partition/segment assignment strategy (combinadic, modulo, etc.)—currently unspecified

**Before starting Task 4:**
- All tasks 1-3 complete; create unit test harness

**Before starting Task 5:**
- All tasks 1-4 complete; PostgreSQL environment ready

---

## Open Questions (To Be Resolved Before Implementation)

1. **Partition/Segment Assignment:** How are values mapped to partitions/segments?
   - By combinadic rank range?
   - By modulo arithmetic?
   - By direct offset (value → partition_id, segment_id)?
   - Current spec silent on this; affects `hierarchy_add_value()` logic.

2. **Chunk Metadata Format:** Are chunks packed as 2-bit fields per [include/ssk_decoded.h](include/ssk_decoded.h), or stored separately?
   - Decision affects segment encoding/decoding.

3. **RLE vs. Raw:** When do segments use RLE (run-length encoding) vs. raw chunks?
   - Currently not specified; assume raw chunks for demo.

4. **Finalization Logic:** After all values added, does hierarchy need normalization/cleanup before encoding?
   - Currently assumed no (immediate encode is acceptable for demo).

---

## Success Metrics

✅ **Task 1:** Memory functions allocate, grow, and free without crash or memory leak.  
✅ **Task 2:** Keystore round-trips (encode → decode → encode) with bit-for-bit identity.  
✅ **Task 3:** Aggregate function adds values and produces valid encoded SSK.  
✅ **Task 4:** All unit tests pass in debug mode.  
✅ **Task 5:** PostgreSQL demo runs without error and produces non-empty SSK values.

---

## Notes for Implementation

- All debug assertions should use `assert()` macro; fail hard on unexpected conditions.
- Avoid recovery logic (no fallback, no graceful degradation).
- Document assumptions in comments for future refactoring.
- Log progress in [priv/context/updates.md](priv/context/updates.md) as tasks complete.
- Keep implementation focused on correctness, not optimization.
