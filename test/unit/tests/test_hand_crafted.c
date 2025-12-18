/*
 * tests/test_hand_crafted.c
 *
 * Hand-crafted SSKDecoded test structures for codec validation.
 *
 * Each test case is laid out with:
 *   - Fields ordered in encoding sequence
 *   - Comments at line end showing expected encoding:
 *       ??? = field IS encoded (with expected bits/value)
 *       --- = field is NOT encoded (housekeeping only)
 *
 * ENCODING ORDER (from segment.c, partition.c, cdu.c):
 * =====================================================
 * [format_version]         CDU CDU_DEFAULT
 *
 * For each partition (ascending order):
 *   [partition_delta]      CDU CDU_LARGE_INT
 *   [segment_count]        CDU CDU_SMALL_INT
 *
 *   For each segment:
 *     [seg_kind]           1 bit: 0=RLE, 1=MIX
 *     [initial_delta]      CDU CDU_INITIAL_DELTA
 *     [length_bits]        CDU CDU_MEDIUM_INT
 *
 *     If RLE:
 *       [membership_bit]   1 bit
 *
 *     If MIX:
 *       [token_stream]     token_tag(2) + token_data
 *
 * STRUCTURE SIZES:
 *   SSKDecoded:   32 bytes, partition_offs[] FAM at offset 32
 *   SSKPartition: 28 bytes, segment_offs[] FAM at offset 28
 *   SSKSegment:   24 bytes, data[] FAM at offset 24
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../include/ssk_decoded.h"

/* Forward declaration for testing */
int ssk_encode_impl(const SSKDecoded *ssk, uint8_t *buffer, size_t buffer_size, 
                   uint16_t target_format, FILE *debug_log, char *mock_output, 
                   size_t mock_output_size, size_t *mock_output_used);

/* ============================================================================
 * TEST VECTOR 1: Single element {42}
 *
 * CANONICAL FORM: Single bit always becomes RLE segment.
 *   - Segment offset = 42 (the bit position)
 *   - Segment length = 1 (just that one bit)
 *   - membership_bit = 1 (the bit is set)
 *
 * Expected encoding:
 *   format_version = 0    CDU_DEFAULT: 1 bit
 *   partition_delta = 0   CDU_LARGE_INT: 1 bit
 *   segment_count = 1     CDU_SMALL_INT: bits
 *   seg_kind = RLE        1 bit: 0
 *   initial_delta = 42    CDU_INITIAL_DELTA: bits
 *   length_bits = 1       CDU_MEDIUM_INT: bits
 *   membership_bit = 1    1 bit: 1
 *
 * Total: 5+6+5+1+6+7+1 = 31 bits = 4 bytes
 * ============================================================================
 */
static void
build_test_single_42(uint8_t *buf, size_t bufsize)
{
    memset(buf, 0, bufsize);

    SSKDecoded   *ssk       = (SSKDecoded *)buf;
    SSKPartition *partition = (SSKPartition *)(buf + 36);
    SSKSegment   *segment   = (SSKSegment *)(buf + 68);

    /* --- SSKDecoded --- */
    *ssk = (SSKDecoded){
        .format_version     = 0,            // ??? CDU: bits
        .rare_bit           = 1,            // ??? (global rare bit)
        .n_partitions       = 1,            // ??? (implies 1 partition follows)
        .cardinality        = 1,            // --- (computed, not encoded)
        .var_data_off       = 4,            // ---
        .var_data_used      = 56,           // ---
        .var_data_allocated = bufsize - 36, // ---
    };
    ssk->partition_offs[0]  = 4;            // --- (partition at buf+36)

    /* --- SSKPartition --- */
    *partition = (SSKPartition){
        .partition_id       = 0,            // ??? delta=0: CDU bits
        .n_segments         = 1,            // ??? CDU: bits
        .rare_bit           = 1,            // ??? (partition rare bit)
        .cardinality        = 1,            // --- (computed)
        .var_data_off       = 4,            // ---
        .var_data_used      = 24,           // ---
        .var_data_allocated = 48,           // ---
    };
    partition->segment_offs[0] = 4;         // --- (segment at buf+68)

    /* --- SSKSegment (RLE) --- */
    *segment = (SSKSegment){
        .segment_type       = SEG_TYPE_RLE, // ??? 1 bit: 0
        .start_bit          = 42,           // ??? delta=42: CDU bits
        .n_bits             = 1,            // ??? CDU: bits
        .rare_bit           = 1,            // ??? membership_bit=1 (1 bit)
        .cardinality        = 1,            // --- (computed)
        .blocks_off         = 0,            // --- (no blocks for RLE)
        .blocks_allocated   = 0,            // ---
    };
    /* No chunks/blocks for RLE */
}

