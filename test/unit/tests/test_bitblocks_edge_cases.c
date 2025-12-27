/*
 * test_bitblocks_edge_cases.c
 * 
 * Edge case testing for bitblocks.h functions.
 * FOCUS: Misaligned bit positions and lengths
 * 
 * The critical test cases are when read/write operations:
 * - Start at non-byte-aligned positions (bit_pos % 8 != 0)
 * - Have lengths that don't align to byte boundaries (n_bits % 8 != 0)
 * - Span across 64-bit unit boundaries with misaligned start/end
 * 
 * These test the core arithmetic: bit_offset, bit_shift, mask calculation,
 * and verify that shift/mask values are computed correctly.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* Include the header under test */
#include "bitblocks.h"

#define TEST_PASS() do { printf("  ✓\n"); } while(0)
#define TEST_FAIL(msg) do { printf("  ✗ %s\n", msg); return 1; } while(0)
#define SECTION(name) printf("\n%s:\n", name)

/* Test 1: Misaligned reads - various start offsets and lengths */
static int
test_misaligned_reads()
{
    printf("  Test: Misaligned reads (non-byte-aligned bit positions)\n");
    
    uint8_t buf[16];
    
    /* Test case 1: Read 3 bits starting at bit offset 1 */
    memset(buf, 0x00, sizeof(buf));
    buf[0] = 0xAE;  /* 10101110 - bits 1,2,3 are 111 = 0x7 */
    uint64_t val = bb_read_bits(buf, 1, 3);
    if (val != 0x07)  /* bits 1,2,3 = 111 = 7 */
        TEST_FAIL("Read 3 bits at offset 1: wrong value");
    TEST_PASS();
    
    /* Test case 2: Read 7 bits starting at bit offset 3 */
    memset(buf, 0x00, sizeof(buf));
    buf[0] = 0xF8;  /* 11111000 - bits 3-7 are 11111 */
    buf[1] = 0x01;  /* 00000001 - bits 0-1 (of byte 1) are 01 when read as continuation */
    val = bb_read_bits(buf, 3, 7);
    /* Reading 7 bits from position 3: bits 3,4,5,6,7,8,9
       From byte 0: bits 3-7 = 11111 
       From byte 1: bits 0-1 = 01
       Result: 01_11111 = 0x3F */
    if (val != 0x3F)
        TEST_FAIL("Read 7 bits at offset 3: wrong value");
    TEST_PASS();
    
    /* Test case 3: Read 11 bits starting at bit offset 5 */
    memset(buf, 0x00, sizeof(buf));
    buf[0] = 0xE0;  /* 11100000 - bits 5-7 = 111 */
    buf[1] = 0xFF;  /* 11111111 - bits 0-7 all set */
    buf[2] = 0x01;  /* 00000001 - bit 0 set */
    val = bb_read_bits(buf, 5, 11);
    /* Reading 11 bits from position 5: bits 5-15
       From byte 0: bits 5-7 = 111
       From byte 1: bits 0-7 = 11111111
       From byte 2: bits 0-0 = 1
       Result: 1_11111111_111 = 0x7FF */
    if (val != 0x7FF)
        TEST_FAIL("Read 11 bits at offset 5: wrong value");
    TEST_PASS();
    
    /* Test case 4: Read 17 bits starting at bit offset 31 (crosses 64-bit boundary) */
    memset(buf, 0x00, sizeof(buf));
    /* Position 31 is within first 64-bit unit (0-63) */
    buf[3] = 0x80;  /* 10000000 - bit 31 set */
    buf[4] = 0xFF;  /* All bits 32-39 set */
    buf[5] = 0xFF;  /* All bits 40-47 set */
    buf[6] = 0x03;  /* 00000011 - bits 48-49 set */
    val = bb_read_bits(buf, 31, 17);
    /* Should read 17 bits starting at position 31 */
    if (val != 0x1FFFF)  /* All 17 bits set = 0x1FFFF */
        TEST_FAIL("Read 17 bits at offset 31: wrong value");
    TEST_PASS();
    
    /* Test case 5: Read 1 bit at every odd position (1,3,5,7,9,11,13,15) */
    for (int offset = 1; offset < 16; offset += 2) {
        memset(buf, 0x00, sizeof(buf));
        uint8_t byte_idx = offset / 8;
        uint8_t bit_idx = offset % 8;
        buf[byte_idx] = (1 << bit_idx);
        val = bb_read_bits(buf, offset, 1);
        if (val != 1)
            TEST_FAIL("Read 1 bit at odd offset: wrong value");
    }
    TEST_PASS();
    
    return 0;
}

