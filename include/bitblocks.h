/*
 * Copyright (c) 2020-2025 Marthin Laubscher
 * All rights reserved. See LICENSE for details.
 */
#ifndef BITBLOCKS_H
#define BITBLOCKS_H
/*
 * include/bitblocks.h
 *
 * Bit-level operations for SSK encoding/decoding.
 *
 * SSK encodes set membership based on an abstract bit vector (Knuth's term),
 * NOT a SQL bitstring. Key distinction:
 *   - SQL bitstrings: human-readable literals, big-endian (left-to-right)
 *   - Abstract bit vector: machine-efficient set membership, little-endian
 *
 * The abstract bit vector is never fully realized in memory (would be 2^64 bits).
 * SSK exists only as compacted binary encoding.
 *
 * Bit ordering is LSB-first (little-endian), matching PostgreSQL's bitmapset.c:
 *   - Bit position 0 = bit 0 of byte 0 (least significant) = lowest ID
 *   - Bit position 7 = bit 7 of byte 0 (most significant)
 *   - Bit position 8 = bit 0 of byte 1
 *   - etc.
 *
 * This means: element ID maps directly to bit position (ID n → bit n).
 * CTZ (count trailing zeros) finds the lowest ID in a set.
 * CLZ (count leading zeros) finds the highest ID in a set.
 *
 * These functions are inline for performance - they're called frequently in
 * tight encode/decode loops and benefit from direct register access.
 *
 * IMPLEMENTATION NOTES:
 * - Uses 64-bit block operations where possible, not bit-by-bit loops
 * - Leverages GCC/Clang intrinsics for CLZ, CTZ, POPCOUNT
 * - Handles arbitrary bit alignment via shift/mask operations
 */

#ifdef TRIVIAL

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

/*
 * None of what is defined in this header plays a role in the TRIVIAL case.
 * It purely addresses issues arising from upscaling the ID domain to BIGINT scale.
 * 
 * Implementations with buf_bits parameter for compatibility and bounds checking.
 */

/**
 * Write fixed-length block (TRIVIAL version with buf_bits parameter)
 */
static inline void
bb_place_fl_block(uint8_t *dst, size_t dst_bit, uint64_t value, uint8_t n_bits, size_t buf_bits)
{
  /* Fixed-length CDU write, 128-bit optimized with 64-bit unit addressing */
  assert(n_bits <= 64);

  value &= (n_bits < 64) ? ((1ULL << n_bits) - 1) : ~0ULL;

  /* Work in 64-bit units: read 128 bits, modify, write back */
  uint64_t unit_offset = dst_bit / 64;
  unsigned int bit_shift = dst_bit % 64;

  /* Read 128 bits at unit boundary */
  __uint128_t block = *(__uint128_t *)((uint64_t *)dst + unit_offset);

  /* Create mask for bits being replaced */
  __uint128_t mask = ((n_bits < 64) ? ((__uint128_t)((1ULL << n_bits) - 1)) : (__uint128_t)~0ULL) << bit_shift;

  /* Clear old bits, insert new (shifted by bit_shift) */
  block = (block & ~mask) | (((__uint128_t)value << bit_shift) & mask);

  /* Write back 128 bits */
  *(__uint128_t *)((uint64_t *)dst + unit_offset) = block;
}

/**
 * Read fixed-length block (TRIVIAL version with buf_bits parameter)
 */
static inline uint64_t
bb_fetch_fl_block(const uint8_t *src, size_t src_bit, uint8_t n_bits, size_t buf_bits)
{
  assert(n_bits <= 64);

  /* Work in 64-bit units: read 128 bits starting at the 64-bit unit containing src_bit */
  uint64_t unit_offset = src_bit / 64;
  unsigned int bit_shift = src_bit % 64;

  /* Single 128-bit read covers two 64-bit units */
  __uint128_t block = *(__uint128_t *)((uint64_t *)src + unit_offset);

  /* Shift entire register by bit offset within the 64-bit unit */
  block >>= bit_shift;

  /* Extract and mask to requested bit count */
  uint64_t result = (uint64_t)block;
  if (n_bits < 64)
    result &= (1ULL << n_bits) - 1;

  return result;
}

/**
 * Write variable-length encoding (TRIVIAL version with buf_bits parameter)
 */
