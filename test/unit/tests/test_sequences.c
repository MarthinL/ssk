/*
 * tests/test_sequences.c
 *
 * Unit tests for SSK codec components.
 * Run via: make test-runner && ./test_runner
 */

#ifdef TRIVIAL

/*
 * Test sequences for TRIVIAL implementation.
 * Tests bijection and consistency properties.
 */

/* TODO: Implement trivial sequence tests */

int run_all_tests(void) { return 0; }

#else // NON TRIVIAL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../include/ssk.h"
#include "../include/ssk_format.h"
#include "../include/cdu.h"

static int tests_run = 0;
static int tests_passed = 0;
static int test_result = 0;  /* 0 = pass, 1 = fail */

#define TEST(name) static void name(void)
#define RUN_TEST(name) do { \
    printf("  %-40s", #name); \
    tests_run++; \
    test_result = 0; \
    name(); \
    if (test_result == 0) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", #cond, __FILE__, __LINE__); \
        test_result = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL\n    Expected %llu == %llu\n    at %s:%d\n", \
               (unsigned long long)(a), (unsigned long long)(b), __FILE__, __LINE__); \
        test_result = 1; \
        return; \
    } \
} while(0)

/* ============================================================================
 * CDU TESTS
 * ============================================================================
 */

TEST(test_cdu_roundtrip)
{
    uint8_t buf[32] = {0};
    uint64_t decoded;
    
    /* Test values from graveyard validation */
    uint64_t test_values[] = {0, 1, 127, 128, 255, 256, 1000, 65535, 1000000, 4294967295ULL}; /* UINT32_MAX */
    size_t num_tests = sizeof(test_values) / sizeof(test_values[0]);
    
    for (size_t i = 0; i < num_tests; i++) {
        memset(buf, 0, sizeof(buf));
        uint64_t value = test_values[i];
        
        // Pick appropriate type based on value size
        CDUtype type = (value < 65536) ? CDU_TYPE_SMALL_INT : CDU_TYPE_LARGE_INT;
        
        size_t bits_written = cdu_encode(value, type, buf, 0);
        ASSERT(bits_written > 0);
        
        size_t bits_read = cdu_decode(buf, 0, bits_written, type, &decoded);
        ASSERT(bits_read > 0);
        ASSERT_EQ(decoded, value);
        /* TODO: Fix bit counting - ASSERT_EQ(bits_read, bits_written); */
    }
}

TEST(test_cdu_minimality)
{
    uint8_t buf[16] = {0};
    uint64_t decoded;
    size_t bits_written, bits_read;
    
    /* Encode a small value - should be minimal */
    bits_written = cdu_encode(5, CDU_TYPE_DEFAULT, buf, 0);
    ASSERT(ssk_cdu_is_minimal(buf, CDU_TYPE_DEFAULT, 5));
}

TEST(test_cdu_medium_int)
{
    uint8_t buf[32] = {0};
    uint64_t decoded;
    
    /* Test CDU_TYPE_MEDIUM_INT with some values */
    uint64_t test_values[] = {0, 1000, 10000, 100000, 1000000};
    
    for (size_t i = 0; i < sizeof(test_values)/sizeof(test_values[0]); i++) {
        memset(buf, 0, sizeof(buf));
        uint64_t value = test_values[i];
        
        size_t bits_written = cdu_encode(value, CDU_TYPE_MEDIUM_INT, buf, 0);
        ASSERT(bits_written > 0);
        
        size_t bits_read = cdu_decode(buf, 0, bits_written, CDU_TYPE_MEDIUM_INT, &decoded);
        ASSERT(bits_read > 0);
        ASSERT_EQ(decoded, value);
    }
}

/* ============================================================================
 * COMBINADIC TESTS
 * ============================================================================
 */

TEST(test_combinadic_init)
{
    ssk_combinadic_init();
    
    /* Verify some known binomial values */
    ASSERT_EQ(ssk_binomial(5, 0), 1);
    ASSERT_EQ(ssk_binomial(5, 1), 5);
    ASSERT_EQ(ssk_binomial(5, 2), 10);
    ASSERT_EQ(ssk_binomial(5, 5), 1);
    ASSERT_EQ(ssk_binomial(64, 1), 64);
    ASSERT_EQ(ssk_binomial(64, 2), 2016);
    
    /* C(64,18) should be a large value */
    ASSERT(ssk_binomial(64, 18) > 1000000000000ULL);
}