/* Test 2: Misaligned writes - verify correct bits modified */
static int
test_misaligned_writes()
{
    printf("  Test: Misaligned writes (non-byte-aligned bit positions and lengths)\n");
    
    uint8_t buf[16];
    
    /* Test case 1: Write 5 bits at offset 2 (spans 1 byte) */
    memset(buf, 0xFF, sizeof(buf));
    bb_write_bits(buf, 2, 0x05, 5);  /* 00101 */
    /* Writing bits 2-6: mask = 0b01111100 = 0x7C
       value = 0x05 << 2 = 0x14
       result = (0xFF & ~0x7C) | (0x14 & 0x7C) = 0x83 | 0x14 = 0x97 */
    if (buf[0] != 0x97)
        TEST_FAIL("Write 5 bits at offset 2: byte 0 wrong");
    if (buf[1] != 0xFF)
        TEST_FAIL("Write 5 bits at offset 2: byte 1 corrupted");
    TEST_PASS();
    
    /* Test case 2: Write 7 bits at offset 3 (spans 2 bytes) */
    memset(buf, 0x00, sizeof(buf));
    bb_write_bits(buf, 3, 0x7F, 7);  /* 1111111 */
    /* Bits 3-9: byte 0 gets bits 3-7 = 11111000 = 0xF8, byte 1 gets bits 0-1 = 00000011 = 0x03 */
    if (buf[0] != 0xF8)
        TEST_FAIL("Write 7 bits at offset 3: byte 0 wrong");
    if (buf[1] != 0x03)
        TEST_FAIL("Write 7 bits at offset 3: byte 1 wrong");
    if (buf[2] != 0x00)
        TEST_FAIL("Write 7 bits at offset 3: byte 2 corrupted");
    TEST_PASS();
    
    /* Test case 3: Write 13 bits at offset 5 (spans 3 bytes) */
    memset(buf, 0xFF, sizeof(buf));
    bb_write_bits(buf, 5, 0x1555, 13);  /* Pattern with misaligned start/end */
    /* Verify neighboring bytes untouched and target bits correct */
    /* Bits 0-4 of byte 0 should still be 0xFF & 0x1F = 0x1F */
    if ((buf[0] & 0x1F) != 0x1F)
        TEST_FAIL("Write 13 bits at offset 5: byte 0 low bits corrupted");
    if (buf[4] != 0xFF)
        TEST_FAIL("Write 13 bits at offset 5: byte 4 corrupted");
    /* Read back to verify */
    uint64_t val = bb_read_bits(buf, 5, 13);
    if (val != 0x1555)
        TEST_FAIL("Write 13 bits at offset 5: readback mismatch");
    TEST_PASS();
    
    /* Test case 4: Write 27 bits at offset 11 (spans 4 bytes, crosses 64-bit boundary at 64) */
    memset(buf, 0xAA, sizeof(buf));
    bb_write_bits(buf, 11, 0x12345, 27);
    /* Verify boundary bytes untouched */
    if ((buf[0] & 0x07) != 0x02)  /* Bits 0-2 should be untouched (from original 0xAA = ...010) */
        TEST_FAIL("Write 27 bits at offset 11: byte 0 low bits corrupted");
    if (buf[5] != 0xAA)
        TEST_FAIL("Write 27 bits at offset 11: byte 5 corrupted");
    /* Readback verification */
    val = bb_read_bits(buf, 11, 27);
    if (val != 0x12345)
        TEST_FAIL("Write 27 bits at offset 11: readback mismatch");
    TEST_PASS();
    
    /* Test case 5: Write 1 bit at every odd position, verify no other bits touched */
    for (int offset = 1; offset < 16; offset += 2) {
        memset(buf, 0x00, sizeof(buf));
        bb_write_bits(buf, offset, 1, 1);
        uint8_t byte_idx = offset / 8;
        uint8_t bit_idx = offset % 8;
        uint8_t expected = (1 << bit_idx);
        if (buf[byte_idx] != expected)
            TEST_FAIL("Write 1 bit at odd offset: wrong position");
        /* Verify other bytes untouched */
        for (int i = 0; i < sizeof(buf); i++) {
            if (i != byte_idx && buf[i] != 0x00)
                TEST_FAIL("Write 1 bit at odd offset: other bytes corrupted");
        }
    }
    TEST_PASS();
    
    return 0;
}

