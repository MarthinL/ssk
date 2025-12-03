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
#include "ssk_format.h"
#include "bitstream.h"

/* ============================================================================
 * ENUM TOKEN ENCODING
 * ============================================================================
 */

/**
 * Compute bits needed to encode an ENUM token.
 *
 * @param n      Chunk size in bits (1-64)
 * @param k      Number of set bits in chunk (0 to K_CHUNK_ENUM_MAX)
 * @param spec   Format specification (for n_bits_for_k)
 * @return Total bits needed, or 0 if k exceeds K_CHUNK_ENUM_MAX
 */
size_t
enum_token_bits(uint8_t n, uint8_t k, const SSKFormatSpec *spec)
{
    if (k > spec->k_enum_max)
        return 0;  /* Should use RAW instead */
    
    /* Token type (2 bits) + k (n_bits_for_k) + rank (rank_bits) */
    uint8_t rank_bits = ssk_get_rank_bits(n, k);
    return 2 + spec->n_bits_for_k + rank_bits;
}

/**
 * Encode a chunk as an ENUM token.
 *
 * @param bits      Chunk bit pattern (right-aligned in uint64_t)
 * @param n         Chunk size in bits (1-64)
 * @param k         Number of set bits (popcount of bits)
 * @param spec      Format specification
 * @param buf       Output buffer
 * @param bit_pos   Starting bit position in buffer
 * @return Number of bits written, or 0 on error
 *
 * Precondition: k <= spec->k_enum_max
 * Precondition: popcount(bits) == k
 */
size_t
enum_token_encode(uint64_t bits, uint8_t n, uint8_t k,
                  const SSKFormatSpec *spec, uint8_t *buf, size_t bit_pos)
{
    if (k > spec->k_enum_max)
        return 0;  /* Invalid: should use RAW */
    
    size_t start_pos = bit_pos;
    
    /* 1. Write token type (2 bits): 00 = ENUM */
    bs_write_bits(buf, bit_pos, TOK_ENUM, 2);
    bit_pos += 2;
    
    /* 2. Write k value in n_bits_for_k bits */
    bs_write_bits(buf, bit_pos, k, spec->n_bits_for_k);
    bit_pos += spec->n_bits_for_k;
    
    /* 3. Compute and write combinadic rank */
    if (k > 0)
    {
        uint64_t rank = ssk_combinadic_rank(bits, n, k);
        uint8_t rank_bits = ssk_get_rank_bits(n, k);
        bs_write_bits(buf, bit_pos, rank, rank_bits);
        bit_pos += rank_bits;
    }
    /* k=0 means no bits set, rank is implicitly 0 with 0 bits */
    
    return bit_pos - start_pos;
}

/**
 * Decode an ENUM token.
 *
 * @param buf        Input buffer
 * @param bit_pos    Starting bit position (after token type already read)
 * @param buf_bits   Total bits available in buffer
 * @param n          Chunk size in bits (1-64)
 * @param spec       Format specification
 * @param bits_out   Output: reconstructed chunk bit pattern
 * @param k_out      Output: number of set bits
 * @param bits_read  Output: total bits consumed (including type if count_type)
 * @return 0 on success, -1 on error
 *
 * Note: Caller has already verified token type is ENUM and consumed those 2 bits.
 * This function reads k and rank.
 */
int
enum_token_decode(const uint8_t *buf, size_t bit_pos, size_t buf_bits,
                  uint8_t n, const SSKFormatSpec *spec,
                  uint64_t *bits_out, uint8_t *k_out, size_t *bits_read)
{
    size_t start_pos = bit_pos;
    
    /* 1. Read k value */
    if (bit_pos + spec->n_bits_for_k > buf_bits)
        return -1;  /* Truncated */
    
    uint8_t k = (uint8_t)bs_read_bits(buf, bit_pos, spec->n_bits_for_k);
    bit_pos += spec->n_bits_for_k;
    
    /* Validate k */
    if (k > spec->k_enum_max || k > n)
        return -1;  /* Invalid: k too large */
    
    /* 2. Read combinadic rank */
    uint64_t rank = 0;
    if (k > 0)
    {
        uint8_t rank_bits = ssk_get_rank_bits(n, k);
        
        if (bit_pos + rank_bits > buf_bits)
            return -1;  /* Truncated */
        
        rank = bs_read_bits(buf, bit_pos, rank_bits);
        bit_pos += rank_bits;
        
        /* Validate rank is in bounds */
        if (!ssk_combinadic_rank_valid(rank, n, k))
            return -1;  /* Invalid rank */
    }
    
    /* 3. Reconstruct bit pattern */
    uint64_t bits = ssk_combinadic_unrank(rank, n, k);
    
    *bits_out = bits;
    *k_out = k;
    *bits_read = bit_pos - start_pos;
    
    return 0;
}

/**
 * Check if a chunk should use ENUM encoding.
 *
 * @param k     Number of set bits in chunk
 * @param spec  Format specification
 * @return true if ENUM should be used, false for RAW
 */
bool
should_use_enum(uint8_t k, const SSKFormatSpec *spec)
{
    return k <= spec->k_enum_max;
}