/* ============================================================================
 * TEST VECTOR 2: Sparse IDs {10, 20, 30}
 *
 * ID to bit mapping: bit_position = ID - 1
 *   ID 10 → bit 9
 *   ID 20 → bit 19
 *   ID 30 → bit 29
 *
 * CANONICAL FORM: Segment trimmed to first/last set bits.
 *   - start_bit = 9 (first set bit, corresponds to ID 10)
 *   - n_bits = 21 (bits 9-29 inclusive: 29 - 9 + 1 = 21)
 *   - Relative positions: 0, 10, 20 (equal spacing ✓)
 *   - Bitmap: 0x100401
 *   - k=3, n=21 (partial chunk), rare_bit=1 (sparse: 3/21 ≈ 14%)
 *
 * Expected encoding:
 *   format_version = 0    CDU_DEFAULT: bits
 *   partition_delta = 0   CDU_LARGE_INT: bits
 *   segment_count = 1     CDU_SMALL_INT: bits
 *   seg_kind = MIX        1 bit: 1
 *   initial_delta = 9     CDU_INITIAL_DELTA: bits
 *   length_bits = 21      CDU_MEDIUM_INT: bits
 *   token_tag = ENUM      2 bits: 00
 *   k = 3                 6 bits: 000011
 *   rank for {0,10,20} in C(21,3)
 * ============================================================================
 */
static void
build_test_sparse_3(uint8_t *buf, size_t bufsize)
{
    memset(buf, 0, bufsize);

    SSKDecoded   *ssk       = (SSKDecoded *)buf;
    SSKPartition *partition = (SSKPartition *)(buf + 36);
    SSKSegment   *segment   = (SSKSegment *)(buf + 68);

    *ssk = (SSKDecoded){
        .format_version     = 0,            // ??? CDU: bits
        .rare_bit           = 1,            // ???
        .n_partitions       = 1,            // ???
        .cardinality        = 3,            // ---
        .var_data_off       = 4,            // ---
        .var_data_used      = 72,           // ---
        .var_data_allocated = bufsize - 36, // ---
    };
    ssk->partition_offs[0]  = 4;            // ---

    *partition = (SSKPartition){
        .partition_id       = 0,            // ??? delta=0
        .n_segments         = 1,            // ??? CDU: bits
        .rare_bit           = 1,            // ???
        .cardinality        = 3,            // ---
        .var_data_off       = 4,            // ---
        .var_data_used      = 40,           // ---
        .var_data_allocated = 64,           // ---
    };
    partition->segment_offs[0] = 4;         // ---

    *segment = (SSKSegment){
        .segment_type       = SEG_TYPE_MIX, // ??? 1 bit: 1
        .start_bit          = 9,            // ??? delta=9: CDU bits
        .n_bits             = 21,           // ??? CDU: bits
        .rare_bit           = 1,            // ??? (sparse, 1s are rare)
        .cardinality        = 3,            // ---
        .blocks_off         = 8,            // ---
        .blocks_allocated   = 1,            // ---
    };

    segment_chunk_meta_set(segment, 0,      // ??? token_tag=00 (ENUM)
        chunk_meta_pack(CHUNK_TYPE_ENUM, CHUNK_FLAG_CLEAN));

    /* IDs {10,20,30} → bits {9,19,29} → relative {0,10,20} at start_bit=9 */
    segment_chunk_block_set(segment, 0,     // ??? k=3, rank in C(21,3)
        (1ULL << 0) | (1ULL << 10) | (1ULL << 20));  // 0x100401
}