/* Test 3: Read/write round-trip with misalignment */
static int
test_roundtrip_misaligned()
{
    printf("  Test: Round-trip read/write with misaligned positions\n");
    
    uint8_t buf[16];
    
    /* Test case 1: Write then read back various bit patterns at offset 3 */
    uint64_t test_values[] = {0x0, 0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF};
    int bit_lengths[] = {1, 2, 3, 4, 5, 6, 7, 8};
    
    for (int v = 0; v < 9; v++) {
        for (int len = 1; len <= 8 && len <= (1 << v); len++) {
            memset(buf, 0xAA, sizeof(buf));
            uint64_t test_val = test_values[v] & ((1ULL << len) - 1);
            bb_write_bits(buf, 3, test_val, len);
            uint64_t readback = bb_read_bits(buf, 3, len);
            if (readback != test_val) {
                printf("      Mismatch at offset 3, len %d, val 0x%llx: got 0x%llx\n",
                       len, (unsigned long long)test_val, (unsigned long long)readback);
                TEST_FAIL("Round-trip mismatch");
            }
        }
    }
    TEST_PASS();
    
    /* Test case 2: Cross-boundary write/read at position 60 (near 64-bit edge) */
    memset(buf, 0xFF, sizeof(buf));
    bb_write_bits(buf, 60, 0xABCD, 16);
    uint64_t val = bb_read_bits(buf, 60, 16);
    if (val != 0xABCD)
        TEST_FAIL("Round-trip at position 60: mismatch");
    TEST_PASS();
    
    /* Test case 3: Multiple misaligned fields in sequence */
    memset(buf, 0x00, sizeof(buf));
    bb_write_bits(buf, 1, 0x1, 3);   /* 3 bits at offset 1 */
    bb_write_bits(buf, 5, 0x5, 4);   /* 4 bits at offset 5 */
    bb_write_bits(buf, 10, 0xF, 5);  /* 5 bits at offset 10 */
    
    uint64_t v1 = bb_read_bits(buf, 1, 3);
    uint64_t v2 = bb_read_bits(buf, 5, 4);
    uint64_t v3 = bb_read_bits(buf, 10, 5);
    
    if (v1 != 0x1 || v2 != 0x5 || v3 != 0xF)
        TEST_FAIL("Round-trip sequential fields: mismatch");
    TEST_PASS();
    
    return 0;
}

/* Test 4: Mask correctness at various bit offsets */
static int
test_mask_calculation()
{
    printf("  Test: Mask calculation at various bit offsets\n");
    
    uint8_t buf[16];
    
    /* Test case 1: Single bit set at positions 0-15 should set exactly one bit */
    for (int pos = 0; pos < 16; pos++) {
        memset(buf, 0x00, sizeof(buf));
        bb_write_bits(buf, pos, 1, 1);
        
        /* Count set bits - should be exactly 1 */
        int count = 0;
        for (int i = 0; i < 2; i++) {
            uint8_t byte = buf[i];
            for (int b = 0; b < 8; b++) {
                if (byte & (1 << b)) count++;
            }
        }
        if (count != 1) {
            printf("      Position %d: set %d bits (expected 1)\n", pos, count);
            TEST_FAIL("Single bit write: wrong bit count");
        }
    }
    TEST_PASS();
    
    /* Test case 2: Verify 64-bit mask at different shifts (0, 16, 32, 48) */
    for (int shift = 0; shift < 64; shift += 16) {
        memset(buf, 0xFF, sizeof(buf));
        bb_write_bits(buf, shift, 0, 16);  /* Write 16 zeros */
        
        /* Verify zeros in right place, 0xFF elsewhere */
        for (int byte_idx = 0; byte_idx < 8; byte_idx++) {
            uint8_t expected = 0xFF;
            int bit_start = byte_idx * 8;
            int bit_end = bit_start + 7;
            if (bit_start < shift || bit_end < shift) {
                /* Before the zero region */
            } else if (bit_start >= shift + 16 || bit_end >= shift + 16) {
                /* After the zero region */
            } else {
                /* In the zero region */
                expected = 0x00;
            }
        }
    }
    TEST_PASS();
    
    return 0;
}