static inline void
bb_place_vl_encoding(uint8_t *dst, size_t dst_bit, uint64_t value, uint8_t n_bits, size_t buf_bits)
{
  assert((n_bits + (dst_bit % 8)) <= 64);

  /* the only midly tricky part here is to create the masks for the n_bits we want to overwrite */
  uint64_t valmask = (n_bits < 64) ? ((1ULL << n_bits) - 1) : ~0ULL;
  uint64_t dstmask = valmask << (dst_bit % 8);
  uint64_t *temp   = (uint64_t *)(dst + dst_bit / 8);
  *temp = (*temp & ~dstmask) | ((value & valmask) << (dst_bit % 8));
}

/**
 * Read variable-length block (TRIVIAL version with buf_bits parameter)
 */
static inline uint64_t
bb_fetch_vl_block(const uint8_t *src, size_t src_bit, size_t buf_bits)
{
  /* NOTE: This function unconditionally loads 8 bytes for performance.
   * If CDUParam for variable-length types ever exceed 64 bits overall,
   * a special case must be added here to handle larger blocks safely.
   * Currently, variable-length CDUs max at 36 bits (verified manually).
   */

  return *((uint64_t *)(src + src_bit / 8)) >> (src_bit % 8);
}

#else // NON TRIVIAL

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * COMPILER INTRINSICS
 * ============================================================================
 */

/**
 * Count leading zeros in a 64-bit value.
 * Returns 64 if value is 0.
 */
static inline int
bb_clz64(uint64_t x)
{
    return x ? __builtin_clzll(x) : 64;
}

/**
 * Count trailing zeros in a 64-bit value.
 * Returns 64 if value is 0.
 */
static inline int
bb_ctz64(uint64_t x)
{
    return x ? __builtin_ctzll(x) : 64;
}

/**
 * Count set bits (popcount) in a 64-bit value.
 */
static inline int
bb_popcount64(uint64_t x)
{
    return __builtin_popcountll(x);
}

/**
 * Find position of first (lowest) set bit.
 * Returns 0-63 for the bit position, or -1 if no bits set.
 * Position 0 is LSB (lowest ID), position 63 is MSB (highest ID).
 */
static inline int
bb_first_set_bit(uint64_t x)
{
    return x ? bb_ctz64(x) : -1;
}

/**
 * Find position of last (highest) set bit.
 * Returns 0-63 for the bit position, or -1 if no bits set.
 * Position 0 is LSB (lowest ID), position 63 is MSB (highest ID).
 */
static inline int
bb_last_set_bit(uint64_t x)
{
    return x ? (63 - bb_clz64(x)) : -1;
}

/* ============================================================================
 * 64-BIT BLOCK OPERATIONS
 * ============================================================================
 */

/**
 * Load a 64-bit block from buffer at byte-aligned position.
 * Little-endian: LSB of value = byte 0, preserves ID-to-bit mapping.
 */
static inline uint64_t
bb_load_block_aligned(const uint8_t *buf)
{
    uint64_t val;
    memcpy(&val, buf, 8);
    /* Little-endian: no conversion needed on x86/ARM, native order matches */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    val = __builtin_bswap64(val);
#endif
    return val;
}

/**
 * Store a 64-bit block to buffer at byte-aligned position.
 * Little-endian: LSB of value → byte 0, preserves ID-to-bit mapping.
 */
static inline void
bb_store_block_aligned(uint8_t *buf, uint64_t val)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    val = __builtin_bswap64(val);
#endif
    memcpy(buf, &val, 8);
}



/* ============================================================================
 * PUBLIC API - MAIN FUNCTIONS
 * ============================================================================
 */

/**
 * Write bits to a buffer at a specified bit position.
 * 128-bit optimized: reads 128 bits, masks and shifts value, writes back.
 *
 * @param dst       Destination buffer (must have space for bits being written)
 * @param dst_bit   Starting bit position in destination (0 = LSB of byte 0)
 * @param value     Value to write (right-aligned, i.e., LSBs contain the data)
 * @param n_bits    Number of bits to write (0-64)
 */