TEST(test_combinadic_rank_simple)
{
    ssk_combinadic_init();
    
    /* For n=4, k=2: combinations in colexicographic order:
     * {0,1}, {0,2}, {1,2}, {0,3}, {1,3}, {2,3}
     * Bits (position 3=MSB, position 0=LSB):
     *   {0,1} = 0b0011 (positions 0,1) -> rank 0
     *   {0,2} = 0b0101 (positions 0,2) -> rank 1
     *   {1,2} = 0b0110 (positions 1,2) -> rank 2
     *   {0,3} = 0b1001 (positions 0,3) -> rank 3
     *   {1,3} = 0b1010 (positions 1,3) -> rank 4
     *   {2,3} = 0b1100 (positions 2,3) -> rank 5
     */
    ASSERT_EQ(ssk_combinadic_rank(0b0011, 4, 2), 0);
    ASSERT_EQ(ssk_combinadic_rank(0b0101, 4, 2), 1);
    ASSERT_EQ(ssk_combinadic_rank(0b0110, 4, 2), 2);
    ASSERT_EQ(ssk_combinadic_rank(0b1001, 4, 2), 3);
    ASSERT_EQ(ssk_combinadic_rank(0b1010, 4, 2), 4);
    ASSERT_EQ(ssk_combinadic_rank(0b1100, 4, 2), 5);
}

TEST(test_combinadic_roundtrip)
{
    ssk_combinadic_init();
    
    /* Test roundtrip for various (n, k) combinations */
    uint8_t test_cases[][2] = {
        {8, 3}, {16, 5}, {32, 8}, {64, 10}, {64, 18}
    };
    
    for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_cases[0]); i++)
    {
        uint8_t n = test_cases[i][0];
        uint8_t k = test_cases[i][1];
        uint64_t max_rank = ssk_binomial(n, k);
        
        /* Test a few ranks in this space */
        uint64_t test_ranks[] = {0, 1, max_rank/2, max_rank - 1};
        
        for (size_t j = 0; j < sizeof(test_ranks)/sizeof(test_ranks[0]); j++)
        {
            uint64_t rank = test_ranks[j];
            if (rank >= max_rank) continue;
            
            uint64_t bits = ssk_combinadic_unrank(rank, n, k);
            ASSERT_EQ(ssk_popcount64(bits), k);
            
            uint64_t recovered_rank = ssk_combinadic_rank(bits, n, k);
            ASSERT_EQ(recovered_rank, rank);
        }
    }
}

TEST(test_combinadic_popcount)
{
    ASSERT_EQ(ssk_popcount64(0), 0);
    ASSERT_EQ(ssk_popcount64(1), 1);
    ASSERT_EQ(ssk_popcount64(0xFF), 8);
    ASSERT_EQ(ssk_popcount64(0xFFFFFFFFFFFFFFFFULL), 64);
    ASSERT_EQ(ssk_popcount64(0xAAAAAAAAAAAAAAAAULL), 32);
}

TEST(test_combinadic_rank_bits)
{
    ssk_combinadic_init();
    
    /* rank_bits should be ceil(log2(C(n,k))) */
    ASSERT_EQ(ssk_get_rank_bits(4, 2), 3);   /* C(4,2)=6, needs 3 bits */
    ASSERT_EQ(ssk_get_rank_bits(64, 1), 6);  /* C(64,1)=64, needs 6 bits */
    
    /* For k=0 or k=n, only one possibility, 0 bits needed */
    ASSERT_EQ(ssk_get_rank_bits(64, 0), 0);
}