/* Test 5: bb_copy_bits with misaligned source and destination */
static int
test_copy_misaligned()
{
    printf("  Test: Copy bits with misaligned source and destination\n");
    
    uint8_t src[16];
    uint8_t dst[16];
    
    /* Test case 1: Copy 7 bits from misaligned source to misaligned destination */
    memset(src, 0x00, sizeof(src));
    memset(dst, 0xFF, sizeof(dst));
    src[0] = 0xFE;  /* 11111110 - bits 1-7 all set */
    bb_copy_bits(src, 1, dst, 3, 7);
    
    /* Verify destination bits 3-9 contain source bits 1-7 */
    uint64_t val = bb_read_bits(dst, 3, 7);
    if (val != 0x7F)  /* 7 bits all set */
        TEST_FAIL("Copy misaligned 7 bits: wrong value");
    if ((dst[0] & 0x07) != 0x07)  /* Bits 0-2 should stay set (from 0xFF) */
        TEST_FAIL("Copy misaligned: dst bits 0-2 corrupted");
    if (dst[2] != 0xFF)
        TEST_FAIL("Copy misaligned: dst byte 2 corrupted");
    TEST_PASS();
    
    /* Test case 2: Copy 23 bits across multiple bytes */
    memset(src, 0xAA, sizeof(src));  /* Alternating pattern */
    memset(dst, 0x00, sizeof(dst));
    bb_copy_bits(src, 5, dst, 11, 23);
    
    for (int i = 0; i < 23; i++) {
        int src_bit = 5 + i;
        int src_byte = src_bit / 8;
        int src_bit_in_byte = src_bit % 8;
        int src_expected = (src[src_byte] >> src_bit_in_byte) & 1;
        
        int dst_bit = 11 + i;
        int dst_byte = dst_bit / 8;
        int dst_bit_in_byte = dst_bit % 8;
        int dst_actual = (dst[dst_byte] >> dst_bit_in_byte) & 1;
        
        if (dst_actual != src_expected) {
            printf("      Bit %d mismatch: expected %d, got %d\n", i, src_expected, dst_actual);
            TEST_FAIL("Copy 23 bits: bit mismatch");
        }
    }
    TEST_PASS();
    
    return 0;
}

