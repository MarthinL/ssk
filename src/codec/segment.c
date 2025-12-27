/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

#ifdef TRIVIAL

/*
 * None of what is defined in this file plays a role in the TRIVIAL case.
 * It purely addresses issues arising from upscaling the ID domain to BIGINT scale.
 */

#else // NON TRIVIAL

/*
 * src/codec/segment.c
 *
 * Segment encoding/decoding for SSK
 *
 * A segment represents a contiguous range of the abvector (within a partition).
 * Segment types:
 *   - RLE (0): Run-Length Encoding for homogeneous runs (all 0s or all 1s)
 *   - MIX (1): Mixed segments containing both 0s and 1s, stored as tokens
 *
 * RLE Segment Format:
 *   [1 bit] segment_type = 0
 *   [1 bit] membership_bit (the repeated bit value)
 *   [CDU] length_bits (number of bits in this run)
 *
 * MIX Segment Format:
 *   [1 bit] segment_type = 1
 *   [CDU] initial_delta (bit offset from partition start or previous segment end)
 *   [CDU] length_bits (total bits in this segment)
 *   [tokens...] sequence of ENUM/RAW/RAW_RUN/ENUM_RUN tokens
 *
 * Note: last_chunk_nbits is NOT stored - derivable as (length_bits % 64) with 0 meaning 64.
 * Similarly, chunk count = (length_bits + 63) / 64. Single source of truth prevents
 * inconsistency between redundant stored values.
 *
 * Canon rules:
 *   - Segments are non-overlapping and ascending within partition
 *   - RLE only for runs >= RARE_RUN_THRESHOLD that span entire segment
 *   - MIX for everything else
 *   - Segment boundaries at dominant gaps >= DOMINANT_RUN_THRESHOLD
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ssk.h"
#include "abv_format.h"
#include "abv_constants.h"
#include "bitblocks.h"

/* ============================================================================
 * RLE SEGMENT ENCODING
 * ============================================================================
 */

/**
 * Compute bits needed for an RLE segment.
 *
 * @param length_bits  Number of bits in the run
 * @param spec         Format specification
 * @return Total bits needed for segment encoding
 */
size_t
rle_segment_bits(uint64_t length_bits, const SSKFormatSpec *spec)
{
    /* 1 (type) + 1 (membership_bit) + CDU(length_bits) */
    uint8_t dummy_buf[8];
    size_t cdu_bits = cdu_encode(length_bits, spec->cdu_length_bits, dummy_buf, 0);
    return 1 + 1 + cdu_bits;
}

/**
 * Encode an RLE segment.
 *
 * @param membership_bit  The repeated bit value (0 or 1)
 * @param length_bits     Number of bits in the run
 * @param spec            Format specification
 * @param buf             Output buffer
 * @param bit_pos         Starting bit position
 * @return Number of bits written
 */
size_t
rle_segment_encode(uint8_t membership_bit, uint64_t length_bits,
                   const SSKFormatSpec *spec, uint8_t *buf, size_t bit_pos)
{
    size_t start_pos = bit_pos;
    
    /* 1. Segment type (1 bit): 0 = RLE */
    bb_write_bits(buf, bit_pos, SEG_RLE, 1);
    bit_pos += 1;
    
    /* 2. Membership bit (1 bit) */
    bb_write_bits(buf, bit_pos, membership_bit & 1, 1);
    bit_pos += 1;
    
    /* 3. Length in bits (CDU) */
    size_t cdu_written = cdu_encode(length_bits, spec->cdu_length_bits, buf, bit_pos);
    bit_pos += cdu_written;
    
    return bit_pos - start_pos;
}

/**
 * Decode an RLE segment header.
 *
 * @param buf              Input buffer
 * @param bit_pos          Starting bit position (after segment type read)
 * @param buf_bits         Total bits available
 * @param membership_out   Output: the repeated bit value
 * @param length_out       Output: number of bits in run
 * @param bits_read        Output: bits consumed (excluding type bit)
 * @return 0 on success, -1 on error
 *
 * Note: Caller has already read and verified segment_type = RLE.
 *       Spec Format 0: length_bits uses CDU_MEDIUM_INT.
 */
int
rle_segment_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                   uint8_t *membership_out, uint64_t *length_out, size_t *bits_read)
{
    size_t start_pos = bit_pos;
    
    /* 1. Read membership bit */
    if (bit_pos >= buf_bits)
        return -1;
    
    *membership_out = (uint8_t)bb_read_bits(buf, bit_pos, 1);
    bit_pos += 1;
    
    /* 2. Read length (CDU MEDIUM_INT) */
    uint64_t length;
    size_t cdu_read = cdu_decode(buf, bit_pos, buf_bits, SSK_CDU_LENGTH_BITS, &length);
    if (cdu_read == 0)
        return -1;
    
    bit_pos += cdu_read;
    
    /* Validate length */
    if (length == 0)
        return -1;  /* Empty RLE segment is invalid */
    
    *length_out = length;
    *bits_read = bit_pos - start_pos;
    
    return 0;
}

/* ============================================================================
 * MIX SEGMENT ENCODING
 * ============================================================================
 */