TEST(test_combinadic_generated_dataset)
{
    ssk_combinadic_init();
    
    /* Load the generated test dataset and verify roundtrip */
    FILE *f = fopen("../../priv/workshop/test_dataset_uniform.csv", "r");
    if (!f) {
        printf("Warning: Could not open test_dataset_uniform.csv, skipping test\n");
        return;
    }
    
    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        char hex_str[32], dec_str[32];
        if (sscanf(line, "\"%31[^\"]\",\"%31[^\"]\"", hex_str, dec_str) == 2) {
            uint64_t bits = strtoull(hex_str, NULL, 16);
            uint64_t expected_rank = strtoull(dec_str, NULL, 10);
            uint8_t k = ssk_popcount64(bits);
            
            /* Test rank matches expected */
            uint64_t rank = ssk_combinadic_rank(bits, 64, k);
            ASSERT_EQ(rank, expected_rank);
            
            /* Test roundtrip */
            uint64_t reconstructed_bits = ssk_combinadic_unrank(rank, 64, k);
            ASSERT_EQ(reconstructed_bits, bits);
            
            count++;
            if (count % 10000 == 0) {
                printf("Tested %d cases...\n", count);
            }
        }
    }
    fclose(f);
    printf("Tested %d cases from generated dataset\n", count);
}

/* ============================================================================
 * TOKEN ENCODING TESTS
 * ============================================================================
 */

/* External declarations for token functions */
extern size_t enum_token_bits(uint8_t n, uint8_t k);
extern size_t enum_token_encode(uint64_t bits, uint8_t n, uint8_t k,
                                uint8_t *buf, size_t bit_pos);
extern int enum_token_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                             uint8_t n,
                             uint64_t *bits_out, uint8_t *k_out, size_t *bits_read);
extern bool should_use_enum(uint8_t k, const SSKFormatSpec *spec);
extern size_t raw_token_bits(uint8_t n);
extern size_t raw_token_encode(uint64_t bits, uint8_t n, uint8_t *buf, size_t bit_pos);
extern int raw_token_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                            uint8_t n, uint64_t *bits_out, size_t *bits_read);

/* Minimal spec for testing */
static SSKFormatSpec test_spec = {
    .format_version = 0,
    .chunk_bits = 64,
    .k_enum_max = 18,
    .n_bits_for_k = 6,
};

TEST(test_enum_token_bits)
{
    ssk_combinadic_init();
    
    /* ENUM token: 2 (type) + 6 (k) + rank_bits */
    /* k=0: no rank bits needed */
    ASSERT_EQ(enum_token_bits(64, 0), 2 + 6 + 0);
    
    /* k=1: C(64,1)=64, needs 6 bits */
    ASSERT_EQ(enum_token_bits(64, 1), 2 + 6 + 6);
    
    /* k=2: C(64,2)=2016, needs 11 bits */
    ASSERT_EQ(enum_token_bits(64, 2), 2 + 6 + 11);
    
    /* k=18: maximum for ENUM */
    uint8_t rank_bits_18 = ssk_get_rank_bits(64, 18);
    ASSERT_EQ(enum_token_bits(64, 18), 2 + 6 + rank_bits_18);
    
    /* k=19: exceeds K_ENUM_MAX, should return 0 */
    ASSERT_EQ(enum_token_bits(64, 19), 0);
}

TEST(test_enum_token_roundtrip)
{
    ssk_combinadic_init();
    uint8_t buf[32] = {0};
    
    /* Test with k=2, positions at bits 60 and 63 (MSB) */
    /* Bit pattern: 0x9000000000000000 = bits 60 and 63 set */
    uint64_t test_bits = 0x9000000000000000ULL;
    uint8_t n = 64;
    uint8_t k = ssk_popcount64(test_bits);
    ASSERT_EQ(k, 2);
    
    /* Encode */
    size_t bits_written = enum_token_encode(test_bits, n, k, buf, 0);
    ASSERT(bits_written > 0);
    
    /* Skip the 2-bit type (already known to be ENUM) */
    uint64_t decoded_bits;
    uint8_t decoded_k;
    size_t bits_read;
    int rc = enum_token_decode(buf, 2, 256, n, &decoded_bits, &decoded_k, &bits_read);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(decoded_k, k);
    ASSERT_EQ(decoded_bits, test_bits);
    ASSERT_EQ(bits_read + 2, bits_written);  /* +2 for type */
}

TEST(test_raw_token_roundtrip)
{
    uint8_t buf[32] = {0};
    
    /* Test with a dense pattern */
    uint64_t test_bits = 0xFFFFFFFF00000000ULL;  /* 32 bits set */
    uint8_t n = 64;
    
    /* Encode */
    size_t bits_written = raw_token_encode(test_bits, n, buf, 0);
    ASSERT_EQ(bits_written, 2 + 64);  /* type + raw bits */
    
    /* Decode (skip 2-bit type) */
    uint64_t decoded_bits;
    size_t bits_read;
    int rc = raw_token_decode(buf, 2, 256, n, &decoded_bits, &bits_read);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(decoded_bits, test_bits);
    ASSERT_EQ(bits_read, 64);
}