/* Test 6: bb_place_fl_block and bb_fetch_fl_block at critical 64-bit boundaries */
static int
test_fl_block_critical_offsets()
{
    printf("  Test: FL block operations at 64-bit modulo boundary offsets\n");
    
    uint8_t buf[32];
    
    /* Critical offsets that expose % 63 vs % 64 arithmetic errors:
       - 63: last bit before 64-bit unit boundary
       - 64: first bit of next unit
       - 127: last bit before second boundary
       - 128: first bit of third unit
     */
    
    /* Test case 1: Place 32 bits at offset 63 (crosses 64-bit boundary) */
    memset(buf, 0x00, sizeof(buf));
    uint32_t test_val_1 = 0xDEADBEEF;
    bb_place_fl_block(buf, 63, test_val_1, 32);
    uint64_t readback_1 = bb_fetch_fl_block(buf, 63, 32);
    if (readback_1 != test_val_1) {
        printf("      Offset 63, 32 bits: wrote 0x%08x, read 0x%llx\n",
               test_val_1, (unsigned long long)readback_1);
        TEST_FAIL("FL block at offset 63: mismatch");
    }
    TEST_PASS();
    
    /* Test case 2: Place 16 bits at offset 64 (aligned to second unit) */
    memset(buf, 0x00, sizeof(buf));
    uint32_t test_val_2 = 0xCAFE;
    bb_place_fl_block(buf, 64, test_val_2, 16);
    uint64_t readback_2 = bb_fetch_fl_block(buf, 64, 16);
    if (readback_2 != test_val_2) {
        printf("      Offset 64, 16 bits: wrote 0x%x, read 0x%llx\n",
               test_val_2, (unsigned long long)readback_2);
        TEST_FAIL("FL block at offset 64: mismatch");
    }
    TEST_PASS();
    
    /* Test case 3: Place 48 bits at offset 48 (within first unit, close to boundary) */
    memset(buf, 0x00, sizeof(buf));
    uint64_t test_val_3 = 0x123456789ABC;
    bb_place_fl_block(buf, 48, test_val_3, 48);
    uint64_t readback_3 = bb_fetch_fl_block(buf, 48, 48);
    if (readback_3 != test_val_3) {
        printf("      Offset 48, 48 bits: wrote 0x%llx, read 0x%llx\n",
               (unsigned long long)test_val_3, (unsigned long long)readback_3);
        TEST_FAIL("FL block at offset 48: mismatch");
    }
    TEST_PASS();
    
    /* Test case 4: Place 32 bits at offset 1 (misaligned start) */
    memset(buf, 0xFF, sizeof(buf));
    uint32_t test_val_4 = 0x55555555;
    bb_place_fl_block(buf, 1, test_val_4, 32);
    uint64_t readback_4 = bb_fetch_fl_block(buf, 1, 32);
    if (readback_4 != test_val_4) {
        printf("      Offset 1, 32 bits: wrote 0x%x, read 0x%llx\n",
               test_val_4, (unsigned long long)readback_4);
        TEST_FAIL("FL block at offset 1: mismatch");
    }
    /* Verify neighboring bits untouched */
    if ((buf[0] & 0x01) != 0x01)
        TEST_FAIL("FL block at offset 1: bit 0 corrupted");
    TEST_PASS();
    
    /* Test case 5: Place 64 bits at offset 0 (full unit) */
    memset(buf, 0x00, sizeof(buf));
    uint64_t test_val_5 = 0x0102030405060708ULL;
    bb_place_fl_block(buf, 0, test_val_5, 64);
    uint64_t readback_5 = bb_fetch_fl_block(buf, 0, 64);
    if (readback_5 != test_val_5) {
        printf("      Offset 0, 64 bits: wrote 0x%llx, read 0x%llx\n",
               (unsigned long long)test_val_5, (unsigned long long)readback_5);
        TEST_FAIL("FL block at offset 0: mismatch");
    }
    TEST_PASS();
    
    /* Test case 6: Place 24 bits at offset 31 (crosses byte/bit boundary awkwardly) */
    memset(buf, 0x00, sizeof(buf));
    uint32_t test_val_6 = 0xABCDEF;
    bb_place_fl_block(buf, 31, test_val_6, 24);
    uint64_t readback_6 = bb_fetch_fl_block(buf, 31, 24);
    if (readback_6 != test_val_6) {
        printf("      Offset 31, 24 bits: wrote 0x%x, read 0x%llx\n",
               test_val_6, (unsigned long long)readback_6);
        TEST_FAIL("FL block at offset 31: mismatch");
    }
    TEST_PASS();
    
    /* Test case 7: Sequential writes at critical offsets verify no interference */
    memset(buf, 0x00, sizeof(buf));
    bb_place_fl_block(buf, 0, 0x11111111, 32);
    bb_place_fl_block(buf, 32, 0x22222222, 32);
    bb_place_fl_block(buf, 64, 0x33333333, 32);
    
    uint64_t v1 = bb_fetch_fl_block(buf, 0, 32);
    uint64_t v2 = bb_fetch_fl_block(buf, 32, 32);
    uint64_t v3 = bb_fetch_fl_block(buf, 64, 32);
    
    if (v1 != 0x11111111 || v2 != 0x22222222 || v3 != 0x33333333) {
        printf("      Sequential writes: got 0x%llx, 0x%llx, 0x%llx\n",
               (unsigned long long)v1, (unsigned long long)v2, (unsigned long long)v3);
        TEST_FAIL("Sequential FL block writes: mismatch");
    }
    TEST_PASS();
    
    return 0;
}