static inline void
bb_write_bits(uint8_t *dst, size_t dst_bit, uint64_t value, uint8_t n_bits)
{
    if (n_bits == 0)
        return;
    
    /* Work in 64-bit units: read 128 bits, modify, write back */
    uint64_t unit_offset = dst_bit / 64;
    unsigned int bit_shift = dst_bit % 64;
    
    /* Mask value to n_bits */
    if (n_bits < 64)
        value &= (1ULL << n_bits) - 1;
    
    /* Read 128 bits at unit boundary */
    __uint128_t block = *(__uint128_t *)((uint64_t *)dst + unit_offset);
    
    /* Create mask for bits being replaced */
    __uint128_t mask = ((n_bits < 64) ? ((__uint128_t)((1ULL << n_bits) - 1)) : (__uint128_t)~0ULL) << bit_shift;
    
    /* Clear old bits, insert new (shifted by bit_shift) */
    block = (block & ~mask) | (((__uint128_t)value << bit_shift) & mask);
    
    /* Write back 128 bits */
    *(__uint128_t *)((uint64_t *)dst + unit_offset) = block;
}

/**
 * Read bits from a buffer at a specified bit position.
 * 128-bit optimized: reads 128 bits, shifts by bit offset, masks to n_bits.
 *
 * @param src       Source buffer
 * @param src_bit   Starting bit position in source (0 = LSB of byte 0)
 * @param n_bits    Number of bits to read (0-64)
 * @return Value with bits right-aligned (LSBs contain the data)
 */
static inline uint64_t
bb_read_bits(const uint8_t *src, size_t src_bit, uint8_t n_bits)
{
    if (n_bits == 0)
        return 0;

    /* Work in 64-bit units: read 128 bits starting at the 64-bit unit containing src_bit */
    uint64_t unit_offset = src_bit / 64;
    unsigned int bit_shift = src_bit % 64;

    /* Single 128-bit read covers two 64-bit units */
    __uint128_t block = *(__uint128_t *)((uint64_t *)src + unit_offset);

    /* Shift entire register by bit offset within the 64-bit unit */
    block >>= bit_shift;

    /* Extract final 64-bit value */
    uint64_t result = (uint64_t)block;

    /* Mask to n_bits */
    if (n_bits < 64)
        result &= (1ULL << n_bits) - 1;

    return result;
}

/**
 * Write the data of a UDC encoding to a buffer at a specified bit position.
 *
 * @param dst       Destination buffer (must have space for bits being written)
 * @param dst_bit   Starting bit position in destination (0 = LSB of byte 0)
 * @param value     Value to write (right-aligned, i.e., LSBs contain the data)
 * @param n_bits    Number of bits to write (0-64)
 */
static inline void
bb_place_vl_encoding(uint8_t *dst, size_t dst_bit, uint64_t value, uint8_t n_bits)
{

  assert((n_bits + (dst_bit % 8)) <= 64);

  // the only midly tricky part here is to create the masks for the n_bits we want to overwrite
  uint64_t valmask = (n_bits < 64) ? ((1ULL << n_bits) - 1) : ~0ULL;
  uint64_t dstmask = valmask << dst_bit % 8;
  uint64_t *temp   = (uint64_t *)(dst + dst_bit / 8);
  *temp = (*temp & ~dstmask) | ((value & valmask) << (dst_bit % 8));
}

/**
 * Read data for a UDC at a specified bit position.
 *
 * @param src       Source buffer
 * @param src_bit   Starting bit position in source (0 = LSB of byte 0)
 * @return Value with bits right-aligned (LSBs contain the data)
 */
static inline uint64_t
bb_fetch_vl_block(const uint8_t *src, size_t src_bit)
{
  // NOTE: This function unconditionally loads 8 bytes for performance.
  // If CDUParam for variable-length types ever exceed 64 bits overall,
  // a special case must be added here to handle larger blocks safely.
  // Currently, variable-length CDUs max at 36 bits (verified manually).

  return *((uint64_t *)(src + src_bit / 8)) >> (src_bit % 8);
}


/**
 * Write the data of a UDC encoding to a buffer at a specified bit position.
 *
 * @param dst       Destination buffer (must have space for bits being written)
 * @param dst_bit   Starting bit position in destination (0 = LSB of byte 0)
 * @param value     Value to write (right-aligned, i.e., LSBs contain the data)
 * @param n_bits    Number of bits to write (0-64)
 */