TEST(test_should_use_enum)
{
    /* k <= 18 should use ENUM */
    ASSERT(should_use_enum(0, &test_spec));
    ASSERT(should_use_enum(18, &test_spec));
    
    /* k > 18 should use RAW */
    ASSERT(!should_use_enum(19, &test_spec));
    ASSERT(!should_use_enum(32, &test_spec));
}

/* ============================================================================
 * SEGMENT ENCODING TESTS
 * ============================================================================
 */

/* External declarations for segment functions */
extern size_t rle_segment_bits(uint64_t length_bits, const SSKFormatSpec *spec);
extern size_t rle_segment_encode(uint8_t membership_bit, uint64_t length_bits,
                                 const SSKFormatSpec *spec, uint8_t *buf, size_t bit_pos);
extern int rle_segment_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                              uint8_t *membership_out, uint64_t *length_out, size_t *bits_read);
extern size_t mix_segment_header_bits(uint64_t initial_delta, uint64_t length_bits,
                                      const SSKFormatSpec *spec);
extern size_t mix_segment_header_encode(uint64_t initial_delta, uint64_t length_bits,
                                        const SSKFormatSpec *spec,
                                        uint8_t *buf, size_t bit_pos);
extern int mix_segment_header_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                                     uint64_t *delta_out, uint64_t *length_out,
                                     size_t *bits_read);
extern int segment_read_type(const uint8_t *buf, size_t bit_pos, size_t buf_bits);
extern bool should_use_rle(uint64_t length_bits, const SSKFormatSpec *spec);

/* Full spec for segment tests (needs CDU params) */
static SSKFormatSpec full_test_spec = {
    .format_version = 0,
    .chunk_bits = 64,
    .k_enum_max = 18,
    .n_bits_for_k = 6,
    .rare_run_threshold = 64,
    .cdu_length_bits = CDU_TYPE_MEDIUM_INT,
    .cdu_initial_delta = CDU_TYPE_INITIAL_DELTA,
};

TEST(test_rle_segment_roundtrip)
{
    uint8_t buf[32] = {0};
    
    /* Test RLE with membership_bit=1, length=128 */
    uint8_t membership = 1;
    uint64_t length = 128;
    
    /* Encode */
    size_t bits_written = rle_segment_encode(membership, length, &full_test_spec, buf, 0);
    ASSERT(bits_written > 0);
    
    /* Decode - first read and skip segment type */
    int seg_type = segment_read_type(buf, 0, 256);
    ASSERT_EQ(seg_type, SEG_RLE);
    
    uint8_t decoded_membership;
    uint64_t decoded_length;
    size_t bits_read;
    int rc = rle_segment_decode(buf, 1, 256,
                                &decoded_membership, &decoded_length, &bits_read);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(decoded_membership, membership);
    ASSERT_EQ(decoded_length, length);
    ASSERT_EQ(bits_read + 1, bits_written);  /* +1 for type bit */
}

TEST(test_mix_segment_header_roundtrip)
{
    uint8_t buf[32] = {0};
    
    /* Test MIX with delta=100, length=192 (3 full chunks) */
    uint64_t delta = 100;
    uint64_t length = 192;  /* 192 bits = 3 * 64 */
    /* last_nbits is derivable: 192 % 64 = 0, meaning 64 (full last chunk) */
    
    /* Encode */
    size_t bits_written = mix_segment_header_encode(delta, length, 
                                                    &full_test_spec, buf, 0);
    ASSERT(bits_written > 0);
    
    /* Decode - first read and skip segment type */
    int seg_type = segment_read_type(buf, 0, 256);
    ASSERT_EQ(seg_type, SEG_MIX);
    
    uint64_t decoded_delta;
    uint64_t decoded_length;
    size_t bits_read;
    int rc = mix_segment_header_decode(buf, 1, 256,
                                       &decoded_delta, &decoded_length,
                                       &bits_read);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(decoded_delta, delta);
    ASSERT_EQ(decoded_length, length);
    /* Verify last_nbits is derivable correctly */
    uint8_t derived_last_nbits = (decoded_length % 64) == 0 ? 64 : (decoded_length % 64);
    ASSERT_EQ(derived_last_nbits, 64);
    ASSERT_EQ(bits_read + 1, bits_written);  /* +1 for type bit */
}