/* ============================================================================
 * TEST VECTOR 3: RAW chunk - firmly in RAW category without dominance flip
 *
 * Set: {1,5,9,11,13,15,17,19,21,23,25,27,29,31,33,35,37,39,41,43,45,47,49,51,53,55,57,59,61,63}
 *      = odd numbers 1-63, excluding 3 and 7
 *      = 30 elements
 *
 * CANONICAL FORM:
 *   - First set bit: 1, Last set bit: 63
 *   - Segment offset = 1, length = 63 (bits 1-63 → relative 0-62)
 *   - popcount = 30 out of 63 bits → 30/63 ≈ 47.6% < 50%
 *   - rare_bit = 1 (1s are minority, no dominance flip)
 *   - 30 > k_enum_max(18) → RAW encoding
 *
 * Relative positions (subtract 1 from globals):
 *   {0,4,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62}
 *
 * Bitmap: Start with 0xAAAAAAAAAAAAAAAA (odd bits 1,3,5,...,63)
 *         Clear bits 3 and 7 → clear relative positions 2 and 6
 *         = 0xAAAAAAAAAAAAAAAA & ~(1<<2) & ~(1<<6) = 0xAAAAAAAAAAAAAAAA & ~0x44
 *         = 0xAAAAAAAAAAAAAABA... wait, let me recalc
 *
 * Global odd bits: 1,3,5,7,9,11,...,63 at positions 1,3,5,7,9,11,...,63
 * Remove 3 and 7: positions 1,5,9,11,13,...,63
 * Relative to offset 1: 0,4,8,10,12,...,62
 * Bitmap (relative): bits 0,4,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62
 *
 * For RAW: token_tag=01, followed by 63 raw bits (partial chunk).
 * ============================================================================
 */
static void
build_test_raw_30(uint8_t *buf, size_t bufsize)
{
    memset(buf, 0, bufsize);

    SSKDecoded   *ssk       = (SSKDecoded *)buf;
    SSKPartition *partition = (SSKPartition *)(buf + 36);
    SSKSegment   *segment   = (SSKSegment *)(buf + 68);

    *ssk = (SSKDecoded){
        .format_version     = 0,            // ??? CDU: bits
        .rare_bit           = 1,            // ??? (1s are rare: 30/63 < 50%)
        .n_partitions       = 1,            // ???
        .cardinality        = 30,           // ---
        .var_data_off       = 4,            // ---
        .var_data_used      = 72,           // ---
        .var_data_allocated = bufsize - 36, // ---
    };
    ssk->partition_offs[0]  = 4;            // ---

    *partition = (SSKPartition){
        .partition_id       = 0,            // ??? delta=0
        .n_segments         = 1,            // ???
        .rare_bit           = 1,            // ???
        .cardinality        = 30,           // ---
        .var_data_off       = 4,            // ---
        .var_data_used      = 40,           // ---
        .var_data_allocated = 64,           // ---
    };
    partition->segment_offs[0] = 4;         // ---

    *segment = (SSKSegment){
        .segment_type       = SEG_TYPE_MIX, // ??? 1 bit: 1
        .start_bit          = 1,            // ??? delta=1: CDU bits
        .n_bits             = 63,           // ??? CDU: bits
        .rare_bit           = 1,            // ??? (1s rare, no flip)
        .cardinality        = 30,           // ---
        .blocks_off         = 8,            // ---
        .blocks_allocated   = 1,            // ---
    };

    segment_chunk_meta_set(segment, 0,      // ??? token_tag=01 (RAW)
        chunk_meta_pack(CHUNK_TYPE_RAW, CHUNK_FLAG_CLEAN));

    /*
     * Relative bitmap for {1,5,9,11,13,...,63} at offset 1:
     * Positions 0,4,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62
     *
     * Build: odd positions 0,2,4,...,62 in relative space = even globals 1,3,5,...,63
     * But we want odds excluding 3,7 → relative 0,4,8,10,12,...
     * Easier: start with all odd globals, mask off 3 and 7
     * 0xAAAAAAAAAAAAAAAA has bits 1,3,5,...,63 set
     * Shift right by 1 to get relative positions: 0,2,4,...,62
     * Then clear relative positions 2 and 6 (which were global 3 and 7)
     */
    uint64_t bitmap = 0xAAAAAAAAAAAAAAAAULL >> 1;  /* relative: 0,2,4,6,8,...,62 */
    bitmap &= ~((1ULL << 2) | (1ULL << 6));         /* clear 2,6 (were global 3,7) */
    segment_chunk_block_set(segment, 0, bitmap);   // ??? 63 raw bits
}

