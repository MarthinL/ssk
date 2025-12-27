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
 * src/codec/chunks/chunk_enum.c
 *
 * ENUM token encoding/decoding for sparse chunks
 *
 * An ENUM token represents a chunk (n-bit segment of the abvector) where
 * the number of set bits (k) is at most K_CHUNK_ENUM_MAX (18 in Format 0).
 * Instead of storing n bits raw, we store:
 *   - 2-bit token type (00 = ENUM)
 *   - k value in n_bits_for_k bits (6 bits in Format 0)
 *   - combinadic rank in ssk_get_rank_bits(n, k) bits
 *
 * This is efficient when k is small relative to n, because rank needs
 * only ceil(log2(C(n,k))) bits instead of n bits.
 *
 * Example: 64-bit chunk with k=2 set bits
 *   - Raw: 64 bits
 *   - ENUM: 2 (type) + 6 (k) + 11 (rank bits for C(64,2)=2016) = 19 bits
 *   - Savings: 45 bits
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ssk.h"
#include "abv_format.h"
#include "abv_constants.h"
#include "bitblocks.h"

/* ============================================================================
 * ENUM TOKEN ENCODING
 * ============================================================================
 */

/**
 * Compute bits needed to encode an ENUM token.
 *
 * @param n  Chunk size in bits (1-64)
 * @param k  Number of set bits in chunk (0 to K_CHUNK_ENUM_MAX)
 * @return Total bits needed, or 0 if k exceeds K_CHUNK_ENUM_MAX
 *
 * As per Spec Format 0: Token type (2 bits) + k encoding (SSK_N_BITS_FOR_K)
 *                       + combinadic rank (variable by k and n)
 */
size_t
enum_token_bits(uint8_t n, uint8_t k)
{
    if (k > SSK_K_CHUNK_ENUM_MAX)
        return 0;  /* Should use RAW instead */
    
    uint8_t rank_bits = ssk_get_rank_bits(n, k);
    return 2 + SSK_N_BITS_FOR_K + rank_bits;
}

/**
 * Encode a chunk as an ENUM token.
 *
 * @param bits    Chunk bit pattern (right-aligned in uint64_t)
 * @param n       Chunk size in bits (1-64)
 * @param k       Number of set bits (popcount of bits)
 * @param buf     Output buffer
 * @param bit_pos Starting bit position in buffer
 * @return Number of bits written, or 0 on error
 *
 * Precondition: k <= SSK_K_CHUNK_ENUM_MAX
 * Precondition: popcount(bits) == k
 *
 * As per Spec Format 0: Writes token type (2 bits: 00=ENUM),
 *                       k value (SSK_N_BITS_FOR_K bits),
 *                       combinadic rank (variable bits).
 */
size_t
enum_token_encode(uint64_t bits, uint8_t n, uint8_t k,
                  uint8_t *buf, size_t bit_pos)
{
    if (k > SSK_K_CHUNK_ENUM_MAX)
        return 0;  /* Invalid: should use RAW */
    
    size_t start_pos = bit_pos;
    
    /* 1. Token type: 2 bits, value 00 (ENUM) */
    bb_write_bits(buf, bit_pos, TOK_ENUM, 2);
    bit_pos += 2;
    
    /* 2. k value: SSK_N_BITS_FOR_K bits */
    bb_write_bits(buf, bit_pos, k, SSK_N_BITS_FOR_K);
    bit_pos += SSK_N_BITS_FOR_K;
    
    /* 3. Combinadic rank: variable bits based on k and n */
    if (k > 0)
    {
        uint64_t rank = ssk_combinadic_rank(bits, n, k);
        uint8_t rank_bits = ssk_get_rank_bits(n, k);
        bb_write_bits(buf, bit_pos, rank, rank_bits);
        bit_pos += rank_bits;
    }
    /* For k=0, no bits set means rank is implicitly 0 with 0 bits */
    
    return bit_pos - start_pos;
}

/**
 * Decode an ENUM token.
 *
 * @param buf       Input buffer
 * @param bit_pos   Starting bit position (after token type already read)
 * @param buf_bits  Total bits available in buffer
 * @param n         Chunk size in bits (1-64)
 * @param bits_out  Output: reconstructed chunk bit pattern
 * @param k_out     Output: number of set bits
 * @param bits_read Output: total bits consumed (including type if count_type)
 * @return 0 on success, -1 on error
 *
 * Caller has already verified token type is ENUM and consumed those 2 bits.
 * This function reads k and rank.
 *
 * As per Spec Format 0: Reads k (SSK_N_BITS_FOR_K bits) and
 *                       combinadic rank (variable bits).
 */
int
enum_token_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                  uint8_t n, uint64_t *bits_out, uint8_t *k_out, size_t *bits_read)
{
    size_t start_pos = bit_pos;
    
    /* 1. Read k value: SSK_N_BITS_FOR_K bits */
    if (bit_pos + SSK_N_BITS_FOR_K > buf_bits)
        return -1;  /* Truncated */
    
    uint8_t k = (uint8_t)bb_read_bits(buf, bit_pos, SSK_N_BITS_FOR_K);
    bit_pos += SSK_N_BITS_FOR_K;
    
    /* Validate k is in bounds for Format 0 */
    if (k > SSK_K_CHUNK_ENUM_MAX || k > n)
        return -1;  /* Invalid: k exceeds limits */
    
    /* 2. Read combinadic rank: variable bits based on k and n */
    uint64_t rank = 0;
    
    if (k > 0)
    {
        uint8_t rank_bits = ssk_get_rank_bits(n, k);
        
        if (bit_pos + rank_bits > buf_bits)
            return -1;  /* Truncated */
        
        rank = bb_read_bits(buf, bit_pos, rank_bits);
        bit_pos += rank_bits;
        
        /* Validate rank is in valid range */
        if (!ssk_combinadic_rank_valid(rank, n, k))
            return -1;  /* Invalid rank value */
    }
    
    /* 3. Reconstruct bit pattern from rank */
    uint64_t bits = ssk_combinadic_unrank(rank, n, k);
    
    *bits_out = bits;
    *k_out = k;
    *bits_read = bit_pos - start_pos;
    
    return 0;
}

/**
 * Check if a chunk should use ENUM encoding (vs RAW).
 *
 * @param k Number of set bits in chunk
 * @return true if ENUM should be used, false for RAW
 *
 * As per Spec Format 0: ENUM used when k <= SSK_K_CHUNK_ENUM_MAX (18).
 */
bool
should_use_enum(uint8_t k)
{
    return k <= SSK_K_CHUNK_ENUM_MAX;
}

#endif // (NON) TRIVIAL