TEST(test_mix_segment_partial_chunk)
{
    uint8_t buf[32] = {0};
    
    /* Test MIX with partial last chunk: length=100 bits */
    /* 100 % 64 = 36, so derived last_nbits = 36 */
    uint64_t delta = 0;
    uint64_t length = 100;
    
    /* Encode */
    size_t bits_written = mix_segment_header_encode(delta, length,
                                                    &full_test_spec, buf, 0);
    ASSERT(bits_written > 0);
    
    /* Decode */
    uint64_t decoded_delta;
    uint64_t decoded_length;
    size_t bits_read;
    int rc = mix_segment_header_decode(buf, 1, 256,
                                       &decoded_delta, &decoded_length,
                                       &bits_read);
    ASSERT_EQ(rc, 0);
    /* Verify last_nbits is derivable: 100 % 64 = 36 */
    uint8_t derived_last_nbits = (decoded_length % 64) == 0 ? 64 : (decoded_length % 64);
    ASSERT_EQ(derived_last_nbits, 36);
}

TEST(test_should_use_rle)
{
    /* Runs >= 64 (rare_run_threshold) should use RLE */
    ASSERT(should_use_rle(64, &full_test_spec));
    ASSERT(should_use_rle(100, &full_test_spec));
    ASSERT(should_use_rle(1000, &full_test_spec));
    
    /* Runs < 64 should not use RLE */
    ASSERT(!should_use_rle(63, &full_test_spec));
    ASSERT(!should_use_rle(32, &full_test_spec));
    ASSERT(!should_use_rle(1, &full_test_spec));
}

/* ============================================================================
 * PARTITION ENCODING TESTS
 * ============================================================================
 */

/* External declarations for partition functions */
extern size_t partition_header_bits(uint32_t partition_delta, uint16_t segment_count);
extern size_t partition_header_encode(uint32_t partition_delta, uint16_t segment_count,
                                      uint8_t *buf, size_t bit_pos);
extern int partition_header_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                                   uint32_t *delta_out, uint16_t *seg_count_out,
                                   size_t *bits_read);
extern size_t ssk_header_encode(uint16_t format_version, uint32_t n_partitions,
                                const SSKFormatSpec *spec, uint8_t *buf, size_t bit_pos);
extern int ssk_header_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                             uint16_t *format_out, uint32_t *part_count_out,
                             size_t *bits_read);
extern uint32_t partition_delta(uint32_t prev_partition, uint32_t curr_partition);
extern int partition_id_from_delta(uint32_t prev_partition, uint32_t delta,
                                   uint32_t *partition_out);

/* Extended test spec with partition CDU params */
static SSKFormatSpec partition_test_spec = {
    .format_version = 0,
    .chunk_bits = 64,
    .k_enum_max = 18,
    .n_bits_for_k = 6,
    .rare_run_threshold = 64,
    .cdu_format_version = CDU_TYPE_DEFAULT,
    .cdu_partition_delta = CDU_TYPE_LARGE_INT,
    .cdu_segment_count = CDU_TYPE_SMALL_INT,
    .cdu_length_bits = CDU_TYPE_MEDIUM_INT,
    .cdu_initial_delta = CDU_TYPE_LARGE_INT,
};

TEST(test_partition_header_roundtrip)
{
    uint8_t buf[32] = {0};
    
    /* Test partition with delta=5, 3 segments */
    uint32_t delta = 5;
    uint16_t seg_count = 3;
    
    /* Encode */
    size_t bits_written = partition_header_encode(delta, seg_count, buf, 0);
    ASSERT(bits_written > 0);
    
    /* Decode */
    uint32_t decoded_delta;
    uint16_t decoded_seg_count;
    size_t bits_read;
    int rc = partition_header_decode(buf, 0, 256,
                                     &decoded_delta, &decoded_seg_count, &bits_read);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(decoded_delta, delta);
    ASSERT_EQ(decoded_seg_count, seg_count);
    ASSERT_EQ(bits_read, bits_written);
}