/* ============================================================================
 * TEST VECTOR 4: RLE segment (all 64 bits set)
 *
 * RLE: entire segment has uniform bit value.
 * Encoding: seg_kind=0, delta, length, membership_bit=1
 * No tokens (n_chunks derived as 0 for RLE conceptually).
 * ============================================================================
 */
static void
build_test_rle_64(uint8_t *buf, size_t bufsize)
{
    memset(buf, 0, bufsize);

    SSKDecoded   *ssk       = (SSKDecoded *)buf;
    SSKPartition *partition = (SSKPartition *)(buf + 36);
    SSKSegment   *segment   = (SSKSegment *)(buf + 68);

    *ssk = (SSKDecoded){
        .format_version     = 0,            // ??? CDU: bits
        .rare_bit           = 1,            // ???
        .n_partitions       = 1,            // ???
        .cardinality        = 64,           // ---
        .var_data_off       = 4,            // ---
        .var_data_used      = 56,           // ---
        .var_data_allocated = bufsize - 36, // ---
    };
    ssk->partition_offs[0]  = 4;            // ---

    *partition = (SSKPartition){
        .partition_id       = 0,            // ??? delta=0
        .n_segments         = 1,            // ???
        .rare_bit           = 1,            // ???
        .cardinality        = 64,           // ---
        .var_data_off       = 4,            // ---
        .var_data_used      = 24,           // ---
        .var_data_allocated = 48,           // ---
    };
    partition->segment_offs[0] = 4;         // ---

    *segment = (SSKSegment){
        .segment_type       = SEG_TYPE_RLE, // ??? 1 bit: 0
        .start_bit          = 0,            // ??? delta=0: CDU bits
        .n_bits             = 64,           // ??? length=64
        .rare_bit           = 1,            // ??? membership_bit=1
        .cardinality        = 64,           // ---
        .blocks_off         = 0,            // --- (no blocks for RLE)
        .blocks_allocated   = 0,            // ---
    };
    /* No chunks/blocks for RLE */
}

/* ============================================================================
 * HELPER: Print bitmap as set bit positions
 * ============================================================================
 */
static void
print_set_bits(uint64_t bitmap)
{
    printf("{");
    bool first = true;
    for (int i = 0; i < 64; i++) {
        if (bitmap & (1ULL << i)) {
            if (!first) printf(", ");
            printf("%d", i);
            first = false;
        }
    }
    printf("}");
}

/* ============================================================================
 * HELPER: Print decoded structure for verification
 * ============================================================================
 */
static void
print_decoded(const uint8_t *buf, const char *name)
{
    const SSKDecoded *ssk = (const SSKDecoded *)buf;

    printf("\n=== %s ===\n", name);
    printf("  format=%u, rare_bit=%u, n_partitions=%u, cardinality=%lu\n",
           ssk->format_version, ssk->rare_bit, ssk->n_partitions,
           (unsigned long)ssk->cardinality);

    if (ssk->n_partitions == 0) {
        printf("  (empty set)\n");
        return;
    }

    for (uint32_t p = 0; p < ssk->n_partitions; p++) {
        SSKPartition *part = decoded_partition((SSKDecoded *)ssk, p);
        printf("\n  Partition %u: id=%u, rare_bit=%u, n_segments=%u, card=%u\n",
               p, part->partition_id, part->rare_bit, part->n_segments,
               part->cardinality);

        for (uint32_t s = 0; s < part->n_segments; s++) {
            SSKSegment *seg = partition_segment(part, s);
            uint32_t n_chunks = segment_n_chunks(seg->n_bits);
            const char *stype = (seg->segment_type == SEG_TYPE_RLE) ? "RLE" : "MIX";

            printf("    Segment %u: %s, start=%u, n_bits=%u, rare=%u, card=%u\n",
                   s, stype, seg->start_bit, seg->n_bits,
                   seg->rare_bit, seg->cardinality);

            if (seg->segment_type == SEG_TYPE_RLE) {
                printf("      (all %us)\n", seg->rare_bit);
                continue;
            }

            for (uint32_t c = 0; c < n_chunks; c++) {
                uint8_t meta = segment_chunk_meta_get(seg, c);
                const char *ctype = chunk_meta_type(meta) ? "RAW" : "ENUM";
                uint64_t block = segment_chunk_block_get(seg, c);
                printf("      Chunk %u: %s, dirty=%u, bits=",
                       c, ctype, chunk_meta_dirty(meta));
                print_set_bits(block);
                printf("\n");
            }
        }
    }
}