/* Test 7: Exhaustive coverage of all start offsets and bit lengths */
static int
test_fl_block_exhaustive_alignment()
{
    printf("  Test: Exhaustive start offset × bit length combinations\n");
    
    uint8_t buf[128];  /* Large buffer to accommodate all test ranges */
    
    /* Two test ranges:
       1. Offsets 0-127, lengths 0-64: covers first two 64-bit units completely
       2. Offsets 380-508, lengths 0-64: arbitrary depth in buffer
    */
    
    int test_ranges[][2] = {
        {0, 127},       /* Range 1: First two units */
        {380, 508}      /* Range 2: Arbitrary depth (offsets 380-508 = bits in units 5-7) */
    };
    
    int total_tests = 0;
    
    for (int r = 0; r < sizeof(test_ranges) / sizeof(test_ranges[0]); r++) {
        int start_offset = test_ranges[r][0];
        int end_offset = test_ranges[r][1];
        
        for (int offset = start_offset; offset <= end_offset; offset++) {
            for (int bit_length = 0; bit_length <= 64; bit_length++) {
                /* Skip combinations that would require reading/writing beyond buffer */
                if (offset + bit_length > 1024)
                    continue;
                
                total_tests++;
                
                memset(buf, 0x00, sizeof(buf));
                
                if (bit_length == 0) {
                    /* Test zero-length read/write */
                    bb_place_fl_block(buf, offset, 0, 0);
                    uint64_t readback = bb_fetch_fl_block(buf, offset, 0);
                    if (readback != 0) {
                        printf("      Offset %d, Length 0: read non-zero\n", offset);
                        TEST_FAIL("Exhaustive test: zero-length failed");
                    }
                    continue;
                }
                
                /* Create distinctive pattern that encodes both offset and length */
                uint64_t test_pattern = ((uint64_t)offset << 32) | (uint64_t)bit_length;
                if (bit_length < 64)
                    test_pattern &= (1ULL << bit_length) - 1;  /* Mask to actual bit length */
                
                /* Write at this offset and length */
                bb_place_fl_block(buf, offset, test_pattern, bit_length);
                
                /* Read back immediately */
                uint64_t readback = bb_fetch_fl_block(buf, offset, bit_length);
                
                /* Verify exact match */
                if (readback != test_pattern) {
                    printf("      Offset %d, Length %d: wrote 0x%llx, read 0x%llx\n",
                           offset, bit_length, (unsigned long long)test_pattern, (unsigned long long)readback);
                    TEST_FAIL("Exhaustive test: value mismatch");
                }
                
                /* Verify no bits set before target range (at start of affected bytes) */
                if (offset > 0) {
                    int first_byte = offset / 8;
                    int first_bit_in_byte = offset % 8;
                    if (first_bit_in_byte > 0) {
                        /* Check bits before target in first affected byte */
                        uint8_t mask_before = (1 << first_bit_in_byte) - 1;
                        if ((buf[first_byte] & mask_before) != 0) {
                            printf("      Offset %d, Length %d: bits before target corrupted\n",
                                   offset, bit_length);
                            TEST_FAIL("Exhaustive test: bits before target corrupted");
                        }
                    }
                }
                
                /* Verify no bits set after target range (at end of affected bytes) */
                int end_bit = offset + bit_length;
                if (end_bit < 1024) {
                    int last_byte = (end_bit - 1) / 8;
                    int last_bit_in_byte = (end_bit - 1) % 8;
                    if (last_bit_in_byte < 7) {
                        /* Check bits after target in last affected byte */
                        uint8_t mask_after = ~((1 << (last_bit_in_byte + 1)) - 1);
                        if ((buf[last_byte] & mask_after) != 0) {
                            printf("      Offset %d, Length %d: bits after target corrupted\n",
                                   offset, bit_length);
                            TEST_FAIL("Exhaustive test: bits after target corrupted");
                        }
                    }
                    
                    /* Verify all bytes after the target range are untouched (0x00) */
                    int first_untouched_byte = (end_bit + 7) / 8;
                    for (int i = first_untouched_byte; i < sizeof(buf); i++) {
                        if (buf[i] != 0x00) {
                            printf("      Offset %d, Length %d: byte %d corrupted\n",
                                   offset, bit_length, i);
                            TEST_FAIL("Exhaustive test: buffer beyond target corrupted");
                        }
                    }
                }
            }
        }
    }
    
    printf("    (ran %d combinations) ", total_tests);
    TEST_PASS();
    
    return 0;
}