/**
 * Compute bits needed for MIX segment header (excluding tokens).
 *
 * @param initial_delta    Bit offset from partition start or previous segment
 * @param length_bits      Total bits in this segment
 * @param spec             Format specification
 * @return Bits needed for header (add token bits separately)
 */
size_t
mix_segment_header_bits(uint64_t initial_delta, uint64_t length_bits,
                        const SSKFormatSpec *spec)
{
    /* 1 (type) + CDU(initial_delta) + CDU(length_bits) */
    uint8_t dummy_buf[8];
    size_t delta_bits = cdu_encode(initial_delta, spec->cdu_initial_delta, dummy_buf, 0);
    size_t length_cdu = cdu_encode(length_bits, spec->cdu_length_bits, dummy_buf, 0);
    
    return 1 + delta_bits + length_cdu;
}

/**
 * Encode a MIX segment header (tokens encoded separately).
 *
 * @param initial_delta    Bit offset from start/previous segment
 * @param length_bits      Total bits in this segment
 * @param spec             Format specification
 * @param buf              Output buffer
 * @param bit_pos          Starting bit position
 * @return Bits written for header
 */
size_t
mix_segment_header_encode(uint64_t initial_delta, uint64_t length_bits,
                          const SSKFormatSpec *spec,
                          uint8_t *buf, size_t bit_pos)
{
    size_t start_pos = bit_pos;
    
    /* 1. Segment type (1 bit): 1 = MIX */
    bb_write_bits(buf, bit_pos, SEG_MIX, 1);
    bit_pos += 1;
    
    /* 2. Initial delta (CDU) */
    size_t delta_written = cdu_encode(initial_delta, spec->cdu_initial_delta, buf, bit_pos);
    bit_pos += delta_written;
    
    /* 3. Length in bits (CDU) */
    size_t length_written = cdu_encode(length_bits, spec->cdu_length_bits, buf, bit_pos);
    bit_pos += length_written;
    
    /* last_chunk_nbits is NOT encoded - derivable as length_bits % 64 (0 means 64) */
    
    return bit_pos - start_pos;
}

/**
 * Decode a MIX segment header.
 *
 * @param buf              Input buffer
 * @param bit_pos          Starting bit position (after segment type read)
 * @param buf_bits         Total bits available
 * @param delta_out        Output: initial delta
 * @param length_out       Output: total bits in segment
 * @param bits_read        Output: bits consumed (excluding type bit)
 * @return 0 on success, -1 on error
 *
 * Spec Format 0: initial_delta uses CDU_LARGE_INT, length_bits uses CDU_MEDIUM_INT.
 * 
 * Note: last_chunk_nbits is derivable from length_out as (*length_out % 64),
 * where 0 means 64. Caller should use segment_last_chunk_nbits() helper.
 */
int
mix_segment_header_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                          uint64_t *delta_out, uint64_t *length_out,
                          size_t *bits_read)
{
    size_t start_pos = bit_pos;
    
    /* 1. Read initial delta (CDU LARGE_INT) */
    uint64_t delta;
    size_t cdu_read = cdu_decode(buf, bit_pos, buf_bits, SSK_CDU_INITIAL_DELTA, &delta);
    if (cdu_read == 0)
        return -1;
    bit_pos += cdu_read;
    
    /* 2. Read length (CDU MEDIUM_INT) */
    uint64_t length;
    cdu_read = cdu_decode(buf, bit_pos, buf_bits, SSK_CDU_LENGTH_BITS, &length);
    if (cdu_read == 0)
        return -1;
    bit_pos += cdu_read;
    
    /* Validate length */
    if (length == 0)
        return -1;  /* Empty MIX segment is invalid */
    
    /* last_chunk_nbits is NOT read - derivable as (length % 64), 0 means 64 */
    /* Use segment_last_chunk_nbits(length) helper when needed */
    
    *delta_out = delta;
    *length_out = length;
    *bits_read = bit_pos - start_pos;
    
    return 0;
}

/* ============================================================================
 * SEGMENT TYPE DETECTION
 * ============================================================================
 */

/**
 * Read segment type from buffer.
 *
 * @param buf      Input buffer
 * @param bit_pos  Bit position
 * @param buf_bits Total bits available
 * @return SEG_RLE (0), SEG_MIX (1), or -1 on error
 */
int
segment_read_type(const uint8_t *buf, size_t bit_pos, size_t buf_bits)
{
    if (bit_pos >= buf_bits)
        return -1;
    
    return (int)bb_read_bits(buf, bit_pos, 1);
}

/**
 * Check if a run should be encoded as RLE.
 * RLE is used when:
 *   1. The run length >= RARE_RUN_THRESHOLD
 *   2. The run spans the entire segment (no mixed content)
 *
 * @param length_bits  Length of the homogeneous run
 * @param spec         Format specification
 * @return true if RLE should be used
 */
bool
should_use_rle(uint64_t length_bits, const SSKFormatSpec *spec)
{
    return length_bits >= spec->rare_run_threshold;
}

#endif // (NON) TRIVIAL