static inline void
bb_place_fl_block(uint8_t *dst, size_t dst_bit, uint64_t value, uint8_t n_bits)
{
  /* Fixed-length CDU write, 128-bit optimized with 64-bit unit addressing */
  assert(n_bits <= 64);

  value &= (n_bits < 64) ? ((1ULL << n_bits) - 1) : ~0ULL;

  /* Work in 64-bit units: read 128 bits, modify, write back */
  uint64_t unit_offset = dst_bit / 64;
  unsigned int bit_shift = dst_bit % 64;

  /* Read 128 bits at unit boundary */
  __uint128_t block = *(__uint128_t *)((uint64_t *)dst + unit_offset);

  /* Create mask for bits being replaced */
  __uint128_t mask = ((n_bits < 64) ? ((__uint128_t)((1ULL << n_bits) - 1)) : (__uint128_t)~0ULL) << bit_shift;

  /* Clear old bits, insert new (shifted by bit_shift) */
  block = (block & ~mask) | (((__uint128_t)value << bit_shift) & mask);

  /* Write back 128 bits */
  *(__uint128_t *)((uint64_t *)dst + unit_offset) = block;
}

/**
 * Read data for a UDC at a specified bit position (128-bit optimized).
 *
 * @param src       Source buffer
 * @param src_bit   Starting bit position in source (0 = LSB of byte 0)
 * @param n_bits    Number of bits to read (0-64)
 * @return Value with bits right-aligned (LSBs contain the data)
 *
 * Uses 128-bit load when possible for faster data extraction from aligned buffers.
 * Falls back to 64-bit + 8-bit for small reads.
 */
static inline uint64_t
bb_fetch_fl_block(const uint8_t *src, size_t src_bit, uint8_t n_bits)
{
  assert(n_bits <= 64);

  /* Work in 64-bit units: read 128 bits starting at the 64-bit unit containing src_bit */
  uint64_t unit_offset = src_bit / 64;
  unsigned int bit_shift = src_bit % 64;

  /* Single 128-bit read covers two 64-bit units */
  __uint128_t block = *(__uint128_t *)((uint64_t *)src + unit_offset);

  /* Shift entire register by bit offset within the 64-bit unit */
  block >>= bit_shift;

  /* Extract and mask to requested bit count */
  uint64_t result = (uint64_t)block;
  if (n_bits < 64)
    result &= (1ULL << n_bits) - 1;

  return result;
}



/**
 * Copy bits from one buffer position to another.
 *
 * @param src       Source buffer
 * @param src_bit   Starting bit position in source
 * @param dst       Destination buffer (may be same as src if ranges don't overlap)
 * @param dst_bit   Starting bit position in destination
 * @param n_bits    Number of bits to copy
 */
static inline void
bb_copy_bits(const uint8_t *src, size_t src_bit,
             uint8_t *dst, size_t dst_bit, size_t n_bits)
{
    /* Copy in 64-bit chunks where possible */
    while (n_bits >= 64)
    {
        uint64_t block = bb_read_bits(src, src_bit, 64);
        bb_write_bits(dst, dst_bit, block, 64);
        src_bit += 64;
        dst_bit += 64;
        n_bits -= 64;
    }
    
    /* Handle remaining bits */
    if (n_bits > 0)
    {
        uint64_t block = bb_read_bits(src, src_bit, (uint8_t)n_bits);
        bb_write_bits(dst, dst_bit, block, (uint8_t)n_bits);
    }
}

/**
 * Compute bytes needed to hold n bits.
 *
 * @param n_bits Number of bits
 * @return Minimum bytes needed (ceiling division)
 */
static inline size_t
bb_bytes_for_bits(size_t n_bits)
{
    return (n_bits + 7) / 8;
}

/**
 * Set a single bit in a buffer.
 *
 * @param buf      Buffer
 * @param bit_pos  Bit position (0 = LSB of byte 0 = lowest ID)
 */
static inline void
bb_set_bit(uint8_t *buf, size_t bit_pos)
{
    size_t byte_idx = bit_pos / 8;
    unsigned int bit_idx = bit_pos % 8;
    buf[byte_idx] |= (1 << bit_idx);
}

/**
 * Clear a single bit in a buffer.
 *
 * @param buf      Buffer
 * @param bit_pos  Bit position (0 = LSB of byte 0 = lowest ID)
 */
static inline void
bb_clear_bit(uint8_t *buf, size_t bit_pos)
{
    size_t byte_idx = bit_pos / 8;
    unsigned int bit_idx = bit_pos % 8;
    buf[byte_idx] &= ~(1 << bit_idx);
}