/* ============================================================================
 * TEST VECTORS TABLE
 * ============================================================================
 */
typedef struct {
    const char *name;
    void (*build)(uint8_t *, size_t);
    uint64_t expected_card;
} TestVector;

static TestVector vectors[] = {
    { "Single {42} → RLE",              build_test_single_42, 1  },
    { "Sparse IDs {10,20,30}",          build_test_sparse_3,  3  },
    { "RAW {1,5,9,11,...,63} k=30",     build_test_raw_30,    30 },
    { "RLE all-64",                     build_test_rle_64,    64 },
};

#define N_VECTORS (sizeof(vectors) / sizeof(vectors[0]))

/* ============================================================================
 * TEST RUNNER
 * ============================================================================
 */
static int tests_passed = 0;
static int tests_failed = 0;

static void
run_vector(const TestVector *v)
{
    uint8_t buf[256];

    v->build(buf, sizeof(buf));

    const SSKDecoded *ssk = (const SSKDecoded *)buf;

    /* Verify cardinality */
    if (ssk->cardinality != v->expected_card) {
        printf("FAIL: %s - cardinality mismatch (got %lu, expected %lu)\n",
               v->name, (unsigned long)ssk->cardinality,
               (unsigned long)v->expected_card);
        tests_failed++;
        return;
    }

    /* Verify traversal */
    if (ssk->n_partitions > 0) {
        SSKPartition *part = decoded_partition((SSKDecoded *)ssk, 0);
        if (part->n_segments > 0) {
            SSKSegment *seg = partition_segment(part, 0);
            if (seg->segment_type != SEG_TYPE_MIX &&
                seg->segment_type != SEG_TYPE_RLE) {
                printf("FAIL: %s - invalid segment type %u\n",
                       v->name, seg->segment_type);
                tests_failed++;
                return;
            }
        }
    }

    print_decoded(buf, v->name);
    
    /* Test encoding */
    uint8_t encoded_buf[256];
    char audit_buf[1024];
    size_t audit_used;
    
    int encoded_bytes = ssk_encode_impl((const SSKDecoded *)buf, encoded_buf, sizeof(encoded_buf), 0, 
                                       NULL, audit_buf, sizeof(audit_buf), &audit_used);
    
    if (encoded_bytes < 0) {
        printf("  ENCODE FAIL: encoding returned %d\n", encoded_bytes);
        tests_failed++;
        return;
    }
    
    printf("  Encoded to %d bytes\n", encoded_bytes);
    printf("  AUDIT: %s\n", audit_buf);
    
    printf("  PASS\n");
    tests_passed++;
}

int
run_hand_crafted_tests(void)
{
    printf("\n========================================\n");
    printf("Hand-Crafted SSKDecoded Test Vectors\n");
    printf("========================================\n");

    for (size_t i = 0; i < N_VECTORS; i++) {
        run_vector(&vectors[i]);
    }

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
    return tests_failed;
}

/* Allow standalone execution */
#ifdef STANDALONE_TEST
int main(void)
{
    run_hand_crafted_tests();
    return tests_failed > 0 ? 1 : 0;
}
#endif