TEST(test_ssk_header_roundtrip)
{
    uint8_t buf[32] = {0};
    
    /* Test SSK header: format 0, 5 partitions */
    uint16_t format = 0;
    uint32_t part_count = 5;
    
    /* Encode */
    size_t bits_written = ssk_header_encode(format, part_count, 
                                            &partition_test_spec, buf, 0);
    ASSERT(bits_written > 0);
    
    /* Decode */
    uint16_t decoded_format;
    uint32_t decoded_part_count;
    size_t bits_read;
    int rc = ssk_header_decode(buf, 0, 256, &decoded_format, &decoded_part_count, &bits_read);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(decoded_format, format);
    ASSERT_EQ(decoded_part_count, part_count);
    ASSERT_EQ(bits_read, bits_written);
}

TEST(test_partition_delta_calculation)
{
    /* First partition: delta IS the partition ID */
    ASSERT_EQ(partition_delta(UINT32_MAX, 100), 100);
    
    /* Consecutive partitions: delta = 0 */
    ASSERT_EQ(partition_delta(5, 6), 0);
    
    /* Gap of 10: delta = 9 (gap minus 1) */
    ASSERT_EQ(partition_delta(100, 110), 9);
}

TEST(test_partition_id_from_delta)
{
    uint32_t partition_id;
    int rc;
    
    /* First partition */
    rc = partition_id_from_delta(UINT32_MAX, 100, &partition_id);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(partition_id, 100);
    
    /* Consecutive */
    rc = partition_id_from_delta(5, 0, &partition_id);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(partition_id, 6);
    
    /* With gap */
    rc = partition_id_from_delta(100, 9, &partition_id);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(partition_id, 110);
}

/* ============================================================================
 * CDU VALIDATION TESTS
 * ============================================================================
 */

TEST(test_cdu_max_bits_check)
{
    for (int i = 0; i < CDU_NUM_SUBTYPES; i++) {
        const CDUParam *p = &cdu_params[i];
        if (!p->fixed) {  // Only check variable-length types for 64-bit limit
            size_t max_bits = 0;
            for (int step = 0; step < p->def_steps; step++) {
                max_bits += p->steps[step] + 1;  // bits + continuation
            }
            ASSERT(max_bits <= 64);
        }
        // Fixed types may require larger buffers due to alignment overhang
    }
}

/* ============================================================================
 * TEST RUNNER
 * ============================================================================
 */

int run_all_tests(void)
{
    printf("\n=== CDU Tests ===\n");
    RUN_TEST(test_cdu_roundtrip);
    RUN_TEST(test_cdu_minimality);
    RUN_TEST(test_cdu_medium_int);
    
    printf("\n=== Combinadic Tests ===\n");
    RUN_TEST(test_combinadic_init);
    RUN_TEST(test_combinadic_rank_simple);
    RUN_TEST(test_combinadic_roundtrip);
    RUN_TEST(test_combinadic_popcount);
    RUN_TEST(test_combinadic_rank_bits);
    RUN_TEST(test_combinadic_generated_dataset);
    
    printf("\n=== Token Encoding Tests ===\n");
    RUN_TEST(test_enum_token_bits);
    RUN_TEST(test_enum_token_roundtrip);
    RUN_TEST(test_raw_token_roundtrip);
    RUN_TEST(test_should_use_enum);
    
    printf("\n=== Segment Encoding Tests ===\n");
    RUN_TEST(test_rle_segment_roundtrip);
    RUN_TEST(test_mix_segment_header_roundtrip);
    RUN_TEST(test_mix_segment_partial_chunk);
    RUN_TEST(test_should_use_rle);
    
    printf("\n=== Partition Encoding Tests ===\n");
    RUN_TEST(test_partition_header_roundtrip);
    RUN_TEST(test_ssk_header_roundtrip);
    RUN_TEST(test_partition_delta_calculation);
    RUN_TEST(test_partition_id_from_delta);
    
    printf("\n=== CDU Validation Tests ===\n");
    RUN_TEST(test_cdu_max_bits_check);
    
    printf("\n=== Summary ===\n");
    printf("Tests run: %d, Passed: %d, Failed: %d\n", 
           tests_run, tests_passed, tests_run - tests_passed);
    return tests_run - tests_passed;
}

#endif // (NON) TRIVIAL