/**
 * Test a single bit in a buffer.
 *
 * @param buf      Buffer
 * @param bit_pos  Bit position (0 = LSB of byte 0 = lowest ID)
 * @return 1 if bit is set, 0 if clear
 */
static inline int
bb_test_bit(const uint8_t *buf, size_t bit_pos)
{
    size_t byte_idx = bit_pos / 8;
    unsigned int bit_idx = bit_pos % 8;
    return (buf[byte_idx] >> bit_idx) & 1;
}

/* ============================================================================
 * BLOCK ANALYSIS PRIMITIVES (POLARITY-AWARE, PARTIAL-BLOCK)
 * ============================================================================
 *
 * SSK encoding works in terms of "dominant" and "rare" bits, not "set" and
 * "unset". The dominant bit is whichever value (0 or 1) appears more often
 * in the data - this determines encoding polarity.
 *
 * These primitives:
 *   1. Accept a bit count (n_bits) for partial blocks - only the LOW n_bits
 *      are valid, the rest are masked out before analysis.
 *   2. Accept a dominant parameter (0 or 1) to define polarity.
 *   3. Return positions relative to LSB (position 0 = bit 0 = lowest ID).
 *
 * A "rare" bit is any bit that differs from the dominant value.
 * When dominant=1, rare bits are 0s. When dominant=0, rare bits are 1s.
 */

/**
 * Mask a block to its valid low bits.
 * Given n_bits valid bits, clears the high (64-n_bits) bits.
 *
 * @param block   The 64-bit block (data in LOW bits, LSB-first)
 * @param n_bits  Number of valid bits (1-64), positioned at LSB
 * @return Block with only valid bits, rest cleared
 */
static inline uint64_t
bb_mask_block(uint64_t block, uint8_t n_bits)
{
    if (n_bits >= 64)
        return block;
    if (n_bits == 0)
        return 0;
    /* Clear high (64 - n_bits) bits */
    return block & ((1ULL << n_bits) - 1);
}

/**
 * Get a block with rare bits marked as 1s.
 * Converts the block based on polarity so that 1s represent rare positions.
 *
 * @param block     The 64-bit block (masked to valid bits)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Block where 1s mark rare bit positions
 */
static inline uint64_t
bb_rare_bits(uint64_t block, int dominant)
{
    return dominant ? ~block : block;
}

/**
 * Count rare bits in a partial block.
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Number of rare bits in the valid portion
 */
static inline int
bb_count_rare(uint64_t block, uint8_t n_bits, int dominant)
{
    uint64_t masked = bb_mask_block(block, n_bits);
    uint64_t rare = bb_rare_bits(masked, dominant);
    /* If dominant=1, we inverted, so need to re-mask (inversion flips low bits too) */
    if (dominant)
        rare = bb_mask_block(rare, n_bits);
    return bb_popcount64(rare);
}

/**
 * Find position of first (lowest) rare bit in a partial block.
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Position 0-(n_bits-1) of first rare bit, or -1 if none
 *         Position 0 is LSB (lowest ID).
 */
static inline int
bb_first_rare(uint64_t block, uint8_t n_bits, int dominant)
{
    uint64_t masked = bb_mask_block(block, n_bits);
    uint64_t rare = bb_rare_bits(masked, dominant);
    if (dominant)
        rare = bb_mask_block(rare, n_bits);
    
    if (rare == 0)
        return -1;
    
    /* CTZ gives position from LSB (lowest ID) */
    return bb_ctz64(rare);
}

/**
 * Find position of last (highest) rare bit in a partial block.
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Position 0-(n_bits-1) of last rare bit, or -1 if none
 *         Position 0 is LSB (lowest ID).
 */
static inline int
bb_last_rare(uint64_t block, uint8_t n_bits, int dominant)
{
    uint64_t masked = bb_mask_block(block, n_bits);
    uint64_t rare = bb_rare_bits(masked, dominant);
    if (dominant)
        rare = bb_mask_block(rare, n_bits);
    
    if (rare == 0)
        return -1;
    
    /* CLZ from MSB, convert to position from LSB */
    return 63 - bb_clz64(rare);
}

/**
 * Check if a partial block is homogeneous (all dominant bits).
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @return 1 if all valid bits are the same (all 0 or all 1), 0 otherwise
 *         Also returns the dominant value via pointer if non-NULL.
 */