/* Test 8: Verify pattern integrity with overlapping and sequential operations */
static int
test_fl_block_pattern_integrity()
{
    printf("  Test: Pattern integrity with overlapping and sequential operations\n");
    
    uint8_t buf[64];
    
    /* Test 1: Write multiple non-overlapping fields, verify each independently */
    memset(buf, 0x00, sizeof(buf));
    
    bb_place_fl_block(buf, 0, 0x11223344, 32);
    bb_place_fl_block(buf, 32, 0x55667788, 32);
    
    uint64_t r1 = bb_fetch_fl_block(buf, 0, 32);
    uint64_t r2 = bb_fetch_fl_block(buf, 32, 32);
    
    if (r1 != 0x11223344 || r2 != 0x55667788) {
        printf("      Sequential 32-bit writes: got 0x%llx, 0x%llx\n",
               (unsigned long long)r1, (unsigned long long)r2);
        TEST_FAIL("Pattern integrity: sequential writes mismatch");
    }
    TEST_PASS();
    
    /* Test 2: Mixed length writes at various offsets */
    memset(buf, 0x00, sizeof(buf));
    
    bb_place_fl_block(buf, 1, 0x7F, 7);      /* 7 bits at offset 1 */
    bb_place_fl_block(buf, 10, 0xFF, 8);     /* 8 bits at offset 10 */
    bb_place_fl_block(buf, 20, 0x3FF, 10);   /* 10 bits at offset 20 */
    bb_place_fl_block(buf, 32, 0xFFFF, 16);  /* 16 bits at offset 32 */
    
    uint64_t m1 = bb_fetch_fl_block(buf, 1, 7);
    uint64_t m2 = bb_fetch_fl_block(buf, 10, 8);
    uint64_t m3 = bb_fetch_fl_block(buf, 20, 10);
    uint64_t m4 = bb_fetch_fl_block(buf, 32, 16);
    
    if (m1 != 0x7F || m2 != 0xFF || m3 != 0x3FF || m4 != 0xFFFF) {
        printf("      Mixed lengths: got 0x%llx, 0x%llx, 0x%llx, 0x%llx\n",
               (unsigned long long)m1, (unsigned long long)m2, 
               (unsigned long long)m3, (unsigned long long)m4);
        TEST_FAIL("Pattern integrity: mixed length mismatch");
    }
    
    /* Verify boundary bits untouched */
    if ((buf[0] & 0x01) != 0x00) {
        printf("      Mixed lengths: bit 0 corrupted\n");
        TEST_FAIL("Pattern integrity: boundary bit corrupted");
    }
    TEST_PASS();
    
    /* Test 3: Write, modify, re-read pattern (RMW cycle) */
    memset(buf, 0x00, sizeof(buf));
    
    bb_place_fl_block(buf, 5, 0xFF, 8);
    uint64_t before = bb_fetch_fl_block(buf, 5, 8);
    
    bb_place_fl_block(buf, 5, 0xAA, 8);  /* Overwrite */
    uint64_t after = bb_fetch_fl_block(buf, 5, 8);
    
    if (before != 0xFF || after != 0xAA) {
        printf("      RMW cycle: before 0x%llx, after 0x%llx\n",
               (unsigned long long)before, (unsigned long long)after);
        TEST_FAIL("Pattern integrity: RMW cycle failed");
    }
    TEST_PASS();
    
    return 0;
}

int
main(void)
{
    int failures = 0;
    
    printf("\n========================================\n");
    printf("BITBLOCKS.H MISALIGNMENT EDGE CASE TESTS\n");
    printf("Focus: Non-byte-aligned bit positions\n");
    printf("        and lengths\n");
    printf("========================================\n");
    
    SECTION("Misaligned Reads");
    failures += test_misaligned_reads();
    
    SECTION("Misaligned Writes");
    failures += test_misaligned_writes();
    
    SECTION("Round-trip Misaligned");
    failures += test_roundtrip_misaligned();
    
    SECTION("Mask Calculation");
    failures += test_mask_calculation();
    
    SECTION("Copy with Misalignment");
    failures += test_copy_misaligned();
    
    SECTION("FL Block Operations at Critical Offsets");
    failures += test_fl_block_critical_offsets();
    
    SECTION("FL Block Operations at ALL Offsets (0-63)");
    failures += test_fl_block_exhaustive_alignment();
    
    SECTION("FL Block Pattern Integrity");
    failures += test_fl_block_pattern_integrity();
    
    printf("\n========================================\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED ✓\n");
        printf("Bit offset/length arithmetic verified.\n");
    } else {
        printf("FAILURES: %d\n", failures);
    }
    printf("========================================\n\n");
    
    return failures ? 1 : 0;
}

