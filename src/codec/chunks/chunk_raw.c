/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */

/*
 * src/codec/chunks/chunk_raw.c
 *
 * RAW and RAW_RUN token encoding/decoding for dense chunks
 *
 * RAW token: Used for individual chunks where k > K_CHUNK_ENUM_MAX.
 * Stores the chunk bits directly:
 *   - 2-bit token type (01 = RAW)
 *   - n bits of raw chunk data
 *
 * RAW_RUN token: Coalesces consecutive RAW chunks into a single token.
 * Required by canon: consecutive RAW chunks MUST be coalesced.
 *   - 2-bit token type (10 = RAW_RUN)
 *   - run_length encoded with CDU (CDU_TYPE_SMALL_INT)
 *   - run_length * chunk_bits of raw data
 *   - Optional final incomplete chunk (if segment doesn't end on chunk boundary)
 *
 * Example: 3 consecutive RAW chunks of 64 bits each
 *   - As separate RAW: 3 * (2 + 64) = 198 bits
 *   - As RAW_RUN: 2 + cdu(3) + 3*64 = ~199 bits (plus run_len saves on many chunks)
 *   - More importantly: canonical form requires coalescing
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ssk.h"
#include "ssk_format.h"
#include "bitblocks.h"

/* ============================================================================
 * RAW TOKEN ENCODING
 * ============================================================================
 */

/**
 * Compute bits needed for a RAW token.
 *
 * @param n  Chunk size in bits
 * @return Total bits: 2 (type) + n (raw data)
 */
size_t
raw_token_bits(uint8_t n)
{
    return 2 + n;
}

/**
 * Encode a chunk as a RAW token.
 *
 * @param bits      Chunk bit pattern (right-aligned)
 * @param n         Chunk size in bits (1-64)
 * @param buf       Output buffer
 * @param bit_pos   Starting bit position
 * @return Number of bits written
 */
size_t
raw_token_encode(uint64_t bits, uint8_t n, uint8_t *buf, size_t bit_pos)
{
    size_t start_pos = bit_pos;
    
    /* 1. Write token type (2 bits): 01 = RAW */
    bb_write_bits(buf, bit_pos, TOK_RAW, 2);
    bit_pos += 2;
    
    /* 2. Write raw bits */
    bb_write_bits(buf, bit_pos, bits, n);
    bit_pos += n;
    
    return bit_pos - start_pos;
}

/**
 * Decode a RAW token.
 *
 * @param buf        Input buffer
 * @param bit_pos    Starting bit position (after token type read)
 * @param buf_bits   Total bits available
 * @param n          Chunk size in bits
 * @param bits_out   Output: chunk bit pattern
 * @param bits_read  Output: bits consumed (excluding type)
 * @return 0 on success, -1 on error
 */
int
raw_token_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                 uint8_t n, uint64_t *bits_out, size_t *bits_read)
{
    if (bit_pos + n > buf_bits)
        return -1;  /* Truncated */
    
    *bits_out = bb_read_bits(buf, bit_pos, n);
    *bits_read = n;
    
    return 0;
}

/* ============================================================================
 * RAW_RUN TOKEN ENCODING
 * ============================================================================
 */

/**
 * Compute bits needed for a RAW_RUN token.
 *
 * @param run_len          Number of full chunks in run
 * @param chunk_bits       Bits per full chunk (typically 64)
 * @param final_nbits      Bits in final incomplete chunk (0 if none)
 * @return Total bits needed
 */
size_t
raw_run_token_bits(uint16_t run_len, uint8_t chunk_bits, uint8_t final_nbits)
{
    /* 2 (type) + cdu(run_len) + run_len*chunk_bits + final_nbits */
    uint8_t dummy_buf[8]; // CDU max 8 bytes
    size_t cdu_bits = cdu_encode(run_len, CDU_TYPE_SMALL_INT, dummy_buf, 0);
    size_t data_bits = (size_t)run_len * chunk_bits + final_nbits;
    
    return 2 + cdu_bits + data_bits;
}

/**
 * Encode a RAW_RUN token header (without raw data).
 * Caller must copy raw bits separately.
 *
 * @param run_len    Number of full chunks
 * @param buf        Output buffer
 * @param bit_pos    Starting bit position
 * @return Bits written (type + run_len), ready for raw data copy
 */
size_t
raw_run_header_encode(uint16_t run_len, uint8_t *buf, size_t bit_pos)
{
    size_t start_pos = bit_pos;
    
    /* 1. Write token type (2 bits): 10 = RAW_RUN */
    bb_write_bits(buf, bit_pos, TOK_RAW_RUN, 2);
    bit_pos += 2;
    
    /* 2. Write run length with CDU */
    size_t cdu_bits = cdu_encode(run_len, CDU_TYPE_SMALL_INT, buf, bit_pos);
    bit_pos += cdu_bits;
    
    return bit_pos - start_pos;
}

/**
 * Decode a RAW_RUN token header.
 *
 * @param buf          Input buffer
 * @param bit_pos      Starting bit position (after token type read)
 * @param buf_bits     Total bits available
 * @param run_len_out  Output: number of full chunks
 * @param bits_read    Output: bits consumed for header
 * @return 0 on success, -1 on error
 *
 * After calling this, caller should read run_len * chunk_bits of raw data.
 */
int
raw_run_header_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                      uint16_t *run_len_out, size_t *bits_read)
{
    uint64_t run_len;
    
    size_t cdu_read = cdu_decode(buf, bit_pos, buf_bits, CDU_TYPE_SMALL_INT, &run_len);
    if (cdu_read == 0)
        return -1;
    
    if (run_len > UINT16_MAX)
        return -1;  /* Run too long */
    
    *run_len_out = (uint16_t)run_len;
    *bits_read = cdu_read;
    
    return 0;
}

/**
 * Full RAW_RUN encode: header + data copy.
 *
 * @param src_bits     Source buffer with raw chunk data
 * @param src_bit_pos  Starting bit in source
 * @param run_len      Number of full chunks
 * @param chunk_bits   Bits per chunk
 * @param final_nbits  Bits in final incomplete chunk
 * @param dst          Destination buffer
 * @param dst_bit_pos  Starting bit in destination
 * @return Total bits written
 */
size_t
raw_run_encode(const uint8_t *src_bits, size_t src_bit_pos,
               uint16_t run_len, uint8_t chunk_bits, uint8_t final_nbits,
               uint8_t *dst, size_t dst_bit_pos)
{
    size_t start_pos = dst_bit_pos;
    
    /* Write header */
    size_t header_bits = raw_run_header_encode(run_len, dst, dst_bit_pos);
    dst_bit_pos += header_bits;
    
    /* Copy raw data */
    size_t data_bits = (size_t)run_len * chunk_bits + final_nbits;
    bb_copy_bits(src_bits, src_bit_pos, dst, dst_bit_pos, data_bits);
    dst_bit_pos += data_bits;
    
    return dst_bit_pos - start_pos;
}