static inline int
bb_is_homogeneous(uint64_t block, uint8_t n_bits, int *dominant_out)
{
    uint64_t masked = bb_mask_block(block, n_bits);
    
    /* All zeros? */
    if (masked == 0)
    {
        if (dominant_out) *dominant_out = 0;
        return 1;
    }
    
    /* All ones in valid region? */
    uint64_t all_ones = (n_bits >= 64) ? ~0ULL : ((1ULL << n_bits) - 1);
    if (masked == all_ones)
    {
        if (dominant_out) *dominant_out = 1;
        return 1;
    }
    
    return 0;
}

/**
 * Determine dominant bit value for a partial block.
 * Returns whichever value (0 or 1) appears more often.
 * Ties go to 0 (arbitrary but consistent).
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @return 0 if zeros dominate or tie, 1 if ones dominate
 */
static inline int
bb_dominant_bit(uint64_t block, uint8_t n_bits)
{
    uint64_t masked = bb_mask_block(block, n_bits);
    int ones = bb_popcount64(masked);
    int zeros = n_bits - ones;
    return (ones > zeros) ? 1 : 0;
}

/**
 * Full block analysis structure for encoding decisions.
 */
typedef struct {
    uint64_t value;        /* The masked block value */
    uint8_t  n_bits;       /* Valid bits in this block */
    int      dominant;     /* Dominant bit value (0 or 1) */
    int      rare_count;   /* Number of rare bits */
    int      first_rare;   /* Position of first rare bit, or -1 */
    int      last_rare;    /* Position of last rare bit, or -1 */
} BlockAnalysis;

/**
 * Analyze a partial block for encoding decisions.
 * Determines polarity automatically based on bit counts.
 *
 * @param block   The 64-bit block (data in LOW bits)
 * @param n_bits  Number of valid bits (1-64)
 * @return Full analysis with polarity, rare bit positions, etc.
 */
static inline BlockAnalysis
bb_analyze_block(uint64_t block, uint8_t n_bits)
{
    BlockAnalysis a;
    a.value = bb_mask_block(block, n_bits);
    a.n_bits = n_bits;
    a.dominant = bb_dominant_bit(a.value, n_bits);
    a.rare_count = bb_count_rare(a.value, n_bits, a.dominant);
    a.first_rare = bb_first_rare(a.value, n_bits, a.dominant);
    a.last_rare = bb_last_rare(a.value, n_bits, a.dominant);
    return a;
}

/**
 * Analyze a partial block with explicit polarity.
 * Use when polarity is predetermined (e.g., from partition header).
 *
 * @param block     The 64-bit block (data in LOW bits)
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  Predetermined dominant bit value
 * @return Full analysis using the given polarity
 */
static inline BlockAnalysis
bb_analyze_block_polar(uint64_t block, uint8_t n_bits, int dominant)
{
    BlockAnalysis a;
    a.value = bb_mask_block(block, n_bits);
    a.n_bits = n_bits;
    a.dominant = dominant;
    a.rare_count = bb_count_rare(a.value, n_bits, dominant);
    a.first_rare = bb_first_rare(a.value, n_bits, dominant);
    a.last_rare = bb_last_rare(a.value, n_bits, dominant);
    return a;
}

/* ============================================================================
 * RUN DETECTION (FOR RLE ENCODING)
 * ============================================================================
 */

/**
 * Count leading dominant bits in a partial block.
 * Returns how many consecutive dominant bits from the LSB (lowest ID).
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Count of leading dominant bits (0 to n_bits)
 */
static inline int
bb_leading_dominant(uint64_t block, uint8_t n_bits, int dominant)
{
    int first = bb_first_rare(block, n_bits, dominant);
    return (first < 0) ? n_bits : first;
}

/**
 * Count trailing dominant bits in a partial block.
 * Returns how many consecutive dominant bits from the MSB (highest ID) of valid region.
 *
 * @param block     The 64-bit block
 * @param n_bits    Number of valid bits (1-64)
 * @param dominant  The dominant bit value (0 or 1)
 * @return Count of trailing dominant bits (0 to n_bits)
 */
static inline int
bb_trailing_dominant(uint64_t block, uint8_t n_bits, int dominant)
{
    int last = bb_last_rare(block, n_bits, dominant);
    return (last < 0) ? n_bits : (n_bits - 1 - last);
}

#endif // (NON) TRIVIAL

#endif /* BITBLOCKS_H */
